## SHERPA Communication Mediator

This folder contains the SHERPA Proxy implementation.

### Dependencies
- C libraries and gcc compiler
- zmq: https://github.com/zeromq/libzmq
- czmq: https://github.com/zeromq/czmq
- zyre: https://github.com/zeromq/zyre
- jansson JSON library: https://jansson.readthedocs.org/en/latest/gettingstarted.html

For more information see the Dependencies file.

### Installation
- clone the project and move into this directory:
```sh
~/$ git clone https://gitlab.mech.kuleuven.be:u0097847/sherpa-proxy.git
~/$ cd sherpa-proxy
```


- Build the project

```
~/sherpa-proxy$ make
~/sherpa-proxy$ mkdir build
~/sherpa-proxy$ cd build
~/sherpa-proxy/build$ cmake ..
~/sherpa-proxy/build$ make
~/sherpa-proxy/build$ cd ..
```

- Open start the mediator and provide it with a config file

```
~/sherpa-proxy/$ ./bin/sherpa_comm_mediator examples/configs/donkey.json
```

## Examples:
Find examples and their explanation in the corresponding subfolders. There is an example for each, C and Python code. The C example shows how to query for a list of connected robots and their configuration. The Python example still needs to be done :-)

### C example: Query for list of connected nodes
In this exaple we launch several instances of mediators (i.e. robots). Then we start a local component that queries for a list of all available robots. It will send the query and then wait for the reply, print the list of robots with their header/configuration data and exit.

#### Build the example
Run the makefile in the folder.

#### Run the example
Open three terminals and run the following command in each.
Start the mediator that will connect to our local compoenent in the first terminal.

```
~/sherpa-proxy/$ ./bin/sherpa_comm_mediator examples/configs/donkey.json
```
Start a second robot in the second terminal.
```
~/sherpa-proxy/$ ./bin/sherpa_comm_mediator examples/configs/wasp1.json
```
Start the local component that will query for the peer list.
```
~/sherpa-proxy/$ cd examples/request_peers/
~/sherpa-proxy/examples/request_peers/$ ./request_peers_example
```
For code explanations see the comments in the code. If specific questions remain, ask the authors. 


### Python: Send messages and handle replies

handle errors



## Missing features:
* Subscribe to network changes (e.g. node becomes (un-)available)

## TODOs:
* Refactor messy example code
* Make features composable
* Port gossip protocol to pyre and provide python example