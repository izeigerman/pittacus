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
#include "gossip.h"
#include "messages.h"
#include "member.h"
#include "vector_clock.h"
#include "config.h"
#include "errors.h"
#include <stdlib.h>
#include <string.h>

#define RETURN_IF_NOT_CONNECTED(state) if ((state) != STATE_CONNECTED) return PITTACUS_ERR_BAD_STATE;

typedef struct message_envelope_in {
    const pt_sockaddr_storage *sender;
    pt_socklen_t sender_len;

    const uint8_t *buffer;
    size_t buffer_size;
} message_envelope_in_t;

typedef struct message_envelope_out {
    pt_sockaddr_storage recipient;
    pt_socklen_t recipient_len;

    const uint8_t *buffer;
    size_t buffer_size;

    uint32_t sequence_num;
    uint64_t attempt_ts;
    uint16_t attempt_num;
    uint16_t max_attempts;

    struct message_envelope_out *prev;
    struct message_envelope_out *next;
} message_envelope_out_t;

typedef struct message_queue {
    message_envelope_out_t *head;
    message_envelope_out_t *tail;
} message_queue_t;

#define INPUT_BUFFER_SIZE MESSAGE_MAX_SIZE
#define OUTPUT_BUFFER_SIZE MAX_OUTPUT_MESSAGES * MESSAGE_MAX_SIZE
struct pittacus_gossip {
    pt_socket_fd socket;

    uint8_t input_buffer[INPUT_BUFFER_SIZE];
    uint8_t output_buffer[OUTPUT_BUFFER_SIZE];
    size_t output_buffer_offset;

    message_queue_t outbound_messages;

    uint32_t sequence_num;
    uint32_t data_counter;
    vector_clock_t data_version;

    pittacus_gossip_state_t state;
    cluster_member_t self_address;
    cluster_member_map_t members;

    data_receiver_t data_receiver;
    void *data_receiver_context;
};

static message_envelope_out_t *gossip_envelope_create(
        uint32_t sequence_number,
        const uint8_t *buffer, size_t buffer_size,
        uint16_t max_attempts,
        const pt_sockaddr_storage *recipient, pt_socklen_t recipient_len) {
    message_envelope_out_t *envelope = (message_envelope_out_t *) malloc(sizeof(message_envelope_out_t));
    if (envelope == NULL) return NULL;
    envelope->sequence_num = sequence_number;
    envelope->next = NULL;
    envelope->prev = NULL;
    envelope->attempt_num = 0;
    envelope->attempt_ts = 0;
    envelope->buffer = buffer;
    envelope->buffer_size = buffer_size;
    memcpy(&envelope->recipient, recipient, recipient_len);
    envelope->recipient_len = recipient_len;
    envelope->max_attempts = max_attempts;
    return envelope;
}

static void gossip_envelope_destroy(message_envelope_out_t *envelope) {
    free(envelope);
}

static void gossip_envelope_clear(message_queue_t *queue) {
    message_envelope_out_t *head = queue->head;
    while (head != NULL) {
        message_envelope_out_t *current = head;
        head = head->next;
        gossip_envelope_destroy(current);
    }
    queue->head = NULL;
    queue->tail = NULL;
}

static int gossip_envelope_enqueue(message_queue_t *queue, message_envelope_out_t *envelope) {
    envelope->next = NULL;
    if (queue->head == NULL || queue->tail == NULL) {
        queue->head = envelope;
        queue->tail = envelope;
    } else {
        envelope->prev = queue->tail;
        queue->tail->next = envelope;
        queue->tail = envelope;
    }
    return PITTACUS_ERR_NONE;
}

static int gossip_envelope_remove(message_queue_t *queue, message_envelope_out_t *envelope) {
    message_envelope_out_t *prev = envelope->prev;
    message_envelope_out_t *next = envelope->next;
    if (next != NULL) {
        next->prev = prev;
    } else {
        queue->tail = prev;
    }
    if (prev != NULL) {
        prev->next = next;
    } else {
        queue->head = next;
    }
    gossip_envelope_destroy(envelope);
    return PITTACUS_ERR_NONE;
}

static message_envelope_out_t *gossip_envelope_find_by_sequence_num(message_queue_t *queue, uint32_t sequence_num) {
    message_envelope_out_t *head = queue->head;
    while (head != NULL) {
        if (head->sequence_num == sequence_num) return head;
        head = head->next;
    }
    return NULL;
}

