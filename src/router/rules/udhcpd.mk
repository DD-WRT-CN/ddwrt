udhcpd-clean:
	make -C udhcpc clean
	make -C udhcpd clean
	
udhcpd: shared nvram
	make -C udhcpc
	make -C udhcpd

udhcpd-install:
ifeq ($(CONFIG_UDHCPD),y)
#	install -D udhcpd/udhcpd $(INSTALLDIR)/udhcpd/usr/sbin/udhcpd
#	install -D udhcpd/dumpleases $(INSTALLDIR)/udhcpd/usr/sbin/dumpleases
#ifeq ($(CONFIG_BUFFALO),y)
#	install -D udhcpd/config/dhcpd.webservices.buffalo $(INSTALLDIR)/udhcpd/etc/config/dhcpd.webservices
#else
	install -D udhcpd/config/dhcpd.webservices $(INSTALLDIR)/udhcpd/etc/config/dhcpd.webservices
#endif
#	install -D udhcpd/config/dhcpd.startup $(INSTALLDIR)/udhcpd/etc/config/dhcpd.startup
#	$(STRIP) $(INSTALLDIR)/udhcpd/usr/sbin/udhcpd
#	$(STRIP) $(INSTALLDIR)/udhcpd/usr/sbin/dumpleases
endif
#ifneq ($(CONFIG_BUSYBOX_UDHCPC),y)
#	install -D udhcpc/udhcpc $(INSTALLDIR)/udhcpd/usr/sbin/udhcpc
#	install -m 777 -D udhcpc/scripts/cidrroute.sh $(INSTALLDIR)/udhcpd/etc/cidrroute.sh
#else
	install -m 777 -D udhcpc/scripts/udhcpcroutes.sh $(INSTALLDIR)/udhcpd/etc/cidrroute.sh
	#mkdir -p $(INSTALLDIR)/udhcpc/etc/
	#cd $(INSTALLDIR)/udhcpd/etc/ && ln -sf /tmp/cidrroute.sh cidrroute.sh
#endif
#ifeq ($(CONFIG_DIST),"micro")
#	rm -f $(INSTALLDIR)/udhcpd/usr/sbin/udhcpd
#endif
#ifeq ($(CONFIG_DIST),"micro-special")
#	rm -f $(INSTALLDIR)/udhcpd/usr/sbin/udhcpd
#endif

