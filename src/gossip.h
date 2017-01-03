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

typedef enum pittacus_gossip_state {
    STATE_INITIALIZED,
    STATE_JOINING,
    STATE_CONNECTED,
    STATE_LEAVING,
    STATE_DISCONNECTED,
    STATE_DESTROYED
} pittacus_gossip_state_t;

typedef void (*data_receiver_t)(void *context, const uint8_t *buffer, size_t buffer_size);

typedef struct pittacus_addr {
    const pt_sockaddr *addr;
    socklen_t addr_len;
} pittacus_addr_t;

typedef struct pittacus_gossip pittacus_gossip_t;

pittacus_gossip_t *pittacus_gossip_create(const pittacus_addr_t *self_addr,
                                          data_receiver_t data_receiver, void *data_receiver_context);

int pittacus_gossip_destroy(pittacus_gossip_t *self);

int pittacus_gossip_join(pittacus_gossip_t *self,
                         const pittacus_addr_t *seed_nodes, uint16_t seed_nodes_len);

int pittacus_gossip_process_receive(pittacus_gossip_t *self);

int pittacus_gossip_process_send(pittacus_gossip_t *self);

int pittacus_gossip_send_data(pittacus_gossip_t *self, const uint8_t *data, uint32_t data_size);

pittacus_gossip_state_t pittacus_gossip_state(pittacus_gossip_t *self);

pt_socket_fd pittacus_gossip_socket_fd(pittacus_gossip_t *self);

#endif //PITTACUS_GOSSIP_H