static const uint8_t *gossip_find_available_output_buffer(pittacus_gossip_t *self) {
    pt_bool_t buffer_is_occupied[MAX_OUTPUT_MESSAGES];
    memset(buffer_is_occupied, 0, MAX_OUTPUT_MESSAGES * sizeof(pt_bool_t));

    message_envelope_out_t *oldest_envelope = NULL;
    message_envelope_out_t *head = self->outbound_messages.head;
    while(head != NULL) {
        if (oldest_envelope == NULL || head->attempt_num > oldest_envelope->attempt_num) {
            oldest_envelope = head;
        }
        const uint8_t *buffer = head->buffer;
        uint32_t index = (buffer - self->output_buffer) / MESSAGE_MAX_SIZE;
        buffer_is_occupied[index] = PT_TRUE;

        head = head->next;
    }

    // Looking for a buffer that is not used by any message in the outbound queue.
    for (int i = 0; i < MAX_OUTPUT_MESSAGES; ++i) {
        if (!buffer_is_occupied[i]) return self->output_buffer + (i * MESSAGE_MAX_SIZE);
    }

    // No available buffers were found. Removing the oldest message in a queue
    // to overwrite its buffer.
    const uint8_t *chosen_buffer = oldest_envelope->buffer;
    while (oldest_envelope != NULL && oldest_envelope->buffer == chosen_buffer) {
        // Remove all messages that share the same buffer's region.
        message_envelope_out_t *to_remove = oldest_envelope;
        oldest_envelope = oldest_envelope->next;
        gossip_envelope_remove(&self->outbound_messages, to_remove);
    }
    return chosen_buffer;
}

static uint32_t gossip_update_output_buffer_offset(pittacus_gossip_t *self) {
    uint32_t offset = 0;
    if (self->outbound_messages.head != NULL) {
        offset = gossip_find_available_output_buffer(self) - self->output_buffer;
    }
    self->output_buffer_offset = offset;
    return offset;
}

