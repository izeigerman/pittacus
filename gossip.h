/*
 * Copyright 2016 Iaroslav Zeigerman
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
#include "member.h"
#include "vector_clock.h"
#include "config.h"

#define INPUT_BUFFER_SIZE MESSAGE_MAX_SIZE
#define OUTPUT_BUFFER_SIZE MAX_OUTPUT_MESSAGES * MESSAGE_MAX_SIZE

typedef struct message_envelope_out message_envelope_out_t;

typedef struct message_queue {
    message_envelope_out_t *head;
    message_envelope_out_t *tail;
} message_queue_t;

typedef enum member_state {
    STATE_INITIALIZED,
    STATE_JOINING,
    STATE_CONNECTED,
    STATE_LEAVING,
    STATE_DISCONNECTED,
    STATE_DESTROYED
} member_state_t;

typedef void (*data_receiver_t)(void *context, const uint8_t *buffer, size_t buffer_size);

typedef struct gossip_descriptor {
    pt_socket_fd socket;

    uint8_t input_buffer[INPUT_BUFFER_SIZE];
    uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
    size_t output_buffer_offset;

    message_queue_t outbound_messages;

    uint32_t sequence_num;
    uint32_t data_counter;
    vector_clock_t data_version;

    member_state_t state;
    cluster_member_t self_address;
    cluster_member_map_t members;

    data_receiver_t data_receiver;
    void *data_receiver_context;
} gossip_descriptor_t;

int gossip_init(gossip_descriptor_t *self,
                const pt_sockaddr *self_addr, socklen_t self_addr_len,
                data_receiver_t data_receiver, void *data_receiver_context);

int gossip_destroy(gossip_descriptor_t *self);



int gossip_process_receive(gossip_descriptor_t *self);

int gossip_process_send(gossip_descriptor_t *self);

int gossip_send_data(gossip_descriptor_t *self, const uint8_t *data, uint32_t data_size);

#endif //PITTACUS_GOSSIP_H
