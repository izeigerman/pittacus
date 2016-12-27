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
#include "network.h"
#include <unistd.h>

static pt_socket_fd pt_socket(int ipv6, int type) {
    int domain = ipv6 ? AF_INET6 : AF_INET;
    return socket(domain, type, 0);
}

pt_socket_fd pt_socket_datagram(int ipv6) {
    return pt_socket(ipv6, SOCK_DGRAM);
}

pt_socket_fd pt_socket_stream(int ipv6) {
    return pt_socket(ipv6, SOCK_STREAM);
}

int pt_bind(pt_socket_fd fd, const pt_sockaddr_storage *addr, pt_socklen_t addr_len) {
    return bind(fd, (const struct sockaddr *) addr, addr_len);
}

int pt_listen(pt_socket_fd fd, int backlog) {
    return listen(fd, backlog);
}

pt_socket_fd pt_accept(pt_socket_fd fd, pt_sockaddr_storage *addr, pt_socklen_t *addr_len) {
    return accept(fd, (struct sockaddr *) addr, addr_len);
}

ssize_t pt_recv_from(pt_socket_fd fd, uint8_t *buffer, size_t buffer_size, pt_sockaddr_storage *addr, pt_socklen_t *addr_len) {
    return recvfrom(fd, buffer, buffer_size, MSG_WAITALL, (struct sockaddr *) addr, addr_len);
}

ssize_t pt_send_to(pt_socket_fd fd, const uint8_t *buffer, size_t buffer_size, const pt_sockaddr_storage *addr, pt_socklen_t addr_len) {
    return sendto(fd, buffer, buffer_size, 0, (const struct sockaddr *) addr, addr_len);
}

void pt_close(pt_socket_fd fd) {
    close(fd);
}