static int gossip_enqueue_to_outbound(pittacus_gossip_t *self,
                                      const uint8_t *buffer,
                                      size_t buffer_size,
                                      uint16_t max_attempts,
                                      const pt_sockaddr_storage *receiver,
                                      pt_socklen_t receiver_size) {
    uint32_t seq_num = ++self->sequence_num;
    message_envelope_out_t *new_envelope = gossip_envelope_create(seq_num,
                                                                  buffer, buffer_size,
                                                                  max_attempts,
                                                                  receiver, receiver_size);
    if (new_envelope == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;
    gossip_envelope_enqueue(&self->outbound_messages, new_envelope);
    return PITTACUS_ERR_NONE;
}

typedef enum gossip_spreading_type {
    GOSSIP_DIRECT = 0,
    GOSSIP_RANDOM = 1,
    GOSSIP_BROADCAST = 2
} gossip_spreading_type_t;

static int gossip_enqueue_message(pittacus_gossip_t *self,
                                  uint8_t msg_type,
                                  const void *msg,
                                  const pt_sockaddr_storage *recipient,
                                  pt_socklen_t recipient_len,
                                  gossip_spreading_type_t spreading_type) {
    uint32_t offset = gossip_update_output_buffer_offset(self);
    uint8_t *buffer = self->output_buffer + offset;
    int encode_result = 0;
    uint16_t max_attempts = MESSAGE_RETRY_ATTEMPTS;

    // Serialize the message.
    switch(msg_type) {
        case MESSAGE_HELLO_TYPE:
            encode_result = message_hello_encode((const message_hello_t *) msg,
                                                 buffer, MESSAGE_MAX_SIZE);
            break;
        case MESSAGE_WELCOME_TYPE:
            encode_result = message_welcome_encode((const message_welcome_t *) msg,
                                                   buffer, MESSAGE_MAX_SIZE);
            // Welcome message can't be acknowledged. It should be removed from the
            // outbound queue after the first attempt.
            max_attempts = 1;
            break;
        case MESSAGE_MEMBER_LIST_TYPE:
            encode_result = message_member_list_encode((const message_member_list_t *) msg,
                                                       buffer, MESSAGE_MAX_SIZE);
            break;
        case MESSAGE_DATA_TYPE:
            encode_result = message_data_encode((const message_data_t *) msg,
                                                buffer, MESSAGE_MAX_SIZE);
            break;
        case MESSAGE_ACK_TYPE:
            encode_result = message_ack_encode((const message_ack_t *) msg,
                                               buffer, MESSAGE_MAX_SIZE);
            // ACK message can't be acknowledged. It should be removed from the
            // outbound queue after the first attempt.
            max_attempts = 1;
            break;
        default:
            return PITTACUS_ERR_INVALID_MESSAGE;
    }
    if (encode_result < 0) return encode_result;

    int result = PITTACUS_ERR_NONE;

    // Distribute the message.
    switch (spreading_type) {
        case GOSSIP_DIRECT:
            // Send message to a single recipient.
            return gossip_enqueue_to_outbound(self, buffer, encode_result, max_attempts,
                                              recipient, recipient_len);
        case GOSSIP_RANDOM: {
            // Choose some number of random members to distribute the message.
            cluster_member_t *reservoir[MESSAGE_RUMOR_FACTOR];
            int receivers_num = cluster_member_map_random_member(&self->members,
                                                                 reservoir, MESSAGE_RUMOR_FACTOR);
            for (int i = 0; i < receivers_num; ++i) {
                // Create a new envelope for each recipient.
                // Note: all created envelopes share the same buffer.
                result = gossip_enqueue_to_outbound(self, buffer, encode_result, max_attempts,
                                                    reservoir[i]->address, reservoir[i]->address_len);
                if (result < 0) return result;
            }
            break;
        }
        case GOSSIP_BROADCAST: {
            // Distribute the message to all known members.
            for (int i = 0; i < self->members.capacity; ++i) {
                // Create a new envelope for each recipient.
                // Note: all created envelopes share the same buffer.
                if (self->members.map[i] != NULL) {
                    cluster_member_t *member = self->members.map[i];
                    result = gossip_enqueue_to_outbound(self, buffer, encode_result, max_attempts,
                                                        member->address, member->address_len);
                    if (result < 0) return result;
                }
            }
            break;
        }
    }
    return result;
}

static int gossip_enqueue_ack(pittacus_gossip_t *self,
                              uint32_t sequence_num,
                              const pt_sockaddr_storage *recipient,
                              pt_socklen_t recipient_len) {
    message_ack_t ack_msg;
    message_header_init(&ack_msg.header, MESSAGE_ACK_TYPE, 0);
    ack_msg.ack_sequence_num = sequence_num;
    return gossip_enqueue_message(self, MESSAGE_ACK_TYPE, &ack_msg,
                                  recipient, recipient_len, GOSSIP_DIRECT);
}

static int gossip_enqueue_welcome(pittacus_gossip_t *self,
                                  uint32_t hello_sequence_num,
                                  const pt_sockaddr_storage *recipient,
                                  pt_socklen_t recipient_len) {
    message_welcome_t welcome_msg;
    message_header_init(&welcome_msg.header, MESSAGE_WELCOME_TYPE, 0);
    welcome_msg.hello_sequence_num = hello_sequence_num;
    welcome_msg.this_member = &self->self_address;
    return gossip_enqueue_message(self, MESSAGE_WELCOME_TYPE, &welcome_msg,
                                  recipient, recipient_len, GOSSIP_DIRECT);
}

static int gossip_enqueue_hello(pittacus_gossip_t *self,
                                const pt_sockaddr_storage *recipient,
                                pt_socklen_t recipient_len) {
    message_hello_t hello_msg;
    message_header_init(&hello_msg.header, MESSAGE_HELLO_TYPE, 0);
    hello_msg.this_member = &self->self_address;
    return gossip_enqueue_message(self, MESSAGE_HELLO_TYPE, &hello_msg,
                                  recipient, recipient_len, GOSSIP_DIRECT);
}

static int gossip_enqueue_data(pittacus_gossip_t *self,
                               const uint8_t *data,
                               uint16_t data_size) {
    // Update the local data version.
    uint32_t clock_counter = ++self->data_counter;
    vector_record_t *record = vector_clock_set(&self->data_version, &self->self_address,
                                               clock_counter);

    message_data_t data_msg;
    message_header_init(&data_msg.header, MESSAGE_DATA_TYPE, 0);
    vector_clock_record_copy(&data_msg.data_version, record);
    data_msg.data = (uint8_t *) data;
    data_msg.data_size = data_size;
    return gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &data_msg,
                                  NULL, 0, GOSSIP_RANDOM);
}

