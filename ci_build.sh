#!/usr/bin/env bash

set -x
set -e

mkdir tmp
BUILD_PREFIX=$PWD/tmp

CONFIG_OPTS=()
CONFIG_OPTS+=("CFLAGS=-I${BUILD_PREFIX}/include")
CONFIG_OPTS+=("CPPFLAGS=-I${BUILD_PREFIX}/include")
CONFIG_OPTS+=("CXXFLAGS=-I${BUILD_PREFIX}/include")
CONFIG_OPTS+=("LDFLAGS=-L${BUILD_PREFIX}/lib")
CONFIG_OPTS+=("PKG_CONFIG_PATH=${BUILD_PREFIX}/lib/pkgconfig")
CONFIG_OPTS+=("--prefix=${BUILD_PREFIX}")
CONFIG_OPTS+=("--with-docs=no")
CONFIG_OPTS+=("--quiet")

# Clone and build dependencies
git clone https://github.com/akheron/jansson.git
cd jansson
autoreconf -i
./configure
make
sudo make install
cd ..
git clone --quiet --depth 1 https://github.com/zeromq/libzmq libzmq
cd libzmq
git --no-pager log --oneline -n1
if [ -e autogen.sh ]; then
./autogen.sh 2> /dev/null
fi
if [ -e buildconf ]; then
./buildconf 2> /dev/null
fi
./configure "${CONFIG_OPTS[@]}"
make -j4
sudo make install
cd ..
git clone --quiet --depth 1 https://github.com/zeromq/czmq czmq
cd czmq
git --no-pager log --oneline -n1
if [ -e autogen.sh ]; then
./autogen.sh 2> /dev/null
fi
if [ -e buildconf ]; then
./buildconf 2> /dev/null
fi
./configure "${CONFIG_OPTS[@]}"
make -j4
sudo make install
cd ..
git clone --quiet --depth 1 https://github.com/zeromq/zyre zyre
cd zyre
git --no-pager log --oneline -n1
if [ -e autogen.sh ]; then
./autogen.sh 2> /dev/null
fi
if [ -e buildconf ]; then
./buildconf 2> /dev/null
fi
./configure "${CONFIG_OPTS[@]}"
make -j4
sudo make install
cd ..
sudo updatedb
mkdir build
cd build
cmake ..
make
