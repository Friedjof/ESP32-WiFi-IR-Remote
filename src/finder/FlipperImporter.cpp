#include "FlipperImporter.h"
#include "FlipperButtonLayout.h"
#include "storage/ConfigStorage.h"
#include "Config.h"

namespace {
uint8_t reverse8(uint8_t value) {
  value = (value >> 4) | (value << 4);
  value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
  return ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
}
uint32_t reverse32(uint32_t value) {
  return (uint32_t(reverse8(value & 0xFF)) << 24) |
         (uint32_t(reverse8((value >> 8) & 0xFF)) << 16) |
         (uint32_t(reverse8((value >> 16) & 0xFF)) << 8) |
         reverse8((value >> 24) & 0xFF);
}
uint32_t reverseValue(uint32_t value, uint8_t bits) {
  uint32_t result = 0;
  for (uint8_t i = 0; i < bits; ++i) {
    result = (result << 1) | (value & 1);
    value >>= 1;
  }
  return result;
}
bool hexBytes(const String& input, uint32_t& value) {
  char b[9] = {};
  unsigned n[4] = {};
  if (sscanf(input.c_str(), "%x %x %x %x %8s", &n[0], &n[1], &n[2], &n[3], b) != 4) return false;
  for (unsigned x : n) if (x > 255) return false;
  value = n[0] | (n[1] << 8) | (n[2] << 16) | (n[3] << 24);
  return true;
}
bool parseTimings(const String& value, JsonArray output, String& error) {
  uint32_t total = 0;
  const char* cursor = value.c_str();
  while (*cursor) {
    while (*cursor == ' ' || *cursor == '\t') ++cursor;
    if (!*cursor) break;
    char* end;
    unsigned long timing = strtoul(cursor, &end, 10);
    if (end == cursor || timing < 1 || timing > 65535 ||
        (*end && *end != ' ' && *end != '\t')) { error = "Invalid RAW timing"; return false; }
    if (output.size() >= MAX_RAW_TIMINGS || (total += timing) > 300000) {
      error = "RAW signal exceeds the device limit"; return false;
    }
    output.add(static_cast<uint16_t>(timing));
    cursor = end;
  }
  if (!output.size()) { error = "RAW signal has no timings"; return false; }
  return true;
}
bool addSignal(JsonDocument& target, const String& label, const String& type,
               const String& protocol, const String& address, const String& command,
               const String& frequency, const String& duty, const String& timings,
               String& error) {
  if (label.isEmpty() || label.length() > 64) { error = "Signal needs a short name"; return false; }
  String key = FlipperButtonLayout::commandKey(label);
  if (!ConfigStorage::validName(key)) { error = "Invalid signal name"; return false; }
  JsonObject commands = target["commands"].as<JsonObject>();
  if (commands[key].is<JsonObject>()) { error = "Duplicate signal name after normalization: " + key; return false; }
  if (commands.size() >= 96) { error = "At most 96 signals can be imported"; return false; }
  JsonObject spec = commands[key].to<JsonObject>();
  spec["source_name"] = label;
  if (type == "raw") {
    unsigned long carrier = frequency.toInt();
    if (carrier < 30000 || carrier > 60000 || String(carrier) != frequency) {
      error = "RAW frequency must be 30000..60000 Hz"; return false;
    }
    spec["protocol"] = "RAW";
    spec["frequency"] = carrier;
    if (!duty.isEmpty()) spec["duty_cycle"] = duty;
    if (!parseTimings(timings, spec["timings"].to<JsonArray>(), error)) return false;
  } else if (type == "parsed") {
    uint32_t addr, cmd;
    if (!hexBytes(address, addr) || !hexBytes(command, cmd)) {
      error = "Parsed signal needs four-byte address and command"; return false;
    }
    uint64_t value = 0;
    uint16_t bits = 0;
    uint32_t carrier = 38000;
    if (protocol == "NEC") {
      if (addr > 255 || cmd > 255) { error = "NEC address/command out of range"; return false; }
      uint8_t a = reverse8(addr), c = reverse8(cmd);
      value = (uint32_t(a) << 24) | (uint32_t(a ^ 0xFF) << 16) | (uint32_t(c) << 8) | (c ^ 0xFF);
      bits = 32;
    } else if (protocol == "NECext") {
      if (addr > 65535 || cmd > 65535) { error = "NECext address/command out of range"; return false; }
      value = reverse32((cmd << 16) | addr);
      bits = 32;
    } else if (protocol == "Samsung32") {
      if (addr > 255 || cmd > 255) { error = "Samsung32 address/command out of range"; return false; }
      uint8_t a = reverse8(addr), c = reverse8(cmd);
      value = (uint32_t(a) << 24) | (uint32_t(a) << 16) | (uint32_t(c) << 8) | (c ^ 0xFF);
      bits = 32;
    } else if (protocol == "SIRC" || protocol == "SIRC15") {
      bits = protocol == "SIRC" ? 12 : 15;
      if (addr > (protocol == "SIRC" ? 31U : 255U) || cmd > 127) {
        error = "Sony SIRC address/command out of range"; return false;
      }
      value = reverseValue((addr << 7) | cmd, bits);
      carrier = 40000;
    } else if (protocol == "RC5") {
      if (addr > 31 || cmd > 63) { error = "RC5 address/command out of range"; return false; }
      value = (addr << 6) | cmd;
      bits = 12; carrier = 36000;
    } else if (protocol == "RC6") {
      if (addr > 255 || cmd > 255) { error = "RC6 mode 0 address/command out of range"; return false; }
      value = (addr << 8) | cmd;
      bits = 20; carrier = 36000;
    } else {
      error = "Unsupported Flipper protocol: " + protocol + ". Import RAW instead"; return false;
    }
    char hex[19];
    snprintf(hex, sizeof(hex), "0x%llX", static_cast<unsigned long long>(value));
    spec["protocol"] = protocol == "NECext" ? "NEC" : protocol == "Samsung32" ? "SAMSUNG" :
                        (protocol == "SIRC" || protocol == "SIRC15") ? "SONY" : protocol;
    spec["source_protocol"] = protocol;
    spec["address"] = address;
    spec["command"] = command;
    spec["code"] = hex;
    spec["bits"] = bits;
    spec["frequency"] = carrier;
  } else {
    error = "Signal type must be parsed or raw"; return false;
  }
  return true;
}
}

