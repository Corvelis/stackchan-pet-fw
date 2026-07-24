#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <M5Unified.h>
#include <Preferences.h>

#include "config.h"

struct StepDayRecord {
  uint32_t activityDay = 0;
  uint32_t steps = 0;
};

class StepCounterController {
public:
  static constexpr uint8_t kHistoryDays = STEP_COUNTER_HISTORY_DAYS;

  void begin(Preferences* preferences);
  void update(unsigned long now, const m5::imu_data_t& data, bool imuUpdated,
              uint32_t localUnix, bool timeValid);

  uint32_t todaySteps() const;
  bool todayValid() const;
  uint32_t currentActivityDay() const;
  uint8_t historyCount() const;
  const StepDayRecord* recordAt(uint8_t index) const;
  void writeHistory(JsonArray target) const;

private:
  struct Detector {
    bool initialized = false;
    float gx = 0.0f;
    float gy = 0.0f;
    float gz = 1.0f;
    float energyLP = 0.0f;
    float noiseLP = 0.0f;
    bool high = false;
    uint32_t lastCandidateMs = 0;
    uint32_t lastStepMs = 0;
    uint8_t pendingSteps = 0;
    uint16_t stepScaleRemainder = 0;
    bool walking = false;
  };

  void load();
  void save(unsigned long now, bool force);
  bool shouldSave(unsigned long now) const;
  void markDirty();
  uint32_t activityDayFromLocalUnix(uint32_t localUnix) const;
  void applyActivityDay(uint32_t activityDay, unsigned long now);
  int8_t findRecord(uint32_t activityDay) const;
  uint8_t ensureRecord(uint32_t activityDay);
  uint8_t chooseEvictionIndex() const;
  uint8_t detectSteps(const m5::imu_data_t& data, unsigned long now);
  uint32_t checksumState(const uint8_t* data, size_t length) const;

  Preferences* preferences_ = nullptr;
  StepDayRecord records_[kHistoryDays];
  uint8_t recordCount_ = 0;
  uint8_t todayIndex_ = 0;
  uint32_t currentActivityDay_ = 0;
  bool timeKnown_ = false;
  bool dirty_ = false;
  unsigned long lastSaveMs_ = 0;
  uint32_t lastSavedSteps_ = 0;
  Detector detector_;
};
