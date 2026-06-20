#include "StreetPassController.h"

#include <LittleFS.h>
#include <esp_system.h>

namespace {
constexpr const char* kSettingsNamespace = "streetpass";
constexpr const char* kRecordsPath = "/streetpass_records.json";
constexpr size_t kNameMaxLen = 48;
constexpr size_t kMessageMaxLen = 80;
constexpr size_t kPeerKeyMaxLen = 48;
constexpr uint32_t kEncounterMergeWindowSeconds = 24UL * 60UL * 60UL;
}

void StreetPassController::begin(Preferences* preferences) {
  preferences_ = preferences;
  loadSettings();
  ensureProfileId();
  bootId_ += 1;
  saveSettings();
  loadRecords();
}

void StreetPassController::update(unsigned long now) {
  (void)now;
  if (dirty_) {
    saveRecords();
  }
}

bool StreetPassController::enabled() const {
  return profile_.enabled;
}

uint8_t StreetPassController::storedCount() const {
  return recordCount_;
}

uint8_t StreetPassController::unreadCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < recordCount_; ++i) {
    if (records_[i].unread) {
      ++count;
    }
  }
  return count;
}

uint8_t StreetPassController::unsyncedCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < recordCount_; ++i) {
    if (!records_[i].synced) {
      ++count;
    }
  }
  return count;
}

uint32_t StreetPassController::droppedCount() const {
  return droppedCount_;
}

const StreetPassRecord* StreetPassController::recordAt(uint8_t index) const {
  return index < recordCount_ ? &records_[index] : nullptr;
}

const StreetPassProfile& StreetPassController::profile() const {
  return profile_;
}

void StreetPassController::markAllRead() {
  bool changed = false;
  for (uint8_t i = 0; i < recordCount_; ++i) {
    if (records_[i].unread) {
      records_[i].unread = false;
      changed = true;
    }
  }
  if (changed) {
    dirty_ = true;
    saveRecords();
  }
}

bool StreetPassController::syncTime(uint32_t unixTime, const char* timezone, unsigned long now) {
  if (unixTime == 0) {
    return false;
  }
  timeSynced_ = true;
  timeAnchorUnix_ = unixTime;
  timeAnchorMillis_ = static_cast<uint32_t>(now);
  timezone_ = timezone == nullptr || timezone[0] == '\0' ? "UTC" : timezone;
  saveSettings();
  return true;
}

bool StreetPassController::recordPublicCard(const char* publicCardJson, int rssi, unsigned long now) {
  if (!profile_.enabled || publicCardJson == nullptr || publicCardJson[0] == '\0') {
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, publicCardJson);
  if (error) {
    Serial.printf("[streetpass] public card parse failed: %s\n", error.c_str());
    return false;
  }

  const char* profileId = doc["profileId"] | "";
  const char* name = doc["name"] | "Stack-chan";
  const char* message = doc["message"] | "";
  const uint32_t cardSeq = doc["cardSeq"] | 0;
  if (profileId[0] == '\0') {
    Serial.println("[streetpass] public card missing profileId");
    return false;
  }
  if (profile_.profileId.length() > 0 && profile_.profileId == profileId) {
    Serial.println("[streetpass] ignored own public card");
    return false;
  }

  return upsertRecord(profileId, name, message, cardSeq, rssi, now, "ble");
}

void StreetPassController::writeStatus(JsonObject target) const {
  target["enabled"] = profile_.enabled;
  target["shareProfile"] = profile_.shareProfile;
  target["storedCount"] = recordCount_;
  target["capacity"] = kMaxRecords;
  target["unreadCount"] = unreadCount();
  target["unsyncedCount"] = unsyncedCount();
  target["droppedCount"] = droppedCount_;
  target["timeSynced"] = timeSynced_;
}

