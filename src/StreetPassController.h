#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "config.h"

struct StreetPassRecord {
  uint32_t recordId = 0;
  String peerKey;
  String peerName;
  String peerMessage;
  uint32_t cardSeq = 0;
  bool unread = true;
  bool synced = false;
  uint32_t bootId = 0;
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
  uint16_t seenCount = 0;
  int8_t rssiMax = -127;
  uint32_t seenUnix = 0;
  String timeQuality = "unknown";
};

struct StreetPassProfile {
  bool enabled = true;
  bool shareProfile = true;
  String profileId;
  String name = STREETPASS_DEFAULT_NAME;
  String message = STREETPASS_DEFAULT_MESSAGE;
  uint32_t cardSeq = 1;
};

class StreetPassController {
public:
  static constexpr uint8_t kMaxRecords = 30;

  void begin(Preferences* preferences);
  void update(unsigned long now);

  bool enabled() const;
  uint8_t storedCount() const;
  uint8_t unreadCount() const;
  uint8_t unsyncedCount() const;
  uint32_t droppedCount() const;
  const StreetPassRecord* recordAt(uint8_t index) const;
  const StreetPassProfile& profile() const;

  void markAllRead();
  bool syncTime(uint32_t unixTime, const char* timezone, unsigned long now);
  uint32_t estimatedUnix(unsigned long now) const;
  const char* currentTimeQuality(unsigned long now) const;
  bool recordPublicCard(const char* publicCardJson, int rssi, unsigned long now);
  void writeStatus(JsonObject target) const;
  bool handleJsonCommand(JsonDocument& request, JsonDocument& response, unsigned long now);

private:
  void loadSettings();
  void saveSettings();
  void loadRecords();
  void saveRecords();
  void ensureProfileId();
  String generateProfileId();
  void writeProfileResponse(JsonDocument& response, const char* requestId) const;
  void writeEncountersResponse(JsonDocument& response, const char* requestId,
                               uint32_t sinceRecordId, uint8_t limit) const;
  void writeTimeResponse(JsonDocument& response, const char* requestId, unsigned long now) const;
  bool applyProfileSet(JsonDocument& request);
  bool applyTimeSet(JsonDocument& request, unsigned long now);
  bool applyAck(JsonDocument& request);
  uint8_t applyDelete(JsonDocument& request);
  bool injectRecord(JsonDocument& request, unsigned long now);
  bool upsertRecord(const char* peerKey, const char* name, const char* message,
                    uint32_t cardSeq, int rssi, unsigned long now, const char* source);
  StreetPassRecord* findRecordForMerge(const String& peerKey, uint32_t nowUnix);
  uint8_t chooseEvictionIndex() const;
  StreetPassRecord& allocateRecordSlot();
  void writeRecord(JsonObject item, const StreetPassRecord& record) const;
  void copyLimited(String& target, const char* value, size_t maxLen);

  Preferences* preferences_ = nullptr;
  StreetPassProfile profile_;
  StreetPassRecord records_[kMaxRecords];
  uint8_t recordCount_ = 0;
  uint32_t nextRecordId_ = 1;
  uint32_t bootId_ = 0;
  uint32_t droppedCount_ = 0;
  bool dirty_ = false;
  bool timeSynced_ = false;
  uint32_t timeAnchorUnix_ = 0;
  uint32_t timeAnchorMillis_ = 0;
  String timezone_ = "UTC";
};
