#ifndef IPTRAF_NG_TUI_INPUT_H
#define IPTRAF_NG_TUI_INPUT_H

/***

input.h - structure declarations and function prototypes for input.c

***/

#include "winops.h"

#define CTRL_X 24

struct FIELD {
	char *buf;
	unsigned int len;
	unsigned int tlen;
	unsigned int xpos;
	unsigned int ypos;
	struct FIELD *prevfield;
	struct FIELD *nextfield;
};

struct FIELDLIST {
	struct FIELD *list;
	WINDOW *fieldwin;
	PANEL *fieldpanel;
	int dlgtextattr;
	int fieldattr;
};

void tx_initfields(struct FIELDLIST *list, int leny, int lenx, int begy,
		   int begx, int dlgtextattr, int dlgfieldattr);
void tx_addfield(struct FIELDLIST *list, unsigned int len, unsigned int y,
		 unsigned int x, const char *initstr);
void tx_getinput(struct FIELDLIST *list, struct FIELD *field, int *exitkey);
void tx_fillfields(struct FIELDLIST *list, int *aborted);
void tx_destroyfields(struct FIELDLIST *list);

#endif	/* IPTRAF_NG_TUI_INPUT_H */
