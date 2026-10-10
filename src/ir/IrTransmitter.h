#pragma once
#include <IRsend.h>
#include "Types.h"

class IrTransmitter {
 public:
  IrTransmitter() : sender_(IR_TX_PIN) {}
  void begin() { sender_.begin(); }
  bool enqueue(const IrJob& job);
  void loop();
  uint8_t queued() const { return count_; }
  bool busy() const { return sending_; }

 private:
  IRsend sender_;
  IrJob queue_[IR_QUEUE_SIZE];
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint8_t count_ = 0;
  bool sending_ = false;
};
