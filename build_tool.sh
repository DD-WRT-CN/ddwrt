#!/bin/sh
#
# title: ready_ddwrt.sh
# version: 1.14
# author: Jeremy Collake <jeremy@bitsum.com>
#
# This silly script will prepare a build environment
# for DD-WRT. You must also run ready_ddwrt_root.sh.
#
# RL 11/01/2006: Added line to compile tools/webcomp.c and write4.c
#
PWD=`pwd`
ME=`whoami`
DDROOT=$PWD

echo I am $ME
echo DD-WRT is at $DDROOT

#echo ................................................................
#echo creating some symlinks
#echo ................................................................
#rm $DDROOT/src/linux/brcm/linux.v23/include/asm
#ln -s $DDROOT/src/linux/brcm/linux.v23/include/asm-mips $DDROOT/src/linux/brcm/linux.v23/include/asm
# for CFE building
#ln -s $DDROOT/src/linux/brcm/linux.v23 $DDROOT/src/linux/linux
#echo done

echo ................................................................
echo adjusting some attributes
echo ................................................................
chmod +x $DDROOT/src/router/iptables/extensions/.dccp-test
chmod +x $DDROOT/src/router/iptables/extensions/.layer7-test
echo done

#echo ................................................................
#echo fixing alconf's
#echo ................................................................
#cd src/router/pptpd
#aclocal
#cd ../../..

echo ................................................................
echo "re-building some tools"
echo ................................................................
cd $DDROOT

# make bb_mkdep
cd src/router/busybox/scripts 
rm bb_mkdep
make bb_mkdep 
cd ../../../.. 

# make jsformat
cd src/router/tools 
rm jsformat
make jsformat 
cd ../../..

# make mksquashfs-lzma
cd src/squashfs-tools/
rm mksquashfs-lzma
make 
cp mksquashfs-lzma ../linux/brcm/linux.v23/scripts/squashfs
cd ../..

# make strip
cd tools
rm ./strip
gcc strip.c -o ./strip
cd ..

# make write3
cd tools
rm ./write3
gcc write3.c -o ./write3
cd ..

# make write4
cd tools
rm ./write4
gcc write4.c -o ./write4
cd ..

# make webcomp
cd tools
rm ./webcomp
gcc -o webcomp -DUEMF -DWEBS -DLINUX webcomp.c
cd ..

echo done
