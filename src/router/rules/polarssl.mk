POLAR_SSL_PATH=polarssl
PKG_BUILD_DIR=$(TOP)/$(POLAR_SSL_PATH)
STAGING_DIR=$(TOP)
PKG_INSTALL:=1

MAKE_FLAGS+=VERBOSE=1

CMAKE_OPTIONS += \
	-DCMAKE_BUILD_TYPE:String="Release" 

polarssl-configure: 
	rm -f $(PKG_BUILD_DIR)/CMakeCache.txt
	(cd $(PKG_BUILD_DIR); \
		cmake \
			-DCMAKE_SYSTEM_NAME=Linux \
			-DCMAKE_SYSTEM_VERSION=1 \
			-DCMAKE_SYSTEM_PROCESSOR=$(ARCH) \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_C_FLAGS_RELEASE="-DNDEBUG $(TARGET_CFLAGS) $(EXTRA_CFLAGS) $(COPTS) -I$(TOP)/$(POLAR_SSL_PATH)/include -I$(TOP)/openssl/include -DNEED_PRINTF" \
			-DCMAKE_CXX_FLAGS_RELEASE="-DNDEBUG $(TARGET_CFLAGS) $(EXTRA_CFLAGS) $(COPTS) -I$(TOP)/$(POLAR_SSL_PATH)/include  -I$(TOP)/openssl/include -DNEED_PRINTF" \
			-DCMAKE_C_COMPILER=$(CROSS_COMPILE)gcc \
			-DCMAKE_CXX_COMPILER=$(CROSS_COMPILE)g++ \
			-DCMAKE_EXE_LINKER_FLAGS="$(TARGET_LDFLAGS)" \
			-DCMAKE_MODULE_LINKER_FLAGS="$(TARGET_LDFLAGS)" \
			-DCMAKE_SHARED_LINKER_FLAGS="$(TARGET_LDFLAGS)" \
			-DCMAKE_FIND_ROOT_PATH=$(STAGING_DIR) \
			-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=$(STAGING_DIR_HOST) \
			-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=$(STAGING_DIR) \
			-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=$(STAGING_DIR) \
			-DCMAKE_STRIP=: \
			-DCMAKE_INSTALL_PREFIX=/usr \
			$(CMAKE_OPTIONS) \
		. \
	)

polarssl:
	$(MAKE) -C $(POLAR_SSL_PATH) 
	-rm -f $(POLAR_SSL_PATH)/library/libpolarssl.so
	-rm -f $(POLAR_SSL_PATH)/library/libpolarssl.so.1
	-rm -f $(POLAR_SSL_PATH)/library/libpolarssl.so.1.1.0
	-rm -f $(POLAR_SSL_PATH)/library/libpolarssl.so.1.0.0

polarssl-clean:
	$(MAKE) -C $(POLAR_SSL_PATH) clean 

polarssl-install:
#	install -D $(POLAR_SSL_PATH)/library/libpolarssl.so $(INSTALLDIR)/polarssl/usr/lib/libpolarssl.so.0
	@true
