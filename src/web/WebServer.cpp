#include "WebServer.h"
#include <WiFi.h>
#include "generated/web_files.h"

bool RemoteWebServer::authorize() {
  IPAddress remote = server_.client().remoteIP();
  IPAddress local = WiFi.softAPIP();
  const IPAddress mask(255, 255, 255, 0);
  for (int i = 0; i < 4; ++i) {
    if ((remote[i] & mask[i]) != (local[i] & mask[i])) {
      sendError(403, "Only clients on the remote WiFi are allowed");
      return false;
    }
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
  server_.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
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
  server_.on("/", HTTP_GET, [this]() { serveFile("/", "text/html; charset=utf-8"); });
  server_.on("/style.css", HTTP_GET, [this]() { serveFile("/style.css", "text/css; charset=utf-8"); });
  server_.on("/app.js", HTTP_GET, [this]() { serveFile("/app.js", "application/javascript; charset=utf-8"); });
  server_.on("/manifest.webmanifest", HTTP_GET, [this]() { serveFile("/manifest.webmanifest", "application/manifest+json"); });
  server_.on("/icon.svg", HTTP_GET, [this]() { serveFile("/icon.svg", "image/svg+xml"); });
  for (const char* path : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt", "/redirect"}) {
    server_.on(path, HTTP_GET, [this]() { redirectToRemote(); });
  }
  server_.on("/api/status", HTTP_GET, [this]() { if (authorize()) status(); });
  server_.on("/api/commands", HTTP_GET, [this]() { if (authorize()) commands(); });
  server_.on("/api/remotes", HTTP_GET, [this]() { if (authorize()) remotes(); });
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
  doc["wifi"]["connected"] = network_.apActive();
  doc["wifi"]["mode"] = "ap";
  doc["wifi"]["ssid"] = network_.apName();
  doc["wifi"]["ip"] = WiFi.softAPIP().toString();
  doc["wifi"]["clients"] = WiFi.softAPgetStationNum();
  doc["ir"]["enabled"] = true;
  doc["ir"]["frequency"] = config_.frequency();
  doc["ir"]["queued"] = tx_.queued();
  doc["ir"]["busy"] = tx_.busy();
  doc["receiver"]["available"] = rx_.available();
  doc["receiver"]["enabled"] = rx_.enabled();
  doc["uptime_ms"] = millis();
  doc["free_heap"] = ESP.getFreeHeap();
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
  if (!config_.hasCommand(name)) { sendError(404, "Unknown command"); return; }
  IrJob job;
  String error;
  if (!config_.makeJob(name, job, error)) { sendError(409, error); return; }
  uint32_t now = millis();
  if (lastAccepted_ && now - lastAccepted_ < 120) { sendError(429, "Command rate limit: wait 120 ms"); return; }
  if (!tx_.enqueue(job)) { sendError(503, "IR queue is full"); return; }
  lastAccepted_ = now;
  JsonDocument doc; doc["success"] = true; doc["command"] = name; doc["queued"] = true;
  sendJson(202, doc);
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
