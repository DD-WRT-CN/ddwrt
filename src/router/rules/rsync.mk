rsync-configure:
	cd rsync && autoconf
	cd rsync && ./configure --host=$(ARCH)-linux --prefix=/usr --disable-locate --disable-simd --disable-iconv --disable-xxhash --disable-lz4 \
		CFLAGS="$(COPTS) $(MIPS16_OPT) -I$(TOP)/openssl/include -ffunction-sections -fdata-sections -Wl,--gc-sections -Drpl_malloc=malloc -L$(TOP)/openssl -L$(TOP)/zstd/lib -I$(TOP)/zstd/lib"

	$(MAKE) -C rsync reconfigure
rsync:
	$(MAKE) -C rsync

rsync-clean:
	$(MAKE) -C rsync clean

rsync-install:
	rm -rf $(INSTALLDIR)/rsync
	$(MAKE) -C rsync install DESTDIR=$(INSTALLDIR)/rsync
	rm -rf $(INSTALLDIR)/rsync/usr/share
	mkdir -p $(INSTALLDIR)/rsync/usr/sbin
	cd $(INSTALLDIR)/rsync/usr/sbin && ln -s ../bin/rsync rsyncd
	install -D rsync/config/rsync.webnas $(INSTALLDIR)/rsync/etc/config/05rsync.webnas
	install -D rsync/config/rsync.nvramconfig $(INSTALLDIR)/rsync/etc/config/rsync.nvramconfig
