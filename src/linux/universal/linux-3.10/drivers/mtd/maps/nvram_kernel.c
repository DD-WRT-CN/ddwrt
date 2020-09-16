
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/bootmem.h>
#include "wrapper.h"
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mtd/mtd.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
//#include <asm/addrspace.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "nvram_linux.h"

void *MALLOC(size_t size)
{
	void *ptr = kmalloc(size, GFP_ATOMIC | __GFP_NOWARN);
	if (!ptr)
		ptr = vmalloc(size);
	return ptr;
}

void MFREE(void *ptr)
{
	if (is_vmalloc_addr(ptr))
		vfree(ptr);
	else
		kfree(ptr);
}

/* In BSS to minimize text size and page aligned so it can be mmap()-ed */
static char nvram_buf[NVRAM_SPACE] __attribute__((aligned(PAGE_SIZE)));

extern char *_nvram_get(const char *name);
extern int _nvram_set(const char *name, const char *value);
extern int _nvram_unset(const char *name);
extern int _nvram_getall(char *buf, int count);
extern int _nvram_commit(struct nvram_header *header);
extern int _nvram_init(void);
extern void _nvram_exit(void);

/* Globals */
DEFINE_SPINLOCK(nvram_lock);
static struct mutex nvram_sem;
static unsigned long nvram_offset = 0;
static int nvram_major = -1;
//static devfs_handle_t nvram_handle = NULL;
static struct mtd_info *nvram_mtd = NULL;

int _nvram_read(char *buf)
{
	size_t len;
	struct nvram_header *header = (struct nvram_header *)buf;
	if (nvram_mtd) {
		mtd_read(nvram_mtd, nvram_mtd->size - NVRAM_SPACE, NVRAM_SPACE, &len, buf);
		if (header->magic != NVRAM_MAGIC) {
			mtd_read(nvram_mtd, nvram_mtd->size - (NVRAM_SPACE / 2), (NVRAM_SPACE / 2), &len, buf);
			if (header->magic == NVRAM_MAGIC)
				printk(KERN_INFO "convert old nvram to new one\n");
			else
				printk(KERN_INFO "nvram empty\n");
		}
	}
	return 0;
}

struct nvram_tuple *_nvram_realloc(struct nvram_tuple *t, const char *name, const char *value)
{
	if ((nvram_offset + strlen(value) + 1) > NVRAM_SPACE)
		return NULL;

	if (!t) {
		if (!(t = MALLOC(sizeof(struct nvram_tuple) + strlen(name) + 1)))
			return NULL;

		/* Copy name */
		t->name = (char *)&t[1];
		strcpy(t->name, name);

		t->value = NULL;
	}

	/* Copy value */
	if (t->value == NULL || strlen(t->value) < strlen(value)) {
		/* Alloc value space */
		t->value = &nvram_buf[nvram_offset];
		strcpy(t->value, value);
		nvram_offset += strlen(value) + 1;
	} else if (0 != strcmp(t->value, value)) {
		/* In place */
		strcpy(t->value, value);
	}

	return t;
}

void _nvram_free(struct nvram_tuple *t)
{
	if (!t) {
		nvram_offset = 0;
		memset(nvram_buf, 0, sizeof(nvram_buf));
	} else {
		MFREE(t);
	}
}

