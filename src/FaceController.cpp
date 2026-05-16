#include "FaceController.h"

#include "config.h"

void FaceController::begin() {
  canvas_.setPsram(true);
  canvas_.setColorDepth(16);
  canvasReady_ = canvas_.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;
  if (!canvasReady_) {
    Serial.println("[face] failed to allocate canvas; direct drawing fallback enabled");
  }
  prepareTalkCache();

  scheduleBlink(millis());
  scheduleSmile(millis());
  showBaseFace();
}

void FaceController::setState(ChanState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  if (state_ == ChanState::Idle) {
    authFaceMode_ = AuthFaceMode::Unknown;
  }
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);
  scheduleSmile(lastLipSyncMs_);

  if (enabled_) {
    showBaseFace();
  }
}

void FaceController::showFace(const char* name) {
  if (strcmp(name, "idle") == 0) {
    drawFace(FACE_IDLE_PATH);
  } else if (strcmp(name, "listen") == 0) {
    drawFace(FACE_LISTEN_PATH);
  } else if (strcmp(name, "talk_0") == 0) {
    drawFace(FACE_TALK_0_PATH);
  } else if (strcmp(name, "talk_1") == 0) {
    drawFace(FACE_TALK_1_PATH);
  } else if (strcmp(name, "idle_guarded_0") == 0) {
    drawFace(FACE_IDLE_GUARDED_0_PATH);
  } else if (strcmp(name, "idle_attached_0") == 0) {
    drawFace(FACE_IDLE_ATTACHED_0_PATH);
  } else if (strcmp(name, "blink_guarded_0") == 0) {
    drawFace(FACE_BLINK_GUARDED_0_PATH);
  } else if (strcmp(name, "blink_attached_0") == 0) {
    drawFace(FACE_BLINK_ATTACHED_0_PATH);
  } else if (strcmp(name, "talk_guarded_0") == 0) {
    drawFace(FACE_TALK_GUARDED_0_PATH);
  } else if (strcmp(name, "talk_guarded_1") == 0) {
    drawFace(FACE_TALK_GUARDED_1_PATH);
  } else if (strcmp(name, "talk_attached_0") == 0) {
    drawFace(FACE_TALK_ATTACHED_0_PATH);
  } else if (strcmp(name, "talk_attached_1") == 0) {
    drawFace(FACE_TALK_ATTACHED_1_PATH);
  } else if (strcmp(name, "bad_0") == 0) {
    drawFace(FACE_BAD_0_PATH);
  } else if (strcmp(name, "bad_1") == 0) {
    drawFace(FACE_BAD_1_PATH);
  } else if (strcmp(name, "good_0") == 0) {
    drawFace(FACE_GOOD_0_PATH);
  } else if (strcmp(name, "good_1") == 0) {
    drawFace(FACE_GOOD_1_PATH);
  } else if (strcmp(name, "good_blink") == 0) {
    drawFace(FACE_GOOD_BLINK_PATH);
  } else if (strcmp(name, "photo_0") == 0) {
    drawFace(FACE_PHOTO_0_PATH);
  } else if (strcmp(name, "photo_1") == 0) {
    drawFace(FACE_PHOTO_1_PATH);
  } else if (strcmp(name, "photo_blink") == 0) {
    drawFace(FACE_PHOTO_BLINK_PATH);
  } else if (strcmp(name, "photo_blink_talk") == 0) {
    drawFace(FACE_PHOTO_BLINK_TALK_PATH);
  } else if (strcmp(name, "photo_master_0") == 0) {
    drawFace(FACE_PHOTO_MASTER_0_PATH);
  } else if (strcmp(name, "photo_master_1") == 0) {
    drawFace(FACE_PHOTO_MASTER_1_PATH);
  } else if (strcmp(name, "nadenade_0") == 0) {
    drawFace(FACE_NADENADE_0_PATH);
  } else if (strcmp(name, "nadenade_1") == 0) {
    drawFace(FACE_NADENADE_1_PATH);
  } else if (strcmp(name, "pet_guarded_0") == 0) {
    drawFace(FACE_PET_GUARDED_0_PATH);
  } else if (strcmp(name, "pet_guarded_1") == 0) {
    drawFace(FACE_PET_GUARDED_1_PATH);
  } else if (strcmp(name, "pet_blink_guarded_0") == 0) {
    drawFace(FACE_PET_BLINK_GUARDED_0_PATH);
  } else if (strcmp(name, "pet_attached_0") == 0) {
    drawFace(FACE_PET_ATTACHED_0_PATH);
  } else if (strcmp(name, "pet_attached_1") == 0) {
    drawFace(FACE_PET_ATTACHED_1_PATH);
  } else if (strcmp(name, "pet_blink_attached_0") == 0) {
    drawFace(FACE_PET_BLINK_ATTACHED_0_PATH);
  } else if (strcmp(name, "furifuri_0") == 0) {
    drawFace(FACE_FURIFURI_0_PATH);
  } else if (strcmp(name, "furifuri_1") == 0) {
    drawFace(FACE_FURIFURI_1_PATH);
  } else if (strcmp(name, "shake_guarded_0") == 0) {
    drawFace(FACE_SHAKE_GUARDED_0_PATH);
  } else if (strcmp(name, "shake_guarded_1") == 0) {
    drawFace(FACE_SHAKE_GUARDED_1_PATH);
  } else if (strcmp(name, "shake_attached_0") == 0) {
    drawFace(FACE_SHAKE_ATTACHED_0_PATH);
  } else if (strcmp(name, "shake_attached_1") == 0) {
    drawFace(FACE_SHAKE_ATTACHED_1_PATH);
  } else if (strcmp(name, "blink") == 0) {
    drawFace(FACE_BLINK_PATH);
  } else if (strcmp(name, "smile") == 0) {
    drawFace(FACE_SMILE_PATH);
  } else if (strcmp(name, "tired_0") == 0) {
    drawFace(FACE_TIRED_0_PATH);
  } else if (strcmp(name, "tired_talk") == 0) {
    drawFace(FACE_TIRED_TALK_PATH);
  } else if (strcmp(name, "tired_blink") == 0) {
    drawFace(FACE_TIRED_BLINK_PATH);
  } else if (strcmp(name, "exhausted_0") == 0) {
    drawFace(FACE_EXHAUSTED_0_PATH);
  } else if (strcmp(name, "exhausted_talk") == 0) {
    drawFace(FACE_EXHAUSTED_TALK_PATH);
  } else if (strcmp(name, "exhausted_blink") == 0) {
    drawFace(FACE_EXHAUSTED_BLINK_PATH);
  } else if (strcmp(name, "low_power_0") == 0) {
    drawFace(FACE_LOW_POWER_0_PATH);
  } else if (strcmp(name, "low_power_talk") == 0) {
    drawFace(FACE_LOW_POWER_TALK_PATH);
  } else if (strcmp(name, "low_power_blink") == 0) {
    drawFace(FACE_LOW_POWER_BLINK_PATH);
  } else {
    Serial.printf("[face] unknown face: %s\n", name);
  }
}

