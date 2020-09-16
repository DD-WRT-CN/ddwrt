#ifndef __SWITCH_CORE_H
#define __SWITCH_CORE_H

#include <linux/list.h>
#define SWITCH_MAX_BUFSZ	4096

#define SWITCH_MEDIA_AUTO	1
#define SWITCH_MEDIA_100	2
#define SWITCH_MEDIA_FD		4

typedef int (*switch_handler)(char *buf, int nr);

typedef struct {
	char *name;
	switch_handler read, write;
} switch_config;


typedef struct {
	struct list_head list;
	char *name;
	int ports;
	int vlans;
	switch_config *driver_handlers, *port_handlers, *vlan_handlers;
	void *data;
} switch_driver;


extern int switch_register_driver(switch_driver *driver);
extern void switch_unregister_driver(char *name);
extern int parse_vlan_ports(char *buf);
extern int parse_media(char *buf);
extern int print_media(char *buf, int media);

#endif
