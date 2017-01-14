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
#include "vector_clock.h"
#include "network.h"
#include "errors.h"
#include <string.h>
#include <stdio.h>

static void vector_clock_create_member_id(const cluster_member_t *member, member_id_t *result) {
    // copy 4 bytes of address and 2 bytes of port
    pt_sockaddr_storage *addr = member->address;
    uint8_t *result_buf = (uint8_t *) result;
    if (addr->ss_family == AF_INET) {
        pt_sockaddr_in *addr_in = (pt_sockaddr_in *) addr;
        memcpy(result_buf, &addr_in->sin_addr, 4);
        memcpy(result_buf + 4, &addr_in->sin_port, 2);
    } else {
        pt_sockaddr_in6 *addr_in6 = (pt_sockaddr_in6 *) addr;
        memcpy(result_buf, &addr_in6->sin6_addr, 4);
        memcpy(result_buf + 4, &addr_in6->sin6_port, 2);
    }
    // fill the remaining 2 bytes with member's uid.
    uint32_t uid_network = PT_HTONL(member->uid);
    memcpy(result_buf + 6, &uid_network, 2);
}

static int vector_clock_find_by_member_id(const vector_clock_t *clock, const member_id_t *member_id) {
    for (int i = 0; i < clock->size; ++i) {
        if (clock->records[i].member_id == *member_id) return i;
    }
    return PITTACUS_ERR_NOT_FOUND;
}

vector_record_t *vector_clock_find_record(vector_clock_t *clock, const cluster_member_t *member) {
    member_id_t member_id;
    vector_clock_create_member_id(member, &member_id);
    int idx = vector_clock_find_by_member_id(clock, &member_id);
    if (idx < 0) return NULL;
    return &clock->records[idx];
}

int vector_clock_init(vector_clock_t *clock) {
    if (clock == NULL) return PITTACUS_ERR_INIT_FAILED;
    memset(clock, 0, sizeof(vector_clock_t));
    return PITTACUS_ERR_NONE;
}

static vector_record_t *vector_clock_set_by_id(vector_clock_t *clock,
                                               const member_id_t *member_id,
                                               uint32_t seq_num) {
    int idx = vector_clock_find_by_member_id(clock, member_id);
    if (idx < 0) {
        // insert or override the latest record with the new record.
        uint32_t new_idx = clock->current_idx;
        clock->records[new_idx].member_id = *member_id;
        clock->records[new_idx].sequence_number = seq_num;

        if (clock->size < MAX_VECTOR_SIZE) ++clock->size;
        if (++clock->current_idx >= MAX_VECTOR_SIZE) clock->current_idx = 0;
        return &clock->records[new_idx];
    } else {
        clock->records[idx].sequence_number = seq_num;
        return &clock->records[idx];
    }
}

vector_record_t *vector_clock_increment(vector_clock_t *clock, const cluster_member_t *member) {
    vector_record_t *record = vector_clock_find_record(clock, member);
    if (record == NULL) return NULL;
    ++record->sequence_number;
    return record;
}

vector_record_t *vector_clock_set(vector_clock_t *clock,
                                  const cluster_member_t *member,
                                  uint32_t seq_num) {
    member_id_t member_id;
    vector_clock_create_member_id(member, &member_id);
    return vector_clock_set_by_id(clock, &member_id, seq_num);
}

void vector_clock_to_string(const vector_clock_t *clock, char *result) {
    char *cursor = result;
    int str_size = 0;
    for (int i = 0; i < clock->size; ++i) {
        str_size = sprintf(cursor, "(%llx:%u)  ",
                           clock->records[i].member_id,
                           clock->records[i].sequence_number);
        cursor += str_size;
    }
}

int vector_clock_record_copy(vector_record_t *dst, const vector_record_t *src) {
    dst->member_id = src->member_id;
    dst->sequence_number = src->sequence_number;
    return PITTACUS_ERR_NONE;
}

int vector_clock_copy(vector_clock_t *dst, const vector_clock_t *src) {
    dst->size = src->size;
    dst->current_idx = src->current_idx;
    for (int i = 0; i < src->size; ++i) {
        vector_clock_record_copy(&dst->records[i], &src->records[i]);
    }
    return PITTACUS_ERR_NONE;
}

static vector_clock_comp_res_t vector_clock_resolve_comp_result(vector_clock_comp_res_t prev,
                                                                vector_clock_comp_res_t new) {
    return (prev != VC_EQUAL && new != prev) ? VC_CONFLICT : new;
}

