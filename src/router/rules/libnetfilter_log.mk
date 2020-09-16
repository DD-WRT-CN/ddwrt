libnetfilter_log-configure: libnfnetlink
	cd libnetfilter_log && \
	LIBNFNETLINK_CFLAGS="-I$(TOP)/libnfnetlink/include" \
	LIBNFNETLINK_LIBS="-L$(TOP)/libnfnetlink/src/.libs -lnfnetlink" \
	./configure \
		--build=$(ARCH)-linux \
		--host=$(ARCH)-linux-gnu \
		--prefix=/usr \
		--libdir=$(TOP)/libnfnetlink/src/.libs

libnetfilter_log: libnfnetlink
	$(MAKE) -C libnetfilter_log CFLAGS="$(COPTS) $(MIPS16_OPT) -D_GNU_SOURCE"
	$(MAKE) -C libnetfilter_log/utils nfulnl_test CFLAGS="$(COPTS) $(MIPS16_OPT) -D_GNU_SOURCE" LDFLAGS="-L$(TOP)/libnfnetlink/src/.libs -lnfnetlink"
	#$(MAKE) -C libnetfilter_log/utils CFLAGS="$(COPTS) $(MIPS16_OPT) -D_GNU_SOURCE"

libnetfilter_log-install:
	install -D libnetfilter_log/src/.libs/libnetfilter_log.so.1 $(INSTALLDIR)/libnetfilter_log/usr/lib/libnetfilter_log.so.1
