#!/bin/bash
# Start up a proxy using the name that was provided as argument
./proxy https://gitlab.mech.kuleuven.be/rob-picknpack/pnp-reference/raw/master/lcsm.json $1 tcp://localhost:9001 tcp://*:9002 33033