void FaceController::setBadSpeakingFace(bool enabled) {
  setAuthFaceMode(enabled ? AuthFaceMode::NotMaster : AuthFaceMode::Unknown);
}

void FaceController::setPhotoFaceMode(bool enabled) {
  if (photoFaceMode_ == enabled && (!photoMasterFaceMode_ || enabled)) {
    return;
  }

  photoFaceMode_ = enabled;
  photoMasterFaceMode_ = false;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);

  if (enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setPhotoMasterFaceMode(bool enabled) {
  if (photoMasterFaceMode_ == enabled && (!photoFaceMode_ || enabled)) {
    return;
  }

  photoMasterFaceMode_ = enabled;
  photoFaceMode_ = false;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);

  if (enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setPetFaceMode(bool enabled) {
  if (petFaceMode_ == enabled) {
    return;
  }

  petFaceMode_ = enabled;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);

  if (enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setShakeFaceMode(bool enabled) {
  if (shakeFaceMode_ == enabled) {
    return;
  }

  shakeFaceMode_ = enabled;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);

  if (enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setAuthFaceMode(AuthFaceMode mode) {
  if (authFaceMode_ == mode) {
    return;
  }

  authFaceMode_ = mode;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);

  if (enabled_ && (state_ == ChanState::Listening || state_ == ChanState::Speaking)) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setEnabled(bool enabled) {
  if (enabled_ == enabled) {
    return;
  }

  enabled_ = enabled;
  if (enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setThermalFaceMode(ThermalFaceMode mode) {
  if (thermalFaceMode_ == mode) {
    return;
  }

  thermalFaceMode_ = mode;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  currentPath_ = "";
  if (enabled_) {
    showBaseFace();
  }
}

void FaceController::setBatteryState(int level, bool charging) {
  level = level < 0 ? -1 : constrain(level, 0, 100);
  if (batteryLevel_ == level && batteryCharging_ == charging) {
    return;
  }
  batteryLevel_ = level;
  batteryCharging_ = charging;
  batteryOverlayDirty_ = true;
  if (enabled_) {
    drawBatteryOverlay();
  }
}

void FaceController::setMicState(bool connected, bool muted, bool streaming) {
  if (micConnected_ == connected && micMuted_ == muted && micStreaming_ == streaming) {
    return;
  }
  micConnected_ = connected;
  micMuted_ = muted;
  micStreaming_ = streaming;
  micOverlayDirty_ = true;
  if (enabled_) {
    drawMicOverlay();
  }
}

void FaceController::setAffectionState(const AffectionState& state) {
  if (affectionState_.affection == state.affection &&
      affectionState_.mood == state.mood &&
      affectionState_.confusion == state.confusion &&
      affectionState_.seq == state.seq &&
      affectionState_.levelIndex == state.levelIndex) {
    return;
  }

  const uint8_t oldTier = visualTierIndex();
  affectionState_ = state;
  const uint8_t newTier = visualTierIndex();
  affectionOverlayDirty_ = true;
  if (enabled_) {
    if (oldTier != newTier && !photoFaceMode_ && !photoMasterFaceMode_ && authFaceMode_ == AuthFaceMode::Unknown) {
      currentPath_ = "";
      showBaseFace();
    }
    drawAffectionOverlay(millis());
  }
}

void FaceController::showAffectionDelta(int delta, unsigned long now) {
  if (delta == 0) {
    return;
  }
  affectionDelta_ = delta;
  affectionDeltaUntilMs_ = now + 1800;
  affectionOverlayDirty_ = true;
  if (enabled_) {
    drawAffectionOverlay(now);
  }
}

void FaceController::update(unsigned long now) {
  if (!enabled_) {
    return;
  }

  if (state_ == ChanState::Speaking) {
    if (now - lastLipSyncMs_ >= LIP_SYNC_INTERVAL_MS) {
      lastLipSyncMs_ = now;
      lipOpen_ = !lipOpen_;
      drawFace(talkFacePath(lipOpen_ ? 1 : 0));
    }
    return;
  }

  if (affectionOverlayDirty_ || batteryOverlayDirty_ || micOverlayDirty_ || (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_)) {
    drawAffectionOverlay(now);
    drawBatteryOverlay();
    drawMicOverlay();
  }

  if (blinking_) {
    if (now >= blinkEndMs_) {
      blinking_ = false;
      showBaseFace();
      scheduleBlink(now);
    }
    return;
  }

  if (smiling_) {
    if (now >= smileEndMs_) {
      smiling_ = false;
      showBaseFace();
      scheduleSmile(now);
    }
    return;
  }

  if (!shakeFaceMode_ &&
      !petFaceMode_ &&
      !photoFaceMode_ &&
      !photoMasterFaceMode_ &&
      thermalFaceMode_ == ThermalFaceMode::Normal &&
      visualTierIndex() == 2 &&
      state_ == ChanState::Idle &&
      now >= nextSmileMs_) {
    smiling_ = true;
    smileEndMs_ = now + IDLE_SMILE_DURATION_MS;
    drawFace(FACE_SMILE_PATH);
    return;
  }

  if (now >= nextBlinkMs_) {
    blinking_ = true;
    blinkEndMs_ = now + BLINK_DURATION_MS;
    drawFace(blinkFacePath());
  }
}

const char* FaceController::talkFacePath(uint8_t index) const {
  if (shakeFaceMode_) {
    return shakeFacePath(index);
  }
  if (petFaceMode_) {
    return petFacePath(index);
  }
  if (photoMasterFaceMode_) {
    return index == 0 ? FACE_PHOTO_MASTER_0_PATH : FACE_PHOTO_MASTER_1_PATH;
  }
  if (photoFaceMode_) {
    return index == 0 ? FACE_PHOTO_0_PATH : FACE_PHOTO_1_PATH;
  }
  if (authFaceMode_ == AuthFaceMode::NotMaster) {
    return index == 0 ? FACE_BAD_0_PATH : FACE_BAD_1_PATH;
  }
  if (authFaceMode_ == AuthFaceMode::Master) {
    return index == 0 ? FACE_GOOD_0_PATH : FACE_GOOD_1_PATH;
  }
  if (thermalFaceMode_ != ThermalFaceMode::Normal) {
    return thermalFacePath(index);
  }
  if (visualTierIndex() == 1) {
    return fallbackFacePath(index == 0 ? FACE_TALK_GUARDED_0_PATH : FACE_TALK_GUARDED_1_PATH,
                            index == 0 ? fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_TALK_0_PATH) : FACE_TALK_1_PATH);
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(index == 0 ? FACE_TALK_ATTACHED_0_PATH : FACE_TALK_ATTACHED_1_PATH,
                            index == 0 ? fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_TALK_0_PATH) : FACE_TALK_1_PATH);
  }
  return index == 0 ? FACE_TALK_0_PATH : FACE_TALK_1_PATH;
}

const char* FaceController::listeningFacePath() const {
  if (shakeFaceMode_) {
    return shakeFacePath(0);
  }
  if (petFaceMode_) {
    return petFacePath(0);
  }
  if (photoMasterFaceMode_) {
    return FACE_PHOTO_MASTER_0_PATH;
  }
  if (photoFaceMode_) {
    return FACE_PHOTO_0_PATH;
  }
  if (authFaceMode_ == AuthFaceMode::NotMaster) {
    return FACE_BAD_0_PATH;
  }
  if (authFaceMode_ == AuthFaceMode::Master) {
    return FACE_GOOD_0_PATH;
  }
  if (thermalFaceMode_ != ThermalFaceMode::Normal) {
    return thermalFacePath(0);
  }
  if (visualTierIndex() == 1) {
    return fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_LISTEN_PATH);
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_LISTEN_PATH);
  }
  return FACE_LISTEN_PATH;
}

const char* FaceController::blinkFacePath() const {
  if (shakeFaceMode_) {
    return shakeFacePath(0);
  }
  if (petFaceMode_) {
    return petBlinkFacePath();
  }
  if (photoMasterFaceMode_) {
    return FACE_PHOTO_MASTER_0_PATH;
  }
  if (photoFaceMode_) {
    return FACE_PHOTO_BLINK_PATH;
  }
  if (authFaceMode_ == AuthFaceMode::Master) {
    return FACE_GOOD_BLINK_PATH;
  }
  if (authFaceMode_ == AuthFaceMode::NotMaster) {
    return FACE_BAD_0_PATH;
  }
  if (thermalFaceMode_ != ThermalFaceMode::Normal) {
    return thermalBlinkFacePath();
  }
  if (visualTierIndex() == 1) {
    return fallbackFacePath(FACE_BLINK_GUARDED_0_PATH, FACE_BLINK_PATH);
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(FACE_BLINK_ATTACHED_0_PATH, FACE_BLINK_PATH);
  }
  return FACE_BLINK_PATH;
}

const char* FaceController::idleFacePath() const {
  if (thermalFaceMode_ != ThermalFaceMode::Normal) {
    return thermalFacePath(0);
  }
  if (visualTierIndex() == 1) {
    return fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_IDLE_PATH);
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_IDLE_PATH);
  }
  return FACE_IDLE_PATH;
}