static int gossip_enqueue_member_list(pittacus_gossip_t *self,
                                      const pt_sockaddr_storage *recipient,
                                      pt_socklen_t recipient_len) {
    message_member_list_t member_list_msg;
    message_header_init(&member_list_msg.header, MESSAGE_MEMBER_LIST_TYPE, 0);

    const cluster_member_map_t *members = &self->members;
    uint32_t members_num = (members->size > MEMBER_LIST_SYNC_SIZE) ? MEMBER_LIST_SYNC_SIZE : members->size;

    // TODO: get rid of the redundant copying.
    cluster_member_t *members_to_send = (cluster_member_t *) malloc(members_num * sizeof(cluster_member_t));
    if (members_to_send == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;

    int result = PITTACUS_ERR_NONE;
    int to_send_idx = 0;
    int member_idx = 0;
    while (member_idx < members->capacity) {
        // Send the list of all known members to a recipient.
        // The list can be pretty big, so we split it into multiple messages.
        while (to_send_idx < members_num && member_idx < members->capacity) {
            if (members->map[member_idx] != NULL) {
                memcpy(&members_to_send[to_send_idx], members->map[member_idx], sizeof(cluster_member_t));
                ++to_send_idx;
            }
            ++member_idx;
        }

        member_list_msg.members_n = to_send_idx;
        member_list_msg.members = members_to_send;
        result = gossip_enqueue_message(self, MESSAGE_MEMBER_LIST_TYPE, &member_list_msg,
                                        recipient, recipient_len, GOSSIP_DIRECT);
        if (result < 0) {
            free(members_to_send);
            return result;
        }
        to_send_idx = 0;
    }
    free(members_to_send);
    return result;
}

static int gossip_handle_hello(pittacus_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_hello_t msg;
    int decode_result = message_hello_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }

    // Send back a Welcome message.
    gossip_enqueue_welcome(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    // Send the list of known members to a newcomer node.
    if (self->members.size > 0) {
        gossip_enqueue_member_list(self, envelope_in->sender, envelope_in->sender_len);
    }

    // Notify other nodes about a newcomer.
    message_member_list_t member_list_msg;
    message_header_init(&member_list_msg.header, MESSAGE_MEMBER_LIST_TYPE, 0);
    member_list_msg.members = msg.this_member;
    member_list_msg.members_n = 1;
    gossip_enqueue_message(self, MESSAGE_MEMBER_LIST_TYPE, &member_list_msg, NULL, 0, GOSSIP_BROADCAST);

    // Update our local storage with a new member.
    cluster_member_map_put(&self->members, msg.this_member, 1);

    // FIXME: send the existing data messages

    message_hello_destroy(&msg);
    return PITTACUS_ERR_NONE;
}

static int gossip_handle_welcome(pittacus_gossip_t *self, const message_envelope_in_t *envelope_in) {
    message_welcome_t msg;
    int decode_result = message_welcome_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }
    self->state = STATE_CONNECTED;

    // Now when the seed node responded we can
    // safely add it to the list of known members.
    cluster_member_map_put(&self->members, msg.this_member, 1);

    // Remove the hello message from the outbound queue.
    message_envelope_out_t *hello_envelope =
            gossip_envelope_find_by_sequence_num(&self->outbound_messages,
                                                 msg.hello_sequence_num);
    if (hello_envelope != NULL) gossip_envelope_remove(&self->outbound_messages, hello_envelope);

    message_welcome_destroy(&msg);
    return PITTACUS_ERR_NONE;
}

static int gossip_handle_member_list(pittacus_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_member_list_t msg;
    int decode_result = message_member_list_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    };

    // Update our local collection of members with arrived records.
    cluster_member_map_put(&self->members, msg.members, msg.members_n);

    // Send ACK message back to sender.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    message_member_list_destroy(&msg);
    return PITTACUS_ERR_NONE;
}

