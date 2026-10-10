#include "NecRmtSender.h"

#if defined(ARDUINO_XIAO_ESP32C3)

#include <Arduino.h>
#include <driver/rmt.h>
#include <ir_NEC.h>

namespace {
constexpr rmt_channel_t kChannel = RMT_CHANNEL_0;

void restoreGpio() {
  // IRremoteESP8266 uses this pin for software PWM on other protocols.
  pinMode(IR_TX_PIN, OUTPUT);
  digitalWrite(IR_TX_PIN, LOW);
}
}  // namespace

NecRmtResult sendNecRmt(uint64_t code, uint16_t bits, uint32_t frequency,
                        DiagnosticLog& log) {
  if (bits != 32 || frequency < 30000 || frequency > 60000)
    return NecRmtResult::Unavailable;

  // One header, 32 data bits, a footer, and at most one extra gap item.
  rmt_item32_t items[35] = {};
  size_t count = 0;
  items[count].duration0 = kNecHdrMark;
  items[count].level0 = 1;
  items[count].duration1 = kNecHdrSpace;
  items[count++].level1 = 0;
  uint32_t elapsed = kNecHdrMark + kNecHdrSpace;

  for (int bit = 31; bit >= 0; --bit) {
    const uint16_t space = (code & (1ULL << bit)) ? kNecOneSpace : kNecZeroSpace;
    items[count].duration0 = kNecBitMark;
    items[count].level0 = 1;
    items[count].duration1 = space;
    items[count++].level1 = 0;
    elapsed += kNecBitMark + space;
  }

  elapsed += kNecBitMark;
  const uint32_t needed = kNecMinCommandLength > elapsed
                              ? kNecMinCommandLength - elapsed : 0;
  uint32_t gap = needed > kNecMinGap ? needed : kNecMinGap;
  items[count].duration0 = kNecBitMark;
  items[count].level0 = 1;
  items[count].duration1 = gap > 32767 ? 32767 : gap;
  items[count++].level1 = 0;
  gap -= items[count - 1].duration1;
  if (gap) {
    items[count].duration0 = gap;
    items[count].level0 = 0;
    items[count].duration1 = 1;
    items[count++].level1 = 0;
  }

  rmt_config_t config = {};
  config.rmt_mode = RMT_MODE_TX;
  config.channel = kChannel;
  config.gpio_num = static_cast<gpio_num_t>(IR_TX_PIN);
  config.clk_div = 80;  // 80 MHz APB clock / 80 = one microsecond per tick.
  config.mem_block_num = 1;
  config.tx_config.carrier_freq_hz = frequency;
  config.tx_config.carrier_duty_percent = 33;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.carrier_en = true;
  config.tx_config.idle_output_en = true;

  esp_err_t result = rmt_config(&config);
  if (result != ESP_OK) {
    restoreGpio();
    log.add("ir", "RMT configuration failed: %d", static_cast<int>(result));
    return NecRmtResult::Unavailable;
  }
  result = rmt_driver_install(kChannel, 0, 0);
  if (result != ESP_OK) {
    restoreGpio();
    log.add("ir", "RMT driver failed: %d", static_cast<int>(result));
    return NecRmtResult::Unavailable;
  }

  result = rmt_write_items(kChannel, items, count, true);
  const esp_err_t cleanup = rmt_driver_uninstall(kChannel);
  restoreGpio();
  if (result != ESP_OK) {
    log.add("ir", "RMT send failed: %d", static_cast<int>(result));
    return NecRmtResult::Failed;
  }
  if (cleanup != ESP_OK) {
    log.add("ir", "RMT cleanup failed: %d", static_cast<int>(cleanup));
    return NecRmtResult::Failed;
  }
  return NecRmtResult::Sent;
}

#endif
