nvram:
	make -C nvram

nvram-install:
	make -C nvram install INSTALLDIR=$(INSTALLDIR)/nvram
	
nvram-clean:
	make -C nvram clean


