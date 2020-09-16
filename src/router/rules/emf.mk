emf:
	#$(MAKE) -C emf/emfconf CROSS=$(CROSS_COMPILE)
	#$(MAKE) -C emf/igsconf CROSS=$(CROSS_COMPILE)
	#$(MAKE) -C emf/emf
	#$(MAKE) -C emf/igs

emf-clean:
	#$(MAKE) -C emf/emfconf clean
	#$(MAKE) -C emf/igsconf clean
	#$(MAKE) -C emf/emf clean
	#$(MAKE) -C emf/igs clean

emf-install:
	#$(MAKE) -C emf/igsconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR)/emf install
	#$(MAKE) -C emf/emfconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR)/emf install
	#$(MAKE) -C emf/igs CROSS=$(CROSS_COMPILE) INSTALL_MOD_PATH=$(INSTALLDIR)/emf install
	#$(MAKE) -C emf/emf CROSS=$(CROSS_COMPILE) INSTALL_MOD_PATH=$(INSTALLDIR)/emf install
	install -D emf_bin/$(ARCH)/emf.ko $(INSTALLDIR)/emf/lib/modules/$(KERNELRELEASE)/emf.ko
	install -D emf_bin/$(ARCH)/igs.ko $(INSTALLDIR)/emf/lib/modules/$(KERNELRELEASE)/igs.ko
	install -D emf_bin/$(ARCH)/emf $(INSTALLDIR)/emf/usr/sbin/emf
	install -D emf_bin/$(ARCH)/igs $(INSTALLDIR)/emf/usr/sbin/igs
