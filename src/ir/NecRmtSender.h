#pragma once

#include <stdint.h>
#include "diagnostics/DiagnosticLog.h"

#if defined(ARDUINO_XIAO_ESP32C3)
enum class NecRmtResult { Unavailable, Sent, Failed };

// Send one complete NEC frame using the ESP32-C3 RMT peripheral.
// The payload is already encoded in IRremoteESP8266's MSB-first form.
NecRmtResult sendNecRmt(uint64_t code, uint16_t bits, uint32_t frequency,
                        DiagnosticLog& log);
#endif
