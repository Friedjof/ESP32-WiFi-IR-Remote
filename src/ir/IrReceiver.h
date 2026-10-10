#pragma once
#include <ArduinoJson.h>
#include "Config.h"
#if IR_ENABLE_RX
#include <IRrecv.h>
#endif

class IrReceiver {
 public:
  IrReceiver();
  void begin();
  void loop();
  bool start(const String& command, String& error);
  bool commit(JsonDocument& config, String& error);
  bool available() const { return IR_ENABLE_RX; }
  void setEnabled(bool enabled);
  void markSaved() { state_ = "saved"; }
  bool enabled() const { return enabled_ && available(); }
  bool learning() const { return active_; }
  const String& command() const { return command_; }
  const String& state() const { return state_; }
  JsonDocument& candidate() { return candidate_; }

 private:
#if IR_ENABLE_RX
  IRrecv receiver_;
  decode_results results_;
#endif
  bool enabled_ = false;
  bool hardwareActive_ = false;
  bool active_ = false;
  uint32_t startTime_ = 0;
  String command_;
  String state_ = "idle";
  JsonDocument candidate_;
};