const char* FaceController::thermalFacePath(uint8_t index) const {
  switch (thermalFaceMode_) {
    case ThermalFaceMode::LowPower:
      return fallbackFacePath(index == 0 ? FACE_LOW_POWER_0_PATH : FACE_LOW_POWER_TALK_PATH,
                              fallbackFacePath(index == 0 ? FACE_EXHAUSTED_0_PATH : FACE_EXHAUSTED_TALK_PATH,
                                               index == 0 ? FACE_IDLE_PATH : FACE_TALK_1_PATH));
    case ThermalFaceMode::Hot:
      return fallbackFacePath(index == 0 ? FACE_EXHAUSTED_0_PATH : FACE_EXHAUSTED_TALK_PATH,
                              fallbackFacePath(index == 0 ? FACE_TIRED_0_PATH : FACE_TIRED_TALK_PATH,
                                               index == 0 ? FACE_IDLE_PATH : FACE_TALK_1_PATH));
    case ThermalFaceMode::Warm:
      return fallbackFacePath(index == 0 ? FACE_TIRED_0_PATH : FACE_TIRED_TALK_PATH,
                              index == 0 ? FACE_IDLE_PATH : FACE_TALK_1_PATH);
    case ThermalFaceMode::Normal:
    default:
      return FACE_IDLE_PATH;
  }
}

