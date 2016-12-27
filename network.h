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

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#define PT_NTOHS(i) ntohs((i))
#define PT_NTOHL(i) ntohl((i))
#define PT_HTONS(i) htons((i))
#define PT_HTONL(i) htonl((i))

typedef socklen_t pt_socklen_t;
typedef struct sockaddr pt_sockaddr;
typedef struct sockaddr_in pt_sockaddr_in;
typedef struct sockaddr_in6 pt_sockaddr_in6;
typedef struct sockaddr_storage pt_sockaddr_storage;

typedef int pt_socket_fd;

pt_socket_fd pt_socket_datagram(int ipv6);
pt_socket_fd pt_socket_stream(int ipv6);
int pt_bind(pt_socket_fd fd, const pt_sockaddr_storage *addr, pt_socklen_t addr_len);
int pt_listen(pt_socket_fd fd, int backlog);
pt_socket_fd pt_accept(pt_socket_fd fd, pt_sockaddr_storage *addr, pt_socklen_t *addr_len);
ssize_t pt_recv_from(pt_socket_fd fd, uint8_t *buffer, size_t buffer_size, pt_sockaddr_storage *addr, pt_socklen_t *addr_len);
ssize_t pt_send_to(pt_socket_fd fd, const uint8_t *buffer, size_t buffer_size, const pt_sockaddr_storage *addr, pt_socklen_t addr_len);
void pt_close(pt_socket_fd fd);

#endif //PITTACUS_UTILS_H
