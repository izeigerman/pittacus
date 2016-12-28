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
#ifndef PITTACUS_UTILS_H
#define PITTACUS_UTILS_H

#include <stdint.h>

uint32_t pt_random();

uint16_t uint16_decode(const uint8_t *buffer);
void uint16_encode(uint16_t n, uint8_t *buffer);
uint32_t uint32_decode(const uint8_t *buffer);
void uint32_encode(uint32_t n, uint8_t *buffer);

#endif //PITTACUS_UTILS_H
