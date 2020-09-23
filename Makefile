#
# Toplevel Makefile for the Sveasoft Firmware
#
# Copyright 2004, Sveasoft
# All Rights Reserved.
#
# THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
# KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
# SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
#
# $Id: Makefile,v 1.2 2004/05/02 14:33:32 rwhitby Exp $
#

RELEASEDIR := $(shell pwd)
SRCBASE := src
IMAGEDIR := $(RELEASEDIR)/image

install:
	install -d $(RELEASEDIR)/image
	$(MAKE) -f Makefile.northstar install

all:
	$(MAKE) -f Makefile.northstar $@
	install -d $(RELEASEDIR)/image
	$(MAKE) -f Makefile.northstar install

distclean clean:
	$(MAKE) -f Makefile.northstar $@
	rm -rf $(IMAGEDIR)

.PHONY: all clean install

