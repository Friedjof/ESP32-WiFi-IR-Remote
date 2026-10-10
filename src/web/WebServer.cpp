#include "WebServer.h"
#include <WiFi.h>
#include "generated/web_files.h"
#include "finder/FlipperImporter.h"
#include "finder/FlipperButtonLayout.h"

bool RemoteWebServer::authorize() {
  IPAddress remote = server_.client().remoteIP();
  const IPAddress apMask(255, 255, 255, 0);
  auto subnetMatch = [&](IPAddress local, IPAddress mask) {
    for (int i = 0; i < 4; ++i) if ((remote[i] & mask[i]) != (local[i] & mask[i])) return false;
    return true;
  };
  if (!subnetMatch(WiFi.softAPIP(), apMask) &&
      !(network_.stationConnected() && subnetMatch(WiFi.localIP(), WiFi.subnetMask()))) {
    sendError(403, "Only local AP or home WiFi clients are allowed");
    return false;
  }
  if (server_.method() != HTTP_GET && !network_.adminPassword().isEmpty() &&
      !server_.authenticate("admin", network_.adminPassword().c_str())) {
    server_.requestAuthentication(BASIC_AUTH, "IR Remote administration", "Admin password required");
    return false;
  }
  return true;
}

void RemoteWebServer::sendJson(int code, JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(code, "application/json", body);
}

void RemoteWebServer::sendError(int code, const String& message) {
  JsonDocument doc;
  doc["success"] = false;
  doc["error"] = message;
  sendJson(code, doc);
}

void RemoteWebServer::redirectToRemote() {
  server_.sendHeader("Location", "/", true);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(302, "text/plain", "");
}

void RemoteWebServer::serveFile(const char* path, const char* mime) {
  if (!authorize()) return;
  for (size_t i = 0; i < embeddedWebFileCount; ++i) {
    const EmbeddedWebFile& asset = embeddedWebFiles[i];
    if (strcmp(asset.path, path) != 0) continue;
    server_.sendHeader("Content-Encoding", "gzip");
    server_.sendHeader("Cache-Control", "no-store");
    server_.send_P(200, mime, reinterpret_cast<PGM_P>(asset.data), asset.size);
    return;
  }
  server_.send(404, "text/plain", "Asset not found");
}

