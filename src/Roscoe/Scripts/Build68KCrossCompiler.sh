apt-get -y install gcc
apt-get -y install g++
apt-get -y install make
apt-get -y install bison
apt-get -y install flex
apt-get -y install libgmp3-dev
apt-get -y install libmpfr-dev
apt-get -y install texinfo
apt-get -y install libmpc-dev
apt-get -y install gcc-multilib
mkdir /opt/cross

# Build binutils
mkdir src
cd src
wget https://ftp.gnu.org/gnu/binutils/binutils-2.38.tar.gz
gunzip binutils-2.38.tar.gz
tar -xvf binutils-2.38.tar
mkdir build-binutils
cd build-binutils
../binutils-2.38/configure --target=m68k-elf --prefix=/opt/cross --with-arch-m68k --with-cpu=m68000 --disable-nils --with-sysroot --disable-werror
make -j
make install
cd ..

# Build 68K gcc
wget https://mirrorservice.org/sites/sourceware.org/pub/gcc/releases/gcc-11.2.0/gcc-11.2.0.tar.gz
gunzip gcc-11.2.0.tar.gz
tar xvf gcc-11.2.0.tar
mkdir gcc-build
cd gcc-build
export PATH="/opt/cross/bin:$PATH"
../gcc-11.2.0/configure --target=m68k-elf --prefix=/opt/cross --disable-nls --enable-language=c,c++ --without-headers --with-arch=m68k --with-cpu=m68000
make all-gcc 
make all-target-libgcc 
make install-gcc
make install-target-libgcc
rm -rf src