static int gossip_handle_data(pittacus_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_data_t msg;
    int decode_result = message_data_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }

    // Send ACK message back to sender.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    // Verify whether we saw the arrived message before.
    vector_clock_comp_res_t res = vector_clock_compare_with_record(&self->data_version,
                                                                   &msg.data_version, PT_TRUE);

    if (res == VC_BEFORE) {
        // Invoke the data receiver callback specified by the user.
        self->data_receiver(self->data_receiver_context, self, msg.data, msg.data_size);
        // Enqueue the same message to send it to N random members later.
        return gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &msg, NULL, 0, GOSSIP_RANDOM);
    }
    return PITTACUS_ERR_NONE;
}

static int gossip_handle_ack(pittacus_gossip_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_ack_t msg;
    int decode_result = message_ack_decode(envelope_in->buffer, envelope_in->buffer_size, &msg);
    if (decode_result < 0) {
        return decode_result;
    }

    // Removing the processed message from the outbound queue.
    message_envelope_out_t *ack_envelope =
            gossip_envelope_find_by_sequence_num(&self->outbound_messages,
                                                 msg.ack_sequence_num);
    if (ack_envelope != NULL) gossip_envelope_remove(&self->outbound_messages, ack_envelope);
    return PITTACUS_ERR_NONE;
}

static int gossip_handle_new_message(pittacus_gossip_t *self, const message_envelope_in_t *envelope_in) {
    int message_type = message_type_decode(envelope_in->buffer, envelope_in->buffer_size);
    int result = 0;
    switch(message_type) {
        case MESSAGE_HELLO_TYPE:
            result = gossip_handle_hello(self, envelope_in);
            break;
        case MESSAGE_WELCOME_TYPE:
            result = gossip_handle_welcome(self, envelope_in);
            break;
        case MESSAGE_MEMBER_LIST_TYPE:
            result = gossip_handle_member_list(self, envelope_in);
            break;
        case MESSAGE_DATA_TYPE:
            result = gossip_handle_data(self, envelope_in);
            break;
        case MESSAGE_ACK_TYPE:
            result = gossip_handle_ack(self, envelope_in);
            break;
        default:
            return PITTACUS_ERR_INVALID_MESSAGE;
    }
    return result;
}

static int pittacus_gossip_init(pittacus_gossip_t *self,
                                const pittacus_addr_t *self_addr,
                                data_receiver_t data_receiver, void *data_receiver_context) {
    self->socket = pt_socket_datagram((const pt_sockaddr_storage *) self_addr->addr, self_addr->addr_len);
    if (self->socket < 0) {
        return PITTACUS_ERR_INIT_FAILED;
    }

    pt_sockaddr_storage updated_self_addr;
    pt_socklen_t updated_self_addr_size = sizeof(pt_sockaddr_storage);
    if (pt_get_sock_name(self->socket, &updated_self_addr, &updated_self_addr_size) < 0) {
        pt_close(self->socket);
        return PITTACUS_ERR_INIT_FAILED;
    }

    self->output_buffer_offset = 0;

    self->outbound_messages = (message_queue_t ) { .head = NULL, .tail = NULL };

    self->sequence_num = 0;
    self->data_counter = 0;
    vector_clock_init(&self->data_version);

    self->state = STATE_INITIALIZED;
    cluster_member_init(&self->self_address, &updated_self_addr, updated_self_addr_size);
    cluster_member_map_init(&self->members);

    self->data_receiver = data_receiver;
    self->data_receiver_context = data_receiver_context;
    return PITTACUS_ERR_NONE;
}

pittacus_gossip_t *pittacus_gossip_create(const pittacus_addr_t *self_addr,
                                          data_receiver_t data_receiver, void *data_receiver_context) {
    pittacus_gossip_t *result = (pittacus_gossip_t *) malloc(sizeof(pittacus_gossip_t));
    if (result == NULL) return NULL;

    int int_res = pittacus_gossip_init(result, self_addr, data_receiver, data_receiver_context);
    if (int_res < 0) {
        free(result);
        return NULL;
    }
    return result;
}

int pittacus_gossip_destroy(pittacus_gossip_t *self) {
    pt_close(self->socket);

    gossip_envelope_clear(&self->outbound_messages);

    self->state = STATE_DESTROYED;
    cluster_member_destroy(&self->self_address);
    cluster_member_map_destroy(&self->members);

    free(self);
    return PITTACUS_ERR_NONE;
}

