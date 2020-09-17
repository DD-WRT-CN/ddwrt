rtpproxy-configure:
	cd rtpproxy && ./configure --host=$(ARCH)-uclibc-linux CFLAGS="$(COPTS) -Drpl_malloc=malloc $(MIPS16_OPT) -ffunction-sections -fdata-sections -Wl,--gc-sections"

rtpproxy: rtpproxy-configure
	$(MAKE) -C rtpproxy

rtpproxy-install:
	install -D rtpproxy/src/rtpproxy $(INSTALLDIR)/rtpproxy/usr/bin/rtpproxy
	
rtpproxy-clean:
	$(MAKE) -C rtpproxy clean
