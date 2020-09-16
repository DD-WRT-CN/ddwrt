ifeq ($(ARCH),arm)
MUSL_LD:=ld-musl-armhf.so.1
KERNEL_HEADER_ARCH:=arm
endif
ifeq ($(ARCH),aarch64)
MUSL_LD:=ld-musl-aarch64.so.1
KERNEL_HEADER_ARCH:=arm64
endif
ifeq ($(ARCHITECTURE),northstar)
MUSL_LD:=ld-musl-arm.so.1
KERNEL_HEADER_ARCH:=arm
endif
ifeq ($(ARCHITECTURE),openrisc)
MUSL_LD:=ld-musl-arm.so.1
KERNEL_HEADER_ARCH:=arm
endif
ifeq ($(ARCH),armeb)
MUSL_LD:=ld-musl-armeb.so.1
KERNEL_HEADER_ARCH:=arm
endif
ifeq ($(ARCH),mips)
MUSL_LD:=ld-musl-mips-sf.so.1
KERNEL_HEADER_ARCH:=mips
endif
ifeq ($(ARCH),mipsel)
MUSL_LD:=ld-musl-mipsel-sf.so.1
KERNEL_HEADER_ARCH:=mips
endif
ifeq ($(ARCH),mips64)
MUSL_LD:=ld-musl-mips64-sf.so.1
KERNEL_HEADER_ARCH:=mips
endif
ifeq ($(ARCH),i386)
MUSL_LD:=ld-musl-i386.so.1
KERNEL_HEADER_ARCH:=i386
endif
ifeq ($(ARCH),x86_64)
MUSL_LD:=ld-musl-x86_64.so.1
KERNEL_HEADER_ARCH:=x86_64
endif
ifeq ($(ARCH),powerpc)
MUSL_LD:=ld-musl-powerpc-sf.so.1
KERNEL_HEADER_ARCH:=powerpc
endif


install_headers:
# important step, required for new kernels
	-mkdir -p $(TOP)/kernel_headers/$(KERNELRELEASE)
	$(MAKE) -C $(LINUXDIR) headers_install ARCH=$(KERNEL_HEADER_ARCH) INSTALL_HDR_PATH=$(TOP)/kernel_headers/$(KERNELRELEASE)

	
