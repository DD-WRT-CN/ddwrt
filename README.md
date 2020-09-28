git clone https://github.com/DD-WRT-CN/ddwrt.git
cd ddwrt
mkdir -p toolchains
cd toolchains
wget https://update.paldier.com/toolchains.tar.xz
tar xvJf toolchains.tar.xz
cd ..
make
cd src/router/arm-uclibc
your firmware are here
