ctorrent-configure:
	cd ctorrent && ./configure --disable-nls --prefix=/usr --host=$(ARCH)-linux CC="$(CC)" LDFLAGS="../openssl/libcrypto.so -L../openssl" CFLAGS="$(COPTS) -L../openssl -I../openssl/include" CPPFLAGS="$(COPTS) -L../openssl -I../openssl/include"

ctorrent-clean:
	make -C ctorrent clean

ctorrent:
	make -C ctorrent

ctorrent-install:
	install -D ctorrent/ctorrent $(INSTALLDIR)/ctorrent/usr/sbin/ctorrent