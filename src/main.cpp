#include <Arduino.h>
#include "storage/ConfigStorage.h"
#include "network/WifiManager.h"
#include "web/WebServer.h"
#include "ir/IrTransmitter.h"
#include "ir/IrReceiver.h"
#include "ota/OtaManager.h"

ConfigStorage config;
NetworkManager network;
IrTransmitter transmitter;
IrReceiver receiver;
OtaManager ota;
RemoteWebServer web(config, network, transmitter, receiver);

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 5000) delay(10);
#endif
  delay(250);
  Serial.printf("ESP32 IR Remote %s\n", FIRMWARE_VERSION);
  if (!config.begin()) Serial.println("ERROR: LittleFS initialization failed");
  transmitter.begin();
  receiver.begin();
  receiver.setEnabled(config.receiverEnabled());
  network.begin();
  web.begin();
  ota.begin(network.otaPassword());
}

void loop() {
  network.loop();
  web.loop();
  receiver.loop();
  transmitter.loop();
  ota.loop();
  delay(1);
}