void RemoteWebServer::begin() {
  const char* headers[] = {"X-IR-Held"};
  server_.collectHeaders(headers, 1);
  server_.on("/", HTTP_GET, [this]() { serveFile("/", "text/html; charset=utf-8"); });
  server_.on("/style.css", HTTP_GET, [this]() { serveFile("/style.css", "text/css; charset=utf-8"); });
  server_.on("/app.js", HTTP_GET, [this]() { serveFile("/app.js", "application/javascript; charset=utf-8"); });
  server_.on("/manifest.webmanifest", HTTP_GET, [this]() { serveFile("/manifest.webmanifest", "application/manifest+json"); });
  server_.on("/icon.svg", HTTP_GET, [this]() { serveFile("/icon.svg", "image/svg+xml"); });
  for (const char* path : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt", "/redirect"}) {
    server_.on(path, HTTP_GET, [this]() { redirectToRemote(); });
  }
  server_.on("/api/status", HTTP_GET, [this]() { if (authorize()) status(); });
  server_.on("/api/logs", HTTP_GET, [this]() {
    if (!authorize()) return;
    JsonDocument doc;
    log_.write(doc);
    sendJson(200, doc);
  });
  server_.on("/api/network", HTTP_GET, [this]() {
    if (!authorize()) return;
    JsonDocument doc;
    doc["ap"]["ssid"] = network_.apName();
    doc["ap"]["ip"] = WiFi.softAPIP().toString();
    doc["station"]["configured"] = network_.stationConfigured();
    doc["station"]["connected"] = network_.stationConnected();
    doc["station"]["ssid"] = network_.stationSsid();
    if (network_.stationConnected()) doc["station"]["ip"] = WiFi.localIP().toString();
    sendJson(200, doc);
  });
  server_.on("/api/network", HTTP_PUT, [this]() {
    if (!authorize()) return;
    JsonDocument input;
    if (deserializeJson(input, server_.arg("plain")) || !input["ssid"].is<const char*>() || !input["password"].is<const char*>()) {
      sendError(400, "Expected ssid and password strings; empty SSID disconnects home WiFi"); return;
    }
    String error;
    if (!network_.configureStation(input["ssid"].as<String>(), input["password"].as<String>(), error)) {
      sendError(400, error); return;
    }
    JsonDocument doc; doc["success"] = true; doc["connecting"] = network_.stationConfigured();
    sendJson(200, doc);
  });
  server_.on("/api/security", HTTP_GET, [this]() {
    if (!authorize()) return;
    JsonDocument doc; doc["admin_password_set"] = !network_.adminPassword().isEmpty();
    sendJson(200, doc);
  });
  server_.on("/api/security", HTTP_PUT, [this]() {
    if (!authorize()) return;
    JsonDocument input;
    if (deserializeJson(input, server_.arg("plain")) || !input["password"].is<const char*>()) {
      sendError(400, "Expected password string; empty removes authentication"); return;
    }
    String error;
    if (!network_.configureAdmin(input["password"].as<String>(), error)) { sendError(400, error); return; }
    JsonDocument doc; doc["success"] = true; doc["admin_password_set"] = !network_.adminPassword().isEmpty();
    sendJson(200, doc);
  });
  server_.on("/api/commands", HTTP_GET, [this]() { if (authorize()) commands(); });
  server_.on("/api/remotes", HTTP_GET, [this]() { if (authorize()) remotes(); });
  server_.on("/api/candidates", HTTP_GET, [this]() {
    if (!authorize()) return;
    JsonDocument doc; candidates_.list(doc); sendJson(200, doc);
  });
  server_.on("/api/import/flipper", HTTP_POST, [this]() { if (authorize()) importFlipper(); });
  server_.on("/api/config/auto-layout", HTTP_POST, [this]() { if (authorize()) arrangeFlipper(); });
  server_.on("/api/remotes", HTTP_POST, [this]() { if (authorize()) createRemote(); });
  server_.on("/api/remotes/active", HTTP_POST, [this]() { if (authorize()) selectRemote(); });
  server_.on("/api/config", HTTP_GET, [this]() { if (authorize()) getConfig(); });
  server_.on("/api/config", HTTP_PUT, [this]() { if (authorize()) putConfig(); });
  server_.on("/api/config/import", HTTP_POST, [this]() { if (authorize()) putConfig(); });
  server_.on("/api/config/export", HTTP_GET, [this]() { if (authorize()) exportConfig(); });
  server_.on("/api/learn/start", HTTP_POST, [this]() { if (authorize()) startLearn(); });
  server_.on("/api/learn/status", HTTP_GET, [this]() { if (authorize()) learnStatus(); });
  server_.on("/api/learn/commit", HTTP_POST, [this]() { if (authorize()) commitLearn(); });
  server_.on("/api/restart", HTTP_POST, [this]() {
    if (!authorize()) return;
    JsonDocument doc; doc["success"] = true; doc["message"] = "Restart scheduled";
    sendJson(202, doc); restartAt_ = millis() + 500;
  });
  server_.onNotFound([this]() { notFound(); });
  server_.begin();
  Serial.println("HTTP server ready");
}

void RemoteWebServer::loop() {
  server_.handleClient();
  if (restartAt_ && static_cast<int32_t>(millis() - restartAt_) >= 0) ESP.restart();
}