bool FlipperImporter::parse(const String& content, const String& id, const String& device,
                           JsonDocument& output, String& error) {
  if (!ConfigStorage::validName(id) || device.isEmpty() || device.length() > 80 ||
      content.length() > MAX_CONFIG_BYTES) { error = "Invalid name, id or file size"; return false; }
  output["id"] = id;
  output["device"] = device;
  output["default_frequency"] = 38000;
  output["repeat_interval_ms"] = 250;
  output["receiver_enabled"] = false;
  output["origin"]["format"] = "Flipper IR";
  output["origin"]["target_verified"] = false;
  output["commands"].to<JsonObject>();
  output["pages"].to<JsonArray>();
  String label, type, protocol, address, command, frequency, duty, timings;
  bool header = false, version = false;
  auto flush = [&]() -> bool {
    if (label.isEmpty() && type.isEmpty()) return true;
    if (!addSignal(output, label, type, protocol, address, command, frequency, duty, timings, error)) return false;
    label = type = protocol = address = command = frequency = duty = timings = "";
    return true;
  };
  int position = 0;
  while (position < content.length()) {
    int end = content.indexOf('\n', position);
    if (end < 0) end = content.length();
    String line = content.substring(position, end);
    line.trim();
    position = end + 1;
    if (line.isEmpty()) continue;
    if (line[0] == '#') { if (!flush()) return false; continue; }
    int separator = line.indexOf(':');
    if (separator < 0) { error = "Malformed Flipper line"; return false; }
    String key = line.substring(0, separator), value = line.substring(separator + 1);
    key.trim(); value.trim();
    if (key == "Filetype") header = value == "IR signals file";
    else if (key == "Version") version = value == "1";
    else if (key == "name") { if (!label.isEmpty() && !flush()) return false; label = value; }
    else if (key == "type") type = value;
    else if (key == "protocol") protocol = value;
    else if (key == "address") address = value;
    else if (key == "command") command = value;
    else if (key == "frequency") frequency = value;
    else if (key == "duty_cycle") duty = value;
    else if (key == "data") timings = value;
  }
  if (!header || !version) { error = "Expected Flipper IR signals file, version 1"; return false; }
  if (!flush()) return false;
  if (!output["commands"].as<JsonObject>().size()) { error = "No IR signals found"; return false; }
  if (!FlipperButtonLayout::arrange(output, error)) return false;
  if (measureJson(output) > MAX_CONFIG_BYTES) { error = "Converted profile is too large"; return false; }
  return true;
}
