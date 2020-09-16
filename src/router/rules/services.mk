services-clean:
	make -C services clean

services: nvram shared json-c
	make -j 4 -C services

services-install:
	make -C services install

