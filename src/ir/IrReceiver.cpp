#include "IrReceiver.h"
#if IR_ENABLE_RX
#include <IRutils.h>
#endif

IrReceiver::IrReceiver()
#if IR_ENABLE_RX
  : receiver_(IR_RX_PIN, 1024, 15, true)
#endif
{}

void IrReceiver::begin() {
  // The receiver is started only when it is enabled in the configuration.
}

void IrReceiver::setEnabled(bool enabled) {
  enabled_ = enabled;
#if IR_ENABLE_RX
  if (enabled_ && !hardwareActive_) { receiver_.enableIRIn(); hardwareActive_ = true; }
  if (!enabled_ && hardwareActive_) { receiver_.disableIRIn(); hardwareActive_ = false; }
#endif
  if (!enabled_ && active_) { active_ = false; state_ = "disabled"; }
}

bool IrReceiver::start(const String& command, String& error) {
  if (!enabled()) { error = "IR receiver is disabled"; return false; }
  if (active_) { error = "Learning already in progress"; return false; }
  command_ = command;
  candidate_.clear();
  state_ = "waiting";
  active_ = true;
  startTime_ = millis();
#if IR_ENABLE_RX
  receiver_.resume();
#endif
  return true;
}

void IrReceiver::loop() {
  if (!active_) return;
  if (millis() - startTime_ > LEARN_TIMEOUT_MS) { active_ = false; state_ = "timeout"; return; }
#if IR_ENABLE_RX
  if (!receiver_.decode(&results_)) return;
  candidate_.clear();
  String protocol = typeToString(results_.decode_type);
  if ((protocol == "NEC" || protocol == "SONY" || protocol == "RC5" || protocol == "RC6") && results_.bits > 0 && results_.bits <= 64) {
    char code[19];
    snprintf(code, sizeof(code), "0x%llX", (unsigned long long)results_.value);
    candidate_["protocol"] = protocol;
    candidate_["code"] = code;
    candidate_["bits"] = results_.bits;
  } else {
    uint16_t length = getCorrectedRawLength(&results_);
    if (length == 0 || length > MAX_RAW_TIMINGS) { state_ = "signal_too_long"; active_ = false; receiver_.resume(); return; }
    uint16_t* raw = resultToRawArray(&results_);
    if (!raw) { state_ = "capture_failed"; active_ = false; receiver_.resume(); return; }
    candidate_["protocol"] = "RAW";
    candidate_["frequency"] = IR_DEFAULT_FREQUENCY;
    candidate_["detected_protocol"] = protocol;
    JsonArray timings = candidate_["timings"].to<JsonArray>();
    for (uint16_t i = 0; i < length; ++i) timings.add(raw[i]);
    delete[] raw;
  }
  active_ = false;
  state_ = "captured";
  receiver_.resume();
#endif
}

bool IrReceiver::commit(JsonDocument& config, String& error) {
  if (state_ != "captured") { error = "No captured signal"; return false; }
  if (!config["commands"][command_].is<JsonObject>()) { error = "Unknown command"; return false; }
  JsonObject target = config["commands"][command_].to<JsonObject>();
  target.clear();
  target.set(candidate_.as<JsonVariantConst>());
  target.remove("detected_protocol");
  return true;
}
