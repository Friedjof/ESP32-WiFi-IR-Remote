#pragma once
#include <Arduino.h>

#ifndef IR_TX_PIN
#define IR_TX_PIN 18
#endif
#ifndef IR_RX_PIN
#define IR_RX_PIN 27
#endif
#ifndef IR_ENABLE_RX
#define IR_ENABLE_RX 1
#endif
#ifndef IR_DEFAULT_FREQUENCY
#define IR_DEFAULT_FREQUENCY 38000
#endif
#ifndef DEVICE_HOSTNAME
#define DEVICE_HOSTNAME "tv-remote"
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

constexpr size_t MAX_CONFIG_BYTES = 24576;
constexpr size_t MAX_RAW_TIMINGS = 300;
constexpr uint8_t IR_QUEUE_SIZE = 4;
constexpr uint32_t LEARN_TIMEOUT_MS = 20000;