realclean: $(obj-clean)
	rm -f .config.old .config.cmd
	#umount $(TARGETDIR)
	rm -rf $(INSTALLDIR)
	rm -rf $(TARGETDIR)
	rm -f $(TARGETDIR)/*
	-rm -f $(ARCH)-uclibc/*

	
clean: rc-clean httpd-clean services-clean radauth-clean shared-clean libutils-clean nvram-clean madwifi-clean madwifi_mimo-clean busybox-clean dnsmasq-clean iptables-clean pppd-clean iproute2-clean
	rm -f .config.old .config.cmd
	#umount $(TARGETDIR)
	rm -rf $(INSTALLDIR)
	rm -rf $(TARGETDIR)
	rm -f $(TARGETDIR)/*
	-rm -f $(ARCH)-uclibc/*

clean_target:
	rm -rf $(TARGETDIR)
	rm -rf $(INSTALLDIR)

distclean mrproper: $(obj-distclean) clean_target
	rm -rf $(INSTALLDIR)
	-$(MAKE) -C $(LINUXDIR) distclean
	-$(MAKE) -C $(LINUXDIR)/arch/mips/bcm947xx/compressed clean
	$(MAKE) -C config clean
	rm -f .config $(LINUXDIR)/.config
	rm -f .config.old .config.cmd

optimize-lib:
ifneq ($(CONFIG_MUSL),y)
	cp ${shell $(ARCH)-linux-gcc -print-file-name=libc.so.0} $(ARCH)-uclibc/target/lib/libc.so.0 
else
	cp ${shell $(ARCH)-linux-gcc -print-file-name=libc.so} $(ARCH)-uclibc/target/lib/libc.so 
endif
ifneq ($(CONFIG_MUSL),y)
	cp ${shell $(ARCH)-linux-gcc -print-file-name=ld-uClibc.so.0} $(ARCH)-uclibc/target/lib/ld-uClibc.so.0 
else
	cd $(ARCH)-uclibc/target/lib && ln -sf libc.so $(MUSL_LD)
endif
	cp ${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1} $(ARCH)-uclibc/target/lib/libgcc_s.so.1 
ifeq ($(CONFIG_LIBDL),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libdl.so.0} $(ARCH)-uclibc/target/lib/libdl.so.0 
endif
ifeq ($(CONFIG_LIBRT),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=librt.so.0} $(ARCH)-uclibc/target/lib/librt.so.0 
endif
ifeq ($(CONFIG_LIBNSL),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libnsl.so.0} $(ARCH)-uclibc/target/lib/libnsl.so.0 
endif
ifeq ($(CONFIG_LIBUTIL),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libutil.so.0} $(ARCH)-uclibc/target/lib/libutil.so.0 
endif
ifeq ($(CONFIG_LIBCPP),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libstdc++.so.6} $(ARCH)-uclibc/target/lib/libstdc++.so.6 
endif
ifeq ($(CONFIG_LIBCRYPT),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libcrypt.so.0} $(ARCH)-uclibc/target/lib/libcrypt.so.0 
endif
ifeq ($(CONFIG_LIBM),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libm.so.0} $(ARCH)-uclibc/target/lib/libm.so.0 
endif
ifeq ($(CONFIG_LIBRESOLV),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libresolv.so.0} $(ARCH)-uclibc/target/lib/libresolv.so.0 
endif
ifeq ($(CONFIG_LIBPTHREAD),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libpthread.so.0} $(ARCH)-uclibc/target/lib/libpthread.so.0 
endif
ifeq ($(CONFIG_SQUID),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libatomic.so.1} $(ARCH)-uclibc/target/lib/libatomic.so.1 
endif
ifeq ($(CONFIG_FREERADIUS),y)
	-cp ${shell $(ARCH)-linux-gcc -print-file-name=libatomic.so.1} $(ARCH)-uclibc/target/lib/libatomic.so.1 
endif
ifeq ($(CONFIG_RELINK),y)
ifneq ($(CONFIG_MUSL),y)
	relink-lib.sh \
		"$(ARCH)-linux-" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libc_so.a}" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libc_so.a}" \
		"$(ARCH)-uclibc/target/lib/libc.so.0" \
		-Wl,-init,__uClibc_init -Wl,-soname=libc.so.0 \
		${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1}
else
	relink-lib.sh \
		"$(ARCH)-linux-" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libc.a}" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libc.a}" \
		"$(ARCH)-uclibc/target/lib/libc.so" \
		-Wl,-init,__uClibc_init -Wl,-soname=libc.so \
		${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1}
endif

	-relink-lib.sh \
		"$(ARCH)-linux-" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libcrypt.so}" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libcrypt_pic.a}" \
		"$(ARCH)-uclibc/target/lib/libcrypt.so.0" \
		${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1} \
		-Wl,-soname=libcrypt.so.0

	-relink-lib.sh \
		"$(ARCH)-linux-" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libm.so}" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libm_pic.a}" \
		"$(ARCH)-uclibc/target/lib/libm.so.0" \
		${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1} \
		-Wl,-soname=libm.so.0 

	-relink-lib.sh \
		"$(ARCH)-linux-" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libpthread.so.0}" \
		"${shell $(ARCH)-linux-gcc -print-file-name=libpthread_so.a}" \
		"$(ARCH)-uclibc/target/lib/libpthread.so.0" \
		-Wl,-z,nodelete,-z,initfirst,-init=__pthread_initialize_minimal_internal \
		${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1} \
		-Wl,-soname=libpthread.so.0
endif
	cp ${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1} $(ARCH)-uclibc/target/lib/libgcc_s.so.1 
#	relink-lib.sh \
#		"$(ARCH)-linux-" \
#		"${shell $(ARCH)-linux-gcc -print-file-name=libgcc_s.so.1}" \
#		"${shell $(ARCH)-linux-gcc -print-file-name=libgcc_pic.a}" \
#		"$(ARCH)-uclibc/target/lib/libgcc_s.so.1" \
#		-Wl,--version-script=${shell $(ARCH)-linux-gcc -print-file-name=libgcc.map} -Wl,-soname=libgcc_s.so.1



ifneq ($(CONFIG_NOOPT),y)
	rm -rf /tmp/$(ARCHITECTURE)/mklibs-out
	rm -f /tmp/$(ARCHITECTURE)/mklibs-progs
	-mkdir -p /tmp/$(ARCHITECTURE)/
	find $(TARGETDIR) -type f -perm /100 -exec \
		file -r -N -F '' {} + | \
		awk ' /executable.*dynamically/ { print $$1 }' > /tmp/$(ARCHITECTURE)/mklibs-progs

	find $(TARGETDIR) -type f -name \*.so\* -exec \
		file -r -N -F '' {} + | \
		awk ' /shared object/ { print $$1 }' >> /tmp/$(ARCHITECTURE)/mklibs-progs

	mkdir -p /tmp/$(ARCHITECTURE)/mklibs-out
ifneq ($(CONFIG_MUSL),y)
	mklibs.py -D \
		-d /tmp/$(ARCHITECTURE)/mklibs-out \
		--sysroot $(TARGETDIR) \
		-L /lib \
		-L /usr/lib \
		--ldlib /lib/ld-uClibc.so.0 \
		--target $(ARCH)-linux-uclibc \
		`cat /tmp/$(ARCHITECTURE)/mklibs-progs` 2>&1
else
	mklibs.py -D \
		-d /tmp/$(ARCHITECTURE)/mklibs-out \
		--sysroot $(TARGETDIR) \
		-L /lib \
		-L /usr/lib \
		--ldlib /lib/$(MUSL_LD) \
		--target $(ARCH)-linux-uclibc \
		`cat /tmp/$(ARCHITECTURE)/mklibs-progs` 2>&1

endif
	cp /tmp/$(ARCHITECTURE)/mklibs-out/* $(TARGETDIR)/lib
endif
#	rm -f $(TARGETDIR)/lib/*.a
#	rm -f $(TARGETDIR)/lib/*.map
#	cp lib.$(ARCH)/libresolv.so.0 $(TARGETDIR)/lib
#	cp lib.$(ARCH)/libgcc_s.so.1 $(TARGETDIR)/lib