const char* FaceController::thermalBlinkFacePath() const {
  switch (thermalFaceMode_) {
    case ThermalFaceMode::LowPower:
      return fallbackFacePath(FACE_LOW_POWER_BLINK_PATH,
                              fallbackFacePath(FACE_EXHAUSTED_BLINK_PATH, FACE_BLINK_PATH));
    case ThermalFaceMode::Hot:
      return fallbackFacePath(FACE_EXHAUSTED_BLINK_PATH,
                              fallbackFacePath(FACE_TIRED_BLINK_PATH, FACE_BLINK_PATH));
    case ThermalFaceMode::Warm:
      return fallbackFacePath(FACE_TIRED_BLINK_PATH, FACE_BLINK_PATH);
    case ThermalFaceMode::Normal:
    default:
      return FACE_BLINK_PATH;
  }
}

const char* FaceController::petFacePath(uint8_t index) const {
  if (visualTierIndex() == 1) {
    return fallbackFacePath(index == 0 ? FACE_PET_GUARDED_0_PATH : FACE_PET_GUARDED_1_PATH,
                            index == 0 ? FACE_NADENADE_0_PATH : FACE_NADENADE_1_PATH);
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(index == 0 ? FACE_PET_ATTACHED_0_PATH : FACE_PET_ATTACHED_1_PATH,
                            index == 0 ? FACE_NADENADE_0_PATH : FACE_NADENADE_1_PATH);
  }
  return index == 0 ? FACE_NADENADE_0_PATH : FACE_NADENADE_1_PATH;
}

const char* FaceController::petBlinkFacePath() const {
  if (visualTierIndex() == 1) {
    return fallbackFacePath(FACE_PET_BLINK_GUARDED_0_PATH, petFacePath(0));
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(FACE_PET_BLINK_ATTACHED_0_PATH, petFacePath(0));
  }
  return petFacePath(0);
}

const char* FaceController::shakeFacePath(uint8_t index) const {
  if (visualTierIndex() == 1) {
    return fallbackFacePath(index == 0 ? FACE_SHAKE_GUARDED_0_PATH : FACE_SHAKE_GUARDED_1_PATH,
                            index == 0 ? FACE_FURIFURI_0_PATH : FACE_FURIFURI_1_PATH);
  }
  if (visualTierIndex() == 3) {
    return fallbackFacePath(index == 0 ? FACE_SHAKE_ATTACHED_0_PATH : FACE_SHAKE_ATTACHED_1_PATH,
                            index == 0 ? FACE_FURIFURI_0_PATH : FACE_FURIFURI_1_PATH);
  }
  return index == 0 ? FACE_FURIFURI_0_PATH : FACE_FURIFURI_1_PATH;
}

const char* FaceController::fallbackFacePath(const char* preferred, const char* fallback) const {
  return LittleFS.exists(preferred) ? preferred : fallback;
}

uint8_t FaceController::visualTierIndex() const {
  if (affectionState_.levelIndex <= 2) {
    return 1;
  }
  if (affectionState_.levelIndex >= 4) {
    return 3;
  }
  return 2;
}

void FaceController::showBaseFace() {
  switch (state_) {
    case ChanState::Idle:
      drawFace(shakeFaceMode_ ? shakeFacePath(0) : (petFaceMode_ ? petFacePath(0) : (photoMasterFaceMode_ ? FACE_PHOTO_MASTER_0_PATH : (photoFaceMode_ ? FACE_PHOTO_0_PATH : idleFacePath()))));
      break;
    case ChanState::Listening:
      drawFace(listeningFacePath());
      break;
    case ChanState::Speaking:
      drawFace(talkFacePath(0));
      break;
  }
}

void FaceController::drawFace(const char* path) {
  if (currentPath_ == path) {
    return;
  }

  if (drawCachedTalkFace(path)) {
    currentPath_ = path;
    return;
  }

  currentPath_ = path;
  const int32_t x = (M5.Display.width() - FACE_IMAGE_WIDTH) / 2;
  const int32_t y = (M5.Display.height() - FACE_IMAGE_HEIGHT) / 2;

  if (!LittleFS.exists(path)) {
    Serial.printf("[face] missing file: %s\n", path);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(16, 80);
    M5.Display.printf("Missing:\n%s", path);
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[face] failed to open: %s\n", path);
    return;
  }

  bool ok = false;
  if (canvasReady_) {
    canvas_.fillScreen(TFT_BLACK);
    ok = canvas_.drawPng(&file, x, y);
    if (ok) {
      drawAffectionOverlayOnCanvas(millis());
      drawBatteryOverlayOnCanvas();
      drawMicOverlayOnCanvas();
      canvas_.pushSprite(&M5.Display, 0, 0);
    }
  } else {
    M5.Display.fillScreen(TFT_BLACK);
    ok = M5.Display.drawPng(&file, x, y);
  }
  file.close();
  if (!ok) {
    Serial.printf("[face] failed to draw: %s\n", path);
  } else if (!canvasReady_) {
    drawAffectionOverlay(millis());
    drawBatteryOverlay();
    drawMicOverlay();
  }
}

