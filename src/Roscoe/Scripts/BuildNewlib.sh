export PATH=$PATH:/opt/cross/bin
rm -rf ../newlibbuild
mkdir ../newlibbuild
rm -rf ../newlib
mkdir ../newlib
cd ../newlibbuild
fullpath=$(realpath ../newlib)
../Utils/newlib/configure --prefix=$fullpath \
    --target=m68k-elf \
    --enable-lite-exit --enable-multilib --disable-newlib-fvwrite-in-streami \
    --enable-newlib-nano-formatted-io
make -j
make install
rm -rf ../newlibbuild


