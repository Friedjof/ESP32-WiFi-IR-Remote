#pragma once
#include <IRsend.h>
#include "Types.h"
#include "diagnostics/DiagnosticLog.h"

class DutyIrSend : public IRsend {
 public:
  explicit DutyIrSend(uint16_t pin) : IRsend(pin) {}
  void sendRawWithDuty(const uint16_t* timings, uint16_t length, uint16_t frequency, uint8_t duty) {
    enableIROut(frequency, duty);
    for (uint16_t i = 0; i < length; ++i) {
      if (i & 1) space(timings[i]);
      else mark(timings[i]);
    }
    ledOff();
  }
};

class IrTransmitter {
 public:
  explicit IrTransmitter(DiagnosticLog& log) : log_(log), sender_(IR_TX_PIN) {}
  void begin() {
    sender_.begin();
#if defined(ARDUINO_XIAO_ESP32C3)
    // The library's default ESP32 PWM offset was measured on a 240 MHz board.
    // Calibrate the 160 MHz XIAO once before WiFi starts transmitting.
    const int8_t offset = sender_.calibrate(IR_DEFAULT_FREQUENCY);
    log_.add("ir", "Carrier calibrated at %lu Hz: %d us offset",
             static_cast<unsigned long>(IR_DEFAULT_FREQUENCY), static_cast<int>(offset));
#endif
    log_.add("ir", "Transmitter ready on GPIO%u", static_cast<unsigned>(IR_TX_PIN));
  }
  bool enqueue(const IrJob& job);
  void loop();
  uint8_t queued() const { return count_; }
  bool busy() const { return sending_; }

 private:
  DiagnosticLog& log_;
  DutyIrSend sender_;
  IrJob queue_[IR_QUEUE_SIZE];
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  bool sending_ = false;
  bool rc5Toggle_ = false;
  bool rc6Toggle_ = false;
};
