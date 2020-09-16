powertop-configure:
	cd powertop && ./configure --disable-nls --prefix=/usr --host=$(ARCH)-linux \
	NCURSES_CFLAGS="-I$(TOP)/ncurses/include" \
	NCURSES_LIBS="-L$(TOP)/ncurses/lib -lncurses" \
	LIBNL_CFLAGS="-I$(TOP)/libnl-tiny/include" \
	LIBNL_LIBS="-L$(TOP)/libnl-tiny -lnl-tiny" \
	PCIUTILS_CFLAGS="-I$(TOP)/pciutils/lib" \
	PCIUTILS_LIBS="-L$(TOP)/pciutils/lib -lpci" \
	CC="$(CC)" \
	CFLAGS="$(COPTS) $(MIPS16_OPT) -DNEED_PRINTF -D_GNU_SOURCE -Drpl_malloc=malloc -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	CXXFLAGS="$(COPTS) $(MIPS16_OPT) -DNEED_PRINTF -D_GNU_SOURCE -Drpl_malloc=malloc -ffunction-sections -fdata-sections -Wl,--gc-sections"

powertop: pciutils
	$(MAKE) -j 4 -C powertop

powertop-clean:
	if test -e "powertop/Makefile"; then make -C powertop clean; fi
	@true

powertop-install:
	$(MAKE) -C powertop install DESTDIR=$(INSTALLDIR)/powertop
	rm -rf $(INSTALLDIR)/powertop/usr/share
