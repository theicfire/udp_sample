#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

#include "common.h"

DroppedPacketMonitor packet_monitor;
void accept_packet(const packet p) {
  packet_monitor.add_packet(p.sequence_id);
  packet_monitor.count_dropped_packets();
}

bool send_host_restart(int sock_fd, struct sockaddr_in host_address) {
  printf("%s - Send host restart\n", timenow().c_str());
  // send an empty message to initiate frame transfer
  const uint32_t first_seq_id = htonl(0);
  int flags = 0;
  socklen_t addrlen = sizeof(host_address);
  if (sendto(sock_fd, &first_seq_id, sizeof(first_seq_id), flags,
             (struct sockaddr*)&host_address, addrlen) == -1) {
    perror("failed to send message to host");
    return false;
  }
  return true;
}

int main(const int argc, const char* argv[]) {
  // setup
  int sock_fd;
  if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("could not create socket");
    return 1;
  }
  
  if (argc != 3) {
    printf("Usage: ./client <ip_address> <port>\n");
    return 1;
  }

  const char* host_address_arg = argv[1];
  int port_arg = atoi(argv[2]);

  /* bind it to all local addresses and pick any port number */
  struct sockaddr_in client_address;
  memset(&client_address, 0, sizeof(client_address));

  client_address.sin_family = AF_INET;
  client_address.sin_addr.s_addr = htonl(INADDR_ANY);
  client_address.sin_port = htons(0);

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 100000;  // 100ms timeout
  if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror("Error");
  }

  if (bind(sock_fd, (struct sockaddr*)&client_address, sizeof(client_address)) <
      0) {
    perror("could not bind to port");
    return 1;
  }
  printf("client bind successful\n");

  struct sockaddr_in host_address;
  memset(&host_address, 0, sizeof(host_address));

  host_address.sin_family = AF_INET;
  host_address.sin_port = htons(port_arg);
  if (inet_aton(host_address_arg, &host_address.sin_addr) == 0) {
    perror("provided host address in invalid");
    return 1;
  }

  send_host_restart(sock_fd, host_address);

  socklen_t addrlen = sizeof(host_address);
  int flags = 0;
  Timer restart_timeout;
  // receive packets
  uint8_t buf[SERIALIZED_PACKET_SIZE];
  for (;;) {
    if (restart_timeout.getElapsedMilliseconds() > 3000) {
      restart_timeout.reset();
      send_host_restart(sock_fd, host_address);
      packet_monitor.reset();
    }
    const int recvlen = recvfrom(sock_fd, &buf, sizeof(packet), flags,
                                 (struct sockaddr*)&host_address, &addrlen);
    if (recvlen >= 0) {
      restart_timeout.reset();
      packet p = packet::deserialize_packet(buf);
      const uint32_t serialized_sequence_id = htonl(p.sequence_id);

      // ack packet
      if (sendto(sock_fd, &serialized_sequence_id,
                 sizeof(serialized_sequence_id), 0,
                 (struct sockaddr*)&host_address, addrlen) == -1) {
        perror("failed to send ack to host");
        return 1;
      }

      accept_packet(p);
    }
  }

  close(sock_fd);
  return 0;
}
