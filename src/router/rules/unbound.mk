ifeq ($(ARCH),arm)
UNBOUND_COPTS += -DNEED_PRINTF
endif
ifeq ($(ARCH),mips64)
UNBOUND_COPTS += -DNEED_PRINTF
endif
ifeq ($(ARCH),i386)
UNBOUND_COPTS += -DNEED_PRINTF
endif
ifeq ($(ARCH),x86_64)
UNBOUND_COPTS += -DNEED_PRINTF
endif
ifeq ($(ARCH),aarch64)
UNBOUND_COPTS += -DNEED_PRINTF
endif

unbound-configure: expat
	cd unbound && ./configure --disable-ecdsa \
		--disable-gost \
		--enable-allsymbols \
		--enable-tfo-client \
		--enable-tfo-server \
		--with-chroot-dir=/tmp \
		--with-ssl="$(TOP)/openssl" \
		--with-pthreads \
		--prefix=/usr \
		--libdir=/usr/lib \
		--sysconfdir=/etc \
		--host=$(ARCH)-linux \
		--with-libexpat="$(STAGING_DIR)/usr/" \
		CC="$(CC)" \
		CFLAGS="$(COPTS) $(MIPS16_OPT) $(UNBOUND_COPTS) -ffunction-sections -fdata-sections -Wl,--gc-sections -L$(TOP)/openssl" \
		LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -L$(TOP)/openssl"

unbound: unbound-configure expat 
	$(MAKE) -C unbound

unbound-clean: 
	if test -e "unbound/Makefile"; then $(MAKE) -C unbound clean ; fi

unbound-install: 
	$(MAKE) -C unbound install DESTDIR=$(INSTALLDIR)/unbound
	mkdir -p $(INSTALLDIR)/unbound/etc/unbound
	cp unbound/config/* $(INSTALLDIR)/unbound/etc/unbound
	rm -rf $(INSTALLDIR)/unbound/usr/include
	rm -rf $(INSTALLDIR)/unbound/usr/share
	rm -f $(INSTALLDIR)/unbound/usr/lib/*.a
	rm -rf $(INSTALLDIR)/unbound/usr/lib/pkgconfig
	rm -f $(INSTALLDIR)/unbound/usr/lib/*.la
#	rm -f $(INSTALLDIR)/unbound/usr/sbin/unbound-checkconf
#	rm -f $(INSTALLDIR)/unbound/usr/sbin/unbound-control
	rm -f $(INSTALLDIR)/unbound/usr/sbin/unbound-control-setup
	rm -f $(INSTALLDIR)/unbound/usr/sbin/unbound-host
	

expat/stamp-h1:
	cd $(TOP)/expat && \
	$(CONFIGURE) --prefix=$(STAGING_DIR)/usr/ \
		--libdir=$(STAGING_DIR)/usr/lib \
		--includedir=$(STAGING_DIR)/usr/include
	touch $@

expat: expat/stamp-h1
	@$(MAKE) -C expat
	-mkdir -p $(STAGING_DIR)/usr/lib
	-mkdir -p $(STAGING_DIR)/usr/include
	make -C expat install

expat-install: expat
	mkdir -p $(INSTALLDIR)/expat/usr/lib
	install -D expat/.libs/libexpat.so.1.5.2 $(INSTALLDIR)/expat/usr/lib/libexpat.so.1.5.2
	$(STRIP) $(INSTALLDIR)/expat/usr/lib/libexpat.so.1.5.2
	cd $(INSTALLDIR)/expat/usr/lib && ln -sf libexpat.so.1.5.2 libexpat.so.1

expat-clean:
	-@$(MAKE) -C expat clean
	@rm -f expat/stamp-h1
