#include "OtaManager.h"
#include "Config.h"

void OtaManager::begin() {
  ArduinoOTA.setHostname(DEVICE_HOSTNAME);
  ArduinoOTA.setMdnsEnabled(false); // NetworkManager owns the mDNS responder.
  ArduinoOTA.setRebootOnSuccess(false);
  ArduinoOTA.onStart([this]() {
    Serial.println("OTA update started");
    log_.add("ota", "Firmware update started");
  });
  ArduinoOTA.onEnd([this]() {
    Serial.println("OTA update complete");
    log_.add("ota", "Firmware update complete; restarting");
    rebootAt_ = millis() + 2000;
  });
  ArduinoOTA.onError([this](ota_error_t error) {
    Serial.printf("OTA error: %u\n", error);
    log_.add("ota", "Firmware update failed (error %u)", static_cast<unsigned>(error));
  });
  ArduinoOTA.begin();
  log_.add("ota", "OTA ready without a password on port 3232");
}

void OtaManager::loop() {
  ArduinoOTA.handle();
  if (rebootAt_ && static_cast<int32_t>(millis() - rebootAt_) >= 0) ESP.restart();
}
