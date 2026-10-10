#pragma once
#include <WebServer.h>
#include "storage/ConfigStorage.h"
#include "network/WifiManager.h"
#include "ir/IrTransmitter.h"
#include "ir/IrReceiver.h"

class RemoteWebServer {
 public:
  RemoteWebServer(ConfigStorage& config, NetworkManager& network, IrTransmitter& tx, IrReceiver& rx)
    : config_(config), network_(network), tx_(tx), rx_(rx), server_(80) {}
  void begin();
  void loop();

 private:
  ConfigStorage& config_;
  NetworkManager& network_;
  IrTransmitter& tx_;
  IrReceiver& rx_;
  WebServer server_;
  uint32_t lastAccepted_ = 0;
  uint32_t restartAt_ = 0;
  bool authorize();
  void sendError(int code, const String& message);
  void sendJson(int code, JsonDocument& doc);
  void redirectToRemote();
  void serveFile(const char* path, const char* mime);
  void status();
  void commands();
  void remotes();
  void createRemote();
  void selectRemote();
  void deleteRemote(const String& id);
  void getConfig();
  void putConfig();
  void exportConfig();
  void startLearn();
  void learnStatus();
  void commitLearn();
  void sendCommand(const String& name);
  void notFound();
};
