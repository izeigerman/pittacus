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
#include "test_utils.h"

int create_test_member(uint16_t port, cluster_member_t *result) {
    pt_sockaddr_in in;
    in.sin_family = AF_INET;
    in.sin_port = PT_HTONS(port);
    inet_aton("127.0.0.1", &in.sin_addr);

    return cluster_member_init(result, (const pt_sockaddr_storage *)&in, sizeof(pt_sockaddr_in));
}
