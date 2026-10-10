#pragma once
#include <WebServer.h>
#include "storage/ConfigStorage.h"
#include "network/WifiManager.h"
#include "ir/IrTransmitter.h"
#include "ir/IrReceiver.h"
#include "finder/CandidateCatalog.h"
#include "diagnostics/DiagnosticLog.h"

class RemoteWebServer {
 public:
  RemoteWebServer(ConfigStorage& config, CandidateCatalog& candidates, NetworkManager& network,
                  IrTransmitter& tx, IrReceiver& rx, DiagnosticLog& log)
    : config_(config), candidates_(candidates), network_(network), tx_(tx), rx_(rx),
      log_(log), server_(80) {}
  void begin();
  void loop();

 private:
  ConfigStorage& config_;
  CandidateCatalog& candidates_;
  NetworkManager& network_;
  IrTransmitter& tx_;
  IrReceiver& rx_;
  DiagnosticLog& log_;
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
  void candidateTest(const String& id);
  void candidateFeedback(const String& id);
  void candidateActivate(const String& id);
  void importFlipper();
  void arrangeFlipper();
  void notFound();
};
