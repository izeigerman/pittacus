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
#include "test_utils.h"
#include <string.h>
#include <assert.h>

void test_vector_clock_init() {
    size_t clock_size = sizeof(vector_clock_t);
    char zero_buf[clock_size];
    memset(zero_buf, 0, clock_size);

    vector_clock_t clock;
    assert(vector_clock_init(&clock) == 0);
    assert(memcmp(&clock, zero_buf, clock_size) == 0);
}

void test_vector_clock_set() {
    vector_clock_t clock;
    assert(vector_clock_init(&clock) == 0);

    cluster_member_t member;
    create_test_member(12345, &member);

    vector_record_t *set_result1 = vector_clock_set(&clock, &member, 1);
    assert(set_result1 != NULL);
    assert(set_result1->sequence_number == 1);
    assert(clock.size == 1);
    assert(clock.current_idx == 1);

    vector_record_t *set_result2 = vector_clock_set(&clock, &member, 2);
    assert(set_result2 != NULL);
    assert(set_result2->sequence_number == 2);
    assert(clock.size == 1);
    assert(clock.current_idx == 1);
    // We've updated the same member, it should be the exactly same vector record.
    assert(set_result1 == set_result2);

    cluster_member_t member2;
    create_test_member(12346, &member2);

    vector_record_t *set_result3 = vector_clock_set(&clock, &member2, 3);
    assert(set_result3 != NULL);
    assert(set_result3->sequence_number == 3);
    assert(clock.size == 2);
    assert(clock.current_idx == 2);
    assert(set_result3 != set_result2);

    cluster_member_destroy(&member);
    cluster_member_destroy(&member2);
}

void test_vector_clock_set_overflow() {
    vector_clock_t clock;
    assert(vector_clock_init(&clock) == 0);

    uint16_t base_port = 1000;
    size_t members_size = MAX_VECTOR_SIZE + 1;
    cluster_member_t members[members_size];
    for (int i = 0; i < members_size; ++i) {
        create_test_member(base_port + i, &members[i]);
        vector_record_t *set_result = vector_clock_set(&clock, &members[i], i);
        assert(set_result != NULL);
        assert(set_result->sequence_number == i);
    }

    assert(clock.size == 20);
    assert(clock.current_idx == members_size - MAX_VECTOR_SIZE);
    assert(clock.records[MAX_VECTOR_SIZE - 1].sequence_number == MAX_VECTOR_SIZE - 1);
    assert(clock.records[0].sequence_number == MAX_VECTOR_SIZE);

    for (int i = 0; i < members_size; ++i) {
        cluster_member_destroy(&members[i]);
    }
}

void test_vector_clock_increment() {
    vector_clock_t clock;
    assert(vector_clock_init(&clock) == 0);

    cluster_member_t member;
    create_test_member(12345, &member);

    // The initial value for member was not set. Nothing to increment.
    assert(vector_clock_increment(&clock, &member) == NULL);

    vector_record_t *set_result = vector_clock_set(&clock, &member, 1);
    assert(set_result != NULL);
    assert(set_result->sequence_number == 1);
    assert(clock.size == 1);
    assert(clock.current_idx == 1);

    vector_record_t *increment_result = vector_clock_increment(&clock, &member);
    assert(increment_result == set_result);
    assert(increment_result->sequence_number == 2);
    assert(clock.size == 1);
    assert(clock.current_idx == 1);

    cluster_member_destroy(&member);
}

