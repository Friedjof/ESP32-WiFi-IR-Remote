#pragma once
#include <ArduinoJson.h>
#include "storage/ConfigStorage.h"

class CandidateCatalog {
 public:
  bool begin();
  void list(JsonDocument& output) const;
  bool makeTestJob(const String& id, const String& command, IrJob& job, String& error) const;
  bool record(const String& id, const String& command, bool worked, String& error);
  bool clearFeedback(const String& id, String& error);
  bool activate(const String& id, ConfigStorage& config, String& error);
  bool exists(const String& id) const;
  bool safeCommand(const String& command) const;
 private:
  JsonDocument catalog_;
  JsonDocument feedback_;
  JsonVariantConst find(const String& id) const;
  bool saveFeedback(String& error);
};
