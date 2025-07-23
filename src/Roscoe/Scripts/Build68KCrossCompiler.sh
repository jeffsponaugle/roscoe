sudo apt-get -y install gcc
sudo apt-get -y install g++
sudo apt-get -y install make
sudo apt-get -y install bison
sudo apt-get -y install flex
sudo apt-get -y install libgmp3-dev
sudo apt-get -y install libmpfr-dev
sudo apt-get -y install texinfo
sudo apt-get -y install libmpc-dev
sudo apt-get -y install gcc-multilib
sudo rm -rf /opt/cross
sudo mkdir /opt
sudo mkdir /opt/cross

# Build binutils
mkdir src
cd src
wget https://ftp.gnu.org/gnu/binutils/binutils-2.38.tar.gz
tar -xvf binutils-2.38.tar.gz
mkdir build-binutils
cd build-binutils
../binutils-2.38/configure --target=m68k-elf --prefix=/opt/cross --with-arch-m68k --with-cpu=m68000 --disable-nils --with-sysroot --disable-werror
make -j
sudo make install
cd ..

# Build 68K gcc
wget https://mirrorservice.org/sites/sourceware.org/pub/gcc/releases/gcc-11.2.0/gcc-11.2.0.tar.gz
tar xvf gcc-11.2.0.tar.gz
mkdir gcc-build
cd gcc-build
export PATH="/opt/cross/bin:$PATH"
../gcc-11.2.0/configure --target=m68k-elf --prefix=/opt/cross --disable-nls --enable-language=c,c++ --without-headers --with-arch=m68k --with-cpu=m68000
make all-gcc 
make all-target-libgcc 
sudo make install-gcc
sudo make install-target-libgcc
cd ..
rm -rf src
