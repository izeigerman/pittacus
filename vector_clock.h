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
#include "utils.h"

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
vector_record_t *vector_clock_find_record(vector_clock_t *clock, const cluster_member_t *member);
int vector_clock_set(vector_clock_t *clock, const cluster_member_t *member, uint32_t seq_num);
int vector_clock_increment(vector_clock_t *clock, const cluster_member_t *member);

/**
 * Compares 2 vector clocks. If the "merge" parameter is set to true, 2 clocks
 * will be merged into the first vector clock instance.
 *
 * @param first the first vector clock. This vector clock will contain the
 *              merge result eventually.
 * @param second the second vector clock.
 * @param merge if true the vectors will be merged into the first vector clock.
 * @return VC_EQUAL - if the 2 clocks are identical.
 *         VC_BEFORE - if the first clock represents the older version.
 *         VC_AFTER - if the first clock represents the newer version.
 *         VC_CONFLICT - if there is a conflict between 2 versions.
 */
vector_clock_comp_res_t vector_clock_compare(vector_clock_t *first,
                                             const vector_clock_t *second,
                                             pt_bool_t merge);

/**
 * Compares the given vector clock with a single record. If the "merge"
 * parameter is set to true, the record will be merged into the vector
 * clock instance.
 *
 * @param clock the first vector clock. This vector clock will contain the
 *              merge result eventually.
 * @param record the vector clock single record.
 * @param merge if true the record will be merged into the clock instance.
 * @return VC_EQUAL - if the given record matches the corresponding record in vector clock.
 *         VC_BEFORE - if the vector clock contains older record version.
 *         VC_AFTER - if the vector clock contains newer record version.
 */
vector_clock_comp_res_t vector_clock_compare_with_record(vector_clock_t *clock,
                                                         const vector_record_t *record,
                                                         pt_bool_t merge);

int vector_clock_record_decode(const uint8_t *buffer, size_t buffer_size, vector_record_t *result);
int vector_clock_record_encode(const vector_record_t *record, uint8_t *buffer, size_t buffer_size);

#endif //PITTACUS_VECTOR_CLOCK_H
