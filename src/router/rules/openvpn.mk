WOLFSSL_SSLPATH=$(TOP)/wolfssl
WOLFSSL_SSL_LIB_PATH=$(WOLFSSL_SSLPATH)/standard/src/.libs
WOLFSSL_SSL_DEP=wolfssl
WOLFSSL_SSL_ADDOPT=OPENSSL_LIBS="-L$(WOLFSSL_SSL_LIB_PATH)" \
	WOLFSSL_LIBS="-L$(WOLFSSL_SSL_LIB_PATH) -lwolfssl" \
	WOLFSSL_CFLAGS="-I$(WOLFSSL_SSLPATH) -I$(WOLFSSL_SSLPATH)/standard -I$(WOLFSSL_SSLPATH)/standard/wolfssl  -I$(WOLFSSL_SSLPATH)/wolfssl"


OVPN=openvpn
OPENSSL_SSLPATH=$(TOP)/openssl
OPENSSL_SSL_LIB_PATH=$(OPENSSL_SSLPATH)
OPENSSL_SSL_DEP=openssl
OPENSSL_SSL_ADDOPT=OPENSSL_LIBS="-L$(OPENSSL_SSL_LIB_PATH) -lssl -lcrypto" \
	OPTIONAL_CRYPTO_LIBS="-L$(OPENSSL_SSL_LIB_PATH) -lssl -lcrypto" \
	OPENSSL_SSL_CFLAGS="-I$(OPENSSL_SSLPATH)/include" \
	OPENSSL_SSL_LIBS="-L$(OPENSSL_SSL_LIB_PATH) -lssl" \
	OPENSSL_CRYPTO_CFLAGS="-I$(OPENSSL_SSLPATH)/include" \
	OPENSSL_CRYPTO_LIBS="-L$(OPENSSL_SSL_LIB_PATH) -lcrypto"



CONFIGURE_ARGS_OVPN += \
	--host=$(ARCH)-linux \
	CPPFLAGS="-I$(TOP)/lzo/include -L$(TOP)/lzo -L$(TOP)/lzo/src/.libs" \
	--prefix=/usr \
	--disable-selinux \
	--disable-systemd \
	--disable-debug \
	--disable-eurephia \
	--disable-pkcs11 \
	--disable-plugins \
	--enable-password-save \
	--enable-management \
	--enable-lzo \
	--enable-fragment \
	--enable-server \
	--enable-multihome \
	--with-crypto-library=openssl \
	$(OPENSSL_SSL_ADDOPT) \
	CFLAGS="$(COPTS) $(LTO) $(MIPS16_OPT) $(LTOFIXUP) -I$(OPENSSL_SSLPATH)/include  -DNEED_PRINTF -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections  $(LDLTO) $(LTOFIXUP) -L$(OPENSSL_SSL_LIB_PATH) -L$(TOP)/lzo -L$(TOP)/lzo/src/.libs -ldl -lpthread" \
	LZO_CFLAGS="-I$(TOP)/lzo/include" \
	LZO_LIBS="-L$(TOP)/lzo -L$(TOP)/lzo/src/.libs -llzo2" \
	AR_FLAGS="cru $(LTOPLUGIN)" RANLIB="$(ARCH)-linux-ranlib $(LTOPLUGIN)" \
	ac_cv_func_epoll_create=yes \
	ac_cv_path_IFCONFIG=/sbin/ifconfig \
	ac_cv_path_ROUTE=/sbin/route \
	ac_cv_path_IPROUTE=/usr/sbin/ip 

CONFIGURE_ARGS_WOLFSSL += \
	--host=$(ARCH)-linux \
	CPPFLAGS="-I$(TOP)/lzo/include -L$(TOP)/lzo -L$(TOP)/lzo/src/.libs" \
	--prefix=/usr \
	--disable-selinux \
	--disable-systemd \
	--disable-debug \
	--disable-eurephia \
	--disable-pkcs11 \
	--disable-plugins \
	--enable-password-save \
	--enable-management \
	--enable-lzo \
	--enable-fragment \
	--enable-server \
	--enable-multihome \
	--with-crypto-library=wolfssl \
	$(WOLFSSL_SSL_ADDOPT) \
	CFLAGS="$(COPTS) $(LTO) $(MIPS16_OPT) $(LTOFIXUP) -I$(WOLFSSL_SSLPATH)/include  -DNEED_PRINTF -ffunction-sections -fdata-sections -Wl,--gc-sections" \
	LDFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections  $(LDLTO) $(LTOFIXUP) -L$(WOLFSSL_SSL_LIB_PATH) -L$(TOP)/lzo -L$(TOP)/lzo/src/.libs -ldl -lpthread" \
	LZO_CFLAGS="-I$(TOP)/lzo/include" \
	LZO_LIBS="-L$(TOP)/lzo -L$(TOP)/lzo/src/.libs -llzo2" \
	AR_FLAGS="cru $(LTOPLUGIN)" RANLIB="$(ARCH)-linux-ranlib $(LTOPLUGIN)" \
	ac_cv_func_epoll_create=yes \
	ac_cv_path_IFCONFIG=/sbin/ifconfig \
	ac_cv_path_ROUTE=/sbin/route \
	ac_cv_path_IPROUTE=/usr/sbin/ip 


