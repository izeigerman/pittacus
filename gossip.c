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
#include "gossip.h"
#include "messages.h"
#include <stdlib.h>
#include <string.h>

#define RETURN_IF_NOT_CONNECTED(state) if ((state) != STATE_CONNECTED) return -3;

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
    uint16_t attempt_num;

    struct message_envelope_out *prev;
    struct message_envelope_out *next;
} message_envelope_out_t;


static message_envelope_out_t *gossip_envelope_create(
        uint32_t sequence_number,
        const uint8_t *buffer, size_t buffer_size,
        const pt_sockaddr_storage *recipient, pt_socklen_t recipient_len) {
    message_envelope_out_t *envelope = (message_envelope_out_t *) malloc(sizeof(message_envelope_out_t));
    envelope->sequence_num = sequence_number;
    envelope->next = NULL;
    envelope->prev = NULL;
    envelope->attempt_num = 0;
    envelope->buffer = buffer;
    envelope->buffer_size = buffer_size;
    memcpy(&envelope->recipient, recipient, recipient_len);
    envelope->recipient_len = recipient_len;
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
    return 0;
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
    return 0;
}

static message_envelope_out_t *gossip_envelope_find_by_sequence_num(message_queue_t *queue, uint32_t sequence_num) {
    message_envelope_out_t *head = queue->head;
    while (head != NULL) {
        if (head->sequence_num == sequence_num) return head;
        head = head->next;
    }
    return NULL;
}

static const uint8_t *gossip_find_available_output_buffer(gossip_descriptor_t *self) {
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

static uint32_t gossip_update_output_buffer_offset(gossip_descriptor_t *self) {
    uint32_t offset = 0;
    if (self->outbound_messages.head != NULL) {
        offset = gossip_find_available_output_buffer(self) - self->output_buffer;
    }
    self->output_buffer_offset = offset;
    return offset;
}

static int gossip_enqueue_to_outbound(gossip_descriptor_t *self,
                                      const uint8_t *buffer,
                                      size_t buffer_size,
                                      const pt_sockaddr_storage *recipient,
                                      pt_socklen_t recipient_len) {
    const pt_sockaddr_storage *receivers[MESSAGE_RUMOR_FACTOR];
    pt_socklen_t receiver_lengths[MESSAGE_RUMOR_FACTOR];
    int receivers_num = MESSAGE_RUMOR_FACTOR;
    if (recipient != NULL && recipient_len != 0) {
        receivers_num = 1;
        receivers[0] = recipient;
        receiver_lengths[0] = recipient_len;
    } else {
        // Choose N random members.
        // FIXME:
    }

    for (int i = 0; i < receivers_num; ++i) {
        uint32_t seq_num = ++self->sequence_num;
        message_envelope_out_t *new_envelope = gossip_envelope_create(seq_num, buffer, buffer_size,
                                                                      receivers[i], receiver_lengths[i]);
        if (new_envelope == NULL) return -1;
        gossip_envelope_enqueue(&self->outbound_messages, new_envelope);
    }
    return 0;
}

static int gossip_enqueue_message(gossip_descriptor_t *self,
                                  uint8_t msg_type,
                                  const void *msg,
                                  const pt_sockaddr_storage *recipient,
                                  pt_socklen_t recipient_len) {
    uint32_t offset = gossip_update_output_buffer_offset(self);
    uint8_t *buffer = self->output_buffer + offset;
    int encode_result = 0;

    switch(msg_type) {
        case MESSAGE_HELLO_TYPE:
            encode_result = message_hello_encode((const message_hello_t *) msg,
                                                 buffer, MESSAGE_MAX_SIZE);
            break;
        case MESSAGE_WELCOME_TYPE:
            encode_result = message_welcome_encode((const message_welcome_t *) msg,
                                                   buffer, MESSAGE_MAX_SIZE);
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
            break;
        default:
            return -1;
    }

    if (encode_result < 0) return encode_result;
    return gossip_enqueue_to_outbound(self, buffer, encode_result, recipient, recipient_len);
}

static int gossip_enqueue_ack(gossip_descriptor_t *self,
                              uint32_t sequence_num,
                              const pt_sockaddr_storage *recipient,
                              pt_socklen_t recipient_len) {
    message_ack_t ack_msg;
    message_header_init(&ack_msg.header, MESSAGE_ACK_TYPE, 0);
    ack_msg.ack_sequence_num = sequence_num;
    return gossip_enqueue_message(self, MESSAGE_ACK_TYPE, &ack_msg,
                                  recipient, recipient_len);
}

static int gossip_enqueue_welcome(gossip_descriptor_t *self,
                                  uint32_t hello_sequence_num,
                                  const pt_sockaddr_storage *recipient,
                                  pt_socklen_t recipient_len) {
    message_welcome_t welcome_msg;
    message_header_init(&welcome_msg.header, MESSAGE_WELCOME_TYPE, 0);
    welcome_msg.hello_sequence_num = hello_sequence_num;
    welcome_msg.this_member = &self->self_address;
    return gossip_enqueue_message(self, MESSAGE_WELCOME_TYPE, &welcome_msg,
                                  recipient, recipient_len);
}

static int gossip_enqueue_hello(gossip_descriptor_t *self,
                                const pt_sockaddr_storage *recipient,
                                pt_socklen_t recipient_len) {
    message_hello_t hello_msg;
    message_header_init(&hello_msg.header, MESSAGE_HELLO_TYPE, 0);
    hello_msg.this_member = &self->self_address;
    return gossip_enqueue_message(self, MESSAGE_HELLO_TYPE, &hello_msg,
                                  recipient, recipient_len);
}

static int gossip_enqueue_data(gossip_descriptor_t *self,
                               const uint8_t *data,
                               uint32_t data_size,
                               const pt_sockaddr_storage *recipient,
                               pt_socklen_t recipient_len) {
    // Update the local data version.
    uint32_t clock_counter = ++self->data_counter;
    vector_record_t *record = vector_clock_set(&self->data_version, &self->self_address,
                                               clock_counter);

    message_data_t data_msg;
    message_header_init(&data_msg.header, MESSAGE_DATA_TYPE, 0);
    memcpy(&data_msg.data_version, record, sizeof(vector_record_t));
    data_msg.data = (uint8_t *) data;
    data_msg.data_size = data_size;
    return gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &data_msg,
                                  recipient, recipient_len);
}

