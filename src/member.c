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
#include <stdlib.h>
#include <string.h>
#include "member.h"
#include "config.h"
#include "utils.h"
#include "errors.h"

static const uint32_t MEMBERS_INITIAL_CAPACITY = 32;
static const uint8_t MEMBERS_EXTENSION_FACTOR = 2;
static const double MEMBERS_LOAD_FACTOR = 0.75;

int cluster_member_init(cluster_member_t *result, const pt_sockaddr_storage *address, pt_socklen_t address_len) {
    result->uid = pt_time() / 1000;
    result->version = PROTOCOL_VERSION;
    result->address_len = address_len;
    result->address = (pt_sockaddr_storage *) malloc(address_len);
    if (result->address == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;
    memcpy(result->address, address, address_len);
    return PITTACUS_ERR_NONE;
}

int cluster_member_copy(cluster_member_t *dst, cluster_member_t *src) {
    dst->uid = src->uid;
    dst->version = src->version;
    dst->address_len = src->address_len;
    dst->address = (pt_sockaddr_storage *) malloc(src->address_len);
    if (dst->address == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;
    memcpy(dst->address, src->address, src->address_len);
    return PITTACUS_ERR_NONE;
}

int cluster_member_equals(cluster_member_t *first, cluster_member_t *second) {
    return first->uid == second->uid &&
            first->version == second->version &&
            first->address_len == second->address_len &&
            memcmp(first->address, second->address, first->address_len) == 0;
}

void cluster_member_destroy(cluster_member_t *result) {
    free(result->address);
}

int cluster_member_decode(const uint8_t *buffer, size_t buffer_size, cluster_member_t *member) {
    if (buffer_size < 2 * sizeof(uint32_t) + sizeof(uint16_t)) return PITTACUS_ERR_BUFFER_NOT_ENOUGH;
    const uint8_t *cursor = buffer;
    member->version = uint16_decode(cursor);
    cursor += sizeof(uint16_t);
    member->uid = uint32_decode(cursor);
    cursor += sizeof(uint32_t);
    member->address_len = uint32_decode(cursor);
    cursor += sizeof(uint32_t);
    member->address = (pt_sockaddr_storage *) cursor;
    cursor += member->address_len;
    return cursor - buffer;
}

int cluster_member_encode(const cluster_member_t *member, uint8_t *buffer, size_t buffer_size) {
    if (buffer_size < 2 * sizeof(uint32_t) + sizeof(uint16_t) + member->address_len) {
        return PITTACUS_ERR_BUFFER_NOT_ENOUGH;
    }
    uint8_t *cursor = buffer;
    uint16_encode(member->version, cursor);
    cursor += sizeof(uint16_t);
    uint32_encode(member->uid, cursor);
    cursor += sizeof(uint32_t);
    uint32_encode(member->address_len, cursor);
    cursor += sizeof(uint32_t);
    memcpy(cursor, member->address, member->address_len);
    cursor += member->address_len;
    return cursor - buffer;
}

static cluster_member_map_t *cluster_member_map_extend(cluster_member_map_t *members, uint32_t required_size) {
    uint32_t new_capacity = members->capacity;
    while (required_size >= new_capacity * MEMBERS_LOAD_FACTOR) new_capacity *= MEMBERS_EXTENSION_FACTOR;

    cluster_member_t **new_member_map = (cluster_member_t **) calloc(new_capacity, sizeof(cluster_member_t *));
    if (new_member_map == NULL) return NULL;

    for (int i = 0; i < members->size; ++i) {
        if (members->map[i] != NULL) {
            new_member_map[i] = members->map[i];
        }
    }
    free(members->map);
    members->capacity = new_capacity;
    members->map = new_member_map;
    return members;
}

int cluster_member_map_init(cluster_member_map_t *members) {
    uint32_t capacity = MEMBERS_INITIAL_CAPACITY;

    cluster_member_t **member_map = (cluster_member_t **) calloc(capacity, sizeof(cluster_member_t *));
    if (member_map == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;

    members->size = 0;
    members->capacity = capacity;
    members->map = member_map;
    return PITTACUS_ERR_NONE;
}

int cluster_member_map_put(cluster_member_map_t *members, cluster_member_t *new_members, size_t new_members_size) {
    uint32_t new_size = members->size + new_members_size;
    // increase the capacity of the map if the new size is >= 0.75 of the current capacity.
    if (new_size >= members->capacity * MEMBERS_LOAD_FACTOR) {
        if (cluster_member_map_extend(members, new_size) == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;
    }

    for (cluster_member_t *current = new_members; current < new_members + new_members_size; ++current) {
        pt_bool_t exists = PT_FALSE;
        for (int i = 0; i < members->size; ++i) {
            if (cluster_member_equals(members->map[i], current)) {
                exists = PT_TRUE;
                break;
            }
        }
        if (!exists) {
            // New member.
            cluster_member_t *new_member = (cluster_member_t *) malloc(sizeof(cluster_member_t));
            if (new_member == NULL) return PITTACUS_ERR_ALLOCATION_FAILED;

            cluster_member_copy(new_member, current);
            members->map[members->size] = new_member;
            ++members->size;
        }
    }
    return PITTACUS_ERR_NONE;
}

void cluster_member_map_item_destroy(cluster_member_t *member) {
    if (member != NULL) {
        cluster_member_destroy(member);
        free(member);
    }
}

void cluster_member_map_destroy(cluster_member_map_t *members) {
    for (int i = 0; i < members->size; ++i) {
        cluster_member_map_item_destroy(members->map[i]);
    }
    free(members->map);
}

int cluster_member_map_remove(cluster_member_map_t *members, cluster_member_t *member) {
    if (members->size == 0) return 0;
    uint32_t idx = 0;
    while (idx < members->size) {
        if (members->map[idx] == member) {
            cluster_member_map_item_destroy(member);
            // Shift the list.
            for (int i = idx; i < members->size - 1; ++i) {
                members->map[i] = members->map[i + 1];
            }
            --members->size;
            return PT_TRUE;
        }
        ++idx;
    }
    return PT_FALSE;
}

cluster_member_t *cluster_member_map_find_by_addr(cluster_member_map_t *members,
                                                  const pt_sockaddr_storage *addr,
                                                  pt_socklen_t addr_size) {
    if (members->size == 0) return NULL;
    for (int i = 0; i < members->size; ++i) {
        if (memcmp(members->map[i]->address, addr, addr_size) == 0) return members->map[i];
    }
    return NULL;
}

size_t cluster_member_map_random_member(cluster_member_map_t *members,
                                        cluster_member_t **reservoir, size_t reservoir_size) {
    // Randomly choosing the specified number of elements using the
    // reservoir sampling algorithm.
    if (members->size == 0) return 0;
    size_t actual_reservoir_size = (members->size > reservoir_size) ? reservoir_size : members->size;

    size_t reservoir_idx = 0;
    size_t member_idx = 0;

    // Fill in the reservoir with first map elements.
    while (reservoir_idx < actual_reservoir_size) {
        reservoir[reservoir_idx] = members->map[member_idx];
        ++member_idx;
        ++reservoir_idx;
    }

    // Randomly replace reservoir's elements with items from the member's map.
    if (actual_reservoir_size < members->size) {
        for (; member_idx < members->size; ++member_idx) {
            size_t random_idx = pt_random() % (member_idx + 1);
            if (random_idx < actual_reservoir_size) {
                reservoir[random_idx] = members->map[member_idx];
            }
        }
    }

    return actual_reservoir_size;
}
