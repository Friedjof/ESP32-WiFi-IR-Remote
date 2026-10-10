#include <Arduino.h>
#include "storage/ConfigStorage.h"
#include "network/WifiManager.h"
#include "web/WebServer.h"
#include "ir/IrTransmitter.h"
#include "ir/IrReceiver.h"
#include "ota/OtaManager.h"
#include "finder/CandidateCatalog.h"
#include "diagnostics/DiagnosticLog.h"

DiagnosticLog diagnostics;
ConfigStorage config;
CandidateCatalog candidates;
NetworkManager network(diagnostics);
IrTransmitter transmitter(diagnostics);
IrReceiver receiver;
OtaManager ota(diagnostics);
RemoteWebServer web(config, candidates, network, transmitter, receiver, diagnostics);

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
  uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 5000) delay(10);
#endif
  delay(250);
  Serial.printf("ESP32 IR Remote %s\n", FIRMWARE_VERSION);
  if (!config.begin()) Serial.println("ERROR: LittleFS initialization failed");
  if (!candidates.begin()) Serial.println("ERROR: Candidate catalog unavailable");
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
