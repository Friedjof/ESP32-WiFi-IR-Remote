#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "Config.h"

class NetworkManager {
 public:
  void begin();
  void loop();
  bool apActive() const { return apActive_; }
  const String& apName() const { return apName_; }
  const String& otaPassword() const { return otaPassword_; }

 private:
  Preferences prefs_;
  DNSServer dns_;
  String otaPassword_;
  String apName_;
  bool apActive_ = false;
  bool serialWasConnected_ = false;
  bool mdnsStarted_ = false;
  uint32_t lastStartAttempt_ = 0;
  String secret(const char* key, size_t length);
  void printInfo();
  bool startAp();
};
