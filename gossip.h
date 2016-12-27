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

#define MAX_MESSAGE_SIZE 512
#define MAX_INPUT_MESSAGES 20
// must be >= MAX_INPUT_MESSAGES
#define MAX_OUTPUT_MESSAGES 25
#define INPUT_BUFFER_SIZE MAX_INPUT_MESSAGES * MAX_MESSAGE_SIZE
#define OUTPUT_BUFFER_SIZE MAX_OUTPUT_MESSAGES * MAX_MESSAGE_SIZE

typedef struct member_map member_list_t;
typedef struct message_queue message_queue_t;

typedef enum member_state {
    STATE_INITIALIZED,
    STATE_JOINING,
    STATE_CONNECTED,
    STATE_LEAVING,
    STATE_DISCONNECTED
} member_state_t;

typedef struct gossip_descriptor {
    pt_socket_fd socket;

    uint8_t input_buffer[INPUT_BUFFER_SIZE];
    size_t input_buffer_offset;
    uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
    size_t output_buffer_offset;

    uint32_t sequence_num;

    member_state_t state;
    cluster_member_t self_address;

    cluster_member_map_t members;
    message_queue_t *outbound_messages;
    message_queue_t *inbound_messages;
} gossip_descriptor_t;

int gossip_handle_buffer(gossip_descriptor_t *self, const uint8_t *buffer, size_t buffer_size);
int gossip_recv_message(gossip_descriptor_t *self);

#endif //PITTACUS_GOSSIP_H
