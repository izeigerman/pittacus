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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include "gossip.h"
#include "config.h"

const char DATA_MESSAGE[] = "Hi there";

void data_receiver(void *context, pittacus_gossip_t *gossip, const uint8_t *data, size_t data_size) {
    // This function is invoked every time when a new data arrives.
    printf("Data arrived: %s\n", data);
}

int main(int argc, char **argv) {
    struct sockaddr_in self_in;
    self_in.sin_family = AF_INET;
    self_in.sin_port = htons(65000);
    inet_aton("127.0.0.1", &self_in.sin_addr);

    // Filling in the address of the current node.
    pittacus_addr_t self_addr = {
        .addr = (const pt_sockaddr *) &self_in,
        .addr_len = sizeof(struct sockaddr_in)
    };

    // Create a new Pittacus descriptor instance.
    pittacus_gossip_t *gossip = pittacus_gossip_create(&self_addr, &data_receiver, NULL);
    if (gossip == NULL) {
        fprintf(stderr, "Gossip initialization failed: %s\n", strerror(errno));
        return -1;
    }

    // No seed nodes are provided.
    int join_result = pittacus_gossip_join(gossip, NULL, 0);
    if (join_result < 0) {
        fprintf(stderr, "Gossip join failed: %d\n", join_result);
        pittacus_gossip_destroy(gossip);
        return -1;
    }

    // Retrieve the socket descriptor.
    pt_socket_fd gossip_fd = pittacus_gossip_socket_fd(gossip);
    struct pollfd gossip_poll_fd = {
        .fd = gossip_fd,
        .events = POLLIN,
        .revents = 0
    };

    int poll_interval = GOSSIP_TICK_INTERVAL;
    int recv_result = 0;
    int send_result = 0;
    int poll_result = 0;
    while (1) {
        gossip_poll_fd.revents = 0;

        poll_result = poll(&gossip_poll_fd, 1, poll_interval);
        if (poll_result > 0) {
            if (gossip_poll_fd.revents & POLLERR) {
                fprintf(stderr, "Gossip socket failure: %s\n", strerror(errno));
                pittacus_gossip_destroy(gossip);
                return -1;
            } else if (gossip_poll_fd.revents & POLLIN) {
                // Tell Pittacus to read a message from the socket.
                recv_result = pittacus_gossip_process_receive(gossip);
                if (recv_result < 0) {
                    fprintf(stderr, "Gossip receive failed: %d\n", recv_result);
                    pittacus_gossip_destroy(gossip);
                    return -1;
                }
            }
        } else if (poll_result < 0) {
            fprintf(stderr, "Poll failed: %s\n", strerror(errno));
            pittacus_gossip_destroy(gossip);
            return -1;
        }
        // Try to trigger the Gossip tick event and recalculate
        // the poll interval.
        poll_interval = pittacus_gossip_tick(gossip);
        if (poll_interval < 0) {
            fprintf(stderr, "Gossip tick failed: %d\n", poll_interval);
            return -1;
        }
        // Tell Pittacus to write existing messages to the socket.
        send_result = pittacus_gossip_process_send(gossip);
        if (send_result < 0) {
            fprintf(stderr, "Gossip send failed: %d, %s\n", send_result, strerror(errno));
            pittacus_gossip_destroy(gossip);
            return -1;
        }
    }
    pittacus_gossip_destroy(gossip);

    return 0;
}
