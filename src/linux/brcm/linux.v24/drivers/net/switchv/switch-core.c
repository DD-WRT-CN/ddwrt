/*
 * switch-core.c
 *
 * Copyright (C) 2005 Felix Fietkau <openwrt@nbd.name>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id: $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/list.h>

#include "switch-core.h"


static struct proc_dir_entry *switch_root;
switch_driver drivers;

typedef struct {
	struct list_head list;
	struct proc_dir_entry *parent;
	int nr;
	switch_config handler;
} switch_proc_handler;

typedef struct {
	struct proc_dir_entry *driver_dir, *port_dir, *vlan_dir;
	struct proc_dir_entry **ports, **vlans;
	switch_proc_handler data;
} switch_priv;


static ssize_t switch_proc_read(struct file *file, char *buf, size_t count, loff_t *ppos);
static ssize_t switch_proc_write(struct file *file, const char *buf, size_t count, void *data);
static struct file_operations switch_proc_fops = {
	read: switch_proc_read,
	write: switch_proc_write
};


static char *strdup(char *str)
{
	char *new = kmalloc(strlen(str) + 1, GFP_KERNEL);
	strcpy(new, str);
	return new;
}

static ssize_t switch_proc_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry *dent = inode->u.generic_ip;
	char *page;
	int len = 0;
	
	if ((page = kmalloc(SWITCH_MAX_BUFSZ, GFP_KERNEL)) == NULL)
		return -ENOBUFS;
	
	if (dent->data != NULL) {
		switch_proc_handler *handler = (switch_proc_handler *) dent->data;
		if (handler->handler.read != NULL)
			len += handler->handler.read(page + len, handler->nr);
	}
	len += 1;

	if (*ppos < len) {
		len = min_t(int, len - *ppos, count);
		if (copy_to_user(buf, (page + *ppos), len)) {
			kfree(page);
			return -EFAULT;
		}
		*ppos += len;
	} else {
		len = 0;
	}

	return len;
}


static ssize_t switch_proc_write(struct file *file, const char *buf, size_t count, void *data)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct proc_dir_entry *dent = inode->u.generic_ip;
	char *page;
	int ret = -EINVAL;

	if ((page = kmalloc(count + 1, GFP_KERNEL)) == NULL)
		return -ENOBUFS;

	if (copy_from_user(page, buf, count)) {
		kfree(page);
		return -EINVAL;
	}
	page[count] = 0;
	
	if (dent->data != NULL) {
		switch_proc_handler *handler = (switch_proc_handler *) dent->data;
		if (handler->handler.write != NULL) {
			if ((ret = handler->handler.write(page, handler->nr)) >= 0)
				ret = count;
		}
	}

	kfree(page);
	return ret;
}

static void add_handlers(switch_priv *priv, switch_config *handlers, struct proc_dir_entry *parent, int nr)
{
	switch_proc_handler *tmp;
	int i, mode;
	struct proc_dir_entry *p;
	
	for (i = 0; handlers[i].name != NULL; i++) {
		tmp = kmalloc(sizeof(switch_proc_handler), GFP_KERNEL);
		INIT_LIST_HEAD(&tmp->list);
		tmp->parent = parent;
		tmp->nr = nr;
		memcpy(&tmp->handler, &(handlers[i]), sizeof(switch_config));
		list_add(&tmp->list, &priv->data.list);
		
		mode = 0;
		if (handlers[i].read != NULL) mode |= S_IRUSR;
		if (handlers[i].write != NULL) mode |= S_IWUSR;
		
		if ((p = create_proc_entry(handlers[i].name, mode, parent)) != NULL) {
			p->data = (void *) tmp;
			p->proc_fops = &switch_proc_fops;
		}
	}
}		

static void remove_handlers(switch_priv *priv)
{
	struct list_head *pos, *q;
	switch_proc_handler *tmp;

	list_for_each_safe(pos, q, &priv->data.list) {
		tmp = list_entry(pos, switch_proc_handler, list);
		list_del(pos);
		remove_proc_entry(tmp->handler.name, tmp->parent);
		kfree(tmp);
	}
}


static void do_unregister(switch_driver *driver)
{
	char buf[4];
	int i;
	switch_priv *priv = (switch_priv *) driver->data;

	for(i = 0; priv->ports[i] != NULL; i++) {
		sprintf(buf, "%d", i);
		remove_proc_entry(buf, priv->port_dir);
	}
	kfree(priv->ports);
	remove_proc_entry("port", priv->driver_dir);

	for(i = 0; priv->vlans[i] != NULL; i++) {
		sprintf(buf, "%d", i);
		remove_proc_entry(buf, priv->vlan_dir);
	}
	kfree(priv->vlans);
	remove_proc_entry("vlan", priv->driver_dir);

	remove_proc_entry(driver->name, switch_root);
	kfree(priv);
	
	remove_handlers(priv);
}

static int do_register(switch_driver *driver)
{
	switch_priv *priv;
	int i;
	char buf[4];
	
	if ((priv = kmalloc(sizeof(switch_priv), GFP_KERNEL)) == NULL)
		return -ENOBUFS;
	driver->data = (void *) priv;

	INIT_LIST_HEAD(&priv->data.list);
	
	priv->driver_dir = proc_mkdir(driver->name, switch_root);
	if (driver->driver_handlers != NULL)
		add_handlers(priv, driver->driver_handlers, priv->driver_dir, 0);
	
	priv->port_dir = proc_mkdir("port", priv->driver_dir);
	priv->ports = kmalloc((driver->ports + 1) * sizeof(struct proc_dir_entry *), GFP_KERNEL);
	for (i = 0; i < driver->ports; i++) {
		sprintf(buf, "%d", i);
		priv->ports[i] = proc_mkdir(buf, priv->port_dir);
		if (driver->port_handlers != NULL)
			add_handlers(priv, driver->port_handlers, priv->ports[i], i);
	}
	priv->ports[i] = NULL;
	
	priv->vlan_dir = proc_mkdir("vlan", priv->driver_dir);
	priv->vlans = kmalloc((driver->vlans + 1) * sizeof(struct proc_dir_entry *), GFP_KERNEL);
	for (i = 0; i < driver->vlans; i++) {
		sprintf(buf, "%d", i);
		priv->vlans[i] = proc_mkdir(buf, priv->vlan_dir);
		if (driver->vlan_handlers != NULL)
			add_handlers(priv, driver->vlan_handlers, priv->vlans[i], i);
	}
	priv->vlans[i] = NULL;

	return 0;
}

static int isspace(char c) {
	switch(c) {
		case ' ':
		case 0x09:
		case 0x0a:
		case 0x0d:
			return 1;
		default:
			return 0;
	}
}

#define toupper(c) (islower(c) ? ((c) ^ 0x20) : (c))
#define islower(c) (((unsigned char)((c) - 'a')) < 26)
														 
int parse_media(char *buf)
{
	char *str = buf;
	while (*buf != 0) {
		*buf = toupper(*buf);
		buf++;
	}

	if (strncmp(str, "AUTO", 4) == 0)
		return SWITCH_MEDIA_AUTO;
	else if (strncmp(str, "100FD", 5) == 0)
		return SWITCH_MEDIA_100 | SWITCH_MEDIA_FD;
	else if (strncmp(str, "100HD", 5) == 0)
		return SWITCH_MEDIA_100;
	else if (strncmp(str, "10FD", 4) == 0)
		return SWITCH_MEDIA_FD;
	else if (strncmp(str, "10HD", 4) == 0)
		return 0;
	else return -1;
}

int print_media(char *buf, int media)
{
	int len = 0;

	if (media & SWITCH_MEDIA_AUTO)
		len = sprintf(buf, "Auto");
	else if (media == (SWITCH_MEDIA_100 | SWITCH_MEDIA_FD))
		len = sprintf(buf, "100FD");
	else if (media == SWITCH_MEDIA_100)
		len = sprintf(buf,  "100HD");
	else if (media == SWITCH_MEDIA_FD)
		len = sprintf(buf,  "10FD");
	else if (media == 0)
		len = sprintf(buf,  "10HD");
	else
		len = sprintf(buf, "Invalid");

	return len;
}

int parse_vlan_ports(char *buf)
{
	char vlan = 0, tag = 0, pvid_port = 0;
	int untag, j;

	while (isspace(*buf)) buf++;
	
	while (*buf >= '0' && *buf <= '9') {
		j = *buf++ - '0';
		vlan |= 1 << j;
		
		untag = 0;
		/* untag if needed, CPU port requires special handling */
		if (*buf == 'u' || (j != 5 && (isspace(*buf) || *buf == 0))) {
			untag = 1;
			if (*buf) buf++;
		} else if (*buf == '*') {
			pvid_port |= (1 << j);
			buf++;
		} else if (*buf == 't' || isspace(*buf)) {
			buf++;
		} else break;

		if (!untag)
			tag |= 1 << j;
		
		while (isspace(*buf)) buf++;
	}
	
	if (*buf)
		return -1;

	return (pvid_port << 16) | (tag << 8) | vlan;
}


