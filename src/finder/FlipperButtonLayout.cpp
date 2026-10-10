#include "FlipperButtonLayout.h"
#include "storage/ConfigStorage.h"
#include <initializer_list>

namespace {
const char* const kMain[] = {"power", "power_on", "power_off", "input"};
const char* const kVolume[] = {"volume_up", "volume_down"};
const char* const kChannel[] = {"channel_up", "channel_down"};
const char* const kNavigation[] = {"up", "left", "ok", "right", "down"};
const char* const kSystem[] = {"home", "back", "menu", "exit"};
const char* const kSystemExtra[] = {"info", "guide", "settings", "smart_hub"};
const char* const kMediaMain[] = {"mute", "play", "pause", "stop"};
const char* const kMediaFull[] = {"mute", "previous", "rewind", "play", "play_pause", "pause", "stop", "fast_forward", "next", "record"};
const char* const kMediaMore[] = {"previous", "rewind", "play_pause", "fast_forward", "next", "record"};
const char* const kBrightness[] = {"brightness_down", "brightness_up"};
const char* const kNumbers[] = {"digit_1", "digit_2", "digit_3", "digit_4", "digit_5", "digit_6", "digit_7", "digit_8", "digit_9", "digit_0"};
const char* const kColors[] = {"red", "green", "yellow", "blue"};
const char* const kExtras[] = {"favorite", "subtitle", "audio", "sleep", "teletext", "teletext_mix", "text", "previous_channel", "channel_list", "tools", "apps"};

struct GroupDef {
  const char* label;
  uint8_t columns;
  const char* const* keys;
  size_t count;
  bool navigation;
};

#define GROUP(label, columns, keys, navigation) {label, columns, keys, sizeof(keys) / sizeof(keys[0]), navigation}
const GroupDef kPrimary[] = {
  GROUP("MAIN", 2, kMain, false),
  GROUP("VOLUME", 2, kVolume, false),
  GROUP("CHANNEL", 2, kChannel, false),
  GROUP("NAVIGATION", 3, kNavigation, true),
  GROUP("SYSTEM", 4, kSystem, false),
  GROUP("MEDIA", 4, kMediaMain, false),
  GROUP("BRIGHTNESS", 2, kBrightness, false),
};
const GroupDef kSecondary[] = {
  GROUP("NUMBERS", 3, kNumbers, false),
  GROUP("MEDIA", 3, kMediaMore, false),
  GROUP("SYSTEM", 4, kSystemExtra, false),
  GROUP("COLOURS", 4, kColors, false),
  GROUP("EXTRAS", 3, kExtras, false),
};
#undef GROUP

bool oneOf(const String& value, std::initializer_list<const char*> names) {
  for (const char* name : names) if (value == name) return true;
  return false;
}

String slug(const String& value) {
  String output;
  for (char ch : value) {
    char c = ch >= 'A' && ch <= 'Z' ? ch + ('a' - 'A') : ch;
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) output += c;
    else if (output.length() && output[output.length() - 1] != '_') output += '_';
  }
  while (output.endsWith("_")) output.remove(output.length() - 1);
  if (output.length() > 28) output.remove(28);
  if (output.isEmpty() || output[0] < 'a' || output[0] > 'z') output = "key_" + output;
  if (output.length() > 32) output.remove(32);
  return output;
}

