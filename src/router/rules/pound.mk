

pound-configure:
	cd pound && ./bootstrap
	cd pound && ./configure --prefix=/usr --with-ssl=$(TOP)/openssl --host=$(ARCH)-linux CC="$(ARCH)-linux-uclibc-gcc" LDFLAGS="-Drpl_malloc=malloc -ffunction-sections -fdata-sections -Wl,--gc-sections -L$(TOP)/openssl -lpthread" CFLAGS="-Drpl_malloc=malloc -DNEED_PRINTF -ffunction-sections -fdata-sections -Wl,--gc-sections $(COPTS) $(MIPS16_OPT) -I$(TOP)/openssl/include"

pound-clean:
	make -C pound clean

pound:
	if ! test -e "pound/Makefile"; then cd pound && ./configure --prefix=/usr --with-ssl=$(TOP)/openssl --host=$(ARCH)-linux CC="$(ARCH)-linux-uclibc-gcc" LDFLAGS="-Drpl_malloc=malloc -ffunction-sections -fdata-sections -Wl,--gc-sections -L$(TOP)/openssl" CFLAGS="-Drpl_malloc=malloc -DNEED_PRINTF -ffunction-sections -fdata-sections -Wl,--gc-sections $(COPTS) $(MIPS16_OPT)  -I$(TOP)/openssl/include"; fi
	make -C pound CC="$(CC)"

pound-install:
	make -C pound install DESTDIR=$(INSTALLDIR)/pound
	rm -rf $(INSTALLDIR)/pound/usr/share
