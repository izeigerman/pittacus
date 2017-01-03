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
#ifndef PITTACUS_CONFIG_H
#define PITTACUS_CONFIG_H

#ifdef  __cplusplus
extern "C" {
#endif

#define PROTOCOL_VERSION 0x01

/** The interval in milliseconds between retry attempts. */
#define MESSAGE_RETRY_INTERVAL 10000
/** The maximum number of attempts to deliver a message. */
#define MESSAGE_RETRY_ATTEMPTS 3
/** The maximum size of the member list that is shared with a newcomer node. */
#define MEMBER_LIST_SYNC_SIZE 10
/** The number of members that are used for further gossip propagation. */
#define MESSAGE_RUMOR_FACTOR 3
/** The maximum supported size of the message including a protocol overhead. */
#define MESSAGE_MAX_SIZE 512
/** The maximum number of unique messages that can be stored in the outbound message queue. */
#define MAX_OUTPUT_MESSAGES 25

#ifdef  __cplusplus
} // extern "C"
#endif

#endif //PITTACUS_CONFIG_H
