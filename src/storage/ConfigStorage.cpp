#include "ConfigStorage.h"
#include <Preferences.h>
#include "generated/default_profiles.h"

namespace {
constexpr size_t MAX_PROFILES = 8;

bool parseHex(const char* value, uint64_t& output) {
  if (!value || !*value) return false;
  if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) value += 2;
  if (!*value) return false;
  uint64_t result = 0;
  size_t digits = 0;
  for (; *value; ++value) {
    char c = *value;
    uint8_t n;
    if (c >= '0' && c <= '9') n = c - '0';
    else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
    else return false;
    if (++digits > 16) return false;
    result = (result << 4) | n;
  }
  output = result;
  return true;
}

String profilePath(const String& id, const char* extension = ".json") {
  return "/remotes/" + id + extension;
}
}

bool ConfigStorage::begin() {
  if (!LittleFS.begin(false)) {
    Preferences prefs;
    prefs.begin("tvremote_fs", false);
    bool initialized = prefs.getBool("ready", false);
    if (initialized || !LittleFS.begin(true)) {
      prefs.end();
      Serial.println("ERROR: LittleFS mount failed; saved profiles were not erased");
      return false;
    }
    prefs.putBool("ready", true);
    prefs.end();
  } else {
    Preferences prefs;
    prefs.begin("tvremote_fs", false);
    prefs.putBool("ready", true);
    prefs.end();
  }
  if (!LittleFS.exists("/remotes") && !LittleFS.mkdir("/remotes")) return false;
  // A power loss after renaming the current file can leave only the backup.
  String backupIds[MAX_PROFILES];
  size_t backupCount = 0;
  File recoveryDir = LittleFS.open("/remotes");
  File recoveryFile = recoveryDir.openNextFile();
  while (recoveryFile) {
    String name = recoveryFile.name();
    recoveryFile.close();
    if (name.endsWith(".bak") && backupCount < MAX_PROFILES) {
      String id = name.substring(name.lastIndexOf('/') + 1, name.length() - 4);
      if (validName(id)) backupIds[backupCount++] = id;
    }
    recoveryFile = recoveryDir.openNextFile();
  }
  recoveryDir.close();
  for (size_t i = 0; i < backupCount; ++i) {
    String id = backupIds[i];
    if (LittleFS.exists(profilePath(id))) continue;
    JsonDocument candidate;
    String error;
    if (loadPath(profilePath(id, ".bak").c_str(), candidate) && validate(candidate, error) &&
        candidate["id"].as<String>() == id) {
      LittleFS.rename(profilePath(id, ".bak"), profilePath(id));
    }
  }
  if (!profileCount()) {
    if (!migrateLegacy()) Serial.println("No legacy configuration to migrate");
    if (!installFactories()) return false;
  }
  Preferences prefs;
  prefs.begin("tvremote", false);
  String preferred = prefs.getString("profile", "toshiba_tv");
  prefs.end();
  String error;
  if (select(preferred, error)) return true;
  File dir = LittleFS.open("/remotes");
  File file = dir.openNextFile();
  while (file) {
    String name = file.name();
    file.close();
    if (name.endsWith(".json")) {
      String id = name.substring(name.lastIndexOf('/') + 1, name.length() - 5);
      if (validName(id) && select(id, error)) { dir.close(); return true; }
    }
    file = dir.openNextFile();
  }
  dir.close();
  Serial.printf("ERROR: No valid remote profile: %s\n", error.c_str());
  return false;
}

bool ConfigStorage::migrateLegacy() {
  JsonDocument legacy;
  if (!loadPath("/config.json", legacy) && !loadPath("/config.bak", legacy)) return false;
  if (!legacy["commands"].is<JsonObject>()) return false;
  JsonDocument profile;
  for (const FactoryProfile& factory : factoryProfiles) {
    if (strcmp(factory.id, "toshiba_tv") == 0) {
      if (deserializeJson(profile, factory.json)) return false;
      break;
    }
  }
  if (!profile.is<JsonObject>()) return false;
  for (const char* key : {"device", "default_frequency", "repeat_interval_ms", "receiver_enabled"}) {
    if (!legacy[key].isNull()) profile[key].set(legacy[key].as<JsonVariantConst>());
  }
  for (JsonPairConst entry : legacy["commands"].as<JsonObjectConst>()) {
    profile["commands"][entry.key().c_str()].set(entry.value());
  }
  String error;
  if (!validate(profile, error) || !writeProfile("toshiba_tv", profile, error)) {
    Serial.printf("Legacy migration failed: %s\n", error.c_str());
    return false;
  }
  Serial.println("Migrated legacy IR codes to /remotes/toshiba_tv.json");
  return true;
}

