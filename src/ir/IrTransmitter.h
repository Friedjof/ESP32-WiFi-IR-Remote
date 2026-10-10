#pragma once
#include <IRsend.h>
#include "Types.h"

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
  IrTransmitter() : sender_(IR_TX_PIN) {}
  void begin() { sender_.begin(); }
  bool enqueue(const IrJob& job);
  void loop();
  uint8_t queued() const { return count_; }
  bool busy() const { return sending_; }

 private:
  DutyIrSend sender_;
  IrJob queue_[IR_QUEUE_SIZE];
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  bool sending_ = false;
  bool rc5Toggle_ = false;
  bool rc6Toggle_ = false;
};