void test_vector_clock_compare() {
    vector_clock_t clock1;
    assert(vector_clock_init(&clock1) == 0);
    vector_clock_t clock2;
    assert(vector_clock_init(&clock2) == 0);

    cluster_member_t member1;
    create_test_member(12345, &member1);
    cluster_member_t member2;
    create_test_member(12346, &member2);

    assert(vector_clock_set(&clock1, &member1, 1) != NULL);
    assert(vector_clock_set(&clock2, &member2, 1) != NULL);

    assert(vector_clock_compare(&clock1, &clock2, PT_TRUE) == VC_CONFLICT);
    assert(clock1.size == 2);
    assert(clock1.records[0].sequence_number == 1);
    assert(clock1.records[1].sequence_number == 1);
    assert(vector_clock_increment(&clock1, &member2) != NULL);
    assert(clock1.records[1].sequence_number == 2);

    assert(vector_clock_compare(&clock2, &clock1, PT_FALSE) == VC_BEFORE);
    assert(clock2.size == 1);
    assert(clock2.records[0].sequence_number == 1);

    assert(vector_clock_compare(&clock1, &clock2, PT_FALSE) == VC_AFTER);
    assert(vector_clock_compare(&clock2, &clock1, PT_TRUE) == VC_BEFORE);

    assert(vector_clock_compare(&clock2, &clock1, PT_TRUE) == VC_EQUAL);
    assert(vector_clock_compare(&clock1, &clock2, PT_TRUE) == VC_EQUAL);

    assert(vector_clock_set(&clock1, &member1, 3) != NULL);
    assert(vector_clock_set(&clock2, &member2, 3) != NULL);
    assert(vector_clock_compare(&clock2, &clock1, PT_FALSE) == VC_CONFLICT);
    assert(vector_clock_compare(&clock1, &clock2, PT_FALSE) == VC_CONFLICT);


    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
}

void test_vector_clock_compare_with_record() {
    vector_clock_t actual_clock;
    assert(vector_clock_init(&actual_clock) == 0);

    cluster_member_t member;
    create_test_member(12345, &member);

    vector_record_t *test_record = vector_clock_set(&actual_clock, &member, 1);
    assert(test_record != NULL);

    vector_clock_t test_clock;
    assert(vector_clock_init(&test_clock) == 0);

    assert(vector_clock_compare_with_record(&test_clock, test_record, PT_TRUE) == VC_BEFORE);
    assert(test_clock.size == 1);
    assert(test_clock.current_idx == 1);
    assert(test_clock.records[0].sequence_number == 1);
    assert(vector_clock_compare_with_record(&test_clock, test_record, PT_FALSE) == VC_EQUAL);

    assert(vector_clock_increment(&test_clock, &member) != NULL);
    assert(vector_clock_compare_with_record(&test_clock, test_record, PT_FALSE) == VC_AFTER);

    test_record = vector_clock_set(&actual_clock, &member, 3);
    assert(vector_clock_compare_with_record(&test_clock, test_record, PT_FALSE) == VC_BEFORE);
    assert(test_clock.size == 1);
    assert(test_clock.current_idx == 1);
    assert(test_clock.records[0].sequence_number == 2);

    cluster_member_destroy(&member);
}

void test_vector_clock_copy() {
    vector_clock_t clock1;
    assert(vector_clock_init(&clock1) == 0);
    vector_clock_t clock2;
    assert(vector_clock_init(&clock2) == 0);

    cluster_member_t member1;
    create_test_member(12345, &member1);
    cluster_member_t member2;
    create_test_member(12346, &member2);

    assert(vector_clock_set(&clock1, &member1, 1) != NULL);
    assert(vector_clock_set(&clock2, &member2, 1) != NULL);

    assert(vector_clock_copy(&clock1, &clock2) == 0);
    assert(clock1.size == 1);
    assert(clock1.current_idx == 1);
    assert(clock1.records[0].member_id == clock2.records[0].member_id);
    assert(clock1.records[0].sequence_number == clock2.records[0].sequence_number);

    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
}

void test_vector_clock_record_copy() {
    vector_clock_t clock1;
    assert(vector_clock_init(&clock1) == 0);
    vector_clock_t clock2;
    assert(vector_clock_init(&clock2) == 0);

    cluster_member_t member1;
    create_test_member(12345, &member1);
    cluster_member_t member2;
    create_test_member(12346, &member2);

    vector_record_t *record1 = vector_clock_set(&clock1, &member1, 1);
    assert(record1 != NULL);
    vector_record_t *record2 = vector_clock_set(&clock2, &member2, 1);
    assert(record2 != NULL);

    assert(vector_clock_record_copy(record1, record2) == 0);
    assert(record1->member_id == record2->member_id);
    assert(record1->sequence_number == record2->sequence_number);
    assert(record1 != record2);

    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
}

int main() {
    test_vector_clock_init();
    test_vector_clock_set();
    test_vector_clock_set_overflow();
    test_vector_clock_increment();
    test_vector_clock_compare();
    test_vector_clock_compare_with_record();
    test_vector_clock_copy();
    test_vector_clock_record_copy();
    return 0;
}
