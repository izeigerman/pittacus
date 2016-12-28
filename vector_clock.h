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
#ifndef PITTACUS_VECTOR_CLOCK_H
#define PITTACUS_VECTOR_CLOCK_H

#include <stdint.h>
#include "member.h"

#define MAX_VECTOR_SIZE 20

#define MEMBER_ID_SIZE 12

typedef struct vector_record {
    uint32_t sequence_number;
    uint8_t member_id[MEMBER_ID_SIZE];
} vector_record_t;

typedef struct vector_clock {
    uint16_t size;
    uint16_t current_idx;
    vector_record_t records[MAX_VECTOR_SIZE];
} vector_clock_t;

typedef enum vector_clock_comp_res {
    VC_BEFORE,
    VC_AFTER,
    VC_EQUAL,
    VC_CONFLICT
} vector_clock_comp_res_t;

int vector_clock_init(vector_clock_t *clock);
int vector_clock_set(vector_clock_t *clock, const cluster_member_t *member, uint32_t seq_num);

/**
 * Compares 2 vector clocks and merges them into the first vector clock instance.
 *
 * @param first the first vector clock. This vector clock will contain the
 *              merge result eventually.
 * @param second the second vector clock.
 * @return VC_EQUAL - if the 2 clocks are identical.
 *         VC_BEFORE - if the first clock represents the older version.
 *         VC_AFTER - if the first clock represents the newer version.
 *         VC_CONFLICT - if there is a conflict between 2 versions.
 */
vector_clock_comp_res_t vector_clock_compare_and_merge(vector_clock_t *first, const vector_clock_t *second);

#endif //PITTACUS_VECTOR_CLOCK_H
