#pragma once
#include <Arduino.h>
#include "Config.h"

enum class IrProtocol : uint8_t { Nec, Sony, Rc5, Rc6, Raw };

struct IrJob {
  IrProtocol protocol = IrProtocol::Nec;
  uint64_t code = 0;
  uint16_t bits = 0;
  uint32_t frequency = IR_DEFAULT_FREQUENCY;
  uint16_t timings[MAX_RAW_TIMINGS] = {};
  uint16_t length = 0;
  bool held = false;
  uint8_t dutyCycle = 50;
  char name[33] = {};
};