void FaceController::drawAffectionOverlay(unsigned long now) {
  lastAffectionOverlayMs_ = now;
  if (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = 0;
    affectionDelta_ = 0;
  }

  const int32_t x = 4;
  const int32_t y = 22;
  const int32_t w = 28;
  const int32_t h = 168;
  const int32_t radius = 7;
  const int32_t fillMax = h - 8;
  const int32_t fillH = map(constrain(affectionState_.affection, 0, 1000), 0, 1000, 0, fillMax);
  const uint16_t color = affectionColor();

  M5.Display.fillRect(0, 0, 39, M5.Display.height(), TFT_BLACK);
  M5.Display.drawRoundRect(x, y, w, h, radius, M5.Display.color565(78, 70, 76));
  M5.Display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, radius - 1, M5.Display.color565(38, 33, 37));

  if (fillH > 0) {
    const int32_t fy = y + h - 4 - fillH;
    M5.Display.fillRoundRect(x + 4, fy, w - 8, fillH, 4, color);
  }

  drawHeart(M5.Display, x + w / 2, y - 8, 9, color);

  if (affectionDelta_ != 0) {
    const bool positive = affectionDelta_ > 0;
    const uint16_t deltaColor = positive ? M5.Display.color565(255, 92, 150) : M5.Display.color565(128, 96, 120);
    const int32_t deltaY = 206;
    drawHeart(M5.Display, 12, deltaY, 6, deltaColor);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(deltaColor, TFT_BLACK);
    M5.Display.setCursor(20, deltaY - 4);
    M5.Display.printf("%+d", affectionDelta_);
  }

  affectionOverlayDirty_ = false;
}

void FaceController::drawBatteryOverlay() {
  const int32_t x = M5.Display.width() - 34;
  const int32_t y = 10;
  const int32_t w = 24;
  const int32_t h = 12;
  const int32_t nubW = 3;
  const uint16_t border = M5.Display.color565(210, 220, 210);
  const uint16_t dim = M5.Display.color565(80, 86, 82);
  const uint16_t fill = batteryLevel_ >= 0 && batteryLevel_ <= 20
                          ? M5.Display.color565(255, 92, 80)
                          : M5.Display.color565(82, 220, 128);

  M5.Display.fillRect(x - 3, y - 4, w + nubW + 8, h + 8, TFT_BLACK);
  M5.Display.drawRect(x, y, w, h, border);
  M5.Display.drawRect(x + w, y + 3, nubW, h - 6, border);

  if (batteryLevel_ >= 0) {
    const int32_t fillW = map(constrain(batteryLevel_, 0, 100), 0, 100, 0, w - 4);
    if (fillW > 0) {
      M5.Display.fillRect(x + 2, y + 2, fillW, h - 4, fill);
    }
  } else {
    M5.Display.drawLine(x + 5, y + 3, x + w - 6, y + h - 4, dim);
  }

  if (batteryCharging_) {
    const uint16_t bolt = M5.Display.color565(255, 230, 90);
    M5.Display.drawLine(x + 11, y + 2, x + 8, y + 7, bolt);
    M5.Display.drawLine(x + 8, y + 7, x + 13, y + 7, bolt);
    M5.Display.drawLine(x + 13, y + 7, x + 10, y + h - 2, bolt);
  }

  batteryOverlayDirty_ = false;
}

void FaceController::drawAffectionOverlayOnCanvas(unsigned long now) {
  lastAffectionOverlayMs_ = now;
  if (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = 0;
    affectionDelta_ = 0;
  }

  const int32_t x = 4;
  const int32_t y = 22;
  const int32_t w = 28;
  const int32_t h = 168;
  const int32_t radius = 7;
  const int32_t fillMax = h - 8;
  const int32_t fillH = map(constrain(affectionState_.affection, 0, 1000), 0, 1000, 0, fillMax);
  const uint16_t color = affectionColor();

  canvas_.fillRect(0, 0, 39, M5.Display.height(), TFT_BLACK);
  canvas_.drawRoundRect(x, y, w, h, radius, M5.Display.color565(78, 70, 76));
  canvas_.drawRoundRect(x + 1, y + 1, w - 2, h - 2, radius - 1, M5.Display.color565(38, 33, 37));

  if (fillH > 0) {
    const int32_t fy = y + h - 4 - fillH;
    canvas_.fillRoundRect(x + 4, fy, w - 8, fillH, 4, color);
  }

  drawHeart(canvas_, x + w / 2, y - 8, 9, color);

  if (affectionDelta_ != 0) {
    const bool positive = affectionDelta_ > 0;
    const uint16_t deltaColor = positive ? M5.Display.color565(255, 92, 150) : M5.Display.color565(128, 96, 120);
    const int32_t deltaY = 206;
    drawHeart(canvas_, 12, deltaY, 6, deltaColor);
    canvas_.setTextDatum(middle_left);
    canvas_.setTextSize(1);
    canvas_.setTextColor(deltaColor, TFT_BLACK);
    canvas_.setCursor(20, deltaY - 4);
    canvas_.printf("%+d", affectionDelta_);
  }

  affectionOverlayDirty_ = false;
}