bool StreetPassController::handleJsonCommand(JsonDocument& request, JsonDocument& response, unsigned long now) {
  const char* type = request["type"] | "";
  const char* requestId = request["requestId"] | "";

  if (strcmp(type, "streetpass.profile.get") == 0) {
    writeProfileResponse(response, requestId);
    return true;
  }
  if (strcmp(type, "streetpass.profile.set") == 0) {
    applyProfileSet(request);
    writeProfileResponse(response, requestId);
    return true;
  }
  if (strcmp(type, "streetpass.encounters.get") == 0) {
    const uint32_t sinceRecordId = request["sinceRecordId"] | 0;
    const uint8_t limit = min<uint8_t>(request["limit"] | kMaxRecords, kMaxRecords);
    writeEncountersResponse(response, requestId, sinceRecordId, limit);
    return true;
  }
  if (strcmp(type, "streetpass.encounters.ack") == 0) {
    const bool changed = applyAck(request);
    response["type"] = "streetpass.encounters.ack";
    if (requestId[0] != '\0') {
      response["requestId"] = requestId;
    }
    response["ok"] = true;
    response["changed"] = changed;
    response["storedCount"] = recordCount_;
    response["unsyncedCount"] = unsyncedCount();
    response["unreadCount"] = unreadCount();
    return true;
  }
  if (strcmp(type, "streetpass.encounters.delete") == 0) {
    const uint8_t deleted = applyDelete(request);
    response["type"] = "streetpass.encounters.delete";
    if (requestId[0] != '\0') {
      response["requestId"] = requestId;
    }
    response["ok"] = true;
    response["deletedCount"] = deleted;
    response["storedCount"] = recordCount_;
    response["unsyncedCount"] = unsyncedCount();
    response["unreadCount"] = unreadCount();
    return true;
  }
  if (strcmp(type, "streetpass.time.get") == 0) {
    writeTimeResponse(response, requestId, now);
    return true;
  }
  if (strcmp(type, "streetpass.time.set") == 0) {
    const bool ok = applyTimeSet(request, now);
    writeTimeResponse(response, requestId, now);
    response["ok"] = ok;
    return true;
  }
  if (strcmp(type, "streetpass.notifications.clear") == 0) {
    markAllRead();
    response["type"] = "streetpass.notifications";
    if (requestId[0] != '\0') {
      response["requestId"] = requestId;
    }
    response["unreadCount"] = unreadCount();
    return true;
  }
  if (strcmp(type, "streetpass.debug.inject") == 0) {
    const bool ok = injectRecord(request, now);
    response["type"] = "streetpass.debug.inject";
    if (requestId[0] != '\0') {
      response["requestId"] = requestId;
    }
    response["ok"] = ok;
    response["storedCount"] = recordCount_;
    response["unreadCount"] = unreadCount();
    response["unsyncedCount"] = unsyncedCount();
    return true;
  }

  return false;
}

void StreetPassController::loadSettings() {
  if (preferences_ == nullptr) {
    return;
  }
  preferences_->begin(kSettingsNamespace, true);
  profile_.enabled = preferences_->getBool("enabled", true);
  profile_.shareProfile = preferences_->getBool("share", true);
  profile_.profileId = preferences_->getString("profile_id", "");
  profile_.name = preferences_->getString("name", STREETPASS_DEFAULT_NAME);
  profile_.message = preferences_->getString("message", STREETPASS_DEFAULT_MESSAGE);
  profile_.cardSeq = preferences_->getUInt("card_seq", 1);
  nextRecordId_ = preferences_->getUInt("next_id", 1);
  bootId_ = preferences_->getUInt("boot_id", 0);
  droppedCount_ = preferences_->getUInt("dropped", 0);
  timeSynced_ = preferences_->getBool("time_sync", false);
  timeAnchorUnix_ = preferences_->getUInt("time_unix", 0);
  timeAnchorMillis_ = preferences_->getUInt("time_ms", 0);
  timezone_ = preferences_->getString("timezone", "UTC");
  preferences_->end();
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  const bool migratedDefaultName = profile_.name == STREETPASS_LEGACY_DEFAULT_NAME;
  if (migratedDefaultName) {
    profile_.name = STREETPASS_DEFAULT_NAME;
  }
#endif
  timeSynced_ = false;
  timeAnchorMillis_ = 0;
  copyLimited(profile_.name, profile_.name.c_str(), kNameMaxLen);
  copyLimited(profile_.message, profile_.message.c_str(), kMessageMaxLen);
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  if (migratedDefaultName) {
    saveSettings();
  }
#endif
}

void StreetPassController::saveSettings() {
  if (preferences_ == nullptr) {
    return;
  }
  preferences_->begin(kSettingsNamespace, false);
  preferences_->putBool("enabled", profile_.enabled);
  preferences_->putBool("share", profile_.shareProfile);
  preferences_->putString("profile_id", profile_.profileId);
  preferences_->putString("name", profile_.name);
  preferences_->putString("message", profile_.message);
  preferences_->putUInt("card_seq", profile_.cardSeq);
  preferences_->putUInt("next_id", nextRecordId_);
  preferences_->putUInt("boot_id", bootId_);
  preferences_->putUInt("dropped", droppedCount_);
  preferences_->putBool("time_sync", timeSynced_);
  preferences_->putUInt("time_unix", timeAnchorUnix_);
  preferences_->putUInt("time_ms", timeAnchorMillis_);
  preferences_->putString("timezone", timezone_);
  preferences_->end();
}

