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
#include "messages.h"
#include "member.h"
#include "network.h"
#include "errors.h"
#include <assert.h>
#include <string.h>
#include <stdint.h>

int create_member(uint16_t port, cluster_member_t *result) {
    pt_sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = PT_HTONS(port);
    inet_aton("127.0.0.1", &in.sin_addr);

    return cluster_member_init(result, (const pt_sockaddr_storage *)&in, sizeof(pt_sockaddr_in));
}

void validate_headers(const message_header_t *expected, const message_header_t *actual) {
    assert(strcmp(expected->protocol_id, actual->protocol_id) == 0);
    assert(expected->message_type == actual->message_type);
    assert(expected->sequence_num == actual->sequence_num);
}

void test_message_header() {
    message_header_t header;
    message_header_init(&header, MESSAGE_ACK_TYPE, 1);
    assert(header.message_type == MESSAGE_ACK_TYPE);
    assert(header.sequence_num == 1);
    assert(strcmp(header.protocol_id, "ptcs") == 0);
}

void test_message_hello_enc_dec() {
    message_hello_t msg;
    message_header_init(&msg.header, MESSAGE_HELLO_TYPE, 1);

    cluster_member_t member;
    assert(create_member(12345, &member) == 0);
    msg.this_member = &member;

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_hello_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    message_hello_t out_msg;
    int decode_result = message_hello_decode(buf, encode_result, &out_msg);
    assert(decode_result > 0);
    assert(decode_result == encode_result);

    validate_headers(&msg.header, &out_msg.header);
    assert(cluster_member_equals(msg.this_member, out_msg.this_member));
    message_hello_destroy(&out_msg);

    assert(message_hello_encode(&msg, buf, 1) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
    assert(message_hello_decode(buf, 12, &out_msg) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);

    cluster_member_destroy(&member);
}

void test_message_welcome_enc_dec() {
    message_welcome_t msg;
    message_header_init(&msg.header, MESSAGE_WELCOME_TYPE, 1);

    msg.hello_sequence_num = 2;

    cluster_member_t member;
    assert(create_member(12345, &member) == 0);
    msg.this_member = &member;

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_welcome_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    message_welcome_t out_msg;
    int decode_result = message_welcome_decode(buf, encode_result, &out_msg);
    assert(decode_result > 0);
    assert(decode_result == encode_result);

    validate_headers(&msg.header, &out_msg.header);
    assert(cluster_member_equals(msg.this_member, out_msg.this_member));
    assert(msg.hello_sequence_num == out_msg.hello_sequence_num);
    message_welcome_destroy(&out_msg);

    assert(message_welcome_encode(&msg, buf, 1) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
    assert(message_welcome_decode(buf, 12, &out_msg) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);


    cluster_member_destroy(&member);
}

void test_message_member_list_enc_dec() {
    message_member_list_t msg;
    message_header_init(&msg.header, MESSAGE_MEMBER_LIST_TYPE, 1);

    cluster_member_t member1;
    assert(create_member(12345, &member1) == 0);
    cluster_member_t member2;
    assert(create_member(12346, &member2) == 0);
    cluster_member_t members[] = { member1, member2 };

    msg.members_n = 2;
    msg.members = members;

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_member_list_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    message_member_list_t out_msg;
    int decode_result = message_member_list_decode(buf, encode_result, &out_msg);
    assert(decode_result > 0);
    assert(decode_result == encode_result);

    validate_headers(&msg.header, &out_msg.header);
    assert(msg.members_n == out_msg.members_n);
    for (int i = 0; i < msg.members_n; ++i) {
        assert(cluster_member_equals(msg.members + i, out_msg.members + i));
    }
    message_member_list_destroy(&out_msg);

    assert(message_member_list_encode(&msg, buf, 1) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
    assert(message_member_list_decode(buf, 12, &out_msg) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);

    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
}

void test_message_ack_enc_dec() {
    message_ack_t msg;
    message_header_init(&msg.header, MESSAGE_ACK_TYPE, 1);
    msg.ack_sequence_num = 2;

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_ack_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    message_ack_t out_msg;
    int decode_result = message_ack_decode(buf, encode_result, &out_msg);
    assert(decode_result > 0);
    assert(decode_result == encode_result);

    validate_headers(&msg.header, &out_msg.header);
    assert(msg.ack_sequence_num == out_msg.ack_sequence_num);

    assert(message_ack_encode(&msg, buf, 1) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
    assert(message_ack_decode(buf, 12, &out_msg) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
}

void test_message_data_enc_dec() {
    message_data_t msg;
    message_header_init(&msg.header, MESSAGE_DATA_TYPE, 1);

    char *data = "data";
    uint16_t data_size = strlen(data) + 1;

    msg.data_size = data_size;
    msg.data = (uint8_t *) data;

    cluster_member_t member;
    assert(create_member(12345, &member) == 0);

    vector_clock_t clock;
    assert(vector_clock_init(&clock) == 0);
    vector_record_t *data_version = vector_clock_set(&clock, &member, 2);
    assert(data_version != NULL);
    assert(vector_clock_record_copy(&msg.data_version, data_version) == 0);

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_data_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    message_data_t out_msg;
    int decode_result = message_data_decode(buf, encode_result, &out_msg);
    assert(decode_result > 0);
    assert(decode_result == encode_result);

    validate_headers(&msg.header, &out_msg.header);
    assert(msg.data_size == out_msg.data_size);
    assert(strcmp((char *) msg.data, (char *) out_msg.data) == 0);
    assert(out_msg.data_version.member_id == data_version->member_id);
    assert(out_msg.data_version.sequence_number == data_version->sequence_number);

    assert(message_data_encode(&msg, buf, 1) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
    assert(message_data_decode(buf, 12, &out_msg) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);

    cluster_member_destroy(&member);
}

void test_message_status_enc_dec() {
    message_status_t msg;
    message_header_init(&msg.header, MESSAGE_STATUS_TYPE, 1);

    cluster_member_t member1;
    assert(create_member(12345, &member1) == 0);
    cluster_member_t member2;
    assert(create_member(12346, &member2) == 0);

    vector_clock_t clock;
    assert(vector_clock_init(&clock) == 0);
    vector_record_t *clock_record1 = vector_clock_set(&clock, &member1, 2);
    assert(clock_record1 != NULL);
    vector_record_t *clock_record2 = vector_clock_set(&clock, &member2, 3);
    assert(clock_record2 != NULL);

    assert(vector_clock_copy(&msg.data_version, &clock) == 0);

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_status_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    message_status_t out_msg;
    int decode_result = message_status_decode(buf, encode_result, &out_msg);
    assert(decode_result > 0);
    assert(decode_result == encode_result);

    validate_headers(&msg.header, &out_msg.header);
    assert(out_msg.data_version.size == 2);
    assert(out_msg.data_version.current_idx == 0);
    assert(out_msg.data_version.records[0].sequence_number == clock_record1->sequence_number);
    assert(out_msg.data_version.records[0].member_id == clock_record1->member_id);
    assert(out_msg.data_version.records[1].sequence_number == clock_record2->sequence_number);
    assert(out_msg.data_version.records[1].member_id == clock_record2->member_id);

    assert(message_status_encode(&msg, buf, 1) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);
    assert(message_status_decode(buf, 12, &out_msg) == PITTACUS_ERR_BUFFER_NOT_ENOUGH);

    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
}

void test_message_invalid_message_type() {
    message_ack_t msg;
    message_header_init(&msg.header, 0xFF, 1);
    msg.ack_sequence_num = 2;

    uint8_t buf[MESSAGE_MAX_SIZE];
    int encode_result = message_ack_encode(&msg, buf, MESSAGE_MAX_SIZE);
    assert(encode_result > 0);

    assert(message_hello_decode(buf, encode_result, NULL) == PITTACUS_ERR_INVALID_MESSAGE);
    assert(message_welcome_decode(buf, encode_result, NULL) == PITTACUS_ERR_INVALID_MESSAGE);
    assert(message_member_list_decode(buf, encode_result, NULL) == PITTACUS_ERR_INVALID_MESSAGE);
    assert(message_ack_decode(buf, encode_result, NULL) == PITTACUS_ERR_INVALID_MESSAGE);
    assert(message_data_decode(buf, encode_result, NULL) == PITTACUS_ERR_INVALID_MESSAGE);
    assert(message_status_decode(buf, encode_result, NULL) == PITTACUS_ERR_INVALID_MESSAGE);
}

int main() {
    test_message_header();
    test_message_hello_enc_dec();
    test_message_welcome_enc_dec();
    test_message_member_list_enc_dec();
    test_message_ack_enc_dec();
    test_message_data_enc_dec();
    test_message_status_enc_dec();
    test_message_invalid_message_type();
    return 0;
}