int switch_register_driver(switch_driver *driver)
{
	struct list_head *pos;
	switch_driver *new;
	int ret;
	
	list_for_each(pos, &drivers.list) {
		if (strcmp(list_entry(pos, switch_driver, list)->name, driver->name) == 0) {
			printk("Switch driver '%s' already exists in the kernel\n", driver->name);
			return -EINVAL;
		}
	}
		
	MOD_INC_USE_COUNT;

	new = kmalloc(sizeof(switch_driver), GFP_KERNEL);
	memcpy(new, driver, sizeof(switch_driver));
	new->name = strdup(driver->name);
	
	if ((ret = do_register(new)) < 0) {
		kfree(new->name);
		kfree(new);
		return ret;
	}
	INIT_LIST_HEAD(&new->list);
	list_add(&new->list, &drivers.list);

	return 0;
}

void switch_unregister_driver(char *name) {
	struct list_head *pos, *q;
	switch_driver *tmp;

	list_for_each_safe(pos, q, &drivers.list) {
		tmp = list_entry(pos, switch_driver, list);
		if (strcmp(tmp->name, name) == 0) {
			do_unregister(tmp);
			list_del(pos);
			kfree(tmp->name);
			kfree(tmp);

			MOD_DEC_USE_COUNT;

			return;
		}
	}
}

static int __init switch_init()
{
	if ((switch_root = proc_mkdir("switch", NULL)) == NULL) {
		printk("%s: proc_mkdir failed.\n", __FILE__);
		return -ENODEV;
	}

	INIT_LIST_HEAD(&drivers.list);
	
	printk("%s: loaded\n", __FILE__);
	return 0;
}

static void __exit switch_exit()
{
	
	remove_proc_entry("vlan", NULL);
	printk("%s: unloaded\n", __FILE__);
}

MODULE_AUTHOR("Felix Fietkau <openwrt@nbd.name>");
MODULE_LICENSE("GPL");

EXPORT_SYMBOL(switch_register_driver);
EXPORT_SYMBOL(switch_unregister_driver);
EXPORT_SYMBOL(parse_vlan_ports);
EXPORT_SYMBOL(parse_media);
EXPORT_SYMBOL(print_media);

module_init(switch_init);
module_exit(switch_exit);
