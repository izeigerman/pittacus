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


typedef struct message_envelope {
    cluster_member_t *recipient;
    const uint8_t *buffer;
    size_t buffer_size;

    uint32_t sequence_num;
    uint16_t attempt_num;

    vector_record_t *clock_record;

    struct message_envelope *prev;
    struct message_envelope *next;
} message_envelope_t;

struct message_queue {
    message_envelope_t *head;
    message_envelope_t *tail;
};

static message_envelope_t *gossip_envelope_create(gossip_descriptor_t *self, const uint8_t *buffer,
                                                  size_t buffer_size, cluster_member_t *recipient) {
    message_envelope_t *envelope = (message_envelope_t *) malloc(sizeof(message_envelope_t));
    envelope->sequence_num = self->sequence_num++;
    envelope->next = NULL;
    envelope->prev = NULL;
    envelope->attempt_num = 0;
    envelope->buffer = buffer;
    envelope->buffer_size = buffer_size;
    envelope->recipient = recipient;
    envelope->clock_record = NULL;
    return envelope;
}

static void gossipe_envelope_destroy(message_envelope_t *envelope) {
    if (envelope->clock_record != NULL) free(envelope->clock_record);
    free(envelope);
}

static int gossip_envelope_enqueue(message_queue_t *queue, message_envelope_t *envelope) {
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

static int gossip_envelope_replace(message_queue_t *queue, message_envelope_t *old,
                                   message_envelope_t *new) {
    message_envelope_t *prev = old->prev;
    message_envelope_t *next = old->next;
    if (next != NULL) {
        next->prev = new;
    } else {
        queue->tail = new;
    }
    if (prev != NULL) {
        prev->next = new;
    } else {
        queue->head = new;
    }
    new->prev = prev;
    new->next = next;
    gossipe_envelope_destroy(old);
    return 0;
}


static int gossip_envelope_remove(message_queue_t *queue, message_envelope_t *envelope) {
    message_envelope_t *prev = envelope->prev;
    message_envelope_t *next = envelope->next;
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
    gossipe_envelope_destroy(envelope);
    return 0;
}

static message_envelope_t *gossip_envelope_find_by_sequence_num(message_queue_t *queue, uint32_t sequence_num) {
    message_envelope_t *head = queue->head;
    while (head != NULL) {
        if (head->sequence_num == sequence_num) return head;
        head = head->next;
    }
    return NULL;
}

static message_envelope_t *gossip_envelope_find_by_buffer(message_queue_t *queue, const uint8_t *buffer) {
    message_envelope_t *head = queue->head;
    while (head != NULL) {
        if (head->buffer == buffer) return head;
        head = head->next;
    }
    return NULL;
}

static int gossip_handle_hello(gossip_descriptor_t *self, message_envelope_t *envelope) {
    message_hello_t msg;
    if (message_hello_decode(envelope->buffer, envelope->buffer_size, &msg) < 0) return -1;

    cluster_member_map_put(&self->members, msg.this_member, 1);
    // TODO: enqueue welcome
    // TODO: enqueue member list update
    // TODO: enqueue data messages

    message_hello_destroy(&msg);
    return 0;
}

static int gossip_handle_welcome(gossip_descriptor_t *self, message_envelope_t *envelope) {
    message_welcome_t msg;
    if (message_welcome_decode(envelope->buffer, envelope->buffer_size, &msg) < 0) return -1;
    self->state = STATE_CONNECTED;
    return 0;
}

static int gossip_handle_member_list(gossip_descriptor_t *self, message_envelope_t *envelope) {
    message_member_list_t msg;
    if (message_member_list_decode(envelope->buffer, envelope->buffer_size, &msg) < 0) return -1;

    cluster_member_map_put(&self->members, msg.members, msg.members_n);
    // TODO: send ack back

    message_member_list_destroy(&msg);
    return 0;
}

static int gossip_handle_data(gossip_descriptor_t *self, message_envelope_t *envelope) {
    message_data_t msg;
    if (message_data_decode(envelope->buffer, envelope->buffer_size, &msg) < 0) return -1;

    // Save the original vector clock record of the arrived data message.
    envelope->clock_record = (vector_record_t *) malloc(sizeof(vector_record_t));
    memcpy(envelope->clock_record, &msg.data_version, sizeof(vector_record_t));

    // Invoke the data receiver callback specified by the user.
    self->data_receiver(self->data_receiver_context, msg.data, msg.data_size);

    // TODO: send ack back

    // Update the offset to keep this data message in a queue.
    self->input_buffer_offset += MAX_MESSAGE_SIZE;
    if (self->input_buffer_offset >= INPUT_BUFFER_SIZE) self->input_buffer_offset = 0;
    return 0;
}

static int gossip_handle_ack(gossip_descriptor_t *self, message_envelope_t *envelope) {
    message_ack_t msg;
    if (message_ack_decode(envelope->buffer, envelope->buffer_size, &msg) < 0) return -1;

    // Removing the processed message from the outbound queue.
    message_envelope_t *ack_envelope =
            gossip_envelope_find_by_sequence_num(self->outbound_messages,
                                                 msg.ack_sequence_num);
    if (ack_envelope != NULL) gossip_envelope_remove(self->outbound_messages, ack_envelope);
    return 0;
}

static int gossip_handle_envelope(gossip_descriptor_t *self, message_envelope_t *envelope) {
    int message_type = message_type_decode(envelope->buffer, envelope->buffer_size);
    int result = 0;
    switch(message_type) {
        case MESSAGE_HELLO_TYPE:
            result = gossip_handle_hello(self, envelope);
            break;
        case MESSAGE_WELCOME_TYPE:
            result = gossip_handle_welcome(self, envelope);
            break;
        case MESSAGE_MEMBER_LIST_TYPE:
            result = gossip_handle_member_list(self, envelope);
            break;
        case MESSAGE_DATA_TYPE:
            result = gossip_handle_data(self, envelope);
            break;
        case MESSAGE_ACK_TYPE:
            result = gossip_handle_ack(self, envelope);
            break;
        default:
            return -1;
    }
    return result;
}

int gossip_recv_message(gossip_descriptor_t *self) {
    pt_sockaddr_storage addr;
    pt_socklen_t addr_len;

    uint8_t *current_buffer = self->input_buffer + self->input_buffer_offset;

    // Read a new message.
    int read_result = pt_recv_from(self->socket, current_buffer, MAX_MESSAGE_SIZE, &addr, &addr_len);
    if (read_result <= 0) return read_result;

    message_envelope_t *old_envelope = gossip_envelope_find_by_buffer(self->inbound_messages,
                                                                      current_buffer);
    message_envelope_t *new_envelope = gossip_envelope_create(self, current_buffer,
                                                              MAX_MESSAGE_SIZE, &self->self_address);

    // Replace the old envelope with the new one or just add it to list.
    int insert_result = 0;
    if (old_envelope != NULL) {
        insert_result = gossip_envelope_replace(self->inbound_messages, old_envelope, new_envelope);
    } else {
        insert_result = gossip_envelope_enqueue(self->inbound_messages, new_envelope);
    }
    if (insert_result != 0) {
        gossipe_envelope_destroy(new_envelope);
        return insert_result;
    }


    return gossip_handle_envelope(self, new_envelope);
}
