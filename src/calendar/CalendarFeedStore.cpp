#include "CalendarFeedStore.h"

#include <ObfuscationUtils.h>

void CalendarFeedStore::toJson(JsonDocument& doc) const {
  doc["enabled"] = config.enabled;
  doc["url_obf"] = obfuscation::obfuscateToBase64(config.url);
}

bool CalendarFeedStore::fromJson(JsonVariantConst doc) {
  config.enabled = doc["enabled"] | false;
  obfuscation::DecodeStatus status = obfuscation::DecodeStatus::INVALID;
  config.url = obfuscation::deobfuscateFromBase64(doc["url_obf"] | "", &status);
  if ((status == obfuscation::DecodeStatus::INVALID || status == obfuscation::DecodeStatus::EMPTY) &&
      config.url.empty()) {
    // Tolerate a hand-edited plaintext link; the next save obfuscates it.
    config.url = doc["url"] | "";
    if (!config.url.empty()) requestResave();
  }
  return true;
}

void CalendarFeedStore::ensureLoaded() const {
  if (loaded_) return;
  auto* self = const_cast<CalendarFeedStore*>(this);
  self->loaded_ = true;
  self->PersistableStore<CalendarFeedStore>::loadFromFile();
}

bool CalendarFeedStore::setConfig(const CalendarFeedConfig& newConfig) {
  config = newConfig;
  loaded_ = true;
  return saveToFile();
}