bool ConfigStorage::installFactories() {
  for (const FactoryProfile& factory : factoryProfiles) {
    String id = factory.id;
    if (LittleFS.exists(profilePath(id))) continue;
    JsonDocument profile;
    String error;
    if (deserializeJson(profile, factory.json) || !validate(profile, error) ||
        !writeProfile(id, profile, error)) {
      Serial.printf("Factory profile %s failed: %s\n", factory.id, error.c_str());
      return false;
    }
  }
  return true;
}

bool ConfigStorage::loadPath(const char* path, JsonDocument& target) const {
  File file = LittleFS.open(path, "r");
  if (!file || file.size() > MAX_CONFIG_BYTES) return false;
  JsonDocument candidate;
  DeserializationError parseError = deserializeJson(candidate, file);
  file.close();
  if (parseError || !candidate.is<JsonObject>()) return false;
  target.clear();
  target.set(candidate.as<JsonVariantConst>());
  return true;
}

bool ConfigStorage::loadProfile(const String& id, JsonDocument& target) const {
  if (!validName(id)) return false;
  for (const char* extension : {".json", ".bak"}) {
    JsonDocument candidate;
    String path = profilePath(id, extension);
    String error;
    if (!loadPath(path.c_str(), candidate) || candidate["id"].as<String>() != id ||
        !validate(candidate, error)) continue;
    target.clear();
    target.set(candidate.as<JsonVariantConst>());
    return true;
  }
  return false;
}

bool ConfigStorage::validName(const String& name) {
  if (name.isEmpty() || name.length() > 32) return false;
  for (char c : name) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
  return true;
}

