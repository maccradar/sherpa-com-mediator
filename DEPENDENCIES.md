# Dependencies

## rfsm-json

```sh
apt-get install liblua5.1-json luarocks
luarocks install luasocket
```

## ZMQ / CZMQ

```sh
apt-get install libtool pkg-config build-essential autoconf automake uuid-dev
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