void FaceController::drawBatteryOverlayOnCanvas() {
  const int32_t x = M5.Display.width() - 34;
  const int32_t y = 10;
  const int32_t w = 24;
  const int32_t h = 12;
  const int32_t nubW = 3;
  const uint16_t border = M5.Display.color565(210, 220, 210);
  const uint16_t dim = M5.Display.color565(80, 86, 82);
  const uint16_t fill = batteryLevel_ >= 0 && batteryLevel_ <= 20
                          ? M5.Display.color565(255, 92, 80)
                          : M5.Display.color565(82, 220, 128);

  canvas_.fillRect(x - 3, y - 4, w + nubW + 8, h + 8, TFT_BLACK);
  canvas_.drawRect(x, y, w, h, border);
  canvas_.drawRect(x + w, y + 3, nubW, h - 6, border);

  if (batteryLevel_ >= 0) {
    const int32_t fillW = map(constrain(batteryLevel_, 0, 100), 0, 100, 0, w - 4);
    if (fillW > 0) {
      canvas_.fillRect(x + 2, y + 2, fillW, h - 4, fill);
    }
  } else {
    canvas_.drawLine(x + 5, y + 3, x + w - 6, y + h - 4, dim);
  }

  if (batteryCharging_) {
    const uint16_t bolt = M5.Display.color565(255, 230, 90);
    canvas_.drawLine(x + 11, y + 2, x + 8, y + 7, bolt);
    canvas_.drawLine(x + 8, y + 7, x + 13, y + 7, bolt);
    canvas_.drawLine(x + 13, y + 7, x + 10, y + h - 2, bolt);
  }

  batteryOverlayDirty_ = false;
}

void FaceController::drawMicOverlay() {
  const int32_t w = 30;
  const int32_t h = 64;
  const int32_t x = M5.Display.width() - w - 5;
  const int32_t y = M5.Display.height() - h - 8;
  M5.Display.fillRoundRect(x - 2, y - 2, w + 4, h + 4, 8, TFT_BLACK);
  if (!micConnected_) {
    micOverlayDirty_ = false;
    return;
  }

  const uint16_t border = micMuted_ ? M5.Display.color565(220, 90, 90) : M5.Display.color565(90, 210, 150);
  const uint16_t fill = micMuted_ ? M5.Display.color565(52, 18, 22) : M5.Display.color565(16, 42, 30);
  const uint16_t icon = micStreaming_ ? M5.Display.color565(120, 255, 175) : M5.Display.color565(210, 220, 210);
  M5.Display.fillRoundRect(x, y, w, h, 8, fill);
  M5.Display.drawRoundRect(x, y, w, h, 8, border);
  M5.Display.fillRoundRect(x + 11, y + 16, 8, 20, 4, icon);
  M5.Display.drawLine(x + 8, y + 32, x + 8, y + 38, icon);
  M5.Display.drawLine(x + 22, y + 32, x + 22, y + 38, icon);
  M5.Display.drawArc(x + 15, y + 32, 8, 7, 0, 180, icon);
  M5.Display.drawLine(x + 15, y + 43, x + 15, y + 49, icon);
  M5.Display.drawLine(x + 8, y + 49, x + 22, y + 49, icon);
  if (micMuted_) {
    M5.Display.drawLine(x + 7, y + 14, x + 23, y + 50, border);
    M5.Display.drawLine(x + 8, y + 14, x + 24, y + 50, border);
  }
  micOverlayDirty_ = false;
}

void FaceController::drawMicOverlayOnCanvas() {
  const int32_t w = 30;
  const int32_t h = 64;
  const int32_t x = M5.Display.width() - w - 5;
  const int32_t y = M5.Display.height() - h - 8;
  canvas_.fillRoundRect(x - 2, y - 2, w + 4, h + 4, 8, TFT_BLACK);
  if (!micConnected_) {
    micOverlayDirty_ = false;
    return;
  }

  const uint16_t border = micMuted_ ? M5.Display.color565(220, 90, 90) : M5.Display.color565(90, 210, 150);
  const uint16_t fill = micMuted_ ? M5.Display.color565(52, 18, 22) : M5.Display.color565(16, 42, 30);
  const uint16_t icon = micStreaming_ ? M5.Display.color565(120, 255, 175) : M5.Display.color565(210, 220, 210);
  canvas_.fillRoundRect(x, y, w, h, 8, fill);
  canvas_.drawRoundRect(x, y, w, h, 8, border);
  canvas_.fillRoundRect(x + 11, y + 16, 8, 20, 4, icon);
  canvas_.drawLine(x + 8, y + 32, x + 8, y + 38, icon);
  canvas_.drawLine(x + 22, y + 32, x + 22, y + 38, icon);
  canvas_.drawArc(x + 15, y + 32, 8, 7, 0, 180, icon);
  canvas_.drawLine(x + 15, y + 43, x + 15, y + 49, icon);
  canvas_.drawLine(x + 8, y + 49, x + 22, y + 49, icon);
  if (micMuted_) {
    canvas_.drawLine(x + 7, y + 14, x + 23, y + 50, border);
    canvas_.drawLine(x + 8, y + 14, x + 24, y + 50, border);
  }
  micOverlayDirty_ = false;
}

void FaceController::drawHeart(M5GFX& target, int32_t cx, int32_t cy, int32_t size, uint16_t color) {
  static const char* kHeart[] = {
    "0011100011100",
    "0111110111110",
    "1111111111111",
    "1111111111111",
    "1111111111111",
    "0111111111110",
    "0011111111100",
    "0001111111000",
    "0000111110000",
    "0000011100000",
    "0000001000000",
  };
  constexpr int32_t kHeartW = 13;
  constexpr int32_t kHeartH = 11;
  const int32_t scale = max<int32_t>(1, size / 5);
  const int32_t x0 = cx - (kHeartW * scale) / 2;
  const int32_t y0 = cy - (kHeartH * scale) / 2;

  for (int32_t row = 0; row < kHeartH; ++row) {
    for (int32_t col = 0; col < kHeartW; ++col) {
      if (kHeart[row][col] == '1') {
        target.fillRect(x0 + col * scale, y0 + row * scale, scale, scale, color);
      }
    }
  }
}