const char* displayName(const String& key) {
  if (key == "power") return "Power";
  if (key == "power_on") return "Power On";
  if (key == "power_off") return "Power Off";
  if (key == "input") return "Source";
  if (key == "volume_down") return "Vol -";
  if (key == "volume_up") return "Vol +";
  if (key == "channel_down") return "CH -";
  if (key == "channel_up") return "CH +";
  if (key == "mute") return "Mute";
  if (key == "up") return "Up";
  if (key == "down") return "Down";
  if (key == "left") return "Left";
  if (key == "right") return "Right";
  if (key == "ok") return "OK";
  if (key == "home") return "Home";
  if (key == "back") return "Back";
  if (key == "menu") return "Menu";
  if (key == "exit") return "Exit";
  if (key == "info") return "Info";
  if (key == "guide") return "Guide";
  if (key == "settings") return "Settings";
  if (key == "play") return "Play";
  if (key == "pause") return "Pause";
  if (key == "stop") return "Stop";
  if (key == "play_pause") return "Play / Pause";
  if (key == "previous") return "Previous";
  if (key == "next") return "Next";
  if (key == "rewind") return "Rewind";
  if (key == "fast_forward") return "Fast Forward";
  if (key == "favorite") return "Favorite";
  if (key == "subtitle") return "Subtitles";
  if (key == "previous_channel") return "Last Channel";
  if (key == "channel_list") return "Channel List";
  if (key == "teletext") return "Teletext";
  if (key == "teletext_mix") return "Teletext Mix";
  if (key == "smart_hub") return "Smart Hub";
  if (key == "brightness_down") return "Dim -";
  if (key == "brightness_up") return "Dim +";
  return nullptr;
}

String buttonLabel(const String& key, JsonVariantConst signal) {
  const char* friendly = displayName(key);
  if (friendly) return friendly;
  if (key.startsWith("digit_") && key.length() == 7) return key.substring(6);
  String label = signal["source_name"].as<String>();
  if (label.isEmpty()) label = key;
  label.replace('_', ' ');
  label.replace('-', ' ');
  label.trim();
  if (label.length() > 32) label.remove(32);
  if (label.length() && label[0] >= 'a' && label[0] <= 'z') label.setCharAt(0, label[0] - ('a' - 'A'));
  return label;
}

const char* iconFor(const String& key) {
  if (key == "power" || key == "power_on" || key == "power_off") return "power";
  if (key == "input") return "input";
  if (key == "volume_up" || key == "channel_up" || key == "brightness_up") return "plus";
  if (key == "volume_down" || key == "channel_down" || key == "brightness_down") return "minus";
  if (key == "mute") return "mute";
  if (key == "up") return "up";
  if (key == "down") return "down";
  if (key == "left") return "left";
  if (key == "right") return "right";
  if (key == "home") return "home";
  if (key == "back") return "back";
  if (key == "menu") return "menu";
  if (key == "exit") return "exit";
  if (key == "play") return "play";
  if (key == "pause") return "pause";
  if (key == "stop") return "stop";
  if (key == "info") return "info";
  if (key == "settings") return "settings";
  if (key == "favorite") return "star";
  return nullptr;
}

bool repeatable(const String& key) {
  return oneOf(key, {"volume_up", "volume_down", "channel_up", "channel_down", "brightness_up", "brightness_down", "up", "down", "left", "right"});
}

void addButton(JsonArray buttons, JsonObjectConst commands, const String& key,
               const String& role, bool navigation = false) {
  JsonObject button = buttons.add<JsonObject>();
  button["command"] = key;
  button["label"] = buttonLabel(role, commands[key]);
  const char* icon = iconFor(role);
  if (icon) button["icon"] = icon;
  if (role == "power" || role == "power_on" || role == "power_off") button["tone"] = "power";
  else if (role == "ok") button["tone"] = "accent";
  else if (oneOf(role, {"red", "green", "yellow", "blue"})) button["tone"] = role;
  if (repeatable(role)) button["repeat"] = true;
  if (navigation) {
    if (role == "up") { button["column"] = 2; button["row"] = 1; }
    else if (role == "down") { button["column"] = 2; button["row"] = 3; }
    else if (role == "left") { button["column"] = 1; button["row"] = 2; }
    else if (role == "right") { button["column"] = 3; button["row"] = 2; }
    else if (role == "ok") { button["column"] = 2; button["row"] = 2; }
  }
}

bool usedKey(const String& used, const String& key) {
  return used.indexOf("," + key + ",") >= 0;
}

String findKey(JsonObjectConst commands, const String& role, const String& used) {
  if (commands[role].is<JsonObjectConst>() && !usedKey(used, role)) return role;
  for (JsonPairConst entry : commands) {
    String key = entry.key().c_str();
    if (usedKey(used, key)) continue;
    String source = entry.value()["source_name"].as<String>();
    if (source.isEmpty()) source = key;
    if (FlipperButtonLayout::commandKey(source) == role) return key;
  }
  return "";
}

