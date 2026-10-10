#include "FlipperImporter.h"
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
bool hexBytes(const String& input, uint32_t& value) {
  char b[9] = {};
  unsigned n[4] = {};
  if (sscanf(input.c_str(), "%x %x %x %x %8s", &n[0], &n[1], &n[2], &n[3], b) != 4) return false;
  for (unsigned x : n) if (x > 255) return false;
  value = n[0] | (n[1] << 8) | (n[2] << 16) | (n[3] << 24);
  return true;
}
String keyFor(const String& label) {
  String lower = label;
  lower.toLowerCase();
  if (lower == "vol_up" || lower == "vol+" || lower == "volume_up" || lower == "vol_p") return "volume_up";
  if (lower == "vol_down" || lower == "vol_dn" || lower == "vol-" || lower == "volume_down") return "volume_down";
  if (lower == "chnl_next" || lower == "ch_next" || lower == "channel_up") return "channel_up";
  if (lower == "chnl_prev" || lower == "ch_prev" || lower == "channel_down") return "channel_down";
  if (lower == "return" || lower == "back") return "back";
  if (lower == "enter" || lower == "select") return "ok";
  if (lower == "source") return "input";
  if (lower == "fav") return "favorite";
  if (lower.length() == 1 && lower[0] >= '0' && lower[0] <= '9') return "digit_" + lower;
  String result;
  for (char c : lower) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) result += c;
    else if (result.length() && result[result.length() - 1] != '_') result += '_';
  }
  while (result.endsWith("_")) result.remove(result.length() - 1);
  if (result.length() > 28) result.remove(28);
  if (result.isEmpty() || !(result[0] >= 'a' && result[0] <= 'z')) result = "key_" + result;
  return result;
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
  String key = keyFor(label);
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
    spec["protocol"] = protocol == "NECext" ? "NEC" : protocol;
    spec["source_protocol"] = protocol;
    spec["address"] = address;
    spec["command"] = command;
    spec["code"] = hex;
    spec["bits"] = bits;
    spec["frequency"] = carrier;
  } else {
    error = "Signal type must be parsed or raw"; return false;
  }
  size_t index = commands.size() - 1;
  size_t pageNumber = index / 24;
  if (index % 24 == 0) {
    JsonObject page = target["pages"].as<JsonArray>().add<JsonObject>();
    page["id"] = String("page_") + (pageNumber + 1);
    page["label"] = pageNumber ? String("Keys ") + (pageNumber + 1) : "Remote";
    page["groups"].to<JsonArray>();
  }
  JsonObject page = target["pages"][pageNumber].as<JsonObject>();
  if (index % 12 == 0) {
    JsonObject group = page["groups"].as<JsonArray>().add<JsonObject>();
    group["label"] = index ? "MORE KEYS" : "KEYS";
    group["columns"] = 3;
    group["buttons"].to<JsonArray>();
  }
  JsonArray groups = page["groups"].as<JsonArray>();
  JsonObject button = groups[groups.size() - 1]["buttons"].as<JsonArray>().add<JsonObject>();
  button["command"] = key;
  button["label"] = label.length() > 32 ? label.substring(0, 32) : label;
  button["repeat"] = key == "volume_up" || key == "volume_down" || key == "channel_up" || key == "channel_down" || key == "up" || key == "down" || key == "left" || key == "right";
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
  if (measureJson(output) > MAX_CONFIG_BYTES) { error = "Converted profile is too large"; return false; }
  return true;
}
