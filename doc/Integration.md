

This is the goal for the first day:

![simple_goal](https://gitlab.mech.kuleuven.be/u0052546/sherpa-proxy/blob/local_gossip/doc/simple_goal.svg)


This is the goal for next week:

![final_goal](https://gitlab.mech.kuleuven.be/u0052546/sherpa-proxy/blob/local_gossip/doc/final_goal.svg)

### Communication within one platform
- ROS node running Delegation code connecting locally through gossip
- Proxy local node binding locally through gossip
- Proxy remote node connecting to remote network
- World Model local node connecting locally through gossip
- Local nodes join SHERPA group
- All local communication uses SHOUT


### Encoding ROS things to JSON

- type: forward forward-all execution-tst update-execution-tst-node peers
- peers return JSON array with JSON objects containing name, id, type, capabilities, ... [{}, {}]
- group-name: SHERPA
- short-name: uav0   (WHISPER)
- peerid: ...
- topic-name:
- payload: ROS OBJECT IN JSON

Requests all peers known to Proxy/WM
{
  metamodel: sherpa_msgs
  model: peer_request
  data: Null
}

List of all known peers
{
  metamodel: sherpa_msgs
  model: peer_response
  data: {list of peers}
}

The contained msg is forwarded to the indicated peer via whisper by the proxy
{
  metamodel: sherpa_msgs
  model: forward
  data: [node_name/uid,msg]
}

The contained msg is forwarded to all known peers by the proxy
{
  metamodel: sherpa_msgs
  model: forward-all
  data: msg
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