void FaceController::drawHeart(M5Canvas& target, int32_t cx, int32_t cy, int32_t size, uint16_t color) {
  static const char* kHeart[] = {
    "0011100011100",
    "0111110111110",
    "1111111111111",
    "1111111111111",
    "1111111111111",
    "0111111111110",
    "0011111111100",
    "0001111111000",
    "0000111110000",
    "0000011100000",
    "0000001000000",
  };
  constexpr int32_t kHeartW = 13;
  constexpr int32_t kHeartH = 11;
  const int32_t scale = max<int32_t>(1, size / 5);
  const int32_t x0 = cx - (kHeartW * scale) / 2;
  const int32_t y0 = cy - (kHeartH * scale) / 2;

  for (int32_t row = 0; row < kHeartH; ++row) {
    for (int32_t col = 0; col < kHeartW; ++col) {
      if (kHeart[row][col] == '1') {
        target.fillRect(x0 + col * scale, y0 + row * scale, scale, scale, color);
      }
    }
  }
}

uint16_t FaceController::affectionColor() const {
  const int affection = constrain(affectionState_.affection, 0, 1000);
  if (affection < 500) {
    return blendColor(0x1A1014, 0xE66A9A, static_cast<float>(affection) / 500.0f);
  }
  return blendColor(0xE66A9A, 0xFF2D55, static_cast<float>(affection - 500) / 500.0f);
}

uint16_t FaceController::blendColor(uint32_t from, uint32_t to, float t) const {
  t = constrain(t, 0.0f, 1.0f);
  const uint8_t fr = (from >> 16) & 0xff;
  const uint8_t fg = (from >> 8) & 0xff;
  const uint8_t fb = from & 0xff;
  const uint8_t tr = (to >> 16) & 0xff;
  const uint8_t tg = (to >> 8) & 0xff;
  const uint8_t tb = to & 0xff;
  const uint8_t r = static_cast<uint8_t>(static_cast<int>(fr) + static_cast<int>((static_cast<int>(tr) - static_cast<int>(fr)) * t));
  const uint8_t g = static_cast<uint8_t>(static_cast<int>(fg) + static_cast<int>((static_cast<int>(tg) - static_cast<int>(fg)) * t));
  const uint8_t b = static_cast<uint8_t>(static_cast<int>(fb) + static_cast<int>((static_cast<int>(tb) - static_cast<int>(fb)) * t));
  return M5.Display.color565(r, g, b);
}

bool FaceController::drawCachedTalkFace(const char* path) {
  int setIndex = -1;
  int index = -1;
  if (strcmp(path, FACE_TALK_0_PATH) == 0) {
    setIndex = 0;
    index = 0;
  } else if (strcmp(path, FACE_TALK_1_PATH) == 0) {
    setIndex = 0;
    index = 1;
  } else if (strcmp(path, FACE_BAD_0_PATH) == 0) {
    setIndex = 1;
    index = 0;
  } else if (strcmp(path, FACE_BAD_1_PATH) == 0) {
    setIndex = 1;
    index = 1;
  } else if (strcmp(path, FACE_GOOD_0_PATH) == 0) {
    setIndex = 2;
    index = 0;
  } else if (strcmp(path, FACE_GOOD_1_PATH) == 0) {
    setIndex = 2;
    index = 1;
  } else if (strcmp(path, FACE_PHOTO_0_PATH) == 0) {
    setIndex = 3;
    index = 0;
  } else if (strcmp(path, FACE_PHOTO_1_PATH) == 0) {
    setIndex = 3;
    index = 1;
  } else if (strcmp(path, FACE_PHOTO_MASTER_0_PATH) == 0) {
    setIndex = 4;
    index = 0;
  } else if (strcmp(path, FACE_PHOTO_MASTER_1_PATH) == 0) {
    setIndex = 4;
    index = 1;
  } else if (strcmp(path, FACE_NADENADE_0_PATH) == 0) {
    setIndex = 5;
    index = 0;
  } else if (strcmp(path, FACE_NADENADE_1_PATH) == 0) {
    setIndex = 5;
    index = 1;
  } else if (strcmp(path, FACE_FURIFURI_0_PATH) == 0) {
    setIndex = 6;
    index = 0;
  } else if (strcmp(path, FACE_FURIFURI_1_PATH) == 0) {
    setIndex = 6;
    index = 1;
  } else if (strcmp(path, FACE_TALK_GUARDED_0_PATH) == 0 || strcmp(path, FACE_IDLE_GUARDED_0_PATH) == 0) {
    setIndex = 7;
    index = 0;
  } else if (strcmp(path, FACE_TALK_GUARDED_1_PATH) == 0) {
    setIndex = 7;
    index = 1;
  } else if (strcmp(path, FACE_TALK_ATTACHED_0_PATH) == 0 || strcmp(path, FACE_IDLE_ATTACHED_0_PATH) == 0) {
    setIndex = 8;
    index = 0;
  } else if (strcmp(path, FACE_TALK_ATTACHED_1_PATH) == 0) {
    setIndex = 8;
    index = 1;
  } else if (strcmp(path, FACE_PET_GUARDED_0_PATH) == 0) {
    setIndex = 9;
    index = 0;
  } else if (strcmp(path, FACE_PET_GUARDED_1_PATH) == 0) {
    setIndex = 9;
    index = 1;
  } else if (strcmp(path, FACE_PET_ATTACHED_0_PATH) == 0) {
    setIndex = 10;
    index = 0;
  } else if (strcmp(path, FACE_PET_ATTACHED_1_PATH) == 0) {
    setIndex = 10;
    index = 1;
  } else if (strcmp(path, FACE_SHAKE_GUARDED_0_PATH) == 0) {
    setIndex = 11;
    index = 0;
  } else if (strcmp(path, FACE_SHAKE_GUARDED_1_PATH) == 0) {
    setIndex = 11;
    index = 1;
  } else if (strcmp(path, FACE_SHAKE_ATTACHED_0_PATH) == 0) {
    setIndex = 12;
    index = 0;
  } else if (strcmp(path, FACE_SHAKE_ATTACHED_1_PATH) == 0) {
    setIndex = 12;
    index = 1;
  } else if (strcmp(path, FACE_TIRED_0_PATH) == 0) {
    setIndex = 13;
    index = 0;
  } else if (strcmp(path, FACE_TIRED_TALK_PATH) == 0) {
    setIndex = 13;
    index = 1;
  } else if (strcmp(path, FACE_EXHAUSTED_0_PATH) == 0) {
    setIndex = 14;
    index = 0;
  } else if (strcmp(path, FACE_EXHAUSTED_TALK_PATH) == 0) {
    setIndex = 14;
    index = 1;
  } else if (strcmp(path, FACE_LOW_POWER_0_PATH) == 0) {
    setIndex = 15;
    index = 0;
  } else if (strcmp(path, FACE_LOW_POWER_TALK_PATH) == 0) {
    setIndex = 15;
    index = 1;
  }

  if (setIndex < 0 || index < 0 || !talkCacheReady_[setIndex][index]) {
    return false;
  }

  const int32_t x = (M5.Display.width() - FACE_IMAGE_WIDTH) / 2;
  const int32_t y = (M5.Display.height() - FACE_IMAGE_HEIGHT) / 2;
  talkCanvas_[setIndex][index].pushSprite(&M5.Display, x, y);
  drawAffectionOverlay(millis());
  drawBatteryOverlay();
  drawMicOverlay();
  return true;
}