size_t addGroup(JsonObject page, JsonObjectConst commands, const GroupDef& definition, String& used) {
  size_t count = 0;
  for (size_t i = 0; i < definition.count; ++i)
    if (!findKey(commands, definition.keys[i], used).isEmpty()) ++count;
  if (!count) return 0;
  JsonObject group = page["groups"].as<JsonArray>().add<JsonObject>();
  group["label"] = definition.label;
  group["columns"] = definition.columns;
  JsonArray buttons = group["buttons"].to<JsonArray>();
  for (size_t i = 0; i < definition.count; ++i) {
    String role = definition.keys[i];
    String key = findKey(commands, role, used);
    if (key.isEmpty()) continue;
    addButton(buttons, commands, key, role, definition.navigation);
    used += key + ",";
  }
  return count;
}

JsonObject addPage(JsonArray pages, const String& id, const String& label) {
  JsonObject page = pages.add<JsonObject>();
  page["id"] = id;
  page["label"] = label;
  page["groups"].to<JsonArray>();
  return page;
}
}  // namespace

String FlipperButtonLayout::commandKey(const String& sourceName) {
  String lower = sourceName;
  lower.trim(); lower.toLowerCase();
  if (oneOf(lower, {"vol+", "volume+"})) return "volume_up";
  if (oneOf(lower, {"vol-", "volume-"})) return "volume_down";
  if (oneOf(lower, {"ch+", "channel+"})) return "channel_up";
  if (oneOf(lower, {"ch-", "channel-"})) return "channel_down";
  String key = slug(lower);
  if (oneOf(key, {"pwr", "power_toggle", "on_off", "off_on"})) return "power";
  if (oneOf(key, {"pwr_on", "on"})) return "power_on";
  if (oneOf(key, {"pwr_off", "off"})) return "power_off";
  if (oneOf(key, {"vol_up", "vol_p", "vol_plus", "volume_plus"})) return "volume_up";
  if (oneOf(key, {"vol_dn", "vol_down", "vol_minus", "volume_dn", "volume_minus"})) return "volume_down";
  if (oneOf(key, {"ch_next", "chnl_next", "ch_up", "chnl_up", "channel_next", "channel_plus", "program_up"})) return "channel_up";
  if (oneOf(key, {"ch_prev", "chnl_prev", "ch_down", "chnl_down", "channel_prev", "channel_previous", "channel_minus", "program_down"})) return "channel_down";
  if (key == "mte") return "mute";
  if (oneOf(key, {"source", "source_input"})) return "input";
  if (oneOf(key, {"enter", "select", "confirm"})) return "ok";
  if (key == "return") return "back";
  if (key == "fav" || key == "favorites") return "favorite";
  if (key == "subtitles") return "subtitle";
  if (oneOf(key, {"playpause", "play_pause_toggle"})) return "play_pause";
  if (oneOf(key, {"prev", "track_prev", "skip_prev"})) return "previous";
  if (oneOf(key, {"track_next", "skip_next"})) return "next";
  if (oneOf(key, {"ff", "fastforward", "forwards"})) return "fast_forward";
  if (oneOf(key, {"rew", "rw", "reverse", "backwards"})) return "rewind";
  if (oneOf(key, {"pre_ch", "last_channel"})) return "previous_channel";
  if (key == "ch_list") return "channel_list";
  if (key == "ttx_mix") return "teletext_mix";
  if (key == "ttx" || key == "txt") return "teletext";
  if (key == "bright_up" || key == "dim_up") return "brightness_up";
  if (key == "bright_dn" || key == "bright_down" || key == "dim_down") return "brightness_down";
  if (oneOf(key, {"nav_up", "cursor_up", "arrow_up"})) return "up";
  if (oneOf(key, {"nav_down", "cursor_down", "arrow_down"})) return "down";
  if (oneOf(key, {"nav_left", "cursor_left", "arrow_left"})) return "left";
  if (oneOf(key, {"nav_right", "cursor_right", "arrow_right"})) return "right";
  if (key.length() == 5 && key.startsWith("key_") && key[4] >= '0' && key[4] <= '9') return "digit_" + key.substring(4);
  if (key.length() == 5 && key.startsWith("num_") && key[4] >= '0' && key[4] <= '9') return "digit_" + key.substring(4);
  if (key.length() == 8 && key.startsWith("number_") && key[7] >= '0' && key[7] <= '9') return "digit_" + key.substring(7);
  if (oneOf(key, {"color_red", "colour_red"})) return "red";
  if (oneOf(key, {"color_green", "colour_green"})) return "green";
  if (oneOf(key, {"color_yellow", "colour_yellow"})) return "yellow";
  if (oneOf(key, {"color_blue", "colour_blue"})) return "blue";
  return key;
}