static int gossip_enqueue_member_list(gossip_descriptor_t *self,
                                      const pt_sockaddr_storage *recipient,
                                      pt_socklen_t recipient_len) {
    message_member_list_t member_list_msg;
    message_header_init(&member_list_msg.header, MESSAGE_MEMBER_LIST_TYPE, 0);

    const cluster_member_map_t *members = &self->members;
    uint32_t members_num = (members->size > MEMBER_LIST_SYNC_SIZE) ? MEMBER_LIST_SYNC_SIZE : members->size;

    // TODO: get rid of the redundant copying.
    cluster_member_t *members_to_send = (cluster_member_t *) malloc(members_num * sizeof(cluster_member_t));
    int to_send_idx = 0;
    int member_idx = 0;
    while (to_send_idx < members_num) {
        if (members->map[member_idx] != NULL) {
            memcpy(&members_to_send[to_send_idx], members->map[member_idx], sizeof(cluster_member_t));
            ++to_send_idx;
        }
        ++member_idx;
    }

    member_list_msg.members_n = members_num;
    member_list_msg.members = members_to_send;
    int result = gossip_enqueue_message(self, MESSAGE_MEMBER_LIST_TYPE, &member_list_msg,
                                        recipient, recipient_len);
    free(members_to_send);
    return result;
}

static int gossip_handle_hello(gossip_descriptor_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_hello_t msg;
    if (message_hello_decode(envelope_in->buffer, envelope_in->buffer_size, &msg) < 0) return -1;

    // Update our local storage with a new member.
    cluster_member_map_put(&self->members, msg.this_member, 1);

    // Send back a Welcome message.
    gossip_enqueue_welcome(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    // Send some portion of known members to a newcomer node.
    gossip_enqueue_member_list(self, envelope_in->sender, envelope_in->sender_len);

    // FIXME: send the existing data messages

    message_hello_destroy(&msg);
    return 0;
}

static int gossip_handle_welcome(gossip_descriptor_t *self, const message_envelope_in_t *envelope_in) {
    message_welcome_t msg;
    if (message_welcome_decode(envelope_in->buffer, envelope_in->buffer_size, &msg) < 0) return -1;
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
    return 0;
}

static int gossip_handle_member_list(gossip_descriptor_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_member_list_t msg;
    if (message_member_list_decode(envelope_in->buffer, envelope_in->buffer_size, &msg) < 0) return -1;

    // Update our local collection of members with arrived records.
    cluster_member_map_put(&self->members, msg.members, msg.members_n);

    // Send ACK message back to sender.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    message_member_list_destroy(&msg);
    return 0;
}

static int gossip_handle_data(gossip_descriptor_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_data_t msg;
    if (message_data_decode(envelope_in->buffer, envelope_in->buffer_size, &msg) < 0) return -1;

    // Send ACK message back to qsender.
    gossip_enqueue_ack(self, msg.header.sequence_num, envelope_in->sender, envelope_in->sender_len);

    // Verify whether we saw the arrived message before.
    vector_clock_comp_res_t res = vector_clock_compare_with_record(&self->data_version,
                                                                   &msg.data_version, PT_TRUE);

    if (res == VC_BEFORE) {
        // Invoke the data receiver callback specified by the user.
        self->data_receiver(self->data_receiver_context, msg.data, msg.data_size);
        // Enqueue the same message to send it to N random members later.
        return gossip_enqueue_message(self, MESSAGE_DATA_TYPE, &msg, NULL, 0);
    }
    return 0;
}

static int gossip_handle_ack(gossip_descriptor_t *self, const message_envelope_in_t *envelope_in) {
    RETURN_IF_NOT_CONNECTED(self->state);
    message_ack_t msg;
    if (message_ack_decode(envelope_in->buffer, envelope_in->buffer_size, &msg) < 0) return -1;

    // Removing the processed message from the outbound queue.
    message_envelope_out_t *ack_envelope =
            gossip_envelope_find_by_sequence_num(&self->outbound_messages,
                                                 msg.ack_sequence_num);
    if (ack_envelope != NULL) gossip_envelope_remove(&self->outbound_messages, ack_envelope);
    return 0;
}

static int gossip_handle_new_message(gossip_descriptor_t *self, const message_envelope_in_t *envelope_in) {
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
            return -1;
    }
    return result;
}