bool ConfigStorage::validate(JsonDocument& doc, String& error) const {
  if (!doc.is<JsonObject>()) { error = "Root must be an object"; return false; }
  if (!doc["id"].is<const char*>() || !validName(doc["id"].as<String>())) { error = "Invalid profile id"; return false; }
  if (!doc["device"].is<const char*>() || strlen(doc["device"].as<const char*>()) > 80 ||
      !strlen(doc["device"].as<const char*>())) { error = "Invalid device"; return false; }
  if (!doc["default_frequency"].is<uint32_t>()) { error = "Invalid default_frequency"; return false; }
  uint32_t freq = doc["default_frequency"].as<uint32_t>();
  if (freq < 30000 || freq > 60000) { error = "Frequency must be 30000..60000 Hz"; return false; }
  if (!doc["repeat_interval_ms"].is<uint32_t>()) { error = "Invalid repeat_interval_ms"; return false; }
  uint32_t repeat = doc["repeat_interval_ms"].as<uint32_t>();
  if (repeat < 120 || repeat > 1000) { error = "Repeat interval must be 120..1000 ms"; return false; }
  if (!doc["receiver_enabled"].is<bool>()) { error = "Invalid receiver_enabled"; return false; }
  if (!doc["commands"].is<JsonObject>()) { error = "Missing commands object"; return false; }
  JsonObject commands = doc["commands"].as<JsonObject>();
  if (!commands.size() || commands.size() > 96) { error = "Command count must be 1..96"; return false; }
  for (JsonPair entry : commands) {
    String name = entry.key().c_str();
    if (!validName(name) || !entry.value().is<JsonObject>()) { error = "Invalid command: " + name; return false; }
    JsonObject cmd = entry.value().as<JsonObject>();
    const char* proto = cmd["protocol"] | "";
    if (strcmp(proto, "NEC") && strcmp(proto, "SONY") && strcmp(proto, "RC5") && strcmp(proto, "RC6") && strcmp(proto, "RAW")) {
      error = "Unsupported protocol: " + name; return false;
    }
    uint32_t commandFreq = cmd["frequency"] | freq;
    if (commandFreq < 30000 || commandFreq > 60000) { error = "Invalid frequency: " + name; return false; }
    if (!strcmp(proto, "RAW")) {
      if (!cmd["timings"].is<JsonArray>()) { error = "RAW needs timings: " + name; return false; }
      JsonArray timings = cmd["timings"].as<JsonArray>();
      if (timings.size() > MAX_RAW_TIMINGS) { error = "RAW too long: " + name; return false; }
      uint32_t total = 0;
      for (JsonVariant timing : timings) {
        if (!timing.is<uint16_t>() || timing.as<uint16_t>() < 1) { error = "Invalid RAW timing: " + name; return false; }
        total += timing.as<uint16_t>();
      }
      if (total > 300000) { error = "RAW signal too long: " + name; return false; }
    } else {
      if (!cmd["bits"].is<uint16_t>()) { error = "Invalid bits: " + name; return false; }
      uint16_t bits = cmd["bits"].as<uint16_t>();
      if (bits < 1 || bits > 64) { error = "Bits out of range: " + name; return false; }
      if (!cmd["code"].isNull()) {
        uint64_t value;
        if (!cmd["code"].is<const char*>() || !parseHex(cmd["code"].as<const char*>(), value)) { error = "Code must be a hex string: " + name; return false; }
        if (bits < 64 && (value >> bits)) { error = "Code exceeds bit width: " + name; return false; }
      }
    }
  }
  if (!doc["pages"].is<JsonArray>()) { error = "Missing pages"; return false; }
  JsonArray pages = doc["pages"].as<JsonArray>();
  if (!pages.size() || pages.size() > 6) { error = "Page count must be 1..6"; return false; }
  size_t visibleButtons = 0;
  String seenPages = ",";
  for (JsonObject page : pages) {
    String pageId = page["id"].as<String>();
    if (!validName(pageId) || !page["label"].is<const char*>() ||
        strlen(page["label"].as<const char*>()) > 24 || !page["groups"].is<JsonArray>()) {
      error = "Invalid page"; return false;
    }
    if (seenPages.indexOf("," + pageId + ",") >= 0) { error = "Duplicate page id"; return false; }
    seenPages += pageId + ",";
    JsonArray groups = page["groups"].as<JsonArray>();
    if (!groups.size() || groups.size() > 12) { error = "Invalid group count"; return false; }
    for (JsonObject group : groups) {
      if (!group["label"].is<const char*>() || strlen(group["label"].as<const char*>()) > 32 ||
          !group["columns"].is<uint8_t>() || !group["buttons"].is<JsonArray>()) {
        error = "Invalid group"; return false;
      }
      uint8_t columns = group["columns"].as<uint8_t>();
      if (columns < 1 || columns > 4) { error = "Columns must be 1..4"; return false; }
      JsonArray buttons = group["buttons"].as<JsonArray>();
      if (!buttons.size()) { error = "Group has no buttons"; return false; }
      for (JsonObject button : buttons) {
        ++visibleButtons;
        String command = button["command"].as<String>();
        if (!validName(command) || !commands[command].is<JsonObject>() ||
            !button["label"].is<const char*>() || strlen(button["label"].as<const char*>()) > 32) {
          error = "Button needs an existing command and short label"; return false;
        }
        if (!button["icon"].isNull() && (!button["icon"].is<const char*>() || strlen(button["icon"].as<const char*>()) > 20)) { error = "Invalid icon"; return false; }
        if (!button["tone"].isNull() && (!button["tone"].is<const char*>() || strlen(button["tone"].as<const char*>()) > 12)) { error = "Invalid tone"; return false; }
        if (!button["repeat"].isNull() && !button["repeat"].is<bool>()) { error = "Invalid repeat flag"; return false; }
        if (!button["span"].isNull() && !button["span"].is<uint8_t>()) { error = "Invalid button span"; return false; }
        if (!button["column"].isNull() && !button["column"].is<uint8_t>()) { error = "Invalid button column"; return false; }
        if (!button["row"].isNull() && (!button["row"].is<uint8_t>() || button["row"].as<uint8_t>() < 1 || button["row"].as<uint8_t>() > 12)) { error = "Invalid button row"; return false; }
        uint8_t span = button["span"] | 1;
        uint8_t column = button["column"] | 1;
        if (span < 1 || span > columns || column < 1 || column + span - 1 > columns) { error = "Invalid button span or column"; return false; }
      }
    }
  }
  if (visibleButtons > 96) { error = "Too many visible buttons"; return false; }
  return true;
}