int nvram_set(const char *name, const char *value)
{
	unsigned long flags;
	int ret;
	struct nvram_header *header;

	spin_lock_irqsave(&nvram_lock, flags);
	if ((ret = _nvram_set(name, value))) {
		/* Consolidate space and try again */
		if ((header = MALLOC(NVRAM_SPACE))) {
			if (_nvram_commit(header) == 0)
				ret = _nvram_set(name, value);
			MFREE(header);
		}
	}
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

char *real_nvram_get(const char *name)
{
	unsigned long flags;
	char *value;

	spin_lock_irqsave(&nvram_lock, flags);
	value = _nvram_get(name);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return value;
}

char *nvram_get(const char *name)
{
	if (nvram_major >= 0)
		return real_nvram_get(name);
	else
		return NULL;
}

int nvram_unset(const char *name)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_unset(name);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

static void erase_callback(struct erase_info *done)
{
	wait_queue_head_t *wait_q = (wait_queue_head_t *) done->priv;
	wake_up(wait_q);
}

int nvram_commit(void)
{
	char *buf;
	size_t erasesize, len;
	unsigned int i;
	int ret;
	struct nvram_header *header;
	unsigned long flags;
	u_int32_t offset;
	DECLARE_WAITQUEUE(wait, current);
	wait_queue_head_t wait_q;
	struct erase_info erase;
//      printk(KERN_EMERG "commit\n");

	if (!nvram_mtd) {
		printk("nvram_commit: NVRAM not found\n");
		return -ENODEV;
	}

	if (in_interrupt()) {
		printk("nvram_commit: not committing in interrupt\n");
		return -EINVAL;
	}

	/* Backup sector blocks to be erased */
	erasesize = ROUNDUP(NVRAM_SPACE, nvram_mtd->erasesize);
	if (!(buf = MALLOC(erasesize))) {
		printk("nvram_commit: out of memory\n");
		return -ENOMEM;
	}

	mutex_lock(&nvram_sem);

	if ((i = erasesize - NVRAM_SPACE) > 0) {
		offset = nvram_mtd->size - erasesize;
		len = 0;
		ret = mtd_read(nvram_mtd, offset, i, &len, buf);
		if (ret || len != i) {
			printk("nvram_commit: read error ret = %d, len = %lu/%d\n", ret, len, i);
			ret = -EIO;
			goto done;
		}
		header = (struct nvram_header *)(buf + i);
	} else {
		offset = nvram_mtd->size - NVRAM_SPACE;
		header = (struct nvram_header *)buf;
	}

	/* Regenerate NVRAM */
	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_commit(header);
	spin_unlock_irqrestore(&nvram_lock, flags);
	if (ret)
		goto done;

	/* Erase sector blocks */
	init_waitqueue_head(&wait_q);
	for (; offset < nvram_mtd->size - NVRAM_SPACE + header->len; offset += nvram_mtd->erasesize) {
		erase.mtd = nvram_mtd;
		erase.addr = offset;
		erase.len = nvram_mtd->erasesize;
		erase.callback = erase_callback;
		erase.priv = (u_long) & wait_q;

		set_current_state(TASK_INTERRUPTIBLE);
		add_wait_queue(&wait_q, &wait);

		/* Unlock sector blocks */
		mtd_unlock(nvram_mtd, offset, nvram_mtd->erasesize);

		if ((ret = mtd_erase(nvram_mtd, &erase))) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&wait_q, &wait);
			printk("nvram_commit: erase error\n");
			goto done;
		}

		/* Wait for erase to finish */
		schedule();
		remove_wait_queue(&wait_q, &wait);
	}

	/* Write partition up to end of data area */
	offset = nvram_mtd->size - erasesize;
	if (nvram_mtd->erasesize > NVRAM_SPACE)
		i = erasesize - NVRAM_SPACE + ROUNDUP(header->len, NVRAM_SPACE);
	else
		i = erasesize - NVRAM_SPACE + ROUNDUP(header->len, nvram_mtd->erasesize);
	ret = mtd_write(nvram_mtd, offset, i, &len, buf);
	if (ret || len != i) {
		printk("nvram_commit: write error\n");
		ret = -EIO;
		goto done;
	}

	offset = nvram_mtd->size - erasesize;
	ret = mtd_read(nvram_mtd, offset, 4, &len, buf);

done:
	mutex_unlock(&nvram_sem);
	MFREE(buf);
	return ret;
}

int nvram_getall(char *buf, int count)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&nvram_lock, flags);
	ret = _nvram_getall(buf, count);
	spin_unlock_irqrestore(&nvram_lock, flags);

	return ret;
}

EXPORT_SYMBOL(nvram_get);
EXPORT_SYMBOL(nvram_getall);
EXPORT_SYMBOL(nvram_set);
EXPORT_SYMBOL(nvram_unset);
EXPORT_SYMBOL(nvram_commit);

/* User mode interface below */

static ssize_t dev_nvram_read(struct file *file, char *buf, size_t count, loff_t * ppos)
{
	char tmp[100], *name = tmp, *value;
	ssize_t ret;
	unsigned long off;

	if ((count + 1) > sizeof(tmp)) {
		if (!(name = MALLOC(count + 1)))
			return -ENOMEM;
	}

	if (copy_from_user(name, buf, count)) {
		ret = -EFAULT;
		goto done;
	}
	name[count] = '\0';

	if (*name == '\0') {
		/* Get all variables */
		ret = nvram_getall(name, count);
		if (ret == 0) {
			if (copy_to_user(buf, name, count)) {
				ret = -EFAULT;
				goto done;
			}
			ret = count;
		}
	} else {
		if (!(value = nvram_get(name))) {
			ret = 0;
			goto done;
		}

		/* Provide the offset into mmap() space */
		off = (unsigned long)value - (unsigned long)nvram_buf;

		if (copy_to_user(buf, &off, ret = sizeof(off))) {
			ret = -EFAULT;
			goto done;
		}
	}
#ifdef	_DEPRECATED
	flush_cache_all();
#endif
done:
	if (name != tmp)
		MFREE(name);

	return ret;
}

