#pragma once
#include <ArduinoOTA.h>
#include <Arduino.h>
#include "diagnostics/DiagnosticLog.h"

class OtaManager {
 public:
  explicit OtaManager(DiagnosticLog& log) : log_(log) {}
  void begin(const String& password);
  void loop();

 private:
  DiagnosticLog& log_;
  uint32_t rebootAt_ = 0;
};
