### Agreements
- Payload is opaque to communication proxy
- How to handle updates:
    - Local state updates
    - Local WM receives update message on local group
    - At its own frequency, local WM sends collection of updates to Proxy
    - Proxy forwards message to remote
    - Remote Proxy receives forwarded message, unpacks payload and puts it on its local group
- Payload should be typed so entities can check if this payload is relevant for them 
- One proxy per robot, each proxy loads configuration file (JSON) with:
  {
    short-name: string,
    long-name: unique string,
    type: [genius | hawk | wasp | donkey]
    capabilities: list of strings
  }
- Proxy adds configuration file to header 

This is the goal for the first day:

![simple_goal](http://gitlab.mech.kuleuven.be/u0052546/sherpa-proxy/raw/local_gossip/doc/simple_goal.svg)


This is the goal for next week:

![final_goal](http://gitlab.mech.kuleuven.be/u0052546/sherpa-proxy/raw/local_gossip/doc/final_goal.svg)

### Communication within one platform
- ROS node running Delegation code connecting locally through gossip
- Proxy local node binding locally through gossip
- Proxy remote node connecting to remote network
- World Model local node connecting locally through gossip
- Local nodes join SHERPA group
- All local communication uses SHOUT

### Payload structure:

Typical ROS message:
- type: to-topic
- topic: /fipa_acl_message
- msg: ...

Possible values for type: to-topic, 

### Encoding ROS things to JSON

- type: forward forward-all execution-tst update-execution-tst-node peers
- peers return JSON array with JSON objects containing name, id, type, capabilities, ... [{}, {}]
- group-name: SHERPA
- short-name: uav0   (WHISPER)
- peerid: ...
- topic-name:
- payload: Some payload

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
  model: peers_reply
  data: {list of peers}
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
Only update the fields that are specified.

