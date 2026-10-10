#include "CandidateCatalog.h"
#include <LittleFS.h>
#include "generated/candidates.h"
#include "generated/default_profiles.h"

namespace {
constexpr const char* kFeedbackPath = "/candidate_feedback.json";
constexpr const char* kFeedbackTemp = "/candidate_feedback.tmp";
constexpr const char* kFeedbackBackup = "/candidate_feedback.bak";
const char* const kSafeCommands[] = {
  "volume_up", "volume_down", "mute", "up", "down", "left", "right",
  "ok", "home", "back", "input", "channel_up", "channel_down", "menu", "exit"
};
}

bool CandidateCatalog::begin() {
  if (deserializeJson(catalog_, candidateCatalogJson)) return false;
  File file = LittleFS.open(kFeedbackPath, "r");
  if (file) {
    if (file.size() <= 4096 && !deserializeJson(feedback_, file) && feedback_.is<JsonObject>()) {
      file.close(); return true;
    }
    file.close();
  }
  file = LittleFS.open(kFeedbackBackup, "r");
  if (file && file.size() <= 4096 && !deserializeJson(feedback_, file) && feedback_.is<JsonObject>()) {
    file.close(); return true;
  }
  if (file) file.close();
  feedback_.to<JsonObject>();
  return true;
}

JsonVariantConst CandidateCatalog::find(const String& id) const {
  for (JsonVariantConst profile : catalog_["profiles"].as<JsonArrayConst>())
    if (profile["id"].as<String>() == id) return profile;
  return JsonVariantConst();
}

bool CandidateCatalog::exists(const String& id) const { return !find(id).isNull(); }

bool CandidateCatalog::safeCommand(const String& command) const {
  for (const char* safe : kSafeCommands) if (command == safe) return true;
  return false;
}

void CandidateCatalog::list(JsonDocument& output) const {
  output["profiles"].set(catalog_["profiles"].as<JsonVariantConst>());
  output["feedback"].set(feedback_.as<JsonVariantConst>());
}

bool CandidateCatalog::makeTestJob(const String& id, const String& command,
                                   IrJob& job, String& error) const {
  JsonVariantConst profile = find(id);
  if (profile.isNull()) { error = "Candidate profile not found"; return false; }
  if (!safeCommand(command)) { error = "Only safe, manually selected finder commands can be tested"; return false; }
  JsonVariantConst spec = profile["commands"][command];
  if (spec.isNull()) { error = "Command is unavailable in this candidate"; return false; }
  return ConfigStorage::makeJobFrom(spec, profile["frequency"] | 38000, command, job, error);
}

bool CandidateCatalog::saveFeedback(String& error) {
  if (measureJson(feedback_) > 4096) { error = "Too much feedback data"; return false; }
  File file = LittleFS.open(kFeedbackTemp, "w");
  if (!file) { error = "Cannot open feedback file"; return false; }
  size_t written = serializeJson(feedback_, file);
  file.flush();
  size_t actual = file.size();
  file.close();
  if (!written || actual != written) { LittleFS.remove(kFeedbackTemp); error = "Feedback write failed"; return false; }
  LittleFS.remove(kFeedbackBackup);
  if (LittleFS.exists(kFeedbackPath) && !LittleFS.rename(kFeedbackPath, kFeedbackBackup)) {
    LittleFS.remove(kFeedbackTemp); error = "Feedback backup failed"; return false;
  }
  if (!LittleFS.rename(kFeedbackTemp, kFeedbackPath)) {
    LittleFS.rename(kFeedbackBackup, kFeedbackPath); error = "Feedback save failed"; return false;
  }
  return true;
}

bool CandidateCatalog::record(const String& id, const String& command, bool worked, String& error) {
  JsonVariantConst candidate = find(id);
  if (candidate.isNull() || !safeCommand(command) || candidate["commands"][command].isNull()) {
    error = "Unknown candidate or test command"; return false;
  }
  JsonDocument previous;
  previous.set(feedback_.as<JsonVariantConst>());
  feedback_[id][command] = worked;
  if (!saveFeedback(error)) {
    feedback_.clear(); feedback_.set(previous.as<JsonVariantConst>());
    return false;
  }
  return true;
}

bool CandidateCatalog::clearFeedback(const String& id, String& error) {
  if (!exists(id)) { error = "Candidate profile not found"; return false; }
  JsonDocument previous;
  previous.set(feedback_.as<JsonVariantConst>());
  feedback_.as<JsonObject>().remove(id);
  if (!saveFeedback(error)) {
    feedback_.clear(); feedback_.set(previous.as<JsonVariantConst>());
    return false;
  }
  return true;
}

bool CandidateCatalog::activate(const String& id, ConfigStorage& config, String& error) {
  JsonVariantConst candidate = find(id);
  if (candidate.isNull()) { error = "Candidate profile not found"; return false; }
  size_t successes = 0;
  JsonObjectConst results = feedback_[id].as<JsonObjectConst>();
  for (JsonPairConst result : results) if (result.value().as<bool>()) ++successes;
  if (successes < 2) { error = "Confirm at least two different working commands before saving this candidate"; return false; }
  for (const FactoryProfile& factory : factoryProfiles) {
    if (strcmp(factory.id, "toshiba_tv")) continue;
    JsonDocument target;
    if (deserializeJson(target, factory.json)) { error = "Factory layout unavailable"; return false; }
    target["id"] = id;
    target["device"] = candidate["name"].as<String>();
    target["default_frequency"] = candidate["frequency"].as<uint32_t>();
    target["origin"]["source"] = candidate["source"].as<String>();
    target["origin"]["url"] = candidate["url"].as<String>();
    target["origin"]["target_verified"] = false;
    JsonArray confirmed = target["origin"]["confirmed_commands"].to<JsonArray>();
    for (JsonPairConst result : results) if (result.value().as<bool>()) confirmed.add(result.key().c_str());
    JsonObject commands = target["commands"].as<JsonObject>();
    for (JsonPair item : commands) item.value()["frequency"] = 38000;
    for (JsonPairConst item : candidate["commands"].as<JsonObjectConst>()) {
      if (commands[item.key().c_str()].is<JsonObject>()) commands[item.key().c_str()].set(item.value());
    }
    String json;
    serializeJson(target, json);
    if (config.create(json, error)) return true;
    if (error == "Profile id already exists") return config.select(id, error);
    return false;
  }
  error = "Toshiba layout unavailable";
  return false;
}