vector_clock_comp_res_t vector_clock_compare_with_record(vector_clock_t *clock,
                                                         const vector_record_t *record,
                                                         pt_bool_t merge) {
    vector_clock_comp_res_t result = VC_EQUAL;
    int idx = vector_clock_find_by_member_id(clock, &record->member_id);
    if (idx < 0) {
        result = VC_BEFORE;
        if (merge) {
            vector_clock_set_by_id(clock, &record->member_id, record->sequence_number);
        }
    } else {
        uint32_t first_seq_num = clock->records[idx].sequence_number;
        uint32_t second_seq_num = record->sequence_number;
        if (first_seq_num > second_seq_num) {
            result = VC_AFTER;
        } else if (first_seq_num < second_seq_num) {
            result = VC_BEFORE;
            if (merge) {
                clock->records[idx].sequence_number = second_seq_num;
            }
        }
    }
    return result;
}

vector_clock_comp_res_t vector_clock_compare(vector_clock_t *first,
                                             const vector_clock_t *second,
                                             pt_bool_t merge) {
    vector_clock_comp_res_t result = VC_EQUAL;

    uint32_t second_visited_idxs = 0;

    for (int i = 0; i < first->size; ++i) {
        int second_idx = vector_clock_find_by_member_id(second, &first->records[i].member_id);
        second_visited_idxs |= (1 << second_idx);

        if (second_idx < 0) {
            result = vector_clock_resolve_comp_result(result, VC_AFTER);
        } else {
            uint32_t first_seq_num = first->records[i].sequence_number;
            uint32_t second_seq_num = second->records[second_idx].sequence_number;
            if (first_seq_num > second_seq_num) {
                result = vector_clock_resolve_comp_result(result, VC_AFTER);
            } else if (second_seq_num > first_seq_num) {
                result = vector_clock_resolve_comp_result(result, VC_BEFORE);
                if (merge) {
                    first->records[i].sequence_number = second_seq_num;
                }
            }
        }
    }

    uint32_t second_visited_mask = ((1 << second->size) - 1) & 0xFFFFFFFF;
    uint32_t missing_idxs = (second_visited_idxs ^ second_visited_mask);
    if (missing_idxs != 0) {
        // There are some records in the second clock that are missing
        // in the first one.
        result = vector_clock_resolve_comp_result(result, VC_BEFORE);

        if (merge) {
            for (int i = 0; missing_idxs != 0; ++i) {
                if ((missing_idxs & 0x01) != 0) {
                    vector_clock_set_by_id(first, &second->records[i].member_id,
                                           second->records[i].sequence_number);
                }
                missing_idxs >>= 1;
            }
        }
    }
    return result;
}

int vector_clock_record_decode(const uint8_t *buffer, size_t buffer_size, vector_record_t *result) {
    if (buffer_size < VECTOR_RECORD_SIZE) return PITTACUS_ERR_BUFFER_NOT_ENOUGH;

    const uint8_t *cursor = buffer;
    result->sequence_number = uint32_decode(cursor);
    cursor += sizeof(uint32_t);

    memcpy(&result->member_id, cursor, MEMBER_ID_SIZE);
    cursor += MEMBER_ID_SIZE;

    return cursor - buffer;
}

int vector_clock_record_encode(const vector_record_t *record, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < VECTOR_RECORD_SIZE) return PITTACUS_ERR_BUFFER_NOT_ENOUGH;

    uint8_t *cursor = buffer;
    uint32_encode(record->sequence_number, cursor);
    cursor += sizeof(uint32_t);

    memcpy(cursor, &record->member_id, MEMBER_ID_SIZE);
    cursor += MEMBER_ID_SIZE;

    return cursor - buffer;
}

int vector_clock_decode(const uint8_t *buffer, size_t buffer_size, vector_clock_t *result) {
    const uint8_t *cursor = buffer;
    const uint8_t *buffer_end = buffer + buffer_size;

    uint16_t size = uint16_decode(buffer);
    cursor += sizeof(uint16_t);
    if (buffer_end - cursor < size * VECTOR_RECORD_SIZE) return PITTACUS_ERR_BUFFER_NOT_ENOUGH;

    result->size = size;
    result->current_idx = 0;

    int decode_result = 0;
    for (int i = 0; i < size; ++i) {
        decode_result = vector_clock_record_decode(cursor, buffer_end - cursor, &result->records[i]);
        if (decode_result < 0) return decode_result;
        cursor += VECTOR_RECORD_SIZE;
    }

    return cursor - buffer;
}

int vector_clock_encode(const vector_clock_t *clock, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < sizeof(uint16_t) + clock->size * VECTOR_RECORD_SIZE) return PITTACUS_ERR_BUFFER_NOT_ENOUGH;

    uint8_t *cursor = buffer;
    uint8_t *buffer_end = buffer + buffer_size;

    uint16_encode(clock->size, cursor);
    cursor += sizeof(uint16_t);

    int encode_result = 0;
    for (int i = 0; i < clock->size; ++i) {
        encode_result = vector_clock_record_encode(&clock->records[i], cursor, buffer_end - cursor);
        if (encode_result < 0) return encode_result;
        cursor += encode_result;
    }

    return cursor - buffer;
}
