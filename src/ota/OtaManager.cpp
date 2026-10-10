#include "OtaManager.h"
#include "Config.h"

void OtaManager::begin(const String& password) {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA.setMdnsEnabled(false); // NetworkManager owns the mDNS responder.
  ArduinoOTA.setPassword(password.c_str());
  ArduinoOTA.onStart([]() { Serial.println("OTA update started"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA update complete"); });
  ArduinoOTA.onError([](ota_error_t error) { Serial.printf("OTA error: %u\n", error); });
  ArduinoOTA.begin();
}

void OtaManager::loop() { ArduinoOTA.handle(); }
