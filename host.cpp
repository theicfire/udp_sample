#include <arpa/inet.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <iterator>
#include <map>
#include <random>
#include <set>
#include <thread>
#include <tuple>
#include <vector>

#include "common.h"

#define TIMEOUT 100
#define TEST_FRAME_SIZE 10000
#define MAX_OUTSTANDING_FRAMES 5
#define SECONDS_TO_PAUSE 10

#define REQUEST_NEXT_FRAME_DELAY 500
struct sockaddr_in client_address;
socklen_t addrlen = sizeof(client_address);

DroppedPacketMonitor packet_monitor;

void run_ack_recv(int sock_fd) {
  uint32_t recv_sequence_id = 0;
  struct pollfd pfds[1];
  pfds[0].fd = sock_fd;
  pfds[0].events = POLLIN;
  while (1) {
    while (poll(pfds, 1, TIMEOUT)) {
      if (pfds[0].revents & POLLIN) {
        const int recvlen =
            recvfrom(sock_fd, &recv_sequence_id, sizeof(recv_sequence_id), 0,
                     (struct sockaddr *)&client_address, &addrlen);
        if (recvlen != sizeof(recv_sequence_id)) {
          printf("Error: Failed to receive ack. Len is %d\n", recvlen);
          continue;
        }
        recv_sequence_id = ntohl(recv_sequence_id);
        if (recv_sequence_id == 0) {
          printf(
              "Client sent first packet. Client at address: %s and port %d\n",
              inet_ntoa(client_address.sin_addr), client_address.sin_port);
          packet_monitor.reset();
        } else {
          packet_monitor.add_packet(recv_sequence_id);
          packet_monitor.count_dropped_packets();
        }
      }
    }
  }
}

bool send_packet(int sock_fd, int sequence_id) {
  // Send packet..
  packet p;
  memset(&p, 0, sizeof(p));
  p.frame_id = 3;
  p.num_packets = 0;
  p.sequence_id = sequence_id;
  if (p.send_packet(sock_fd, (struct sockaddr *)&client_address, addrlen) ==
      -1) {
    perror("could not send packet");
    return false;
  }
  return true;
}

int main(const int argc, const char *argv[]) {
  // setup
  int sock_fd;
  if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("could not create socket");
    return 1;
  }

  struct sockaddr_in host_address;
  memset(&host_address, 0, sizeof(host_address));

  host_address.sin_family = AF_INET;
  host_address.sin_addr.s_addr = htonl(INADDR_ANY);
  host_address.sin_port = htons(PORT);

  if (bind(sock_fd, (struct sockaddr *)&host_address, sizeof(host_address)) <
      0) {
    perror("could not bind to port");
    return 1;
  }

  uint32_t sequence_id = 1;
  std::thread recv_thread(run_ack_recv, sock_fd);
  printf("waiting for client...\n");
  for (;;) {
    if (client_address.sin_port == 0) {
      sleep(1);
      continue;
    }
    if (!send_packet(sock_fd, sequence_id)) {
      return 1;
    }
    sequence_id += 1;
    usleep(100 * 1000);
  }
  close(sock_fd);
  return 0;
}
