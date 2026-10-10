#include "DiagnosticLog.h"
#include <cstdarg>
#include <cstdio>

void DiagnosticLog::add(const char* category, const char* format, ...) {
  Entry& entry = entries_[next_];
  entry.id = nextId_++;
  entry.uptimeMs = millis();
  snprintf(entry.category, sizeof(entry.category), "%s", category);
  va_list arguments;
  va_start(arguments, format);
  vsnprintf(entry.message, sizeof(entry.message), format, arguments);
  va_end(arguments);
  next_ = (next_ + 1) % kCapacity;
  if (count_ < kCapacity) ++count_;
}

void DiagnosticLog::write(JsonDocument& output) const {
  output["uptime_ms"] = millis();
  output["capacity"] = kCapacity;
  JsonArray events = output["events"].to<JsonArray>();
  size_t start = (next_ + kCapacity - count_) % kCapacity;
  for (size_t index = 0; index < count_; ++index) {
    const Entry& entry = entries_[(start + index) % kCapacity];
    JsonObject event = events.add<JsonObject>();
    event["id"] = entry.id;
    event["uptime_ms"] = entry.uptimeMs;
    event["category"] = entry.category;
    event["message"] = entry.message;
  }
}
