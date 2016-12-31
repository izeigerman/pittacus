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
#ifndef PITTACUS_CONFIG_H
#define PITTACUS_CONFIG_H

#define PROTOCOL_VERSION 0x01
#define PROTOCOL_ID_LENGTH 5
extern const char PROTOCOL_ID[PROTOCOL_ID_LENGTH];

/** The interval in seconds between retry attempts. */
#define MESSAGE_ATTEMPT_INTERVAL 5
/** The maximum number of attempts to deliver a message. */
#define MESSAGE_SEND_ATTEMPTS 3
/** The maximum size of the member list that is shared with a newcomer node. */
#define MEMBER_LIST_SYNC_SIZE 10
/** The number of members that are used for further gossip propagation. */
#define MESSAGE_RUMOR_FACTOR 3
/** The maximum supported size of the message including a protocol overhead. */
#define MESSAGE_MAX_SIZE 512
/** The maximum number of unique messages that can be stored in the outbound message queue. */
#define MAX_OUTPUT_MESSAGES 25


#endif //PITTACUS_CONFIG_H
