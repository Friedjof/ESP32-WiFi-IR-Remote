#pragma once
#include <ArduinoOTA.h>
#include <Arduino.h>

class OtaManager {
 public:
  void begin(const String& password);
  void loop();
};
