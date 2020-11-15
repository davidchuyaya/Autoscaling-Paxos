# Autoscaling-Paxos
A Paxos protocol that is capable of scaling dynamically.

## Environment Setup (w/ Anna)
We are using [Anna](https://github.com/hydro-project/anna), a low-latency, auto-scaling KVS as the underlying data store for our system. Follow the [instructions here](https://github.com/hydro-project/anna/blob/master/docs/building-anna.md) to build Anna and follow the [instructions here](https://github.com/hydro-project/anna/blob/master/docs/local-mode.md) to run Anna locally.

## Setup
Compile flags are set in `CMakeLists.txt`. I recommend using CLion as your IDE to take advantage of that.

### Protobuf
Messaging between nodes uses Google's Protobuf. Follow the [instructions here](https://github.com/protocolbuffers/protobuf/blob/master/src/README.md) to install it on your machine.

### Anna
We port Anna client code [here](https://github.com/tajshaik24/anna-client) to be able to communicate with it. Remember to run `git submodule init` and `git submodule update` in order to have that code downloaded as well.
