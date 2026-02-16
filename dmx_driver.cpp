#include "dmx_driver.h"

bool DmxOutput::begin(int txPin, int rxPin, int dePin, uart_port_t port) {
  _port  = port;
  _dePin = dePin;

  if (_dePin >= 0) { pinMode(_dePin, OUTPUT); digitalWrite(_dePin, HIGH); }

  uart_config_t cfg = {};
  cfg.baud_rate  = DMX_BAUD;
  cfg.data_bits  = UART_DATA_8_BITS;
  cfg.parity     = UART_PARITY_DISABLE;
  cfg.stop_bits  = UART_STOP_BITS_2;
  cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
  cfg.source_clk = UART_SCLK_APB;

  if (uart_param_config(_port, &cfg) != ESP_OK) return false;
  if (uart_set_pin(_port, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) return false;
  if (uart_driver_install(_port, 1024, 1024, 0, NULL, 0) != ESP_OK) return false;

  _ok = true;
  Serial.printf("[DMX] TX pripravljen: UART%d, TX=GPIO%d, DE=GPIO%d\n", _port, txPin, _dePin);
  return true;
}

void DmxOutput::sendBreak() {
  // Pošlji 0x00 pri 96kHz → 9 bitov LOW (~93.7µs break) + 2 bita HIGH (~20.8µs MAB)
  uart_wait_tx_done(_port, pdMS_TO_TICKS(50));
  uart_set_baudrate(_port, DMX_BREAK_BAUD);
  const uint8_t brk = 0x00;
  uart_write_bytes(_port, (const char*)&brk, 1);
  uart_wait_tx_done(_port, pdMS_TO_TICKS(10));
  uart_set_baudrate(_port, DMX_BAUD);
}

void DmxOutput::sendFrame(const uint8_t* data, uint16_t channels) {
  if (!_ok) return;
  if (channels > DMX_MAX_CHANNELS) channels = DMX_MAX_CHANNELS;

  sendBreak();
  const uint8_t startCode = 0x00;
  uart_write_bytes(_port, (const char*)&startCode, 1);
  uart_write_bytes(_port, (const char*)data, channels);
}

void DmxOutput::blackout() {
  if (!_ok) return;
  uint8_t zeros[DMX_MAX_CHANNELS];
  memset(zeros, 0, sizeof(zeros));
  sendFrame(zeros, DMX_MAX_CHANNELS);
}

void DmxOutput::end() {
  if (!_ok) return;
  if (_dePin >= 0) digitalWrite(_dePin, LOW);
  uart_driver_delete(_port);
  _ok = false;
}
