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
#include "member.h"
#include "test_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <utils.h>

void test_cluster_member_equals() {
    cluster_member_t member1;
    cluster_member_t member2;

    assert(create_test_member(12345, &member1) == 0);
    assert(create_test_member(12345, &member2) == 0);
    member2.uid = member1.uid;

    assert(cluster_member_equals(&member1, &member1));
    assert(cluster_member_equals(&member1, &member2));
    assert(cluster_member_equals(&member2, &member1));

    assert(create_test_member(12346, &member2) == 0);
    assert(!cluster_member_equals(&member1, &member2));
    assert(!cluster_member_equals(&member2, &member1));

    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
}

void test_cluster_member_set_put_remove() {
    cluster_member_set_t set;
    assert(cluster_member_set_init(&set) == 0);
    assert(set.capacity > 0);
    assert(set.size == 0);
    uint32_t init_capacity = set.capacity;

    cluster_member_t member1;
    cluster_member_t member2;

    assert(create_test_member(12345, &member1) == 0);
    assert(cluster_member_set_put(&set, &member1, 1) == 0);
    assert(set.size == 1);
    assert(set.capacity == init_capacity);

    // Shouldn't put the same member twice.
    assert(cluster_member_set_put(&set, &member1, 1) == 0);
    assert(set.size == 1);
    assert(set.capacity == init_capacity);

    // Put a different member.
    assert(create_test_member(12346, &member2) == 0);
    assert(cluster_member_set_put(&set, &member2, 1) == 0);
    assert(set.size == 2);
    assert(set.capacity == init_capacity);

    // Test member remove.
    cluster_member_t *search_result = cluster_member_set_find_by_addr(&set, member1.address,
                                                                      member1.address_len);
    assert(search_result != NULL);
    assert(cluster_member_equals(search_result, &member1));

    assert(cluster_member_set_remove(&set, search_result) == PT_TRUE);
    assert(set.size == 1);
    assert(set.capacity == init_capacity);
    assert(cluster_member_set_remove(&set, search_result) == PT_FALSE);

    search_result = cluster_member_set_find_by_addr(&set, member1.address,
                                                    member1.address_len);
    assert(search_result == NULL);

    // Test remove by address.
    assert(cluster_member_set_remove_by_addr(&set, member2.address, member2.address_len) == PT_TRUE);
    assert(set.size == 0);
    assert(set.capacity == init_capacity);
    assert(cluster_member_set_remove_by_addr(&set, member2.address, member2.address_len) == PT_FALSE);

    search_result = cluster_member_set_find_by_addr(&set, member2.address,
                                                    member2.address_len);
    assert(search_result == NULL);

    cluster_member_destroy(&member1);
    cluster_member_destroy(&member2);
    cluster_member_set_destroy(&set);
}

void test_cluster_member_set_extension() {
    cluster_member_set_t set;
    assert(cluster_member_set_init(&set) == 0);
    uint32_t init_capacity = set.capacity;

    uint16_t base_port = 1000;
    size_t members_size = init_capacity * 3;
    cluster_member_t members[members_size];
    for (int i = 0; i < members_size; ++i) {
        assert(create_test_member(base_port + i, &members[i]) == 0);
    }

    assert(cluster_member_set_put(&set, members, members_size) == 0);
    assert(set.size == members_size);
    assert(set.capacity > members_size);

    cluster_member_t *search_result = NULL;
    for (int i = 0; i < members_size; ++i) {
        search_result = cluster_member_set_find_by_addr(&set, members[i].address, members[i].address_len);
        assert(search_result != NULL);
    }

    // Test duplicates insertion.
    assert(cluster_member_set_put(&set, members, members_size) == 0);
    assert(set.size == members_size);

    for (int i = 0; i < members_size; ++i) {
        cluster_member_destroy(&members[i]);
    }
    cluster_member_set_destroy(&set);
}

void test_cluster_member_set_random_members() {
    cluster_member_set_t set;
    assert(cluster_member_set_init(&set) == 0);

    uint16_t base_port = 1000;
    size_t members_size = 10;
    cluster_member_t members[members_size];
    for (int i = 0; i < members_size; ++i) {
        assert(create_test_member(base_port + i, &members[i]) == 0);
        assert(cluster_member_set_put(&set, &members[i], 1) == 0);
    }

    size_t rnd_members_size1 = 5;
    cluster_member_t *rnd_members1[rnd_members_size1];
    assert(cluster_member_set_random_members(&set, rnd_members1, rnd_members_size1) == rnd_members_size1);

    cluster_member_t *search_result = NULL;
    for (int i = 0; i < rnd_members_size1; ++i) {
        search_result = cluster_member_set_find_by_addr(&set, rnd_members1[i]->address,
                                                        rnd_members1[i]->address_len);
        assert(search_result != NULL);
        assert(search_result == rnd_members1[i]);
    }

    size_t rnd_members_size2 = 10;
    cluster_member_t *rnd_members2[rnd_members_size2];
    assert(cluster_member_set_random_members(&set, rnd_members2, rnd_members_size2) == rnd_members_size2);
    for (int i = 0; i < rnd_members_size2; ++i) {
        assert(rnd_members2[i] == set.set[i]);
    }

    size_t rnd_members_size3 = 15;
    cluster_member_t *rnd_members3[rnd_members_size3];
    assert(cluster_member_set_random_members(&set, rnd_members3, rnd_members_size3) == members_size);

    for (int i = 0; i < members_size; ++i) {
        cluster_member_destroy(&members[i]);
    }
    cluster_member_set_destroy(&set);
}

int main() {
    test_cluster_member_equals();
    test_cluster_member_set_put_remove();
    test_cluster_member_set_extension();
    test_cluster_member_set_random_members();
    return 0;
}