void RemoteWebServer::status() {
  JsonDocument doc;
  doc["device"] = "ESP32 IR Remote";
  doc["target"] = config_.data()["device"].as<const char*>();
  doc["active_remote"] = config_.activeId();
  doc["hostname"] = DEVICE_HOSTNAME;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["firmware_build"] = String(__DATE__) + " " + __TIME__;
  doc["wifi"]["connected"] = network_.apActive() || network_.stationConnected();
  doc["wifi"]["mode"] = network_.stationConfigured() ? "ap_sta" : "ap";
  doc["wifi"]["ssid"] = network_.apName();
  doc["wifi"]["ip"] = WiFi.softAPIP().toString();
  doc["wifi"]["clients"] = WiFi.softAPgetStationNum();
  doc["wifi"]["station_connected"] = network_.stationConnected();
  if (network_.stationConnected()) {
    doc["wifi"]["station_ip"] = WiFi.localIP().toString();
    doc["wifi"]["rssi"] = WiFi.RSSI();
  }
  doc["ir"]["enabled"] = true;
  doc["ir"]["frequency"] = config_.frequency();
  doc["ir"]["queued"] = tx_.queued();
  doc["ir"]["busy"] = tx_.busy();
  doc["receiver"]["available"] = rx_.available();
  doc["receiver"]["enabled"] = rx_.enabled();
  doc["uptime_ms"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
  doc["ota"]["enabled"] = true;
  doc["ota"]["port"] = 3232;
  doc["ota"]["password_protected"] = true;
  sendJson(200, doc);
}

void RemoteWebServer::commands() {
  JsonDocument doc;
  doc["remote"] = config_.activeId();
  JsonObject output = doc["commands"].to<JsonObject>();
  JsonObjectConst input = config_.data()["commands"].as<JsonObjectConst>();
  for (JsonPairConst entry : input) {
    JsonObject item = output[entry.key().c_str()].to<JsonObject>();
    item["configured"] = config_.isConfigured(entry.key().c_str());
    item["protocol"] = entry.value()["protocol"].as<const char*>();
  }
  doc["repeat_interval_ms"] = config_.repeatInterval();
  sendJson(200, doc);
}

void RemoteWebServer::remotes() {
  JsonDocument doc;
  config_.list(doc);
  sendJson(200, doc);
}

void RemoteWebServer::createRemote() {
  String error;
  if (!config_.create(server_.arg("plain"), error)) { sendError(400, error); return; }
  rx_.setEnabled(false);
  rx_.setEnabled(config_.receiverEnabled());
  JsonDocument doc;
  doc["success"] = true;
  doc["active"] = config_.activeId();
  sendJson(201, doc);
}

void RemoteWebServer::selectRemote() {
  JsonDocument input;
  if (deserializeJson(input, server_.arg("plain")) || !input["id"].is<const char*>()) {
    sendError(400, "Expected JSON with id"); return;
  }
  String error;
  if (!config_.select(input["id"].as<String>(), error)) { sendError(404, error); return; }
  rx_.setEnabled(false);
  rx_.setEnabled(config_.receiverEnabled());
  JsonDocument doc;
  doc["success"] = true;
  doc["active"] = config_.activeId();
  sendJson(200, doc);
}

void RemoteWebServer::deleteRemote(const String& id) {
  String error;
  if (!config_.remove(id, error)) { sendError(409, error); return; }
  rx_.setEnabled(false);
  rx_.setEnabled(config_.receiverEnabled());
  JsonDocument doc;
  doc["success"] = true;
  doc["active"] = config_.activeId();
  sendJson(200, doc);
}

void RemoteWebServer::getConfig() {
  JsonDocument doc;
  doc.set(config_.data().as<JsonVariantConst>());
  sendJson(200, doc);
}

void RemoteWebServer::putConfig() {
  String error;
  if (!config_.replace(server_.arg("plain"), error)) { sendError(400, error); return; }
  rx_.setEnabled(false);
  rx_.setEnabled(config_.receiverEnabled());
  JsonDocument doc; doc["success"] = true;
  sendJson(200, doc);
}

void RemoteWebServer::exportConfig() {
  String body;
  serializeJsonPretty(config_.data(), body);
  server_.sendHeader("Content-Disposition", "attachment; filename=tv-remote-backup.json");
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", body);
}

void RemoteWebServer::startLearn() {
  JsonDocument input;
  if (deserializeJson(input, server_.arg("plain")) || !input["command"].is<const char*>()) {
    sendError(400, "Expected JSON with command"); return;
  }
  String name = input["command"].as<String>();
  if (!config_.hasCommand(name)) { sendError(404, "Unknown command"); return; }
  String error;
  if (!rx_.start(name, error)) { sendError(409, error); return; }
  JsonDocument doc; doc["success"] = true; doc["state"] = "waiting";
  sendJson(202, doc);
}

void RemoteWebServer::learnStatus() {
  JsonDocument doc;
  doc["state"] = rx_.state();
  doc["command"] = rx_.command();
  doc["receiver_enabled"] = rx_.enabled();
  if (rx_.state() == "captured") doc["candidate"].set(rx_.candidate().as<JsonVariantConst>());
  sendJson(200, doc);
}

void RemoteWebServer::commitLearn() {
  JsonDocument previous;
  previous.set(config_.data().as<JsonVariantConst>());
  String error;
  if (!rx_.commit(config_.data(), error)) { sendError(409, error); return; }
  if (!config_.save(error)) {
    config_.data().clear();
    config_.data().set(previous.as<JsonVariantConst>());
    sendError(500, error); return;
  }
  rx_.markSaved();
  JsonDocument doc; doc["success"] = true; doc["command"] = rx_.command();
  sendJson(200, doc);
}

void RemoteWebServer::sendCommand(const String& name) {
  if (!config_.hasCommand(name)) {
    log_.add("ir", "Rejected unknown command %s", name.c_str());
    sendError(404, "Unknown command"); return;
  }
  IrJob job;
  String error;
  if (!config_.makeJob(name, job, error)) {
    log_.add("ir", "Rejected %s: %s", name.c_str(), error.c_str());
    sendError(409, error); return;
  }
  job.held = server_.header("X-IR-Held") == "1";
  uint32_t now = millis();
  if (lastAccepted_ && now - lastAccepted_ < 120) {
    log_.add("ir", "Rejected %s: rate limit", name.c_str());
    sendError(429, "Command rate limit: wait 120 ms"); return;
  }
  if (!tx_.enqueue(job)) {
    log_.add("ir", "Rejected %s: queue full", name.c_str());
    sendError(503, "IR queue is full"); return;
  }
  lastAccepted_ = now;
  JsonDocument doc; doc["success"] = true; doc["command"] = name; doc["queued"] = true;
  sendJson(202, doc);
}

void RemoteWebServer::candidateTest(const String& id) {
  JsonDocument input;
  if (deserializeJson(input, server_.arg("plain")) || !input["command"].is<const char*>()) {
    sendError(400, "Expected JSON with command"); return;
  }
  String command = input["command"].as<String>();
  IrJob job;
  String error;
  if (!candidates_.makeTestJob(id, command, job, error)) { sendError(400, error); return; }
  uint32_t now = millis();
  if (lastAccepted_ && now - lastAccepted_ < 120) { sendError(429, "Command rate limit: wait 120 ms"); return; }
  if (!tx_.enqueue(job)) { sendError(503, "IR queue is full"); return; }
  lastAccepted_ = now;
  JsonDocument doc;
  doc["success"] = true; doc["candidate"] = id; doc["command"] = command; doc["queued"] = true;
  sendJson(202, doc);
}

void RemoteWebServer::candidateFeedback(const String& id) {
  JsonDocument input;
  if (deserializeJson(input, server_.arg("plain")) || !input["command"].is<const char*>() ||
      !input["worked"].is<bool>()) { sendError(400, "Expected command and boolean worked"); return; }
  String error;
  if (!candidates_.record(id, input["command"].as<String>(), input["worked"].as<bool>(), error)) {
    sendError(400, error); return;
  }
  JsonDocument doc;
  doc["success"] = true; doc["candidate"] = id;
  sendJson(200, doc);
}

void RemoteWebServer::candidateActivate(const String& id) {
  String error;
  if (!candidates_.activate(id, config_, error)) { sendError(409, error); return; }
  rx_.setEnabled(false); rx_.setEnabled(config_.receiverEnabled());
  JsonDocument doc;
  doc["success"] = true; doc["active"] = config_.activeId();
  sendJson(200, doc);
}

void RemoteWebServer::importFlipper() {
  JsonDocument input;
  if (server_.arg("plain").length() > MAX_CONFIG_BYTES + 8192 ||
      deserializeJson(input, server_.arg("plain")) ||
      !input["id"].is<const char*>() || !input["device"].is<const char*>() ||
      !input["content"].is<const char*>()) {
    sendError(400, "Expected id, device and Flipper file content"); return;
  }
  JsonDocument converted;
  String error;
  if (!FlipperImporter::parse(input["content"].as<String>(), input["id"].as<String>(),
                             input["device"].as<String>(), converted, error)) {
    sendError(400, error); return;
  }
  String json;
  serializeJson(converted, json);
  if (!config_.create(json, error)) { sendError(400, error); return; }
  rx_.setEnabled(false); rx_.setEnabled(config_.receiverEnabled());
  JsonDocument doc;
  doc["success"] = true; doc["active"] = config_.activeId();
  doc["commands"] = converted["commands"].as<JsonObject>().size();
  sendJson(201, doc);
}

void RemoteWebServer::arrangeFlipper() {
  if (config_.data()["origin"]["format"].as<String>() != "Flipper IR") {
    sendError(409, "Auto-layout is available for Flipper imports only"); return;
  }
  JsonDocument updated;
  updated.set(config_.data().as<JsonVariantConst>());
  String error;
  if (!FlipperButtonLayout::arrange(updated, error)) { sendError(400, error); return; }
  String json;
  serializeJson(updated, json);
  if (!config_.replace(json, error)) { sendError(400, error); return; }
  JsonDocument doc;
  doc["success"] = true;
  doc["pages"] = updated["pages"].as<JsonArray>().size();
  doc["commands"] = updated["commands"].as<JsonObject>().size();
  sendJson(200, doc);
}

void RemoteWebServer::notFound() {
  String uri = server_.uri();
  if (!authorize()) return;
  const String prefix = "/api/commands/";
  const String suffix = "/send";
  if (server_.method() == HTTP_POST && uri.startsWith(prefix) && uri.endsWith(suffix)) {
    String name = uri.substring(prefix.length(), uri.length() - suffix.length());
    sendCommand(name);
    return;
  }
  const String candidatePrefix = "/api/candidates/";
  if (server_.method() == HTTP_DELETE && uri.startsWith(candidatePrefix) && uri.endsWith("/feedback")) {
    String id = uri.substring(candidatePrefix.length(), uri.length() - strlen("/feedback"));
    String error;
    if (!candidates_.clearFeedback(id, error)) { sendError(404, error); return; }
    JsonDocument doc; doc["success"] = true; doc["candidate"] = id;
    sendJson(200, doc);
    return;
  }
  if (server_.method() == HTTP_POST && uri.startsWith(candidatePrefix)) {
    String rest = uri.substring(candidatePrefix.length());
    int split = rest.indexOf('/');
    if (split > 0) {
      String id = rest.substring(0, split), action = rest.substring(split + 1);
      if (!candidates_.exists(id)) { sendError(404, "Candidate not found"); return; }
      if (action == "test") { candidateTest(id); return; }
      if (action == "feedback") { candidateFeedback(id); return; }
      if (action == "activate") { candidateActivate(id); return; }
    }
  }
  const String remotePrefix = "/api/remotes/";
  if (server_.method() == HTTP_DELETE && uri.startsWith(remotePrefix)) {
    String id = uri.substring(remotePrefix.length());
    if (!ConfigStorage::validName(id)) { sendError(404, "Profile not found"); return; }
    deleteRemote(id);
    return;
  }
  if (server_.method() == HTTP_GET && !uri.startsWith("/api/")) {
    redirectToRemote();
    return;
  }
  sendError(404, "Not found");
}
