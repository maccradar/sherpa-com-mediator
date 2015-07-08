## Protocol from LKU-KUL Integration meeting in Leuven 2 July to 8 July 

### Introduction

The purpose of this meeting was to desing and implement the
communication part of the SHERPA project related to the work done by
LKU and KUL.

Participants:

- Tommy Persson (LKU)
- Johan Phillips (KUL)
- Nico Huebel (KUL)

### Agreements
- Use Zyre version 1.0
- ZeroMQ version 4.0
- CZMQ version 4.0
- Communication is split into:
    - Local group (for now: com proxy, delegation framework, and world model) using "gossip" via IPC to keep communication local
    - Remote group (between proxies), i.e., each robot is represented by one proxy
- One proxy per robot, each proxy loads configuration file (JSON) with:
  {
    short-name: string,
    long-name: unique string,
    type: [genius | hawk | wasp | donkey]
    capabilities: list of strings
  }
- Proxy adds configuration file to header 
- How to handle (TST) updates:
    - Local state updates
    - Local WM receives update message on local group
    - At its own frequency, local WM sends collection of updates to Proxy
    - Proxy forwards message to remote
    - Remote Proxy receives forwarded message, unpacks payload and puts it on its local group
- Payload should be typed so entities can check if this payload is relevant for them. There is a "type" and "payload" (json) field in the proxy message payload.
- Payload is opaque to communication proxy (but not for all message types...) but it is a JSON structure.
- Envelope is always proxy related, i.e., proxy is throwing away envelope before sending msg
- when new peers arrive, proxy shouts "peer-list"
- when local entity whispers "peers", proxy whispers "peer-list" to that entity

### Communication within one platform (local group)
- ROS node running Delegation code connecting locally through gossip
- Proxy local node binding locally through gossip 
- Proxy remote node connecting to remote network
- World Model local node connecting locally through gossip
- Local nodes join SHERPA group
- Nearly all local communication uses SHOUT
- Response to peers message is a WHISPER

### Proxy envelope structure:

- metamodel: STRING
- model: STRING
- type: STRING
- payload: JSON subpart

### Payload structure for communication envelopte type "forward" and "forward-all" and maybe the tree distribution and tree creation types:

- type: STRING
- language: STRING
- content: STRING

Example payload:

- type: to-topic
- language: "JSON"
- content: string rep of a JSON message

Example Content (JSON sent as string):
- topic: /fipa_acl_message
- msg: ...

Possible values for envelope type: forward, forward-all, update-execution-tst-node, execution-tst, ...

Possible values for payload type: to-topic, update-execution-tst-node, execution-tst, ...

### Encoding ROS things to JSON

- type: forward forward-all execution-tst update-execution-tst-node peers
- peers return JSON array with JSON objects containing name, id, type, capabilities, ... [{}, {}]
- group-name: SHERPA
- short-name: uav0   (WHISPER)
- peerid: ...
- payload: ...

Requests all peers known to Proxy/WM
{
  metamodel: sherpa_msgs
  model: uri
  type: peers
  payload: <empty>
}

List of all known peers
{
  metamodel: sherpa_msgs
  model: uri
  type: peer-list
  payload: {list of peers}
}

The contained msg is forwarded to the indicated peer via whisper by the proxy
{
  metamodel: sherpa_msgs
  model: uri
  type: forward 
  destination: node_name/uuid
  hops: max # hops
  payload: msg
}

The contained msg is forwarded to all known peers by the proxy
{
  metamodel: sherpa_msgs
  model: uri
  type: forward-all
  payload: msg
}

This msg passes the TST to the WM
{
  metamodel: sherpa_msgs
  model: execution-tst
  data: TST
}

This msg updates the TST in the WM
{
  metamodel: sherpa_msgs
  model: update-execution-tst-node
  data: TST-update
}

The TST data:
{
  tree-id: string
  tree: {
  ...
  }
}

The TST-update:
{
  tree-id:
  node-id:
  node: TST-update-node
}

The TST-update-node:
{
  field1: value1,
  field2: value2,
  field3: value3,
  field4: value4,
  field5: value5
}

Create team:
{
   metamodel: sherpa_msgs
   model: uri
   type: create-team
   payload: { team: <team-name>,
              members: [ <peerid>, ...]
            }
}

Only update the fields that are specified.

### TODO
- handshake on group name between proxies
- teams should get a group assigned during delegation
- create a team based on list of peerids
- synchronization of clocks
- query language to query the world model
- confirmation that the TST is distributed
- Release the first version of the proxy
- Release the first version of the world model

### Test cases
- WM-TST integration:
    - setup three WMs, one receives TST and distributes it to the others, they output TST
    - same as above, but the remote WMs respond with TST updates that need to be distributed and the delegation framework needs to be notified of these changes
- Communication: (see above)
    - forward
    - forward-all
    - peers

### BUGS

- payload in envelope is not a char *, so gossip_proxy.c do not work since it tried to unpack to the wrong structure.

- Not getting the local peer on the peer list

