#include "AffectionController.h"

#include <math.h>

namespace {
constexpr int kAffectionMin = 0;
constexpr int kAffectionMax = 1000;
constexpr int kAffectionInitial = 500;
constexpr unsigned long kSaveMinIntervalMs = 30000;
constexpr int kSaveAffectionDelta = 5;

const AffectionController::EventSpec kEventSpecs[] = {
  {"talk", 2, 5, 0, 2500},
  {"positive_talk", 6, 10, 0, 5000},
  {"negative_talk", -10, -20, 0, 8000},
  {"praise", 10, 15, 0, 6000},
  {"thanks", 6, 10, 0, 5000},
  {"apology", 6, 20, 0, 6000},
  {"master_seen", 2, 3, 0, 30000},
  {"session_start", 0, 5, 0, 30000},
  {"petting", 9, 20, 0, 5000},
  {"shake", -5, -10, 35, 4500},
};

float clampUnit(float value) {
  if (!isfinite(value)) {
    return 1.0f;
  }
  if (value < 0.0f) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}
}

void AffectionController::begin(Preferences* preferences) {
  preferences_ = preferences;
  load();
}

void AffectionController::update(unsigned long now) {
  if (dirty_ && shouldSave(now)) {
    save(false);
  }
}

AffectionApplyResult AffectionController::applyEvent(const char* eventName, float confidence, float intensity,
                                                     const char* eventId, unsigned long now) {
  AffectionApplyResult result;
  result.state = state_;

  if (eventName == nullptr || eventName[0] == '\0') {
    return result;
  }
  if (eventId != nullptr && eventId[0] != '\0' && isDuplicate(eventId)) {
    result.duplicate = true;
    return result;
  }

  uint8_t eventIndex = 0;
  const EventSpec* spec = findSpec(eventName, &eventIndex);
  if (spec == nullptr) {
    Serial.printf("[affection] unsupported event: %s\n", eventName);
    return result;
  }

  confidence = clampUnit(confidence);
  intensity = clampUnit(intensity);

  float deltaValue = static_cast<float>(spec->affectionWeight) * confidence * intensity;
  deltaValue *= cooldownFactor(eventIndex, now);
  if (deltaValue > 0.0f) {
    deltaValue *= (1.0f - static_cast<float>(state_.affection) / 1300.0f);
  } else if (deltaValue < 0.0f) {
    if (confidence < 0.85f && strcmp(spec->name, "negative_talk") == 0) {
      deltaValue = 0.0f;
    } else {
      deltaValue *= 0.7f;
    }
  }

  const int delta = static_cast<int>(roundf(deltaValue));
  const int moodDelta = static_cast<int>(roundf(static_cast<float>(spec->moodWeight) * confidence * intensity));
  const int confusionDelta = static_cast<int>(roundf(static_cast<float>(spec->confusionWeight) * confidence * intensity));

  result.applied = applyDelta(delta, moodDelta, confusionDelta);
  result.delta = delta;
  result.state = state_;

  if (result.applied) {
    markEventApplied(eventIndex, now);
    rememberEventId(eventId);
    Serial.printf("[affection] event=%s delta=%d aff=%d mood=%d conf=%d seq=%lu\n",
                  spec->name, delta, state_.affection, state_.mood, state_.confusion,
                  static_cast<unsigned long>(state_.seq));
  }
  return result;
}

AffectionApplyResult AffectionController::reset(int value) {
  AffectionApplyResult result;
  value = constrain(value, kAffectionMin, kAffectionMax);
  const int delta = value - state_.affection;
  state_.affection = value;
  state_.mood = 0;
  state_.confusion = 0;
  ++state_.seq;
  dirty_ = true;
  save(true);

  result.applied = true;
  result.delta = delta;
  result.state = state_;
  Serial.printf("[affection] reset aff=%d seq=%lu\n", state_.affection, static_cast<unsigned long>(state_.seq));
  return result;
}

AffectionApplyResult AffectionController::debugAdjust(int delta) {
  AffectionApplyResult result;
  result.applied = applyDelta(delta, 0, 0);
  result.delta = delta;
  result.state = state_;
  return result;
}

const AffectionState& AffectionController::state() const {
  return state_;
}

