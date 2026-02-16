#ifndef DMX_DRIVER_H
#define DMX_DRIVER_H

#include <Arduino.h>
#include "driver/uart.h"
#include "config.h"

class DmxOutput {
public:
  bool begin(int txPin, int rxPin, int dePin, uart_port_t port = UART_NUM_2);
  void sendFrame(const uint8_t* data, uint16_t channels);
  void blackout();
  void end();

private:
  uart_port_t _port = UART_NUM_2;
  int _dePin = -1;
  bool _ok = false;
  void sendBreak();
};

#endif
