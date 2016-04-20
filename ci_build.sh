#!/usr/bin/env bash

set -x
set -e

# mkdir tmp
# BUILD_PREFIX=$PWD/tmp

CONFIG_OPTS=()
#CONFIG_OPTS+=("CFLAGS=-I${BUILD_PREFIX}/include")
#CONFIG_OPTS+=("CPPFLAGS=-I${BUILD_PREFIX}/include")
#CONFIG_OPTS+=("CXXFLAGS=-I${BUILD_PREFIX}/include")
#CONFIG_OPTS+=("LDFLAGS=-L${BUILD_PREFIX}/lib")
#CONFIG_OPTS+=("PKG_CONFIG_PATH=${BUILD_PREFIX}/lib/pkgconfig")
#CONFIG_OPTS+=("--prefix=${BUILD_PREFIX}")
CONFIG_OPTS+=("--with-docs=no")
CONFIG_OPTS+=("--quiet")

# Clone and build dependencies
# jansson
git clone https://github.com/akheron/jansson.git
cd jansson
autoreconf -i
./configure
make
sudo make install
cd ..
# zmq 4.1.2
wget http://download.zeromq.org/zeromq-4.1.2.tar.gz
tar zxvf zeromq-4.1.2.tar.gz
cd zeromq-4.1.2/
./configure --prefix=/opt/zeromq-4.1.2 --with-libsodium=no
make
sudo make install
cd ..
# czmq 3.0.2
wget https://github.com/zeromq/czmq/archive/v3.0.2.tar.gz
tar zxvf v3.0.2.tar.gz
cd czmq-3.0.2/
sh ./autogen.sh
env zmq_CFLAGS=-I/opt/zeromq-4.1.2/include zmq_LIBS="-L/opt/zeromq-4.1.2/lib -lzmq" ./configure --prefix=/opt/czmq-3.0.2
make
sudo make install
cd ..
## zyre 1.1.0
wget https://github.com/zeromq/zyre/archive/v1.1.0.tar.gz
tar zxvf v1.1.0.tar.gz
cd zyre-1.1.0/
sh ./autogen.sh
env PKG_CONFIG_PATH=/opt/zeromq-4.1.2/lib/pkgconfig:/opt/czmq-3.0.2/lib/pkgconfig ./configure --prefix=/opt/zyre-1.1.0
make
sudo make install
cd ..
sudo updatedb
mkdir build
cd build
cmake ..
make