ifeq ($(ARCHITECTURE),broadcom)
ifneq ($(CONFIG_BCMMODERN),y)
CONFIGURE_ARGS_OVPN += ac_cv_func_epoll_create=no
CONFIGURE_ARGS_WOLFSSL += ac_cv_func_epoll_create=no
else
CONFIGURE_ARGS_OVPN += ac_cv_func_epoll_create=yes
CONFIGURE_ARGS_WOLFSSL += ac_cv_func_epoll_create=yes
endif
else
CONFIGURE_ARGS_OVPN += ac_cv_func_epoll_create=yes
CONFIGURE_ARGS_WOLFSSL += ac_cv_func_epoll_create=yes
endif

openvpn-conf-prep:
	-rm -f openvpn/Makefile
	cd openvpn && aclocal
	cd openvpn && autoconf
	cd openvpn && automake

openvpn-conf: openssl wolfssl
	mkdir -p openvpn/openssl
	mkdir -p openvpn/wolfssl
	cd $(OVPN)/openssl && ../configure $(CONFIGURE_ARGS_OVPN)
	cd $(OVPN)/wolfssl && ../configure $(CONFIGURE_ARGS_WOLFSSL)


openvpn-configure: lzo openvpn-conf-prep openvpn-conf

openvpn: lzo $(SSL_DEP)
#ifeq ($(CONFIG_NEWMEDIA),y)
#else
#	cd $(OVPN) && ./configure --host=$(ARCH)-linux CPPFLAGS="-ffunction-sections -fdata-sections -Wl,--gc-sections -I../lzo/include -I../openssl/include -L../lzo -L../openssl -L../lzo/src/.libs" --enable-static --disable-shared --disable-pthread --disable-plugins --disable-debug --disable-management --disable-socks --enable-lzo --enable-small --enable-server --enable-http --enable-password-save CFLAGS="$(COPTS)  -ffunction-sections -fdata-sections -Wl,--gc-sections" LDFLAGS="-L../openssl -L../lzo -L../lzo/src/.libs  -ffunction-sections -fdata-sections -Wl,--gc-sections"
#endif
ifneq ($(CONFIG_FREERADIUS),y)
ifneq ($(CONFIG_ASTERISK),y)
ifneq ($(CONFIG_AIRCRACK),y)
ifneq ($(CONFIG_POUND),y)
ifneq ($(CONFIG_IPETH),y)
ifneq ($(CONFIG_VPNC),y)
ifneq ($(CONFIG_TOR),y)
ifneq ($(CONFIG_ANCHORFREE),y)
ifeq ($(CONFIG_MATRIXSSL),y)
	rm -f openssl/*.so*
endif
endif
endif
endif
endif
endif
endif
endif
endif
	make -j 4 -C $(OVPN)/openssl clean
ifeq ($(CONFIG_OPENVPN_SSLSTATIC),y)
	rm -f openssl/*.so*
endif
ifeq ($(CONFIG_WOLFSSL),y)
	make -j 4 -C $(OVPN)/wolfssl
else
	make -j 4 -C $(OVPN)/openssl
endif
openvpn-install:
ifeq ($(CONFIG_WOLFSSL),y)
	make -C $(OVPN)/wolfssl install DESTDIR=$(INSTALLDIR)/openvpn
else
	make -C $(OVPN)/openssl install DESTDIR=$(INSTALLDIR)/openvpn
endif
	rm -rf $(INSTALLDIR)/openvpn/usr/share
	rm -rf $(INSTALLDIR)/openvpn/usr/include
ifeq ($(CONFIG_AIRNET),y)
	install -D openvpn/config-airnet/openvpncl.nvramconfig $(INSTALLDIR)/openvpn/etc/config/openvpncl.nvramconfig
	install -D openvpn/config-airnet/openvpncl.webvpn $(INSTALLDIR)/openvpn/etc/config/openvpncl.webvpn
	install -D openvpn/config-airnet/openvpn.nvramconfig $(INSTALLDIR)/openvpn/etc/config/openvpn.nvramconfig
	install -D openvpn/config-airnet/openvpn.webvpn $(INSTALLDIR)/openvpn/etc/config/openvpn.webvpn
else
	install -D openvpn/config/openvpncl.nvramconfig $(INSTALLDIR)/openvpn/etc/config/openvpncl.nvramconfig
	install -D openvpn/config/openvpncl.webvpn $(INSTALLDIR)/openvpn/etc/config/openvpncl.webvpn
	install -D openvpn/config2/openvpn.nvramconfig $(INSTALLDIR)/openvpn/etc/config/openvpn.nvramconfig
	install -D openvpn/config2/openvpn.webvpn $(INSTALLDIR)/openvpn/etc/config/openvpn.webvpn
endif
	cp -f openvpn/config/*.sh $(INSTALLDIR)/openvpn/etc


openvpn-clean:
	-make -C $(OVPN)/wolfssl clean
	-make -C $(OVPN)/openssl clean



