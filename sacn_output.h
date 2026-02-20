#ifndef SACN_OUTPUT_H
#define SACN_OUTPUT_H

#include <WiFiUdp.h>

#define SACN_PORT 5568

class SacnOutput {
public:
  void begin(uint16_t universe);
  void sendFrame(const uint8_t* data, uint16_t channels);
  void setUniverse(uint16_t universe);

private:
  WiFiUDP _udp;
  uint8_t _packet[638];  // Max sACN packet: 126 header + 512 DMX
  uint16_t _universe = 1;
  uint8_t _sequence = 0;
  bool _begun = false;
  void _buildHeader();
};

#endif