int gossip_init(gossip_descriptor_t *self,
                const pt_sockaddr *self_addr, socklen_t self_addr_len,
                data_receiver_t data_receiver, void *data_receiver_context) {
    self->socket = pt_socket_datagram((const pt_sockaddr_storage *) self_addr, self_addr_len);
    if (self->socket < 0) {
        return -1;
    }

    self->output_buffer_offset = 0;

    self->outbound_messages = (message_queue_t ) { .head = NULL, .tail = NULL };

    self->sequence_num = 0;
    self->data_counter = 0;
    vector_clock_init(&self->data_version);

    self->state = STATE_INITIALIZED;
    cluster_member_init(&self->self_address, (const pt_sockaddr_storage *) self_addr, self_addr_len);
    cluster_member_map_init(&self->members);

    self->data_receiver = data_receiver;
    self->data_receiver_context = data_receiver_context;
    return 0;
}

int gossip_destroy(gossip_descriptor_t *self) {
    pt_close(self->socket);

    gossip_envelope_clear(&self->outbound_messages);

    self->state = STATE_DESTROYED;
    cluster_member_destroy(&self->self_address);
    cluster_member_map_destroy(&self->members);
    return 0;
}

int gossip_join(gossip_descriptor_t *self, const seed_node_t *seed_nodes, uint16_t seed_nodes_len) {
    if (self->state != STATE_INITIALIZED) return -1;
    if (seed_nodes == NULL || seed_nodes_len == 0) {
        // No seed nodes were provided.
        self->state = STATE_CONNECTED;
    } else {
        for (int i = 0; i < seed_nodes_len; ++i) {
            seed_node_t node = seed_nodes[i];
            int res = gossip_enqueue_hello(self, (const pt_sockaddr_storage *) node.addr, node.addr_len);
            if (res < 0) return res;
        }
        self->state = STATE_JOINING;
    }
    return 0;
}

int gossip_process_receive(gossip_descriptor_t *self) {
    if (self->state != STATE_JOINING && self->state != STATE_CONNECTED) return -3;

    pt_sockaddr_storage addr;
    pt_socklen_t addr_len;
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

int gossip_process_send(gossip_descriptor_t *self) {
    if (self->state != STATE_JOINING && self->state != STATE_CONNECTED) return -3;
    message_envelope_out_t *head = self->outbound_messages.head;
    int msg_sent = 0;
    while (head != NULL) {
        message_envelope_out_t *current = head;
        head = head->next;

        // Update the sequence number in the buffer in order to correspond
        // the sequence number stored in envelope. This approach violates
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

        if (++current->attempt_num >= MESSAGE_SEND_ATTEMPTS) {
            // The message exceeded the maximum number of attempts. Removing it
            // from the queue.
            gossip_envelope_remove(&self->outbound_messages, current);
        }
        ++msg_sent;
    }
    return msg_sent;
}

int gossip_send_data(gossip_descriptor_t *self, const uint8_t *data, uint32_t data_size) {
    RETURN_IF_NOT_CONNECTED(self->state);
    return gossip_enqueue_data(self, data, data_size, NULL, 0);
}
