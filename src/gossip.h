/*
 * Copyright 2016-2017 Iaroslav Zeigerman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef PITTACUS_GOSSIP_H
#define PITTACUS_GOSSIP_H

#include "network.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct pittacus_gossip pittacus_gossip_t;

typedef enum pittacus_gossip_state {
    STATE_INITIALIZED,
    STATE_JOINING,
    STATE_CONNECTED,
    STATE_LEAVING,
    STATE_DISCONNECTED,
    STATE_DESTROYED
} pittacus_gossip_state_t;

/**
 * The definition of the user's data receiver.
 *
 * @param context a reference to the arbitrary context specified
 *                by a user.
 * @param gossip a reference to the gossip descriptor instance.
 * @param buffer a reference to the buffer where the data payload
 *               is stored.
 * @param buffer_size a size of the data buffer.
 * @return Void.
 */
typedef void (*data_receiver_t)(void *context, pittacus_gossip_t *gossip,
                                const uint8_t *buffer, size_t buffer_size);

typedef struct pittacus_addr {
    const pt_sockaddr *addr; /**< pointer to the address instance. */
    socklen_t addr_len; /**< size of the address. */
} pittacus_addr_t;

/**
 * Creates a new gossip descriptor instance.
 *
 * @param self_addr the address of the current node. This one is used
 *                  for binding as well as for the propagation of this
 *                  node destination address to other nodes.
 *                  Note: don't use "localhost" or INADDR_ANY because other
 *                  nodes won't be able to reach out to this node.
 * @param data_receiver a data receiver callback. It's invoked each time when
 *                      a new data message arrives.
 * @param data_receiver_context an arbitrary context that is always passed to
 *                              a data_receiver callback.
 * @return a new gossip descriptor instance.
 */
pittacus_gossip_t *pittacus_gossip_create(const pittacus_addr_t *self_addr,
                                          data_receiver_t data_receiver, void *data_receiver_context);

/**
 * Destroys a gossip descriptor instance.
 *
 * @param self a gossip descriptor instance.
 * @return zero on success or negative value if the operation failed.
 */
int pittacus_gossip_destroy(pittacus_gossip_t *self);

/**
 * Join the gossip cluster using the list of seed nodes.
 *
 * @param self a gossip descriptor instance.
 * @param seed_nodes a list of seed node addresses.
 * @param seed_nodes_len a size of the list.
 * @return zero on success or negative value if the operation failed.
 */
int pittacus_gossip_join(pittacus_gossip_t *self,
                         const pittacus_addr_t *seed_nodes, uint16_t seed_nodes_len);

/**
 * Suggests Pittacus to read a next message from the socket.
 * Only one message will be read.
 *
 * @param self a gossip descriptor instance.
 * @return zero on success or negative value if the operation failed.
 */
int pittacus_gossip_process_receive(pittacus_gossip_t *self);

/**
 * Suggests Pittacus to write existing outbound messages to the socket.
 * All available messages will be written to the socket.
 *
 * @param self a gossip descriptor instance.
 * @return a number of sent messages or negative value if the operation failed.
 */
int pittacus_gossip_process_send(pittacus_gossip_t *self);

/**
 * Spreads the given data buffer within a gossip cluster.
 * Note: no network transmission will be performed at this
 * point. The message is added to a queue of outbound messages
 * and will be sent to a cluster during the next pittacus_gossip_process_send()
 * invocation.
 *
 * @param self a gossip descriptor instance.
 * @param data a payload.
 * @param data_size a payload size.
 * @return zero on success or negative value if the operation failed.
 */
int pittacus_gossip_send_data(pittacus_gossip_t *self, const uint8_t *data, uint32_t data_size);

/**
 * Retrieves a current state of this node.
 *
 * @param self a gossip descriptor instance.
 * @return this node's state.
 */
pittacus_gossip_state_t pittacus_gossip_state(pittacus_gossip_t *self);

/**
 * Retrieves gossip socket descriptor.
 *
 * @param self  a gossip descriptor instance.
 * @return a socket descriptor.
 */
pt_socket_fd pittacus_gossip_socket_fd(pittacus_gossip_t *self);

#ifdef  __cplusplus
} // extern "C"
#endif

#endif //PITTACUS_GOSSIP_H
