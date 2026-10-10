#include "WifiManager.h"
#include <esp_wifi.h>

String NetworkManager::secret(const char* key, size_t length) {
  String value = prefs_.getString(key, "");
  if (value.length() == length) return value;
  const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  value.reserve(length);
  for (size_t i = 0; i < length; ++i) value += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  prefs_.putString(key, value);
  return value;
}

void NetworkManager::printInfo() {
  Serial.printf("Open WiFi AP: %s\n", apName_.c_str());
  Serial.printf("Remote URL: http://%s/\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("OTA password: %s\n", otaPassword_.c_str());
}

bool NetworkManager::startAp() {
  lastStartAttempt_ = millis();
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  const IPAddress ip(192, 168, 4, 1);
  if (!WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0))) {
    Serial.println("Warning: AP IP configuration failed");
  }
  delay(100);
  bool started = WiFi.softAP(apName_.c_str(), nullptr, 6, false, 4);
  if (!started) {
    Serial.println("AP start failed; retrying");
    delay(200);
    started = WiFi.softAP(apName_.c_str(), nullptr, 6, false, 4);
  }
  if (!started) {
    Serial.println("ERROR: Open WiFi AP did not start");
    return false;
  }
  delay(100);
  apActive_ = true;
  wifi_mode_t mode = WIFI_MODE_NULL;
  wifi_config_t config = {};
  uint8_t channel = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  esp_err_t modeResult = esp_wifi_get_mode(&mode);
  esp_err_t configResult = esp_wifi_get_config(WIFI_IF_AP, &config);
  esp_err_t channelResult = esp_wifi_get_channel(&channel, &secondary);
  Serial.printf("AP radio: mode=%d (%d), config=%d, channel=%u (%d), auth=%d, hidden=%u, MAC=%s, TX power=%d\n",
                mode, modeResult, configResult, channel, channelResult,
                config.ap.authmode, config.ap.ssid_hidden,
                WiFi.softAPmacAddress().c_str(), WiFi.getTxPower());
  dns_.start(53, "*", WiFi.softAPIP());
  mdnsStarted_ = MDNS.begin(DEVICE_HOSTNAME);
  if (mdnsStarted_) {
    MDNS.addService("http", "tcp", 80);
    MDNS.enableArduino(3232, true);
  }
  Serial.printf("Captive DNS ready; mDNS: %s\n", mdnsStarted_ ? "ready" : "unavailable");
  printInfo();
  return true;
}

void NetworkManager::begin() {
  prefs_.begin("tvremote", false);
  otaPassword_ = secret("ota", 20);
  apName_ = "TV-Remote-" + String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xffff), HEX);
  serialWasConnected_ = static_cast<bool>(Serial);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t) {
    if (event == ARDUINO_EVENT_WIFI_AP_START) Serial.println("WiFi event: AP started");
    if (event == ARDUINO_EVENT_WIFI_AP_STOP) Serial.println("WiFi event: AP stopped");
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("WiFi event: client connected");
    if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("WiFi event: client disconnected");
  });
  startAp();
}

void NetworkManager::loop() {
  bool serialConnected = static_cast<bool>(Serial);
  if (serialConnected && !serialWasConnected_) printInfo();
  serialWasConnected_ = serialConnected;
  if (apActive_) {
    dns_.processNextRequest();
  } else if (millis() - lastStartAttempt_ >= 10000) {
    startAp();
  }
}
