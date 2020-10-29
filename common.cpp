#include "common.h"

#include <arpa/inet.h>
#include <string.h>

std::string frame_to_string(const std::vector<uint8_t> frame) {
  std::string to_return = "[";
  for (uint8_t i : frame) {
    to_return += std::to_string(i) + ", ";
  }
  return to_return + "]";
}

std::string packet::to_string() const {
  return "frame_id: " + std::to_string(frame_id) + ", " +
         "num_packets: " + std::to_string(num_packets) + ", " +
         "sequence_id: " + std::to_string(sequence_id) + ", " +
         "data_length: " + std::to_string(data_length) + ", ";
}

ssize_t packet::send_packet(int sock_fd, const struct sockaddr* dest,
                            socklen_t addrlen) {
  uint8_t buf[SERIALIZED_PACKET_SIZE];
  memset(&buf, 0, sizeof(buf));

  uint16_t serialized_frame_id = htons(frame_id);
  memcpy(buf, &serialized_frame_id, 2);

  uint16_t serialized_num_packets = htons(num_packets);
  memcpy(buf + 2, &serialized_num_packets, 2);

  uint32_t serialized_sequence_id = htonl(sequence_id);
  memcpy(buf + 4, &serialized_sequence_id, 4);

  uint16_t serialized_data_length = htons(data_length);
  memcpy(buf + 8, &serialized_data_length, 2);

  memcpy(buf + 10, data, PACKET_CONTENT_SIZE);

  return sendto(sock_fd, (void*)buf, SERIALIZED_PACKET_SIZE, 0, dest, addrlen);
}

/* we only compare metadata, do not use == for data comparison */
bool operator==(const packet& a, const packet& b) {
  return a.frame_id == b.frame_id && a.num_packets == b.num_packets &&
         a.sequence_id == b.sequence_id;
}

packet packet::deserialize_packet(uint8_t buf[SERIALIZED_PACKET_SIZE]) {
  packet p;

  uint16_t network_frame_id;
  memcpy(&network_frame_id, buf, 2);
  p.frame_id = ntohs(network_frame_id);

  uint16_t network_num_packets;
  memcpy(&network_num_packets, buf + 2, 2);
  p.num_packets = ntohs(network_num_packets);

  uint32_t network_sequence_id;
  memcpy(&network_sequence_id, buf + 4, 4);
  p.sequence_id = ntohl(network_sequence_id);

  uint16_t network_data_length;
  memcpy(&network_data_length, buf + 8, 2);
  p.data_length = ntohs(network_data_length);

  memcpy(p.data, buf + 10, PACKET_CONTENT_SIZE);

  return p;
}

std::string timenow() {
  time_t now = time(0);

  char* tn = strtok(ctime(&now), "\n");
  std::string ret(tn);
  return ret;
}

void DroppedPacketMonitor::add_packet(uint32_t seq_id) {
  if (current_seq_id == 0xFFFFFFFF) {
    current_seq_id = seq_id;
    last_counted_seq_id = seq_id;
  }
  // printf("Got packet: %d\n", p.sequence_id);
  uint32_t expected_seq_id = current_seq_id + 1;
  for (uint32_t i = expected_seq_id; i < seq_id; i++) {
    // Insert potential dropped packets
    if (packet_seq_ids.count(i) == 0) {
      drops.insert(i);
    }
  }
  current_seq_id = seq_id;
  packet_seq_ids.insert(current_seq_id);
  drops.erase(current_seq_id);
  if (print_timeout.getElapsedMilliseconds() > 1000) {
    print_timeout.reset();
    int valid_packets =
        current_seq_id - DROP_SEQ_ID_BUFFER - last_counted_seq_id;
    if (valid_packets > 0) {
      int drop_count = count_dropped_packets();
      if (drop_count == 0) {
        printf("%s - Received all %d packets\n", timenow().c_str(),
               valid_packets);
      } else {
        printf("%s - Dropped %d/%d packets\n", timenow().c_str(), drop_count,
               valid_packets);
      }
      last_counted_seq_id = current_seq_id - DROP_SEQ_ID_BUFFER;
    }
  }
}

int DroppedPacketMonitor::count_dropped_packets() {
  int drop_count = 0;

  for (auto it = drops.begin(); it != drops.end();) {
    if (*it < current_seq_id - DROP_SEQ_ID_BUFFER) {
      // printf("Dropped packet %d\n", *it);
      drops.erase(it++);
      drop_count += 1;
    } else {
      ++it;
    }
  }

  for (auto it = packet_seq_ids.begin(); it != packet_seq_ids.end();) {
    if (*it < current_seq_id - DROP_SEQ_ID_BUFFER) {
      packet_seq_ids.erase(it++);
    } else {
      ++it;
    }
  }
  return drop_count;
}

void DroppedPacketMonitor::reset() {
  current_seq_id = 0xFFFFFFFF;
  last_counted_seq_id = 0;
  drops.clear();
  packet_seq_ids.clear();
}