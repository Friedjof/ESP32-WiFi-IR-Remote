#include "WifiManager.h"
#include <esp_wifi.h>

void NetworkManager::printInfo() {
  Serial.printf("Open WiFi AP: %s\n", apName_.c_str());
  Serial.printf("Remote URL: http://%s/\n", WiFi.softAPIP().toString().c_str());
  Serial.println("OTA: port 3232, no password");
  if (stationConnected()) Serial.printf("Home WiFi: %s, IP: %s, http://%s.local/\n",
                                      stationSsid_.c_str(), WiFi.localIP().toString().c_str(), DEVICE_HOSTNAME);
  else if (stationConfigured()) Serial.printf("Home WiFi: connecting to %s\n", stationSsid_.c_str());
}

bool NetworkManager::startAp() {
  lastStartAttempt_ = millis();
  WiFi.mode(stationConfigured() ? WIFI_AP_STA : WIFI_AP);
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
    log_.add("wifi", "Access point failed to start");
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
    MDNS.enableArduino(3232, false);
  }
  Serial.printf("Captive DNS ready; mDNS: %s\n", mdnsStarted_ ? "ready" : "unavailable");
  log_.add("wifi", "Access point ready at %s", WiFi.softAPIP().toString().c_str());
  printInfo();
  return true;
}

void NetworkManager::begin() {
  prefs_.begin("tvremote", false);
  stationSsid_ = prefs_.getString("sta_ssid", "");
  stationPassword_ = prefs_.getString("sta_pass", "");
  adminPassword_ = prefs_.getString("admin", "");
  apName_ = "TV-Remote-" + String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xffff), HEX);
  log_.add("system", "Firmware %s started", FIRMWARE_VERSION);
  serialWasConnected_ = static_cast<bool>(Serial);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t) {
    if (event == ARDUINO_EVENT_WIFI_AP_START) Serial.println("WiFi event: AP started");
    if (event == ARDUINO_EVENT_WIFI_AP_STOP) Serial.println("WiFi event: AP stopped");
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) Serial.println("WiFi event: client connected");
    if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) Serial.println("WiFi event: client disconnected");
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) Serial.printf("Home WiFi IP: %s\n", WiFi.localIP().toString().c_str());
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) Serial.println("Home WiFi disconnected");
  });
  startAp();
  connectStation();
}

void NetworkManager::connectStation() {
  if (!stationConfigured()) return;
  lastStationAttempt_ = millis();
  WiFi.setHostname(DEVICE_HOSTNAME);
  WiFi.setAutoReconnect(true);
  WiFi.begin(stationSsid_.c_str(), stationPassword_.c_str());
  Serial.printf("Connecting to home WiFi: %s\n", stationSsid_.c_str());
  log_.add("wifi", "Connecting to home WiFi");
}

bool NetworkManager::configureStation(const String& ssid, const String& password, String& error) {
  if (ssid.length() > 32 || password.length() > 63 ||
      (!ssid.isEmpty() && password.length() && password.length() < 8)) {
    error = "SSID or WPA2 password length is invalid"; return false;
  }
  if (ssid.isEmpty()) {
    stationSsid_ = stationPassword_ = "";
    prefs_.remove("sta_ssid"); prefs_.remove("sta_pass");
    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    log_.add("wifi", "Home WiFi configuration cleared");
    return true;
  }
  if (password.isEmpty() && ssid == stationSsid_) {
    error = "Enter a password to update this network"; return false;
  }
  stationSsid_ = ssid;
  stationPassword_ = password;
  prefs_.putString("sta_ssid", stationSsid_);
  prefs_.putString("sta_pass", stationPassword_);
  WiFi.mode(WIFI_AP_STA);
  connectStation();
  return true;
}

bool NetworkManager::configureAdmin(const String& password, String& error) {
  if (!password.isEmpty() && (password.length() < 8 || password.length() > 64)) {
    error = "Admin password must be 8..64 characters"; return false;
  }
  adminPassword_ = password;
  if (password.isEmpty()) prefs_.remove("admin");
  else prefs_.putString("admin", password);
  return true;
}

void NetworkManager::loop() {
  bool serialConnected = static_cast<bool>(Serial);
  if (serialConnected && !serialWasConnected_) printInfo();
  serialWasConnected_ = serialConnected;
  bool connected = stationConnected();
  if (connected != stationWasConnected_) {
    stationWasConnected_ = connected;
    if (connected) log_.add("wifi", "Home WiFi connected at %s (RSSI %d dBm)",
                            WiFi.localIP().toString().c_str(), WiFi.RSSI());
    else log_.add("wifi", "Home WiFi disconnected");
  }
  if (apActive_) {
    dns_.processNextRequest();
  } else if (millis() - lastStartAttempt_ >= 10000) {
    startAp();
  }
  if (stationConfigured() && !stationConnected() && millis() - lastStationAttempt_ >= 15000)
    connectStation();
}
