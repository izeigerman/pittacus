# Pittacus

## Description
Pittacus - is an extremely small gossip protocol implementation in pure C. Its main goal is a data dissemination rather than membership management.
The crucial features and advantages of Pittacus are the following:
* Allows to build a fully decentralized P2P cluster without a single server instance.
* Pittacus is a very lightweight library with zero external dependencies.
* Utilizes UDP for the transport layer.
* Very tiny and adjustable memory footprint.
* Small protocol overhead.
* The data spreading is pretty fast (subjective statement, didn't have a chance to compare it to other options).
* It's distributed under the Apache License 2.0.

Don't expect from Pittacus the following:
* Cluster membership tracking and management. As mentioned above Pittacus - is a dissemination protocol. This means that each node has to be aware only of a small part of the cluster to function properly. While Pittacus is pretty good in fast distribution of data across the cluster, it doesn't provide any guarantees about cluster convergence or data consistency (at least for now).
* Transferring of huge amounts of data. Since UDP is not a reliable protocol, it imposes some restrictions on a maximum size of each packet (the larger size is - the higher risk to lose a packet). The default maximum message size for Pittacus is 512 bytes (the value is configurable). This includes the protocol overhead, which is only 26 bytes for the data message. So by default the payload size shouldn't exceed 512 - 26 = 486 bytes per one message. This should be enough for a small command or notification together with PKCS#1 signature to verify an initiator.

So far neither the message delivery order nor the delivery itself have strong guarantees.

NOTE: at this point Pittacus is in active development stage. It can be used for experiments but not for production solutions. A lot of things have to be done in order to release the first version.

## How to build
Install CMake >= 3.0.
```
git clone https://github.com/izeigerman/pittacus.git
cd pittacus
mkdir build && cd build
cmake ..
make
```

To install Pittacus:
```
make install
```

## How to use
First of all include the Pittacus header:
```cpp
#include <pittacus/gossip.h>
```

Now instantiate a Pittacus descriptor with a `sockaddr` structure that represents an address of the current node and a data receiver callback:
```cpp
struct sockaddr_in self_in;
self_in.sin_family = AF_INET;
self_in.sin_port = htons(65000); // use 0 instead to pick up a random port
inet_aton("127.0.0.1", &self_in.sin_addr);

// Filling in the address of the current node.
pittacus_addr_t self_addr = {
    .addr = (const pt_sockaddr *) &self_in,
    .addr_len = sizeof(struct sockaddr_in)
};

// Create a new Pittacus descriptor instance.
pittacus_gossip_t *gossip = pittacus_gossip_create(&self_addr, &data_receiver, NULL);
if (gossip == NULL) {
    fprintf(stderr, "Gossip initialization failed: %s\n", strerror(errno));
    return -1;
}
```

The data receiver callback may look like following:
```cpp
void data_receiver(void *context, pittacus_gossip_t *gossip, const uint8_t *data, size_t data_size) {
    // This function is invoked every time when a new data arrives.
    printf("Data size is: %u\n", data_size);
}
```

It's time join a cluster. There are 2 ways to do this: 1) specify the list of seed nodes that are used as entry points to a cluster or 2) specify nothing if this instance is going to be a seed node in itself.
```cpp
// Provide a seed node destination address.
struct sockaddr_in seed_node_in;
seed_node_in.sin_family = AF_INET;
seed_node_in.sin_port = htons(65000);
inet_aton("127.0.0.1", &seed_node_in.sin_addr);

pittacus_addr_t seed_node_addr = {
    .addr = (const pt_sockaddr *) &seed_node_in,
    .addr_len = sizeof(struct sockaddr_in)
};

// Join a cluster.
int join_result = pittacus_gossip_join(gossip, &seed_node_addr, 1);
if (join_result < 0) {
    fprintf(stderr, "Gossip join failed: %d\n", join_result);
    pittacus_gossip_destroy(gossip);
    return -1;
}
```

To force Pittacus to read a message from the network:
```cpp
recv_result = pittacus_gossip_process_receive(gossip);
if (recv_result < 0) {
    fprintf(stderr, "Gossip receive failed: %d\n", recv_result);
    pittacus_gossip_destroy(gossip);
    return -1;
}
```

To flush the outbound messages to the network:
```cpp
send_result = pittacus_gossip_process_send(gossip);
if (send_result < 0) {
    fprintf(stderr, "Gossip send failed: %d\n", recv_result);
    pittacus_gossip_destroy(gossip);
    return -1;
}
```

In order to enable the anti-entropy in Pittacus you should periodically call the gossip tick function:
```cpp
int time_till_next_tick = pittacus_gossip_tick(gossip);
```
This function returns a time period in milliseconds which indicates when the next tick should occur. Check out the code documentation for further details.

To spread some data within a cluster:
```cpp
pittacus_gossip_send_data(gossip, data, data_size);
```

Destroy a Pittacus descriptor:
```cpp
pittacus_gossip_destroy(gossip);
```

For a more complete examples check out the `demos/demo_node.c` and `demos/demo_seed_node.c` demo applications. Both demo applications will be built automatically together with the library code.

