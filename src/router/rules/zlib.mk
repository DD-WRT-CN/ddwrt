PKG_INSTALL:=1

MAKE_FLAGS+=VERBOSE=1

ZLIB_PKG_BUILD_DIR=$(TOP)/zlib
ZLIB_CMAKE_OPTIONS=-DZLIB_COMPAT=ON
ZLIB_STAGING_DIR=$(TOP)/_staging/usr
ZLIB_EXTRA_CFLAGS=-I$(TOP)/_staging/usr/include $(COPTS) $(MIPS16_OPT)
ZLIB_EXTRA_LDFLAGS=-L$(TOP)/_staging/usr/lib $(COPTS) $(MIPS16_OPT)
#ZLIB_EXTRA_ARFLAGS="cru $(LTOPLUGIN)" 
#RANLIB="$(ARCH)-linux-ranlib $(LTOPLUGIN)"
ifeq ($(ARCHITECTURE),broadcom)
ifneq ($(CONFIG_BCMMODERN),y)
ZLIB_EXTRA_CFLAGS+=-DNOTLS
endif
endif
ifeq ($(ARCH),i386)
ZLIB_CMAKE_OPTIONS+=-DASM686=ON -DWITH_AVX2=ON -DWITH_SSE2=ON -DWITH_SSSE3=ON -DWITH_SSE4=ON -DWITH_PCLMULQDQ=ON
else
ifeq ($(ARCH),x86_64)
ZLIB_CMAKE_OPTIONS+=-DAMD64=ON -DWITH_AVX2=ON -DWITH_SSE2=ON -DWITH_SSSE3=ON -DWITH_SSE4=ON -DWITH_PCLMULQDQ=ON
else
ifeq ($(ARCHITECTURE),ipq806x)
ZLIB_CMAKE_OPTIONS+=-DARMv8=ON -DWITH_NEON=ON
else
ifeq ($(ARCHITECTURE),alpine)
ZLIB_CMAKE_OPTIONS+=-DARMv8=ON -DWITH_NEON=ON
else
ifeq ($(ARCHITECTURE),ventana)
ZLIB_CMAKE_OPTIONS+=-DARMv8=ON -DWITH_NEON=ON
else
ZLIB_CMAKE_OPTIONS+=-DARMv8=OFF -DWITH_NEON=OFF -DWITH_ACLE=OFF -DWITH_NEON=OFF
endif
endif
endif
endif
endif


zlib-configure:
	$(call CMakeClean,$(ZLIB_PKG_BUILD_DIR))
	$(call CMakeConfigure,$(ZLIB_PKG_BUILD_DIR),$(ZLIB_STAGING_DIR),$(ZLIB_CMAKE_OPTIONS),$(ZLIB_EXTRA_CFLAGS),$(ZLIB_EXTRA_LDFLAGS)) 

zlib: zlib-configure
	$(MAKE) -C zlib
	rm -f zlib/libz.a

zlib-install:
	install -D zlib/libz.so.1.2.11 $(INSTALLDIR)/zlib/usr/lib/libz.so.1.2.11
	cd $(INSTALLDIR)/zlib/usr/lib ; ln -s libz.so.1.2.11 libz.so.1  ; true
	cd $(INSTALLDIR)/zlib/usr/lib ; ln -s libz.so.1.2.11 libz.so  ; true

zlib-clean:
	if [ -e "$(ZLIB_PKG_BUILD_DIR)/Makefile" ]; then $(MAKE) -C zlib clean ; fi
#	$(call CMakeClean,$(ZLIB_PKG_BUILD_DIR))