int pittacus_gossip_join(pittacus_gossip_t *self, const pittacus_addr_t *seed_nodes, uint16_t seed_nodes_len) {
    if (self->state != STATE_INITIALIZED) return PITTACUS_ERR_BAD_STATE;
    if (seed_nodes == NULL || seed_nodes_len == 0) {
        // No seed nodes were provided.
        self->state = STATE_CONNECTED;
    } else {
        for (int i = 0; i < seed_nodes_len; ++i) {
            pittacus_addr_t node = seed_nodes[i];
            int res = gossip_enqueue_hello(self, (const pt_sockaddr_storage *) node.addr, node.addr_len);
            if (res < 0) return res;
        }
        self->state = STATE_JOINING;
    }
    return PITTACUS_ERR_NONE;
}

int pittacus_gossip_process_receive(pittacus_gossip_t *self) {
    if (self->state != STATE_JOINING && self->state != STATE_CONNECTED) return PITTACUS_ERR_BAD_STATE;

    pt_sockaddr_storage addr;
    pt_socklen_t addr_len = sizeof(pt_sockaddr_storage);
    // Read a new message.
    int read_result = pt_recv_from(self->socket, self->input_buffer, INPUT_BUFFER_SIZE, &addr, &addr_len);
    if (read_result <= 0) return read_result;

    message_envelope_in_t envelope;
    envelope.buffer = self->input_buffer;
    envelope.buffer_size = read_result;
    envelope.sender = &addr;
    envelope.sender_len = addr_len;

    return gossip_handle_new_message(self, &envelope);
}

int pittacus_gossip_process_send(pittacus_gossip_t *self) {
    if (self->state != STATE_JOINING && self->state != STATE_CONNECTED) return PITTACUS_ERR_BAD_STATE;
    message_envelope_out_t *head = self->outbound_messages.head;
    int msg_sent = 0;
    while (head != NULL) {
        message_envelope_out_t *current = head;
        head = head->next;
        uint64_t current_ts = pt_time();

        if (current->attempt_num != 0 && current->attempt_ts + MESSAGE_RETRY_INTERVAL > current_ts) {
            // It's not yet time to retry this message.
            continue;
        }

        // Update the sequence number in the buffer in order to correspond to
        // a sequence number stored in envelope. This approach violates
        // the protocol interface but prevents copying of the whole
        // buffer each time when a single message has multiple recipients.
        uint32_t seq_num_n = PT_HTONL(current->sequence_num);
        uint32_t offset = sizeof(message_header_t) - sizeof(uint32_t);
        uint8_t *seq_num_buf = (uint8_t *) current->buffer + offset;
        memcpy(seq_num_buf, &seq_num_n, sizeof(uint32_t));

        int write_result = pt_send_to(self->socket, current->buffer, current->buffer_size,
                                      &current->recipient, current->recipient_len);

        if (write_result < 0) {
            // FIXME: better error handling?
            return write_result;
        }

        current->attempt_ts = current_ts;
        if (++current->attempt_num >= current->max_attempts) {
            // If the number of maximum attempts is more than 1, than
            // the message required acknowledgement but we didn't receive it.
            // Remove node from the list since it's unreachable.
            if (current->max_attempts > 1) {
                cluster_member_t *unreachable = cluster_member_map_find_by_addr(&self->members,
                                                                                &current->recipient,
                                                                                current->recipient_len);
                if (unreachable != NULL) {
                    cluster_member_map_remove(&self->members, unreachable);
                }
            }
            // The message exceeded the maximum number of attempts.
            // Remove this message from the queue.
            gossip_envelope_remove(&self->outbound_messages, current);
        }
        ++msg_sent;
    }
    return msg_sent;
}

int pittacus_gossip_send_data(pittacus_gossip_t *self, const uint8_t *data, uint32_t data_size) {
    RETURN_IF_NOT_CONNECTED(self->state);
    return gossip_enqueue_data(self, data, data_size);
}

pittacus_gossip_state_t pittacus_gossip_state(pittacus_gossip_t *self) {
    return self->state;
}

pt_socket_fd pittacus_gossip_socket_fd(pittacus_gossip_t *self) {
    return self->socket;
}
