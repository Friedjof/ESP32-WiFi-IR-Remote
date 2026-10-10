#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "Config.h"
#include "diagnostics/DiagnosticLog.h"

class NetworkManager {
 public:
  explicit NetworkManager(DiagnosticLog& log) : log_(log) {}
  void begin();
  void loop();
  bool apActive() const { return apActive_; }
  const String& apName() const { return apName_; }
  bool stationConfigured() const { return !stationSsid_.isEmpty(); }
  bool stationConnected() const { return WiFi.status() == WL_CONNECTED; }
  const String& stationSsid() const { return stationSsid_; }
  bool configureStation(const String& ssid, const String& password, String& error);
  const String& adminPassword() const { return adminPassword_; }
  bool configureAdmin(const String& password, String& error);

 private:
  DiagnosticLog& log_;
  Preferences prefs_;
  DNSServer dns_;
  String apName_;
  String stationSsid_;
  String stationPassword_;
  String adminPassword_;
  bool apActive_ = false;
  bool serialWasConnected_ = false;
  bool mdnsStarted_ = false;
  bool stationWasConnected_ = false;
  uint32_t lastStartAttempt_ = 0;
  uint32_t lastStationAttempt_ = 0;
  void printInfo();
  bool startAp();
  void connectStation();
};