bool FlipperButtonLayout::arrange(JsonDocument& profile, String& error) {
  JsonObjectConst commands = profile["commands"].as<JsonObjectConst>();
  if (!commands.size() || commands.size() > 96) { error = "No valid commands to arrange"; return false; }
  JsonArray pages = profile["pages"].to<JsonArray>();
  JsonObject remote = addPage(pages, "remote", "Remote");
  String used = ",";
  bool tvLike = !findKey(commands, "channel_up", used).isEmpty() ||
                !findKey(commands, "channel_down", used).isEmpty() ||
                !findKey(commands, "up", used).isEmpty() ||
                !findKey(commands, "down", used).isEmpty();
  bool tvLayout = !findKey(commands, "up", used).isEmpty() &&
                  !findKey(commands, "down", used).isEmpty() &&
                  !findKey(commands, "left", used).isEmpty() &&
                  !findKey(commands, "right", used).isEmpty() &&
                  !findKey(commands, "ok", used).isEmpty() &&
                  (!findKey(commands, "volume_up", used).isEmpty() || !findKey(commands, "volume_down", used).isEmpty()) &&
                  (!findKey(commands, "channel_up", used).isEmpty() || !findKey(commands, "channel_down", used).isEmpty());
  if (tvLayout) remote["layout"] = "tv";
  size_t primaryCount = 0;
  for (const GroupDef& group : kPrimary) {
    GroupDef adjusted = group;
    if (group.keys == kMediaMain && !tvLike) {
      adjusted.keys = kMediaFull;
      adjusted.count = sizeof(kMediaFull) / sizeof(kMediaFull[0]);
      adjusted.columns = 3;
    }
    if (tvLayout && (group.keys == kVolume || group.keys == kChannel)) adjusted.columns = 1;
    primaryCount += addGroup(remote, commands, adjusted, used);
  }
  JsonObject current = remote;
  if (primaryCount && commands.size() > primaryCount) current = addPage(pages, "more", "More");
  size_t secondaryCount = 0;
  for (const GroupDef& group : kSecondary) secondaryCount += addGroup(current, commands, group, used);
  size_t pageButtons = secondaryCount;
  JsonObject otherGroup;
  size_t extraPageNumber = 2;
  for (JsonPairConst entry : commands) {
    String key = entry.key().c_str();
    if (usedKey(used, key)) continue;
    if (pageButtons >= 40) {
      current = addPage(pages, String("keys_") + extraPageNumber, String("Keys ") + extraPageNumber);
      ++extraPageNumber;
      pageButtons = 0;
      otherGroup = JsonObject();
    }
    if (otherGroup.isNull()) {
      otherGroup = current["groups"].as<JsonArray>().add<JsonObject>();
      otherGroup["label"] = "OTHER KEYS";
      otherGroup["columns"] = 3;
      otherGroup["buttons"].to<JsonArray>();
    }
    String source = entry.value()["source_name"].as<String>();
    addButton(otherGroup["buttons"].as<JsonArray>(), commands, key,
              FlipperButtonLayout::commandKey(source.isEmpty() ? key : source));
    ++pageButtons;
  }
  profile["origin"]["layout_version"] = 2;
  if (pages.size() > 6) { error = "Too many pages for this remote"; return false; }
  return true;
}