size_t ConfigStorage::profileCount() const {
  size_t count = 0;
  File dir = LittleFS.open("/remotes");
  File file = dir.openNextFile();
  while (file) {
    String name = file.name();
    if (name.endsWith(".json")) ++count;
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
  return count;
}

bool ConfigStorage::writeProfile(const String& id, const JsonDocument& source, String& error) {
  String path = profilePath(id);
  String temp = profilePath(id, ".tmp");
  String backup = profilePath(id, ".bak");
  if (measureJson(source) > MAX_CONFIG_BYTES) { error = "Profile is too large"; return false; }
  File file = LittleFS.open(temp, "w");
  if (!file) { error = "Cannot open temporary file"; return false; }
  size_t written = serializeJson(source, file);
  file.flush();
  size_t size = file.size();
  file.close();
  if (!written || written != measureJson(source) || size != written) {
    LittleFS.remove(temp); error = "Cannot write complete profile"; return false;
  }
  LittleFS.remove(backup);
  if (LittleFS.exists(path) && !LittleFS.rename(path, backup)) {
    LittleFS.remove(temp); error = "Cannot back up profile"; return false;
  }
  if (!LittleFS.rename(temp, path)) {
    LittleFS.rename(backup, path);
    error = "Cannot commit profile"; return false;
  }
  return true;
}

bool ConfigStorage::save(String& error) {
  if (activeId_.isEmpty()) { error = "No active profile"; return false; }
  return writeProfile(activeId_, config_, error);
}

bool ConfigStorage::replace(const String& json, String& error) {
  if (json.length() > MAX_CONFIG_BYTES) { error = "Profile is too large"; return false; }
  JsonDocument candidate;
  DeserializationError parseError = deserializeJson(candidate, json);
  if (parseError) { error = parseError.c_str(); return false; }
  if (!validate(candidate, error)) return false;
  if (candidate["id"].as<String>() != activeId_) { error = "Profile id cannot be changed"; return false; }
  if (!writeProfile(activeId_, candidate, error)) return false;
  config_.clear();
  config_.set(candidate.as<JsonVariantConst>());
  return true;
}

bool ConfigStorage::create(const String& json, String& error) {
  if (json.length() > MAX_CONFIG_BYTES) { error = "Profile is too large"; return false; }
  JsonDocument candidate;
  DeserializationError parseError = deserializeJson(candidate, json);
  if (parseError) { error = parseError.c_str(); return false; }
  if (!validate(candidate, error)) return false;
  String id = candidate["id"].as<String>();
  if (LittleFS.exists(profilePath(id))) { error = "Profile id already exists"; return false; }
  if (profileCount() >= MAX_PROFILES) { error = "Maximum of eight profiles reached"; return false; }
  if (!writeProfile(id, candidate, error)) return false;
  return select(id, error);
}

bool ConfigStorage::select(const String& id, String& error) {
  JsonDocument candidate;
  if (!loadProfile(id, candidate)) { error = "Profile not found or invalid"; return false; }
  JsonDocument primary;
  String validationError;
  if ((!loadPath(profilePath(id).c_str(), primary) || !validate(primary, validationError)) &&
      LittleFS.exists(profilePath(id, ".bak"))) {
    LittleFS.remove(profilePath(id));
    LittleFS.rename(profilePath(id, ".bak"), profilePath(id));
  }
  config_.clear();
  config_.set(candidate.as<JsonVariantConst>());
  activeId_ = id;
  Preferences prefs;
  prefs.begin("tvremote", false);
  prefs.putString("profile", id);
  prefs.end();
  return true;
}

bool ConfigStorage::remove(const String& id, String& error) {
  if (!validName(id) || !LittleFS.exists(profilePath(id))) { error = "Profile not found"; return false; }
  if (profileCount() < 2) { error = "Keep at least one profile"; return false; }
  if (id == activeId_) {
    File dir = LittleFS.open("/remotes");
    File file = dir.openNextFile();
    bool switched = false;
    while (file) {
      String name = file.name();
      file.close();
      if (name.endsWith(".json")) {
        String other = name.substring(name.lastIndexOf('/') + 1, name.length() - 5);
        if (other != id && select(other, error)) { switched = true; break; }
      }
      file = dir.openNextFile();
    }
    dir.close();
    if (!switched) { error = "No other valid profile"; return false; }
  }
  if (!LittleFS.remove(profilePath(id))) { error = "Cannot delete profile"; return false; }
  LittleFS.remove(profilePath(id, ".bak"));
  return true;
}

void ConfigStorage::list(JsonDocument& output) const {
  output["active"] = activeId_;
  JsonArray entries = output["profiles"].to<JsonArray>();
  File dir = LittleFS.open("/remotes");
  File file = dir.openNextFile();
  while (file) {
    String name = file.name();
    file.close();
    if (name.endsWith(".json")) {
      String id = name.substring(name.lastIndexOf('/') + 1, name.length() - 5);
      JsonDocument profile;
      if (loadProfile(id, profile)) {
        JsonObject entry = entries.add<JsonObject>();
        entry["id"] = id;
        entry["device"] = profile["device"].as<String>();
        entry["pages"] = profile["pages"].as<JsonArrayConst>().size();
        entry["buttons"] = 0;
        for (JsonObjectConst page : profile["pages"].as<JsonArrayConst>()) {
          for (JsonObjectConst group : page["groups"].as<JsonArrayConst>()) {
            entry["buttons"] = entry["buttons"].as<size_t>() + group["buttons"].as<JsonArrayConst>().size();
          }
        }
      }
    }
    file = dir.openNextFile();
  }
  dir.close();
}

bool ConfigStorage::hasCommand(const String& name) const {
  return validName(name) && config_["commands"][name].is<JsonObjectConst>();
}

bool ConfigStorage::isConfigured(const String& name) const {
  if (!hasCommand(name)) return false;
  JsonVariantConst cmd = config_["commands"][name];
  const char* protocol = cmd["protocol"] | "";
  return !strcmp(protocol, "RAW") ? cmd["timings"].as<JsonArrayConst>().size() > 0 : !cmd["code"].isNull();
}

bool ConfigStorage::makeJob(const String& name, IrJob& job, String& error) const {
  if (!hasCommand(name)) { error = "Unknown command"; return false; }
  if (!isConfigured(name)) { error = "Command is not configured"; return false; }
  JsonVariantConst cmd = config_["commands"][name];
  const char* protocol = cmd["protocol"];
  job.protocol = !strcmp(protocol, "NEC") ? IrProtocol::Nec : !strcmp(protocol, "SONY") ? IrProtocol::Sony : !strcmp(protocol, "RC5") ? IrProtocol::Rc5 : !strcmp(protocol, "RC6") ? IrProtocol::Rc6 : IrProtocol::Raw;
  job.frequency = cmd["frequency"] | frequency();
  name.toCharArray(job.name, sizeof(job.name));
  if (job.protocol == IrProtocol::Raw) {
    job.length = 0;
    for (JsonVariantConst timing : cmd["timings"].as<JsonArrayConst>()) job.timings[job.length++] = timing.as<uint16_t>();
  } else {
    job.bits = cmd["bits"].as<uint16_t>();
    parseHex(cmd["code"].as<const char*>(), job.code);
  }
  return true;
}

uint32_t ConfigStorage::repeatInterval() const { return config_["repeat_interval_ms"] | 250; }
uint32_t ConfigStorage::frequency() const { return config_["default_frequency"] | IR_DEFAULT_FREQUENCY; }
bool ConfigStorage::receiverEnabled() const { return config_["receiver_enabled"] | false; }
