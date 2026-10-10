#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class FlipperButtonLayout {
 public:
  static String commandKey(const String& sourceName);
  static bool arrange(JsonDocument& profile, String& error);
};