void StreetPassController::loadRecords() {
  recordCount_ = 0;
  if (!LittleFS.exists(kRecordsPath)) {
    return;
  }

  File file = LittleFS.open(kRecordsPath, "r");
  if (!file) {
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  if (error) {
    Serial.printf("[streetpass] records parse failed: %s\n", error.c_str());
    return;
  }

  JsonArray records = doc["records"].as<JsonArray>();
  for (JsonObject item : records) {
    if (recordCount_ >= kMaxRecords) {
      break;
    }
    StreetPassRecord& record = records_[recordCount_++];
    record.recordId = item["recordId"] | 0;
    record.peerKey = item["peerKey"] | "";
    record.peerName = item["peerName"] | "Stack-chan";
    record.peerMessage = item["peerMessage"] | "";
    record.cardSeq = item["cardSeq"] | 0;
    record.unread = item["unread"] | false;
    record.synced = item["synced"] | false;
    record.bootId = item["bootId"] | 0;
    record.firstSeenMs = item["firstSeenMs"] | 0;
    record.lastSeenMs = item["lastSeenMs"] | 0;
    record.seenCount = item["seenCount"] | 1;
    record.rssiMax = item["rssiMax"] | -127;
    record.seenUnix = item["seenUnix"] | 0;
    record.timeQuality = item["timeQuality"] | "unknown";
    copyLimited(record.peerKey, record.peerKey.c_str(), kPeerKeyMaxLen);
    copyLimited(record.peerName, record.peerName.c_str(), kNameMaxLen);
    copyLimited(record.peerMessage, record.peerMessage.c_str(), kMessageMaxLen);
  }
}

void StreetPassController::saveRecords() {
  JsonDocument doc;
  JsonArray records = doc["records"].to<JsonArray>();
  for (uint8_t i = 0; i < recordCount_; ++i) {
    JsonObject item = records.add<JsonObject>();
    writeRecord(item, records_[i]);
  }

  File file = LittleFS.open(kRecordsPath, "w");
  if (!file) {
    Serial.println("[streetpass] records open failed");
    return;
  }
  serializeJson(doc, file);
  file.close();
  dirty_ = false;
}

void StreetPassController::ensureProfileId() {
  if (profile_.profileId.length() == 0) {
    profile_.profileId = generateProfileId();
  }
}

String StreetPassController::generateProfileId() {
  char buffer[33];
  const uint32_t a = esp_random();
  const uint32_t b = esp_random();
  const uint32_t c = esp_random();
  const uint32_t d = esp_random();
  snprintf(buffer, sizeof(buffer), "%08lx%08lx%08lx%08lx",
           static_cast<unsigned long>(a),
           static_cast<unsigned long>(b),
           static_cast<unsigned long>(c),
           static_cast<unsigned long>(d));
  return String(buffer);
}

void StreetPassController::writeProfileResponse(JsonDocument& response, const char* requestId) const {
  response["type"] = "streetpass.profile";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["enabled"] = profile_.enabled;
  response["shareProfile"] = profile_.shareProfile;
  response["profileId"] = profile_.shareProfile ? profile_.profileId : "";
  response["name"] = profile_.name;
  response["message"] = profile_.message;
  response["cardSeq"] = profile_.cardSeq;
}

void StreetPassController::writeEncountersResponse(JsonDocument& response, const char* requestId,
                                                   uint32_t sinceRecordId, uint8_t limit) const {
  response["type"] = "streetpass.encounters";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["capacity"] = kMaxRecords;
  response["storedCount"] = recordCount_;
  response["unreadCount"] = unreadCount();
  response["unsyncedCount"] = unsyncedCount();
  response["droppedCount"] = droppedCount_;
  JsonArray records = response["records"].to<JsonArray>();

  uint8_t emitted = 0;
  for (uint8_t i = 0; i < recordCount_ && emitted < limit; ++i) {
    const StreetPassRecord& record = records_[i];
    if (record.recordId <= sinceRecordId) {
      continue;
    }
    JsonObject item = records.add<JsonObject>();
    writeRecord(item, record);
    ++emitted;
  }
}

void StreetPassController::writeTimeResponse(JsonDocument& response, const char* requestId, unsigned long now) const {
  response["type"] = "streetpass.time";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["timeSynced"] = timeSynced_;
  response["unixTime"] = estimatedUnix(now);
  response["timezone"] = timezone_;
  response["bootId"] = bootId_;
  response["millis"] = static_cast<uint32_t>(now);
}

bool StreetPassController::applyProfileSet(JsonDocument& request) {
  bool changed = false;
  if (!request["enabled"].isNull()) {
    const bool enabled = request["enabled"] | true;
    changed |= enabled != profile_.enabled;
    profile_.enabled = enabled;
  }
  if (!request["shareProfile"].isNull()) {
    const bool share = request["shareProfile"] | true;
    changed |= share != profile_.shareProfile;
    profile_.shareProfile = share;
  }
  if (!request["name"].isNull()) {
    String next;
    copyLimited(next, request["name"] | STREETPASS_DEFAULT_NAME, kNameMaxLen);
    changed |= next != profile_.name;
    profile_.name = next;
  }
  if (!request["message"].isNull()) {
    String next;
    copyLimited(next, request["message"] | "", kMessageMaxLen);
    changed |= next != profile_.message;
    profile_.message = next;
  }
  if (changed) {
    ++profile_.cardSeq;
    saveSettings();
  }
  return changed;
}

bool StreetPassController::applyTimeSet(JsonDocument& request, unsigned long now) {
  const uint32_t unixTime = request["unixTime"] | 0;
  return syncTime(unixTime, request["timezone"] | "UTC", now);
}

bool StreetPassController::applyAck(JsonDocument& request) {
  JsonArray ids = request["recordIds"].as<JsonArray>();
  const bool markSynced = request["markSynced"] | true;
  const bool markRead = request["markRead"] | false;
  bool changed = false;

  for (JsonVariant value : ids) {
    const uint32_t recordId = value.as<uint32_t>();
    for (uint8_t i = 0; i < recordCount_; ++i) {
      StreetPassRecord& record = records_[i];
      if (record.recordId != recordId) {
        continue;
      }
      if (markSynced && !record.synced) {
        record.synced = true;
        changed = true;
      }
      if (markRead && record.unread) {
        record.unread = false;
        changed = true;
      }
    }
  }

  if (changed) {
    dirty_ = true;
    saveRecords();
  }
  return changed;
}

uint8_t StreetPassController::applyDelete(JsonDocument& request) {
  JsonArray ids = request["recordIds"].as<JsonArray>();
  uint8_t deleted = 0;

  for (JsonVariant value : ids) {
    const uint32_t recordId = value.as<uint32_t>();
    for (uint8_t i = 0; i < recordCount_; ++i) {
      if (records_[i].recordId != recordId) {
        continue;
      }
      for (uint8_t j = i; j + 1 < recordCount_; ++j) {
        records_[j] = records_[j + 1];
      }
      --recordCount_;
      records_[recordCount_] = StreetPassRecord();
      ++deleted;
      break;
    }
  }

  if (deleted > 0) {
    dirty_ = true;
    saveRecords();
  }
  return deleted;
}

bool StreetPassController::injectRecord(JsonDocument& request, unsigned long now) {
  if (!profile_.enabled) {
    return false;
  }

  String peerKey;
  copyLimited(peerKey, request["peerKey"] | "", kPeerKeyMaxLen);
  if (peerKey.length() == 0) {
    peerKey = String("debug-") + String(nextRecordId_);
  }
  return upsertRecord(peerKey.c_str(),
                      request["name"] | STREETPASS_DEFAULT_NAME,
                      request["message"] | "",
                      request["cardSeq"] | 0,
                      request["rssi"] | -55,
                      now,
                      "debug");
}

bool StreetPassController::upsertRecord(const char* peerKeyValue, const char* name, const char* message,
                                        uint32_t cardSeq, int rssi, unsigned long now, const char* source) {
  if (!profile_.enabled || peerKeyValue == nullptr || peerKeyValue[0] == '\0') {
    return false;
  }

  String peerKey;
  copyLimited(peerKey, peerKeyValue, kPeerKeyMaxLen);
  const uint32_t nowUnix = estimatedUnix(now);
  StreetPassRecord* existing = findRecordForMerge(peerKey, nowUnix);

  StreetPassRecord* record = existing;
  bool contentChanged = false;
  if (record == nullptr) {
    record = &allocateRecordSlot();
    record->recordId = nextRecordId_++;
    record->peerKey = peerKey;
    record->firstSeenMs = static_cast<uint32_t>(now);
    record->seenCount = 0;
    record->unread = true;
    record->synced = false;
    record->bootId = bootId_;
    record->cardSeq = cardSeq;
    saveSettings();
  } else {
    String nextName;
    String nextMessage;
    copyLimited(nextName, name, kNameMaxLen);
    copyLimited(nextMessage, message, kMessageMaxLen);
    contentChanged = record->peerName != nextName ||
                     record->peerMessage != nextMessage ||
                     record->cardSeq != cardSeq;
    if (contentChanged) {
      record->recordId = nextRecordId_++;
      record->unread = true;
      record->synced = false;
      saveSettings();
    }
  }

  copyLimited(record->peerName, name, kNameMaxLen);
  copyLimited(record->peerMessage, message, kMessageMaxLen);
  record->cardSeq = cardSeq;
  record->lastSeenMs = static_cast<uint32_t>(now);
  record->seenCount = min<uint16_t>(static_cast<uint16_t>(record->seenCount + 1), 65535);
  record->rssiMax = max<int8_t>(record->rssiMax, static_cast<int8_t>(constrain(rssi, -127, 20)));
  record->timeQuality = currentTimeQuality(now);
  record->seenUnix = estimatedUnix(now);
  if (existing != nullptr) {
    for (uint8_t i = 0; i < recordCount_; ++i) {
      if (&records_[i] != record) {
        continue;
      }
      StreetPassRecord updated = records_[i];
      for (uint8_t j = i; j + 1 < recordCount_; ++j) {
        records_[j] = records_[j + 1];
      }
      records_[recordCount_ - 1] = updated;
      record = &records_[recordCount_ - 1];
      break;
    }
  }
  dirty_ = true;
  saveRecords();
  Serial.printf("[streetpass] %s record id=%lu peer=%s stored=%u changed=%d\n",
                source == nullptr ? "upsert" : source,
                static_cast<unsigned long>(record->recordId),
                record->peerName.c_str(),
                static_cast<unsigned>(recordCount_),
                contentChanged ? 1 : 0);
  return true;
}

StreetPassRecord* StreetPassController::findRecordForMerge(const String& peerKey, uint32_t nowUnix) {
  for (int i = static_cast<int>(recordCount_) - 1; i >= 0; --i) {
    StreetPassRecord& record = records_[i];
    if (record.peerKey != peerKey) {
      continue;
    }
    if (nowUnix == 0 || record.seenUnix == 0) {
      return &record;
    }
    if (nowUnix >= record.seenUnix && nowUnix - record.seenUnix < kEncounterMergeWindowSeconds) {
      return &record;
    }
  }
  return nullptr;
}

uint8_t StreetPassController::chooseEvictionIndex() const {
  uint8_t selected = 0;
  bool found = false;
  for (uint8_t pass = 0; pass < 3; ++pass) {
    for (uint8_t i = 0; i < recordCount_; ++i) {
      const StreetPassRecord& record = records_[i];
      const bool eligible = pass == 0 ? record.synced : (pass == 1 ? !record.unread : true);
      if (!eligible) {
        continue;
      }
      if (!found || record.recordId < records_[selected].recordId) {
        selected = i;
        found = true;
      }
    }
    if (found) {
      return selected;
    }
  }
  return 0;
}

StreetPassRecord& StreetPassController::allocateRecordSlot() {
  if (recordCount_ < kMaxRecords) {
    StreetPassRecord& record = records_[recordCount_++];
    record = StreetPassRecord();
    return record;
  }

  const uint8_t index = chooseEvictionIndex();
  ++droppedCount_;
  saveSettings();
  records_[index] = StreetPassRecord();
  return records_[index];
}

uint32_t StreetPassController::estimatedUnix(unsigned long now) const {
  if (!timeSynced_ || timeAnchorUnix_ == 0) {
    return 0;
  }
  return timeAnchorUnix_ + ((static_cast<uint32_t>(now) - timeAnchorMillis_) / 1000UL);
}

const char* StreetPassController::currentTimeQuality(unsigned long now) const {
  (void)now;
  return timeSynced_ ? "exact" : "unknown";
}

void StreetPassController::writeRecord(JsonObject item, const StreetPassRecord& record) const {
  item["recordId"] = record.recordId;
  item["peerKey"] = record.peerKey;
  item["peerName"] = record.peerName;
  item["peerMessage"] = record.peerMessage;
  item["cardSeq"] = record.cardSeq;
  item["unread"] = record.unread;
  item["synced"] = record.synced;
  item["bootId"] = record.bootId;
  item["firstSeenMs"] = record.firstSeenMs;
  item["lastSeenMs"] = record.lastSeenMs;
  item["seenCount"] = record.seenCount;
  item["rssiMax"] = record.rssiMax;
  item["seenUnix"] = record.seenUnix;
  item["timeQuality"] = record.timeQuality;
}

void StreetPassController::copyLimited(String& target, const char* value, size_t maxLen) {
  target = value == nullptr ? "" : value;
  if (target.length() > maxLen) {
    target.remove(maxLen);
  }
}
