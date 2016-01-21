## SHERPA Proxy implementation

This folder contains the SHERPA Proxy implementation in C.

### Dependencies
- C libraries and gcc compiler
- zmq: https://github.com/zeromq/libzmq
- czmq: https://github.com/zeromq/czmq
- jansson JSON library: https://jansson.readthedocs.org/en/latest/gettingstarted.html

### Usage
- clone the project and move into this directory:

```sh
~/$ git clone https://gitlab.mech.kuleuven.be:u0097847/sherpa-proxy.git
~/$ cd sherpa-proxy
```


- edit the env.sh file to update LD_LIBRARY_PATH
```sh
~/sherpa-proxy$ cat env.sh
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:~/workspace/sherpa-proxy/install/lib"
```

- run the Makefile

```
~/sherpa-proxy$ make
gcc -ggdb $< -Linstall/lib -lczmq -lzmq -lzyre -ljansson -luuid -o $@
```

- start the mediator and provide it with a config file

```
~/sherpa-proxy/$ ./sherpa_comm_mediator donkey.json
```
