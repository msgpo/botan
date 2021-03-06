#!/bin/bash

# 1. Configure botan for includeos

# 1.1. Mac
# ./configure.py --os=includeos --cpu=x86_32 --disable-shared --minimized  --enable-modules=sha2_64,sha2_32,x509,hkdf,hmac,dev_random,tls,system_rng --cc=clang --cc-bin=/usr/local/bin/clang++  --cc-abi-flags="--target=i686-elf -D_LIBCPP_HAS_NO_THREADS=1 -D_GNU_SOURCE -nostdlib -stdlib=libc++ -nostdinc++ -L/usr/local/includeos/lib -Wl,-rpath,/usr/local/includeos/lib -msse3 -I/usr/local/includeos/api/sys -I/usr/local/includeos/include/libcxx -I/usr/local/includeos/include/newlib -v"
# 1.2. Edit Makefile to i686-elf-ar

###
# 1.1 Linux 32-bit
# ./configure.py --os=includeos --cpu=x86_32 --disable-shared --cc=clang --cc-abi-flags="--target=i686-pc-linux-elf -msse3 -D_LIBCPP_HAS_MUSL_LIBC -D_GNU_SOURCE -nostdlib -nostdinc++ -I$INCLUDEOS_PREFIX/includeos/api -I$INCLUDEOS_PREFIX/includeos/include -I$INCLUDEOS_PREFIX/includeos/i686/include/libcxx -I$INCLUDEOS_PREFIX/includeos/i686/include/musl"
# 1.2 Linux 64-bit
# ./configure.py --os=includeos --cpu=x86_64 --disable-shared --cc=clang --cc-abi-flags="--target=x86_64-pc-linux-elf -msse3 -D_LIBCPP_HAS_MUSL_LIBC -D_GNU_SOURCE -nostdlib -nostdinc++ -I$INCLUDEOS_PREFIX/includeos/api -I$INCLUDEOS_PREFIX/includeos/include -I$INCLUDEOS_PREFIX/includeos/x86_64/include/libcxx -I$INCLUDEOS_PREFIX/includeos/x86_64/include/musl"
# 1.3 Build command
# make -j32 libs
###

# 2. make libs
# 3. create_bundle.sh

TARFILE="botan-includeos.tar.gz"
rm -f $TARFILE
pushd build/include
tar --exclude="botan/internal" -chzf ../../$TARFILE botan/ -C "../../" libbotan-2.a
popd
echo -e "> Created $TARFILE"
