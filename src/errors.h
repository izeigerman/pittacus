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
#ifndef PITTACUS_ERRORS_H
#define PITTACUS_ERRORS_H

typedef enum pittacus_error {
    PITTACUS_ERR_NONE = 0,
    PITTACUS_ERR_INIT_FAILED = -1,
    PITTACUS_ERR_ALLOCATION_FAILED = -2,
    PITTACUS_ERR_BAD_STATE = -3,
    PITTACUS_ERR_INVALID_MESSAGE = -4,
    PITTACUS_ERR_BUFFER_NOT_ENOUGH = -5,
    PITTACUS_ERR_NOT_FOUND = -6,
} pittacus_error_t;

#endif //PITTACUS_ERRORS_H
