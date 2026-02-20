#include "sacn_output.h"
#include <WiFi.h>

// E1.31 (sACN) packet structure:
// Root Layer (38 bytes) + Framing Layer (77 bytes) + DMP Layer (11 bytes header) + DMX data
// Total header = 126 bytes, then up to 512 bytes of DMX data

void SacnOutput::begin(uint16_t universe) {
  _universe = universe;
  _sequence = 0;
  _buildHeader();
  _udp.begin(SACN_PORT);
  _begun = true;
}

void SacnOutput::setUniverse(uint16_t universe) {
  _universe = universe;
  // Update universe in framing layer
  _packet[113] = (universe >> 8) & 0xFF;
  _packet[114] = universe & 0xFF;
}

void SacnOutput::_buildHeader() {
  memset(_packet, 0, sizeof(_packet));

  // ---- Root Layer (bytes 0-37) ----
  // Preamble size (2 bytes, big-endian)
  _packet[0] = 0x00; _packet[1] = 0x10;  // 16

  // Post-amble size (2 bytes)
  _packet[2] = 0x00; _packet[3] = 0x00;

  // ACN Packet Identifier (12 bytes)
  static const uint8_t acnId[] = {0x41,0x53,0x43,0x2d,0x45,0x31,0x2e,0x31,0x37,0x00,0x00,0x00};
  memcpy(_packet + 4, acnId, 12);

  // Flags + Length for Root Layer (2 bytes) — updated per frame
  // _packet[16], _packet[17] set in sendFrame

  // Vector (4 bytes) — VECTOR_ROOT_E131_DATA = 0x00000004
  _packet[18] = 0x00; _packet[19] = 0x00;
  _packet[20] = 0x00; _packet[21] = 0x04;

  // CID (16 bytes) — use MAC address + padding
  uint8_t mac[6];
  WiFi.macAddress(mac);
  memcpy(_packet + 22, mac, 6);
  // Remaining 10 bytes of CID stay 0

  // ---- Framing Layer (bytes 38-114) ----
  // Flags + Length (2 bytes) — updated per frame
  // _packet[38], _packet[39] set in sendFrame

  // Vector (4 bytes) — VECTOR_E131_DATA_PACKET = 0x00000002
  _packet[40] = 0x00; _packet[41] = 0x00;
  _packet[42] = 0x00; _packet[43] = 0x02;

  // Source Name (64 bytes)
  const char* srcName = "ESP32 DMX Node";
  strncpy((char*)(_packet + 44), srcName, 63);

  // Priority (1 byte)
  _packet[108] = 100;  // Default priority

  // Synchronization Address (2 bytes)
  _packet[109] = 0x00; _packet[110] = 0x00;

  // Sequence Number (1 byte) — updated per frame
  // _packet[111]

  // Options (1 byte)
  _packet[112] = 0x00;

  // Universe (2 bytes, big-endian)
  _packet[113] = (_universe >> 8) & 0xFF;
  _packet[114] = _universe & 0xFF;

  // ---- DMP Layer (bytes 115-125, then DMX data from 126) ----
  // Flags + Length (2 bytes) — updated per frame
  // _packet[115], _packet[116] set in sendFrame

  // Vector (1 byte) — VECTOR_DMP_SET_PROPERTY = 0x02
  _packet[117] = 0x02;

  // Address Type & Data Type (1 byte)
  _packet[118] = 0xA1;

  // First Property Address (2 bytes, big-endian)
  _packet[119] = 0x00; _packet[120] = 0x00;

  // Address Increment (2 bytes, big-endian)
  _packet[121] = 0x00; _packet[122] = 0x01;

  // Property value count (2 bytes) — updated per frame
  // _packet[123], _packet[124] set in sendFrame

  // DMX512 Start Code (byte 125)
  _packet[125] = 0x00;

  // DMX data starts at byte 126
}

void SacnOutput::sendFrame(const uint8_t* data, uint16_t channels) {
  if (!_begun) return;
  if (channels > 512) channels = 512;

  // Property value count = channels + 1 (start code)
  uint16_t propCount = channels + 1;

  // DMP Layer length = 10 + propCount (flags+len=2, vector=1, addrType=1, firstAddr=2, addrInc=2, propCount=2, then data)
  uint16_t dmpLen = 10 + propCount;

  // Framing Layer length = 77 + propCount (from byte 38 to end)
  uint16_t framingLen = 77 + propCount;

  // Root Layer length = 22 + framingLen (from byte 16 to end)
  uint16_t rootLen = 22 + framingLen;

  // Set Flags + Length fields (0x7000 | length)
  _packet[16] = 0x70 | ((rootLen >> 8) & 0x0F);
  _packet[17] = rootLen & 0xFF;

  _packet[38] = 0x70 | ((framingLen >> 8) & 0x0F);
  _packet[39] = framingLen & 0xFF;

  _packet[115] = 0x70 | ((dmpLen >> 8) & 0x0F);
  _packet[116] = dmpLen & 0xFF;

  // Property value count
  _packet[123] = (propCount >> 8) & 0xFF;
  _packet[124] = propCount & 0xFF;

  // Sequence
  _packet[111] = _sequence++;

  // Copy DMX data (after start code at byte 125)
  memcpy(_packet + 126, data, channels);

  // Multicast address: 239.255.UHI.ULO
  IPAddress multicast(239, 255, (_universe >> 8) & 0xFF, _universe & 0xFF);

  _udp.beginPacket(multicast, SACN_PORT);
  _udp.write(_packet, 126 + channels);
  _udp.endPacket();
}
