#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct AffectionState {
  int16_t affection = 500;
  int16_t mood = 0;
  int16_t confusion = 0;
  uint32_t seq = 0;
};

struct AffectionApplyResult {
  bool applied = false;
  bool duplicate = false;
  int16_t delta = 0;
  AffectionState state;
};

class AffectionController {
public:
  struct EventSpec {
    const char* name;
    int8_t affectionWeight;
    int8_t moodWeight;
    int8_t confusionWeight;
    uint16_t cooldownMs;
  };

  void begin(Preferences* preferences);
  void update(unsigned long now);

  AffectionApplyResult applyEvent(const char* eventName, float confidence, float intensity,
                                  const char* eventId, unsigned long now);
  AffectionApplyResult reset(int value);
  AffectionApplyResult debugAdjust(int delta);

  const AffectionState& state() const;
  const char* level() const;

private:
  static constexpr uint8_t kRecentEventCount = 8;
  static constexpr uint8_t kEventSpecCount = 10;

  const EventSpec* findSpec(const char* eventName, uint8_t* index);
  bool isDuplicate(const char* eventId) const;
  void rememberEventId(const char* eventId);
  float cooldownFactor(uint8_t eventIndex, unsigned long now) const;
  void markEventApplied(uint8_t eventIndex, unsigned long now);
  bool applyDelta(int delta, int moodDelta, int confusionDelta);
  void load();
  void save(bool force);
  bool shouldSave(unsigned long now) const;

  Preferences* preferences_ = nullptr;
  AffectionState state_;
  int16_t lastSavedAffection_ = 500;
  uint32_t lastSavedSeq_ = 0;
  unsigned long lastSaveMs_ = 0;
  unsigned long lastEventMs_[kEventSpecCount] = {};
  String recentEventIds_[kRecentEventCount];
  uint8_t recentEventWriteIndex_ = 0;
  bool dirty_ = false;
};
