

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

- type: forward forward-all execution-tst update-execution-tst peers
- peers return [{}, {}]
- group-name: SHERPA
- short-name: uav0   (WHISPER)
- peerid: ...
- topic-name:
- payload: ROS OJECT IN JSON


