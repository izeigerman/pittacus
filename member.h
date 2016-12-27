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
#ifndef PITTACUS_MEMBER_H
#define PITTACUS_MEMBER_H

#include <stdint.h>
#include "network.h"

typedef struct cluster_member {
    uint16_t version;
    uint32_t uid;
    pt_socklen_t address_len;
    pt_sockaddr_storage *address;
} cluster_member_t;

int cluster_member_init(cluster_member_t *result, uint32_t uid, pt_sockaddr_storage *address, pt_socklen_t address_len);
int cluster_member_copy(cluster_member_t *dst, cluster_member_t *src);
int cluster_member_equals(cluster_member_t *first, cluster_member_t *second);
void cluster_member_destroy(cluster_member_t *result);

typedef struct cluster_member_map {
    cluster_member_t **map;
    uint32_t size;
    uint32_t capacity;
} cluster_member_map_t;

int cluster_member_map_init(cluster_member_map_t *members);
int cluster_member_map_put(cluster_member_map_t *members, cluster_member_t *new_members, size_t new_members_size);
int cluster_member_map_remove(cluster_member_map_t *members, cluster_member_t *member);
cluster_member_t *cluster_member_map_find_by_uid(cluster_member_map_t *members, uint32_t uid);
void cluster_member_map_item_destroy(cluster_member_t *member);
void cluster_member_map_destroy(cluster_member_map_t *members);

#endif //PITTACUS_MEMBER_H