static ssize_t dev_nvram_write(struct file *file, const char *buf, size_t count, loff_t * ppos)
{
	char tmp[100], *name = tmp, *value;
	ssize_t ret;

	if (count >= sizeof(tmp)) {
		if (!(name = MALLOC(count + 1)))
			return -ENOMEM;
	}

	if (copy_from_user(name, buf, count)) {
		ret = -EFAULT;
		goto done;
	}
	name[count] = '\0';
	value = name;
	name = strsep(&value, "=");
	if (value)
		ret = nvram_set(name, value);
	else
		ret = nvram_unset(name);

	if (0 == ret)
		ret = count;
done:
	if (name != tmp)
		MFREE(name);

	return ret;
}

static long dev_nvram_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

	switch (cmd) {
	case NVRAM_MAGIC:
		nvram_commit();
		return 0;
		break;
	case NVRAM_SPACE_MAGIC:
		return NVRAM_SPACE;
		break;
	default:
		return -EINVAL;
		break;

	}
}

//int remap_pfn_range(struct vm_area_struct *vma, unsigned long addr,
//                  unsigned long pfn, unsigned long size, pgprot_t prot)

static DEFINE_MUTEX(mtd_mutex);

static long nvram_unlocked_ioctl(struct file *file, u_int cmd, u_long arg)
{
	int ret;

	mutex_lock(&mtd_mutex);
	ret = dev_nvram_ioctl(file, cmd, arg);
	mutex_unlock(&mtd_mutex);

	return ret;
}

static int dev_nvram_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long offset = virt_to_phys(nvram_buf);
	if (remap_pfn_range(vma, vma->vm_start, offset >> PAGE_SHIFT, vma->vm_end - vma->vm_start, vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}

static int dev_nvram_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int dev_nvram_release(struct inode *inode, struct file *file)
{
	return 0;
}

static struct file_operations dev_nvram_fops = {
      owner:THIS_MODULE,
      open:dev_nvram_open,
      release:dev_nvram_release,
      read:dev_nvram_read,
      write:dev_nvram_write,
#ifdef CONFIG_COMPAT
      compat_ioctl:dev_nvram_ioctl,
#endif
      unlocked_ioctl:nvram_unlocked_ioctl,
      mmap:dev_nvram_mmap,
};

static void dev_nvram_exit(void)
{
	int order = 0;
	struct page *page, *end;

//      if (nvram_handle)
//              devfs_unregister(nvram_handle);

//      if (nvram_major >= 0)
//              devfs_unregister_chrdev(nvram_major, "nvram");

	if (nvram_mtd)
		put_mtd_device(nvram_mtd);

	while ((PAGE_SIZE << order) < NVRAM_SPACE)
		order++;
	end = virt_to_page(nvram_buf + (PAGE_SIZE << order) - 1);
	for (page = virt_to_page(nvram_buf); page <= end; page++)
		mem_map_unreserve(page);

	_nvram_exit();
}

static int __init dev_nvram_init(void)
{
	int order = 0, ret = 0;
	struct page *page, *end;
	unsigned int i;

	/* Allocate and reserve memory to mmap() */
	while ((PAGE_SIZE << order) < NVRAM_SPACE)
		order++;
	end = virt_to_page(nvram_buf + (PAGE_SIZE << order) - 1);
	for (page = virt_to_page(nvram_buf); page <= end; page++)
		mem_map_reserve(page);

#ifdef CONFIG_MTD
	printk(KERN_INFO "searching for nvram\n");
	/* Find associated MTD device */
	for (i = 0; i < 32; i++) {
		nvram_mtd = get_mtd_device(NULL, i);
		if (nvram_mtd) {
			if (!strcmp(nvram_mtd->name, "nvram") && nvram_mtd->size >= NVRAM_SPACE) {
				printk(KERN_INFO "nvram size = %llu\n", nvram_mtd->size);
				break;
			}
			put_mtd_device(nvram_mtd);
		}
	}
	if (i >= 32) {
		printk(KERN_EMERG "no nvram partition found\n");
		nvram_mtd = NULL;
		return -1;
	}
#endif

	/* Initialize hash table lock */
	spin_lock_init(&nvram_lock);

	/* Initialize commit semaphore */
	mutex_init(&nvram_sem);

	/* Register char device */
	if ((nvram_major = register_chrdev(229, "nvram", &dev_nvram_fops)) < 0) {
		ret = nvram_major;
		goto err;
	}

	/* Initialize hash table */
	if (_nvram_init()) ;
	return -1;

	/* Create /dev/nvram handle */

//      nvram_handle = devfs_register(NULL, "nvram", DEVFS_FL_NONE, nvram_major, 0,
//                                    S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP, &dev_nvram_fops, NULL);

	return 0;

err:
	dev_nvram_exit();
	return ret;
}

late_initcall(dev_nvram_init);
module_exit(dev_nvram_exit);
