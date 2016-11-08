# Dependencies

## Ubuntu packages

```sh
sudp apt-get install libtool pkg-config build-essential autoconf automake autogen uuid-dev
```

## JSON

```sh
git clone https://github.com/akheron/jansson.git
cd jansson
autoreconf -i
./configure
make
make install
```
## ZMQ
Stable Release 4.1.2
```sh
wget https://archive.org/download/zeromq_4.1.2/zeromq-4.1.2.tar.gz
tar zxvf zeromq-4.1.2.tar.gz
cd zeromq-4.1.2/
./configure --prefix=/opt/zeromq-4.1.2 --with-libsodium=no
make
sudo make install
```

## czmq
```sh
wget https://github.com/zeromq/czmq/archive/v3.0.2.tar.gz
tar zxvf v3.0.2.tar.gz
cd czmq-3.0.2/
sh ./autogen.sh
env zmq_CFLAGS=-I/opt/zeromq-4.1.2/include zmq_LIBS="-L/opt/zeromq-4.1.2/lib -lzmq" ./configure --prefix=/opt/czmq-3.0.2
make
sudo make install
```

## zyre
Stable release version 1.1.0
```sh
wget https://github.com/zeromq/zyre/archive/v1.1.0.tar.gz
tar zxvf v1.1.0.tar.gz
cd zyre-1.1.0/
sh ./autogen.sh
env PKG_CONFIG_PATH=/opt/zeromq-4.1.2/lib/pkgconfig:/opt/czmq-3.0.2/lib/pkgconfig ./configure --prefix=/opt/zyre-1.1.0
make
sudo make install
```