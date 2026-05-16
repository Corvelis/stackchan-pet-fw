#include "AffectionController.h"

#include <math.h>

namespace {
constexpr int kAffectionMin = 0;
constexpr int kAffectionMax = 1000;
constexpr int kAffectionInitial = 500;
constexpr unsigned long kSaveMinIntervalMs = 30000;
constexpr int kSaveAffectionDelta = 5;
constexpr unsigned long kMoodDecayIntervalMs = 10000;
constexpr unsigned long kConfusionDecayIntervalMs = 10000;
constexpr int kMoodDecayStep = 2;
constexpr int kConfusionDecayStep = 10;

const int kLevelUpThresholds[] = {
  0,
  220,
  420,
  620,
  820,
};

const int kLevelDownThresholds[] = {
  0,
  0,
  170,
  370,
  570,
  770,
};

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
  {"shake", -5, -10, 100, 4500},
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

AffectionApplyResult AffectionController::update(unsigned long now) {
  AffectionApplyResult result;
  result.previousLevelIndex = levelIndex_;
  result.levelIndex = levelIndex_;
  result.state = state_;

  int nextMood = state_.mood;
  int nextConfusion = state_.confusion;

  if (nextConfusion > 0 && now - lastConfusionDecayMs_ >= kConfusionDecayIntervalMs) {
    nextConfusion = max(0, nextConfusion - kConfusionDecayStep);
    lastConfusionDecayMs_ = now;
  }

  if (nextMood != 0 && now - lastMoodDecayMs_ >= kMoodDecayIntervalMs) {
    if (nextMood > 0) {
      nextMood = max(0, nextMood - kMoodDecayStep);
    } else {
      nextMood = min(0, nextMood + kMoodDecayStep);
    }
    lastMoodDecayMs_ = now;
  }

  result.applied = applyTransientState(nextMood, nextConfusion);
  result.state = state_;
  result.levelIndex = levelIndex_;
  result.levelChanged = result.previousLevelIndex != result.levelIndex;

  if (dirty_ && shouldSave(now)) {
    save(false);
  }
  return result;
}

AffectionApplyResult AffectionController::applyEvent(const char* eventName, float confidence, float intensity,
                                                     const char* eventId, unsigned long now) {
  AffectionApplyResult result;
  result.state = state_;
  result.previousLevelIndex = levelIndex_;
  result.levelIndex = levelIndex_;

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
  result.levelIndex = levelIndex_;
  result.levelChanged = result.previousLevelIndex != result.levelIndex;

  if (result.applied) {
    if (moodDelta != 0) {
      lastMoodDecayMs_ = now;
    }
    if (confusionDelta != 0) {
      lastConfusionDecayMs_ = now;
    }
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
  result.previousLevelIndex = levelIndex_;
  value = constrain(value, kAffectionMin, kAffectionMax);
  const int delta = value - state_.affection;
  state_.affection = value;
  state_.mood = 0;
  state_.confusion = 0;
  lastMoodDecayMs_ = millis();
  lastConfusionDecayMs_ = lastMoodDecayMs_;
  levelIndex_ = initialLevelIndexForAffection(state_.affection);
  state_.levelIndex = levelIndex_;
  ++state_.seq;
  dirty_ = true;
  save(true);

  result.applied = true;
  result.delta = delta;
  result.state = state_;
  result.levelIndex = levelIndex_;
  result.levelChanged = result.previousLevelIndex != result.levelIndex;
  Serial.printf("[affection] reset aff=%d seq=%lu\n", state_.affection, static_cast<unsigned long>(state_.seq));
  return result;
}

AffectionApplyResult AffectionController::resetTransient() {
  AffectionApplyResult result;
  result.previousLevelIndex = levelIndex_;
  result.levelIndex = levelIndex_;
  result.applied = applyTransientState(0, 0);
  result.state = state_;
  result.levelChanged = result.previousLevelIndex != result.levelIndex;
  if (result.applied) {
    lastMoodDecayMs_ = millis();
    lastConfusionDecayMs_ = lastMoodDecayMs_;
    Serial.printf("[affection] transient reset mood=%d conf=%d seq=%lu\n",
                  state_.mood, state_.confusion, static_cast<unsigned long>(state_.seq));
  }
  return result;
}

AffectionApplyResult AffectionController::debugAdjust(int delta) {
  AffectionApplyResult result;
  result.previousLevelIndex = levelIndex_;
  result.applied = applyDelta(delta, 0, 0);
  result.delta = delta;
  result.state = state_;
  result.levelIndex = levelIndex_;
  result.levelChanged = result.previousLevelIndex != result.levelIndex;
  return result;
}

AffectionApplyResult AffectionController::debugSet(bool hasAffection, int affection,
                                                   bool hasLevelIndex, uint8_t requestedLevelIndex,
                                                   bool hasMood, int mood,
                                                   bool hasConfusion, int confusion,
                                                   bool persist) {
  AffectionApplyResult result;
  result.previousLevelIndex = levelIndex_;

  const int oldAffection = state_.affection;
  const int oldMood = state_.mood;
  const int oldConfusion = state_.confusion;
  const uint8_t oldLevelIndex = levelIndex_;

  if (hasLevelIndex) {
    requestedLevelIndex = constrain(requestedLevelIndex, static_cast<uint8_t>(1), static_cast<uint8_t>(5));
    state_.affection = representativeAffectionForLevel(requestedLevelIndex);
    levelIndex_ = requestedLevelIndex;
  } else if (hasAffection) {
    state_.affection = constrain(affection, kAffectionMin, kAffectionMax);
    levelIndex_ = initialLevelIndexForAffection(state_.affection);
  }

  if (hasMood) {
    state_.mood = constrain(mood, -100, 100);
    lastMoodDecayMs_ = millis();
  }
  if (hasConfusion) {
    state_.confusion = constrain(confusion, 0, 100);
    lastConfusionDecayMs_ = millis();
  }
  state_.levelIndex = levelIndex_;

  result.applied = oldAffection != state_.affection ||
                   oldMood != state_.mood ||
                   oldConfusion != state_.confusion ||
                   oldLevelIndex != levelIndex_;
  result.delta = state_.affection - oldAffection;
  result.state = state_;
  result.levelIndex = levelIndex_;
  result.levelChanged = result.previousLevelIndex != result.levelIndex;

  if (result.applied) {
    ++state_.seq;
    if (persist) {
      dirty_ = true;
      save(true);
    }
    Serial.printf("[affection] debug_set aff=%d mood=%d conf=%d level=%u seq=%lu persist=%d\n",
                  state_.affection, state_.mood, state_.confusion, levelIndex_,
                  static_cast<unsigned long>(state_.seq), persist ? 1 : 0);
  }
  return result;
}

const AffectionState& AffectionController::state() const {
  return state_;
}

const char* AffectionController::level() const {
  return levelForIndex(levelIndex_);
}

uint8_t AffectionController::levelIndex() const {
  return levelIndex_;
}

const char* AffectionController::styleId() const {
  return styleIdForIndex(levelIndex_);
}

const char* AffectionController::visualTier() const {
  return visualTierForIndex(levelIndex_);
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
  const uint8_t oldLevelIndex = levelIndex_;
  levelIndex_ = updateLevelIndexForAffection(state_.affection);
  state_.levelIndex = levelIndex_;

  const bool changed = oldAffection != state_.affection ||
                       oldMood != state_.mood ||
                       oldConfusion != state_.confusion ||
                       oldLevelIndex != levelIndex_;
  if (changed) {
    ++state_.seq;
    dirty_ = true;
  }
  return changed;
}

bool AffectionController::applyTransientState(int mood, int confusion) {
  mood = constrain(mood, -100, 100);
  confusion = constrain(confusion, 0, 100);

  const int oldMood = state_.mood;
  const int oldConfusion = state_.confusion;
  state_.mood = mood;
  state_.confusion = confusion;

  const bool changed = oldMood != state_.mood || oldConfusion != state_.confusion;
  if (changed) {
    ++state_.seq;
  }
  return changed;
}

uint8_t AffectionController::initialLevelIndexForAffection(int affection) const {
  if (affection < 200) {
    return 1;
  }
  if (affection < 400) {
    return 2;
  }
  if (affection < 600) {
    return 3;
  }
  if (affection < 800) {
    return 4;
  }
  return 5;
}

uint8_t AffectionController::updateLevelIndexForAffection(int affection) {
  uint8_t index = constrain(levelIndex_, static_cast<uint8_t>(1), static_cast<uint8_t>(5));
  while (index < 5 && affection >= kLevelUpThresholds[index]) {
    ++index;
  }
  while (index > 1 && affection < kLevelDownThresholds[index]) {
    --index;
  }
  return index;
}

int AffectionController::representativeAffectionForLevel(uint8_t index) const {
  switch (index) {
    case 1:
      return 100;
    case 2:
      return 300;
    case 4:
      return 700;
    case 5:
      return 900;
    case 3:
    default:
      return 500;
  }
}

const char* AffectionController::levelForIndex(uint8_t index) const {
  switch (index) {
    case 1:
      return "cautious";
    case 2:
      return "sulky";
    case 4:
      return "nakayoshi";
    case 5:
      return "daisuki";
    case 3:
    default:
      return "normal";
  }
}

const char* AffectionController::styleIdForIndex(uint8_t index) const {
  switch (index) {
    case 1:
      return "cautious";
    case 2:
      return "sulky";
    case 4:
      return "friendly";
    case 5:
      return "attached";
    case 3:
    default:
      return "normal";
  }
}

const char* AffectionController::visualTierForIndex(uint8_t index) const {
  if (index <= 2) {
    return "guarded";
  }
  if (index >= 4) {
    return "attached";
  }
  return "normal";
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
  lastMoodDecayMs_ = millis();
  lastConfusionDecayMs_ = lastMoodDecayMs_;
  levelIndex_ = initialLevelIndexForAffection(state_.affection);
  state_.levelIndex = levelIndex_;
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
