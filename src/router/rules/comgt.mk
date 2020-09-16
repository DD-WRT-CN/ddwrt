comgt-configure:
	$(MAKE) -C usb_modeswitch configure


comgt-clean:
	$(MAKE) -C usb_modeswitch clean
	$(MAKE) -C comgt clean

comgt:
	$(MAKE) -C usb_modeswitch
	$(MAKE) -C comgt CC="$(CC)"  

comgt-install:
	install -D comgt/comgt $(INSTALLDIR)/comgt/usr/sbin/comgt
	$(STRIP) $(INSTALLDIR)/comgt/usr/sbin/comgt
	#install -D comgt/scripts/dial.comgt $(INSTALLDIR)/comgt/etc/comgt/dial.comgt
	#install -D comgt/scripts/setmode.comgt $(INSTALLDIR)/comgt/etc/comgt/setmode.comgt
	#install -D comgt/scripts/reset.comgt $(INSTALLDIR)/comgt/etc/comgt/reset.comgt
	#install -D comgt/scripts/wakeup.comgt $(INSTALLDIR)/comgt/etc/comgt/wakeup.comgt
	#install -D comgt/scripts/netmode.comgt $(INSTALLDIR)/comgt/etc/comgt/netmode.comgt
	mkdir -p $(INSTALLDIR)/comgt/etc/comgt
	cp comgt/scripts/*.comgt $(INSTALLDIR)/comgt/etc/comgt || true
	cp comgt/scripts/*.sh $(INSTALLDIR)/comgt/etc/comgt || true
	install -D usb_modeswitch/usb_modeswitch $(INSTALLDIR)/comgt/usr/sbin/usb_modeswitch
#	install -D usb_modeswitch/ozerocdoff $(INSTALLDIR)/comgt/usr/sbin/ozerocdoff
	mkdir -p $(INSTALLDIR)/comgt/etc/hso
	cp comgt/hso/* $(INSTALLDIR)/comgt/etc/hso
#	install -D comgt/usb_modeswitch $(INSTALLDIR)/comgt/usr/sbin/usb_modeswitch

