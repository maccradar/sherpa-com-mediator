## SHERPA Proxy implementation

This folder contains the SHERPA Proxy implementation in C.

### Dependencies
- C libraries and gcc compiler
- lua 5.1 and liblua5.1-dev
- zmq: https://github.com/zeromq/libzmq
- liblua5.1-json
- luarocks
- luasocket (install through luarocks!)
- czmq: https://github.com/zeromq/czmq
- jansson JSON library: https://jansson.readthedocs.org/en/latest/gettingstarted.html

### Usage
- clone the project and move into this directory:

```sh
~/$ git clone https://gitlab.mech.kuleuven.be/u0052546/sherpa-proxy.git
~/$ cd sherpa-proxy
```

- install the rfms-json dependency

```sh
~/sherpa-proxy/$ mkdir build && cd build
~/sherpa-proxy/build/$ cmake -DCMAKE_INSTALL_PREFIX=../install ../
-- The C compiler identification is GNU 4.9.2
-- The CXX compiler identification is GNU 4.9.2
-- Check for working C compiler: /usr/bin/cc
-- Check for working C compiler: /usr/bin/cc -- works
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working CXX compiler: /usr/bin/c++
-- Check for working CXX compiler: /usr/bin/c++ -- works
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Found PkgConfig: /usr/bin/pkg-config (found version "0.28") 
-- checking for module 'lua5.1'
--   found lua5.1, version 5.1.5
-- Configuring done
CMake Warning (dev) at rfsmbinding/CMakeLists.txt:1 (add_library):
  Policy CMP0038 is not set: Targets may not link directly to themselves.
  Run "cmake --help-policy CMP0038" for policy details.  Use the cmake_policy
  command to set the policy and suppress this warning.

  Target "rfsmbinding" links to itself.
This warning is for project developers.  Use -Wno-dev to suppress it.

CMake Warning (dev) at rfsmbinding/CMakeLists.txt:1 (add_library):
  Policy CMP0038 is not set: Targets may not link directly to themselves.
  Run "cmake --help-policy CMP0038" for policy details.  Use the cmake_policy
  command to set the policy and suppress this warning.

  Target "rfsmbinding" links to itself.
This warning is for project developers.  Use -Wno-dev to suppress it.

CMake Warning (dev) in rfsmbinding/CMakeLists.txt:
  Policy CMP0022 is not set: INTERFACE_LINK_LIBRARIES defines the link
  interface.  Run "cmake --help-policy CMP0022" for policy details.  Use the
  cmake_policy command to set the policy and suppress this warning.

  Target "rfsmbinding" has an INTERFACE_LINK_LIBRARIES property.  This should
  be preferred as the source of the link interface for this library but
  because CMP0022 is not set CMake is ignoring the property and using the
  link implementation as the link interface instead.

  INTERFACE_LINK_LIBRARIES:

    rfsmbinding;lua5.1

  Link implementation:

    lua5.1

This warning is for project developers.  Use -Wno-dev to suppress it.

-- Generating done
-- Build files have been written to: ~/sherpa-proxy/build
~/sherpa-proxy/build$ make
Scanning dependencies of target rfsmbinding
[ 50%] Building C object rfsmbinding/CMakeFiles/rfsmbinding.dir/rfsmbinding.c.o
Linking C shared library librfsmbinding.so
[ 50%] Built target rfsmbinding
Scanning dependencies of target fsmloader
[100%] Building C object app/CMakeFiles/fsmloader.dir/fsmloader.c.o
Linking C executable pnp-reference/bin/fsmloader
[100%] Built target fsmloader
~/sherpa-proxy/build$ make install
[ 50%] Built target rfsmbinding
[100%] Built target fsmloader
Install the project...
-- Install configuration: ""
-- Installing: ~/sherpa-proxy/install/lib/librfsmbinding.so
-- Installing: ~/sherpa-proxy/install/bin/fsmloader
-- Removed runtime path from "~/sherpa-proxy/install/bin/fsmloader" 
```

- check that the location of your lua headers is set correctly in the Makefile

```sh
~/sherpa-proxy$ cat Makefile 
all: peer

% : %.c
	gcc -ggdb $< -I/usr/include/lua5.1 -lczmq -lzmq -lrfsmbinding -o $@
```
- edit the env.sh file to update LD_LIBRARY_PATH
```sh
~/sherpa-proxy$ cat env.sh
CURR_DIR="scripts/?.lua"
export LUA_PATH=";;;$CURR_DIR;$LUA_PATH"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:~/workspace/sherpa-proxy/install/lib"
```

- run the Makefile

```
~/sherpa-proxy$ make
gcc -ggdb proxy.c -I/usr/include/lua5.1 -lczmq -lzmq -lrfsmbinding -o proxy
```

- start a proxy using the script and provide it with a <name>

```
~/sherpa-proxy/$ ./proxy.sh donkey
```

You can now trigger the state machine by entering events using the keyboard. Which events result in a transition can be found in lcsm.json, shown below.

```json
{
  "type":"rfsm_model",
  "version":2,
  "rfsm": {
    "type" : "state",
    "transitions" : [
      { "tgt": "inactive", "src": "initial", "events": [] },
      { "tgt": "inactive", "src": "active", "events": ["e_deactivate"] },
      { "tgt": "active", "src": "inactive", "events": ["e_activate"] }
    ],
    "containers" : [
      { "id": "inactive", 
         "type": "state",
         "transitions": [
          { "tgt": "creating", "src": "initial", "events": [] },
          { "tgt": "configuring_resources", "src":"creating", "events": [ "e_done" ] },
          { "tgt": "deleting", "src":"initial", "events": ["e_deactivate"] }
         ],
         "containers": [
           { "id": "creating", "type": "state" , "entry": "creating"},
           { "id": "configuring_resources", "type": "state", "entry": "configuring_resources" },
           { "id": "deleting", "type": "state", "entry": "deleting" }
         ]
      },
      { "id": "active", 
        "type": "state",
        "transitions" : [
          { "tgt": "configuring_capabilities", "src": "initial", "events": [] },
          { "tgt": "pausing", "src":"configuring_capabilities", "events": [ "e_done" ] },
          { "tgt": "running", "src":"pausing", "events": ["e_run"] },
          { "tgt": "pausing", "src":"running", "events": ["e_pause"] },
          { "tgt": "configuring_capabilities", "src":"pausing", "events": ["e_configure"] }
        ],
        "containers" :  [
           { "id": "configuring_capabilities", "type": "state", "entry": "configuring_capabilities" },
           { "id": "running", "type": "state", "entry": "running" },
           { "id": "pausing", "type": "state", "entry": "pausing" }
        ]
      }
    ]
  }
}
```
