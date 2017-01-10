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

#ifndef PROTOCOL_VERSION
#define PROTOCOL_VERSION 0x01
#endif

#ifndef MESSAGE_RETRY_INTERVAL
/** The interval in milliseconds between retry attempts. */
#define MESSAGE_RETRY_INTERVAL 10000
#endif

#ifndef MESSAGE_RETRY_ATTEMPTS
/** The maximum number of attempts to deliver a message. */
#define MESSAGE_RETRY_ATTEMPTS 3
#endif

#ifndef MESSAGE_RUMOR_FACTOR
/** The number of members that are used for further gossip propagation. */
#define MESSAGE_RUMOR_FACTOR 3
#endif

#ifndef MESSAGE_MAX_SIZE
/** The maximum supported size of the message including a protocol overhead. */
#define MESSAGE_MAX_SIZE 512
#endif

#ifndef MAX_OUTPUT_MESSAGES
/** The maximum number of unique messages that can be stored in the outbound message queue. */
#define MAX_OUTPUT_MESSAGES 100
#endif

#ifndef GOSSIP_TICK_INTERVAL
/** Determines the gossip tick interval in milliseconds. */
#define GOSSIP_TICK_INTERVAL 1000
#endif

#ifdef  __cplusplus
} // extern "C"
#endif

#endif //PITTACUS_CONFIG_H
