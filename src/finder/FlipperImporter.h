#pragma once
#include <ArduinoJson.h>

class FlipperImporter {
 public:
  static bool parse(const String& content, const String& id, const String& device,
                    JsonDocument& output, String& error);
};
