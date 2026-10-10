#pragma once
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "Config.h"
#include "Types.h"

class ConfigStorage {
 public:
  bool begin();
  JsonDocument& data() { return config_; }
  const JsonDocument& data() const { return config_; }
  bool replace(const String& json, String& error);
  bool save(String& error);
  bool create(const String& json, String& error);
  bool select(const String& id, String& error);
  bool remove(const String& id, String& error);
  void list(JsonDocument& output) const;
  const String& activeId() const { return activeId_; }
  bool makeJob(const String& name, IrJob& job, String& error) const;
  static bool makeJobFrom(JsonVariantConst command, uint32_t defaultFrequency,
                          const String& name, IrJob& job, String& error);
  bool hasCommand(const String& name) const;
  bool isConfigured(const String& name) const;
  uint32_t repeatInterval() const;
  uint32_t frequency() const;
  bool receiverEnabled() const;
  static bool validName(const String& name);

 private:
  JsonDocument config_;
  String activeId_;
  bool loadPath(const char* path, JsonDocument& target) const;
  bool loadProfile(const String& id, JsonDocument& target) const;
  bool writeProfile(const String& id, const JsonDocument& source, String& error);
  bool installFactories();
  bool migrateLegacy();
  size_t profileCount() const;
  bool validate(JsonDocument& doc, String& error) const;
};
