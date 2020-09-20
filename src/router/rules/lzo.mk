lzo-clean:
	make -j 4 -C lzo clean

lzo-configure:
	cd lzo && ./configure --host=$(ARCH)-linux CFLAGS="$(COPTS) $(LTO) $(MIPS16_OPT) $(LTOFIXUP)" AR_FLAGS="cru $(LTOPLUGIN)" RANLIB="$(ARCH)-linux-ranlib $(LTOPLUGIN)"

lzo: lzo-configure
	make -j 4 -C lzo

lzo-install:
	@true