void FaceController::prepareTalkCache() {
  const char* paths[kTalkCacheSetCount][2] = {
    {FACE_TALK_0_PATH, FACE_TALK_1_PATH},
    {FACE_BAD_0_PATH, FACE_BAD_1_PATH},
    {FACE_GOOD_0_PATH, FACE_GOOD_1_PATH},
    {FACE_PHOTO_0_PATH, FACE_PHOTO_1_PATH},
    {FACE_PHOTO_MASTER_0_PATH, FACE_PHOTO_MASTER_1_PATH},
    {FACE_NADENADE_0_PATH, FACE_NADENADE_1_PATH},
    {FACE_FURIFURI_0_PATH, FACE_FURIFURI_1_PATH},
    {fallbackFacePath(FACE_TALK_GUARDED_0_PATH, FACE_IDLE_GUARDED_0_PATH), FACE_TALK_GUARDED_1_PATH},
    {fallbackFacePath(FACE_TALK_ATTACHED_0_PATH, FACE_IDLE_ATTACHED_0_PATH), FACE_TALK_ATTACHED_1_PATH},
    {FACE_PET_GUARDED_0_PATH, FACE_PET_GUARDED_1_PATH},
    {FACE_PET_ATTACHED_0_PATH, FACE_PET_ATTACHED_1_PATH},
    {FACE_SHAKE_GUARDED_0_PATH, FACE_SHAKE_GUARDED_1_PATH},
    {FACE_SHAKE_ATTACHED_0_PATH, FACE_SHAKE_ATTACHED_1_PATH},
    {FACE_TIRED_0_PATH, FACE_TIRED_TALK_PATH},
    {FACE_EXHAUSTED_0_PATH, FACE_EXHAUSTED_TALK_PATH},
    {FACE_LOW_POWER_0_PATH, FACE_LOW_POWER_TALK_PATH},
  };

  for (int setIndex = 0; setIndex < kTalkCacheSetCount; ++setIndex) {
    for (int i = 0; i < 2; ++i) {
      if (!LittleFS.exists(paths[setIndex][i])) {
        Serial.printf("[face] talk cache %d:%d skipped missing %s\n", setIndex, i, paths[setIndex][i]);
        talkCacheReady_[setIndex][i] = false;
        continue;
      }
      talkCanvas_[setIndex][i].setPsram(true);
      talkCanvas_[setIndex][i].setColorDepth(16);
      if (talkCanvas_[setIndex][i].createSprite(FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT) == nullptr) {
        Serial.printf("[face] failed to allocate talk cache %d:%d\n", setIndex, i);
        talkCacheReady_[setIndex][i] = false;
        continue;
      }

      talkCanvas_[setIndex][i].fillScreen(TFT_BLACK);
      talkCacheReady_[setIndex][i] = loadPngToCanvas(talkCanvas_[setIndex][i], paths[setIndex][i], 0, 0);
      Serial.printf("[face] talk cache %d:%d: %s\n", setIndex, i, talkCacheReady_[setIndex][i] ? "ready" : "failed");
    }
  }
}

bool FaceController::loadPngToCanvas(M5Canvas& canvas, const char* path, int32_t x, int32_t y) {
  if (!LittleFS.exists(path)) {
    Serial.printf("[face] missing cache file: %s\n", path);
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[face] failed to open cache file: %s\n", path);
    return false;
  }

  bool ok = canvas.drawPng(&file, x, y);
  file.close();
  return ok;
}

void FaceController::scheduleBlink(unsigned long now) {
  nextBlinkMs_ = now + random(BLINK_MIN_INTERVAL_MS, BLINK_MAX_INTERVAL_MS + 1);
}

void FaceController::scheduleSmile(unsigned long now) {
  nextSmileMs_ = now + random(IDLE_SMILE_MIN_INTERVAL_MS, IDLE_SMILE_MAX_INTERVAL_MS + 1);
}
