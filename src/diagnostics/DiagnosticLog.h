#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// A small RAM-only event history for browser diagnostics. Never store credentials here.
class DiagnosticLog {
 public:
  void add(const char* category, const char* format, ...);
  void write(JsonDocument& output) const;

 private:
  static constexpr size_t kCapacity = 48;
  struct Entry {
    uint32_t id;
    uint32_t uptimeMs;
    char category[12];
    char message[112];
  };
  Entry entries_[kCapacity] = {};
  size_t next_ = 0;
  size_t count_ = 0;
  uint32_t nextId_ = 1;
};
