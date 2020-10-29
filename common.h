#include <sys/socket.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "timer.h"

#define PORT 5940

#define PACKET_CONTENT_SIZE 1300
#define SERIALIZED_PACKET_SIZE PACKET_CONTENT_SIZE + 10

std::string frame_to_string(const std::vector<uint8_t>);

class packet {
 public:
  // metadata
  uint16_t
      frame_id;  // effectively limits how many outstanding frames we can have
  uint16_t num_packets;  // num_packets = 0 is defined as invalid packet
  uint32_t sequence_id;

  // content
  uint16_t data_length;
  uint8_t data[PACKET_CONTENT_SIZE];

  // methods
  std::string to_string() const;
  ssize_t send_packet(int, const struct sockaddr *, socklen_t);
  static packet deserialize_packet(uint8_t buf[SERIALIZED_PACKET_SIZE]);
};

bool operator==(const packet &, const packet &);

std::string timenow();

class DroppedPacketMonitor {
  uint32_t current_seq_id = 0xFFFFFFFF;
  std::set<uint32_t> drops;
  std::set<uint32_t> packet_seq_ids;
  uint32_t last_counted_seq_id = 0;
  const int DROP_SEQ_ID_BUFFER = 2;
  Timer print_timeout;

 public:
  void add_packet(uint32_t seq_id);
  int count_dropped_packets();
  void reset();
};
