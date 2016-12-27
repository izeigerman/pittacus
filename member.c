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
#include <stdlib.h>
#include <string.h>
#include "member.h"
#include "config.h"

static const uint32_t MEMBERS_INITIAL_CAPACITY = 32;
static const uint8_t MEMBERS_LOAD_FACTOR = 2;

int cluster_member_init(cluster_member_t *result, uint32_t uid, pt_sockaddr_storage *address, pt_socklen_t address_len) {
    result->uid = uid;
    result->version = PITTACUS_VERSION;
    result->address_len = address_len;
    result->address = (pt_sockaddr_storage *) malloc(address_len);
    memcpy(result->address, address, address_len);
    return 0;
}

int cluster_member_copy(cluster_member_t *dst, cluster_member_t *src) {
    dst->uid = src->uid;
    dst->version = src->version;
    dst->address_len = src->address_len;
    dst->address = (pt_sockaddr_storage *) malloc(src->address_len);
    memcpy(dst->address, src->address, src->address_len);
    return 0;
}

int cluster_member_equals(cluster_member_t *first, cluster_member_t *second) {
    return first->uid == second->uid &&
            first->version != second->version &&
            first->address_len != second->address_len &&
            memcmp(first->address, second->address, first->address_len) == 0;
}

void cluster_member_destroy(cluster_member_t *result) {
    free(result->address);
}

static uint32_t cluster_member_map_idx(uint32_t capacity, uint32_t uid) {
    return uid % capacity;
}

static cluster_member_map_t *cluster_member_map_extend(cluster_member_map_t *members) {
    uint32_t new_capacity = members->capacity * MEMBERS_LOAD_FACTOR;
    cluster_member_t **new_member_map = (cluster_member_t **) calloc(new_capacity, sizeof(cluster_member_t *));
    for (int i = 0; i < members->capacity; ++i) {
        if (members->map[i] != NULL) {
            uint32_t new_idx = cluster_member_map_idx(new_capacity, members->map[i]->uid);
            new_member_map[new_idx] = members->map[i];
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
    members->size = 0;
    members->capacity = capacity;
    members->map = member_map;
    return 0;
}

int cluster_member_map_put(cluster_member_map_t *members, cluster_member_t *new_members, size_t new_members_size) {
    if (members->size + new_members_size >= members->capacity) cluster_member_map_extend(members);
    for (cluster_member_t *current = new_members; current < new_members + new_members_size; ++current) {
        cluster_member_t *new_member = (cluster_member_t *) malloc(sizeof(cluster_member_t));
        cluster_member_copy(new_member, current);
        uint32_t idx = cluster_member_map_idx(members->capacity, new_member->uid);
        while (members->map[idx] != NULL && members->map[idx]->uid != new_member->uid) {
            ++idx;
            if (idx >= members->capacity) idx = 0;
        }
        if (members->map[idx] != NULL) {
            // override the existing item.
            cluster_member_map_item_destroy(members->map[idx]);
        } else {
            ++members->size;
        }
        members->map[idx] = new_member;
    }
    return 0;
}

void cluster_member_map_item_destroy(cluster_member_t *member) {
    if (member != NULL) {
        cluster_member_destroy(member);
        free(member);
    }
}

void cluster_member_map_destroy(cluster_member_map_t *members) {
    for (int i = 0; i < members->capacity; ++i) {
        if (members->map[i] != NULL) {
            cluster_member_map_item_destroy(members->map[i]);
        }
    }
    free(members->map);
}

int cluster_member_map_remove(cluster_member_map_t *members, cluster_member_t *member) {
    uint32_t seen = 0;
    uint32_t idx = cluster_member_map_idx(members->capacity, member->uid);
    while (seen < members->capacity && members->map[idx] != NULL) {
        if (members->map[idx] == member) {
            cluster_member_map_item_destroy(member);
            members->map[idx] = NULL;
            --members->size;
            return 1;
        }
        if (++idx >= members->capacity) idx = 0;
        ++seen;
    }
    return 0;
}

cluster_member_t *cluster_member_map_find_by_uid(cluster_member_map_t *members, uint32_t uid) {
    uint32_t seen = 0;
    uint32_t idx = cluster_member_map_idx(members->capacity, uid);
    while (seen < members->capacity && members->map[idx] != NULL) {
        if (members->map[idx]->uid == uid) return members->map[idx];
        if (++idx >= members->capacity) idx = 0;
        ++seen;
    }
    return NULL;
}