const char* AffectionController::level() const {
  if (state_.affection < 200) {
    return "cautious";
  }
  if (state_.affection < 400) {
    return "sulky";
  }
  if (state_.affection < 600) {
    return "normal";
  }
  if (state_.affection < 800) {
    return "nakayoshi";
  }
  return "daisuki";
}

const AffectionController::EventSpec* AffectionController::findSpec(const char* eventName, uint8_t* index) {
  for (uint8_t i = 0; i < kEventSpecCount; ++i) {
    if (strcmp(eventName, kEventSpecs[i].name) == 0) {
      if (index != nullptr) {
        *index = i;
      }
      return &kEventSpecs[i];
    }
  }
  return nullptr;
}

bool AffectionController::isDuplicate(const char* eventId) const {
  for (uint8_t i = 0; i < kRecentEventCount; ++i) {
    if (recentEventIds_[i].length() > 0 && recentEventIds_[i] == eventId) {
      return true;
    }
  }
  return false;
}

void AffectionController::rememberEventId(const char* eventId) {
  if (eventId == nullptr || eventId[0] == '\0') {
    return;
  }
  recentEventIds_[recentEventWriteIndex_] = eventId;
  recentEventWriteIndex_ = (recentEventWriteIndex_ + 1) % kRecentEventCount;
}

float AffectionController::cooldownFactor(uint8_t eventIndex, unsigned long now) const {
  if (eventIndex >= kEventSpecCount || lastEventMs_[eventIndex] == 0) {
    return 1.0f;
  }
  const unsigned long elapsed = now - lastEventMs_[eventIndex];
  const uint16_t cooldown = kEventSpecs[eventIndex].cooldownMs;
  if (elapsed >= cooldown) {
    return 1.0f;
  }
  return max(0.15f, static_cast<float>(elapsed) / static_cast<float>(cooldown));
}

void AffectionController::markEventApplied(uint8_t eventIndex, unsigned long now) {
  if (eventIndex < kEventSpecCount) {
    lastEventMs_[eventIndex] = now;
  }
}

bool AffectionController::applyDelta(int delta, int moodDelta, int confusionDelta) {
  const int oldAffection = state_.affection;
  const int oldMood = state_.mood;
  const int oldConfusion = state_.confusion;

  state_.affection = constrain(state_.affection + delta, kAffectionMin, kAffectionMax);
  state_.mood = constrain(state_.mood + moodDelta, -100, 100);
  state_.confusion = constrain(state_.confusion + confusionDelta, 0, 100);

  const bool changed = oldAffection != state_.affection || oldMood != state_.mood || oldConfusion != state_.confusion;
  if (changed) {
    ++state_.seq;
    dirty_ = true;
  }
  return changed;
}

void AffectionController::load() {
  if (preferences_ == nullptr) {
    return;
  }

  preferences_->begin("affect", true);
  state_.affection = preferences_->getShort("aff", kAffectionInitial);
  state_.seq = preferences_->getUInt("seq", 0);
  preferences_->end();

  state_.affection = constrain(state_.affection, kAffectionMin, kAffectionMax);
  state_.mood = 0;
  state_.confusion = 0;
  lastSavedAffection_ = state_.affection;
  lastSavedSeq_ = state_.seq;
  Serial.printf("[affection] loaded aff=%d seq=%lu\n", state_.affection, static_cast<unsigned long>(state_.seq));
}

void AffectionController::save(bool force) {
  if (preferences_ == nullptr || (!force && !dirty_)) {
    return;
  }

  preferences_->begin("affect", false);
  preferences_->putShort("aff", state_.affection);
  preferences_->putUInt("seq", state_.seq);
  preferences_->end();

  lastSavedAffection_ = state_.affection;
  lastSavedSeq_ = state_.seq;
  lastSaveMs_ = millis();
  dirty_ = false;
  Serial.printf("[affection] saved aff=%d seq=%lu\n", state_.affection, static_cast<unsigned long>(state_.seq));
}

bool AffectionController::shouldSave(unsigned long now) const {
  if (abs(state_.affection - lastSavedAffection_) >= kSaveAffectionDelta) {
    return true;
  }
  if (state_.seq != lastSavedSeq_ && now - lastSaveMs_ >= kSaveMinIntervalMs) {
    return true;
  }
  return false;
}
