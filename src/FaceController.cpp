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

#if FACE_DIAG_LOG_ENABLED
  if (state_ == ChanState::Speaking || state == ChanState::Speaking) {
    Serial.printf("[face_diag] setState %d->%d lip=%d age=%lu current=%s\n",
                  static_cast<int>(state_),
                  static_cast<int>(state),
                  lipOpen_,
                  millis() - lastLipSyncMs_,
                  currentPath_.c_str());
  }
#endif
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

void FaceController::prepareSpeakingCache(AuthFaceMode authMode) {
#if STACKCHAN_ROUND_DISPLAY
  const AuthFaceMode previousAuthMode = authFaceMode_;
  authFaceMode_ = authMode;
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
  authFaceMode_ = previousAuthMode;
#else
  (void)authMode;
#endif
}

void FaceController::startSpeaking(AuthFaceMode authMode) {
  authFaceMode_ = authMode;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
  setState(ChanState::Speaking);
}

void FaceController::restartSpeakingAnimation() {
#if FACE_DIAG_LOG_ENABLED
  Serial.printf("[face_diag] restartSpeaking lip=%d age=%lu current=%s\n",
                lipOpen_,
                millis() - lastLipSyncMs_,
                currentPath_.c_str());
#endif
  state_ = ChanState::Speaking;
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis() - LIP_SYNC_INTERVAL_MS;
  currentPath_ = "";
  if (enabled_) {
    showBaseFace();
  }
}

void FaceController::showFace(const char* name) {
#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("showFace", name ? name[0] : 0);
  if (state_ == ChanState::Speaking) {
    Serial.printf("[face_diag] speaking showFace name=%s current=%s\n",
                  name ? name : "(null)",
                  currentPath_.c_str());
  }
#endif
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

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setPhoto", enabled ? 1 : 0);
#endif
  photoFaceMode_ = enabled;
  photoMasterFaceMode_ = false;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
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

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setPhotoMaster", enabled ? 1 : 0);
#endif
  photoMasterFaceMode_ = enabled;
  photoFaceMode_ = false;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
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

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setPet", enabled ? 1 : 0);
#endif
  petFaceMode_ = enabled;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
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

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setShake", enabled ? 1 : 0);
#endif
  shakeFaceMode_ = enabled;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
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

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setAuth", static_cast<int>(mode));
#endif
  authFaceMode_ = mode;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
  if (state_ == ChanState::Speaking) {
    return;
  }

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

#if FACE_DIAG_LOG_ENABLED
  if (state_ == ChanState::Speaking) {
    Serial.printf("[face_diag] speaking setEnabled=%d age=%lu current=%s\n",
                  enabled ? 1 : 0,
                  millis() - lastLipSyncMs_,
                  currentPath_.c_str());
  }
#endif
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

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setThermal", static_cast<int>(mode));
#endif
  thermalFaceMode_ = mode;
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
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

void FaceController::setClockText(const String& text, bool valid) {
  if (clockText_ == text && clockValid_ == valid) {
    return;
  }
  clockText_ = text;
  clockValid_ = valid;
  clockOverlayDirty_ = true;
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
  cameraOverlayDirty_ = true;
  if (enabled_) {
    drawCameraOverlay();
    drawMicOverlay();
  }
}

void FaceController::setCameraButtonPending(bool pending) {
  if (cameraButtonPending_ == pending) {
    return;
  }
  cameraButtonPending_ = pending;
  cameraOverlayDirty_ = true;
  if (enabled_) {
    drawCameraOverlay();
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
#if STACKCHAN_ROUND_DISPLAY
      prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#endif
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
#if FACE_DIAG_LOG_ENABLED
    if (state_ == ChanState::Speaking) {
      Serial.printf("[face_diag] speaking update skipped disabled age=%lu current=%s\n",
                    now - lastLipSyncMs_,
                    currentPath_.c_str());
    }
#endif
    return;
  }

  if (state_ == ChanState::Speaking) {
#if FACE_DIAG_LOG_ENABLED
    if (lastSpeakingUpdateMs_ != 0 && now - lastSpeakingUpdateMs_ > 700) {
      Serial.printf("[face_diag] speaking update gap=%lu age=%lu lip=%d current=%s\n",
                    now - lastSpeakingUpdateMs_,
                    now - lastLipSyncMs_,
                    lipOpen_,
                    currentPath_.c_str());
    }
    lastSpeakingUpdateMs_ = now;
#endif
    if (now - lastLipSyncMs_ >= LIP_SYNC_INTERVAL_MS) {
      lastLipSyncMs_ = now;
      lipOpen_ = !lipOpen_;
      const char* path = talkFacePath(lipOpen_ ? 1 : 0);
#if FACE_DIAG_LOG_ENABLED
      Serial.printf("[face_diag] lip path=%s lip=%d\n", path, lipOpen_);
#endif
      drawFace(path);
    }
    return;
  }
#if FACE_DIAG_LOG_ENABLED
  lastSpeakingUpdateMs_ = 0;
#endif

  if (affectionOverlayDirty_ || batteryOverlayDirty_ || clockOverlayDirty_ || cameraOverlayDirty_ || micOverlayDirty_ || (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_)) {
    drawAffectionOverlay(now);
    drawBatteryOverlay();
    drawCameraOverlay();
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

void FaceController::logSpeakingInterference(const char* source, int value) const {
#if FACE_DIAG_LOG_ENABLED
  if (state_ != ChanState::Speaking) {
    return;
  }
  const unsigned long now = millis();
  Serial.printf("[face_diag] speaking %s=%d lip=%d age=%lu current=%s modes photo=%d master=%d pet=%d shake=%d thermal=%d auth=%d\n",
                source,
                value,
                lipOpen_,
                now - lastLipSyncMs_,
                currentPath_.c_str(),
                photoFaceMode_ ? 1 : 0,
                photoMasterFaceMode_ ? 1 : 0,
                petFaceMode_ ? 1 : 0,
                shakeFaceMode_ ? 1 : 0,
                static_cast<int>(thermalFaceMode_),
                static_cast<int>(authFaceMode_));
#endif
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
#if FACE_DIAG_LOG_ENABLED
    if (state_ == ChanState::Speaking) {
      Serial.printf("[face_diag] draw skip same path=%s lip=%d age=%lu\n",
                    path,
                    lipOpen_,
                    millis() - lastLipSyncMs_);
    }
#endif
    return;
  }

#if FACE_DIAG_LOG_ENABLED
  if (state_ == ChanState::Speaking) {
    Serial.printf("[face_diag] draw path=%s prev=%s lip=%d age=%lu\n",
                  path,
                  currentPath_.c_str(),
                  lipOpen_,
                  millis() - lastLipSyncMs_);
  }
#endif
  if (drawCachedTalkFace(path)) {
    currentPath_ = path;
    return;
  }

  currentPath_ = path;

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
    ok = drawFaceImageTarget(canvas_, file);
    if (ok) {
      drawAffectionOverlayOnCanvas(millis());
      drawBatteryOverlayOnCanvas();
      drawCameraOverlayOnCanvas();
      drawMicOverlayOnCanvas();
      canvas_.pushSprite(&M5.Display, 0, 0);
    }
  } else {
    M5.Display.fillScreen(TFT_BLACK);
    ok = drawFaceImageTarget(M5.Display, file);
  }
  file.close();
  if (!ok) {
    Serial.printf("[face] failed to draw: %s\n", path);
  } else if (!canvasReady_) {
    drawAffectionOverlay(millis());
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
}

int32_t FaceController::faceImageDrawSize() const {
#if STACKCHAN_ROUND_DISPLAY
  const int32_t displaySize = min(M5.Display.width(), M5.Display.height());
  const int32_t maxDrawSize = max<int32_t>(1, displaySize - ROUND_FACE_IMAGE_MARGIN_PX * 2);
  const int32_t scaledSize = static_cast<int32_t>(FACE_IMAGE_WIDTH * ROUND_FACE_IMAGE_MAX_SCALE + 0.5f);
  return min(maxDrawSize, scaledSize);
#else
  return FACE_IMAGE_WIDTH;
#endif
}

int32_t FaceController::faceImageDrawX() const {
  return (M5.Display.width() - FACE_IMAGE_WIDTH) / 2;
}

int32_t FaceController::faceImageDrawY() const {
  return (M5.Display.height() - FACE_IMAGE_HEIGHT) / 2;
}

float FaceController::faceImageScale() const {
#if STACKCHAN_ROUND_DISPLAY
  return static_cast<float>(faceImageDrawSize()) / static_cast<float>(FACE_IMAGE_WIDTH);
#else
  return 1.0f;
#endif
}

template <typename Target>
bool FaceController::drawFaceImageTarget(Target& target, File& file) const {
#if STACKCHAN_ROUND_DISPLAY
  const int32_t drawSize = faceImageDrawSize();
  const int32_t x = (M5.Display.width() - drawSize) / 2;
  const int32_t y = (M5.Display.height() - drawSize) / 2;
  return target.drawPng(&file,
                        x,
                        y,
                        drawSize,
                        drawSize,
                        0,
                        0,
                        faceImageScale(),
                        faceImageScale(),
                        datum_t::top_left);
#else
  return target.drawPng(&file, faceImageDrawX(), faceImageDrawY());
#endif
}

template <typename Target>
void FaceController::drawRoundAffectionOverlayTarget(Target& target, unsigned long now) {
  lastAffectionOverlayMs_ = now;
  if (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = 0;
    affectionDelta_ = 0;
  }

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  const int32_t outerR = min(M5.Display.width(), M5.Display.height()) / 2 - 8;
  const int32_t innerR = outerR - 16;
  const float startAngle = 205.0f;
  const float endAngle = 335.0f;
  const float amount = static_cast<float>(constrain(affectionState_.affection, 0, 1000)) / 1000.0f;
  const uint16_t track = M5.Display.color565(48, 44, 50);
  const uint16_t color = affectionColor();

  target.fillArc(cx, cy, outerR, innerR, startAngle, endAngle, track);
  if (amount > 0.0f) {
    target.fillArc(cx, cy, outerR, innerR, startAngle, startAngle + (endAngle - startAngle) * amount, color);
  }

  if (affectionDelta_ != 0) {
    const bool positive = affectionDelta_ > 0;
    const uint16_t deltaColor = positive ? M5.Display.color565(255, 92, 150) : M5.Display.color565(128, 96, 120);
    const String text = String(affectionDelta_ > 0 ? "+" : "") + String(affectionDelta_);
    const uint16_t labelBg = M5.Display.color565(10, 8, 12);
    const uint16_t labelBorder = positive ? M5.Display.color565(132, 48, 82) : M5.Display.color565(78, 64, 74);
    const int32_t labelW = 112;
    const int32_t labelH = 34;
    const int32_t labelY = cy - outerR + 38;
    target.fillRoundRect(cx - labelW / 2, labelY - labelH / 2, labelW, labelH, 16, labelBg);
    target.drawRoundRect(cx - labelW / 2, labelY - labelH / 2, labelW, labelH, 16, labelBorder);
    drawHeart(target, cx - 30, labelY, 10, deltaColor);
    target.setFont(&fonts::Font0);
    target.setTextDatum(middle_left);
    target.setTextSize(2);
    target.setTextColor(deltaColor, labelBg);
    target.drawString(text, cx - 8, labelY);
    target.setTextDatum(top_left);
  }

  affectionOverlayDirty_ = false;
}

template <typename Target>
void FaceController::drawRoundBatteryOverlayTarget(Target& target) {
  const int32_t cx = M5.Display.width() / 2;
  const int32_t w = 30;
  const int32_t h = 14;
  const int32_t nubW = 3;
  const int32_t gap = clockText_.isEmpty() ? 0 : 10;
  const int32_t statusCy = M5.Display.height() - 25;
  const uint16_t border = M5.Display.color565(210, 220, 210);
  const uint16_t dim = M5.Display.color565(80, 86, 82);
  const uint16_t fill = batteryLevel_ >= 0 && batteryLevel_ <= 20
                          ? M5.Display.color565(255, 92, 80)
                          : M5.Display.color565(82, 220, 128);

  target.setFont(&fonts::Font0);
  target.setTextSize(2);
  const int32_t textW = clockText_.isEmpty() ? 0 : target.textWidth(clockText_);
  const int32_t batteryW = w + nubW;
  const int32_t totalW = textW + gap + batteryW;
  const int32_t startX = cx - totalW / 2;
  const int32_t x = startX + textW + gap;
  const int32_t y = statusCy - h / 2;
  const int32_t clearX = max<int32_t>(0, startX - 8);
  const int32_t clearY = max<int32_t>(0, statusCy - 18);
  const int32_t clearW = min<int32_t>(M5.Display.width() - clearX, totalW + 18);
  const int32_t clearH = min<int32_t>(M5.Display.height() - clearY, 34);

  target.fillRect(clearX, clearY, clearW, clearH, TFT_BLACK);

  if (!clockText_.isEmpty()) {
    const uint16_t clockColor = clockValid_ ? M5.Display.color565(235, 240, 238) : M5.Display.color565(112, 116, 118);
    target.setTextDatum(middle_left);
    target.setTextColor(clockColor, TFT_BLACK);
    target.drawString(clockText_, startX, statusCy);
    target.setTextDatum(top_left);
  }

  target.drawRect(x, y, w, h, border);
  target.drawRect(x + w, y + 4, nubW, h - 8, border);

  if (batteryLevel_ >= 0) {
    const int32_t fillW = map(constrain(batteryLevel_, 0, 100), 0, 100, 0, w - 4);
    if (fillW > 0) {
      target.fillRect(x + 2, y + 2, fillW, h - 4, fill);
    }
  } else {
    target.drawLine(x + 6, y + 3, x + w - 7, y + h - 4, dim);
  }

  if (batteryCharging_) {
    const uint16_t bolt = M5.Display.color565(255, 230, 90);
    target.drawLine(x + 14, y + 2, x + 10, y + 8, bolt);
    target.drawLine(x + 10, y + 8, x + 16, y + 8, bolt);
    target.drawLine(x + 16, y + 8, x + 12, y + h - 2, bolt);
  }

  batteryOverlayDirty_ = false;
  clockOverlayDirty_ = false;
}

template <typename Target>
void FaceController::drawRoundCameraOverlayTarget(Target& target) {
#if STACKCHAN_HAS_CAMERA
  const int32_t size = min(M5.Display.width(), M5.Display.height());
  const int32_t cx = M5.Display.width() / 2 + size * 36 / 100;
  const int32_t cy = M5.Display.height() / 2 - size * 17 / 100;
  const int32_t r = 28;
  target.fillCircle(cx, cy, r + 4, TFT_BLACK);
  if (!micConnected_) {
    cameraOverlayDirty_ = false;
    return;
  }

  const uint16_t border = cameraButtonPending_ ? M5.Display.color565(150, 150, 150) : M5.Display.color565(90, 170, 230);
  const uint16_t fill = cameraButtonPending_ ? M5.Display.color565(32, 32, 36) : M5.Display.color565(14, 32, 46);
  const uint16_t icon = cameraButtonPending_ ? M5.Display.color565(145, 150, 155) : M5.Display.color565(205, 232, 255);
  target.fillCircle(cx, cy, r, fill);
  target.drawCircle(cx, cy, r, border);
  target.fillRect(cx - 11, cy - 4, 22, 14, icon);
  target.fillRect(cx - 6, cy - 10, 10, 6, icon);
  target.fillCircle(cx, cy + 3, 6, fill);
  target.drawCircle(cx, cy + 3, 6, icon);
  if (cameraButtonPending_) {
    target.drawArc(cx, cy + 18, 6, 5, 30, 330, border);
  }
#else
  (void)target;
#endif
  cameraOverlayDirty_ = false;
}

template <typename Target>
void FaceController::drawRoundMicOverlayTarget(Target& target) {
#if STACKCHAN_ROUND_DISPLAY
  (void)target;
  micOverlayDirty_ = false;
#else
  const int32_t size = min(M5.Display.width(), M5.Display.height());
  const int32_t cx = M5.Display.width() / 2 + size * 36 / 100;
  const int32_t cy = M5.Display.height() / 2 + size * 17 / 100;
  const int32_t r = 28;
  target.fillCircle(cx, cy, r + 4, TFT_BLACK);
  if (!micConnected_) {
    micOverlayDirty_ = false;
    return;
  }

  const uint16_t border = micMuted_ ? M5.Display.color565(220, 90, 90) : M5.Display.color565(90, 210, 150);
  const uint16_t fill = micMuted_ ? M5.Display.color565(52, 18, 22) : M5.Display.color565(16, 42, 30);
  const uint16_t icon = micStreaming_ ? M5.Display.color565(120, 255, 175) : M5.Display.color565(210, 220, 210);
  target.fillCircle(cx, cy, r, fill);
  target.drawCircle(cx, cy, r, border);
  target.fillRoundRect(cx - 5, cy - 13, 10, 22, 5, icon);
  target.drawLine(cx - 10, cy + 4, cx - 10, cy + 10, icon);
  target.drawLine(cx + 10, cy + 4, cx + 10, cy + 10, icon);
  target.drawArc(cx, cy + 4, 12, 11, 0, 180, icon);
  target.drawLine(cx, cy + 15, cx, cy + 22, icon);
  target.drawLine(cx - 9, cy + 22, cx + 9, cy + 22, icon);
  if (micMuted_) {
    target.drawLine(cx - 14, cy - 16, cx + 14, cy + 17, border);
    target.drawLine(cx - 13, cy - 16, cx + 15, cy + 17, border);
  }
  micOverlayDirty_ = false;
#endif
}

void FaceController::drawAffectionOverlay(unsigned long now) {
#if STACKCHAN_ROUND_DISPLAY
  drawRoundAffectionOverlayTarget(M5.Display, now);
  return;
#endif
  lastAffectionOverlayMs_ = now;
  if (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = 0;
    affectionDelta_ = 0;
  }

#if STACKCHAN_SMALL_DISPLAY
  {
    const int32_t x = 6;
    const int32_t y = M5.Display.height() - 10;
    const int32_t w = 74;
    const int32_t h = 4;
    const int32_t fillW = map(constrain(affectionState_.affection, 0, 1000), 0, 1000, 0, w);
    const uint16_t color = affectionColor();

    M5.Display.fillRect(x - 2, y - 3, w + 44, h + 7, TFT_BLACK);
    M5.Display.drawRect(x, y, w, h, M5.Display.color565(78, 70, 76));
    if (fillW > 0) {
      M5.Display.fillRect(x, y, fillW, h, color);
    }
    drawHeart(M5.Display, x + w + 9, y + 2, 5, color);
    if (affectionDelta_ != 0) {
      const bool positive = affectionDelta_ > 0;
      const uint16_t deltaColor = positive ? M5.Display.color565(255, 92, 150) : M5.Display.color565(128, 96, 120);
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(deltaColor, TFT_BLACK);
      M5.Display.setCursor(x + w + 16, y - 4);
      M5.Display.printf("%+d", affectionDelta_);
    }
    affectionOverlayDirty_ = false;
    return;
  }
#endif

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
#if STACKCHAN_ROUND_DISPLAY
  drawRoundBatteryOverlayTarget(M5.Display);
  return;
#endif
#if STACKCHAN_SMALL_DISPLAY
  batteryOverlayDirty_ = false;
  clockOverlayDirty_ = false;
  return;
#endif
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
  clockOverlayDirty_ = false;
}

void FaceController::drawAffectionOverlayOnCanvas(unsigned long now) {
#if STACKCHAN_ROUND_DISPLAY
  drawRoundAffectionOverlayTarget(canvas_, now);
  return;
#endif
  lastAffectionOverlayMs_ = now;
  if (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = 0;
    affectionDelta_ = 0;
  }

#if STACKCHAN_SMALL_DISPLAY
  {
    const int32_t x = 6;
    const int32_t y = M5.Display.height() - 10;
    const int32_t w = 74;
    const int32_t h = 4;
    const int32_t fillW = map(constrain(affectionState_.affection, 0, 1000), 0, 1000, 0, w);
    const uint16_t color = affectionColor();

    canvas_.fillRect(x - 2, y - 3, w + 44, h + 7, TFT_BLACK);
    canvas_.drawRect(x, y, w, h, M5.Display.color565(78, 70, 76));
    if (fillW > 0) {
      canvas_.fillRect(x, y, fillW, h, color);
    }
    drawHeart(canvas_, x + w + 9, y + 2, 5, color);
    if (affectionDelta_ != 0) {
      const bool positive = affectionDelta_ > 0;
      const uint16_t deltaColor = positive ? M5.Display.color565(255, 92, 150) : M5.Display.color565(128, 96, 120);
      canvas_.setTextSize(1);
      canvas_.setTextColor(deltaColor, TFT_BLACK);
      canvas_.setCursor(x + w + 16, y - 4);
      canvas_.printf("%+d", affectionDelta_);
    }
    affectionOverlayDirty_ = false;
    return;
  }
#endif

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
#if STACKCHAN_ROUND_DISPLAY
  drawRoundBatteryOverlayTarget(canvas_);
  return;
#endif
#if STACKCHAN_SMALL_DISPLAY
  batteryOverlayDirty_ = false;
  clockOverlayDirty_ = false;
  return;
#endif
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
  clockOverlayDirty_ = false;
}

void FaceController::drawMicOverlay() {
#if STACKCHAN_ROUND_DISPLAY
  drawRoundMicOverlayTarget(M5.Display);
  return;
#endif
#if STACKCHAN_SMALL_DISPLAY
  {
    const int32_t cx = M5.Display.width() - 13;
    const int32_t cy = 13;
    const int32_t r = 9;
    const uint16_t border = !micConnected_
                              ? M5.Display.color565(90, 94, 100)
                              : (micMuted_ ? M5.Display.color565(220, 90, 90) : M5.Display.color565(90, 210, 150));
    const uint16_t fill = !micConnected_
                            ? M5.Display.color565(22, 24, 28)
                            : (micMuted_ ? M5.Display.color565(52, 18, 22) : M5.Display.color565(16, 42, 30));
    const uint16_t icon = !micConnected_
                            ? M5.Display.color565(116, 122, 130)
                            : (micStreaming_ ? M5.Display.color565(120, 255, 175) : M5.Display.color565(210, 220, 210));
    M5.Display.fillCircle(cx, cy, r, fill);
    M5.Display.drawCircle(cx, cy, r, border);
    M5.Display.fillRoundRect(cx - 3, cy - 5, 6, 9, 3, icon);
    M5.Display.drawLine(cx, cy + 4, cx, cy + 7, icon);
    M5.Display.drawLine(cx - 4, cy + 7, cx + 4, cy + 7, icon);
    if (micMuted_ || !micConnected_) {
      M5.Display.drawLine(cx - 6, cy - 6, cx + 6, cy + 7, border);
    }
    micOverlayDirty_ = false;
    return;
  }
#endif
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

void FaceController::drawCameraOverlay() {
#if STACKCHAN_ROUND_DISPLAY
  drawRoundCameraOverlayTarget(M5.Display);
  return;
#endif
#if !STACKCHAN_HAS_CAMERA
  cameraOverlayDirty_ = false;
  return;
#endif
  const int32_t w = 30;
  const int32_t h = 64;
  const int32_t x = M5.Display.width() - w - 5;
  const int32_t y = M5.Display.height() - 140;
  M5.Display.fillRoundRect(x - 2, y - 2, w + 4, h + 4, 8, TFT_BLACK);
  if (!micConnected_) {
    cameraOverlayDirty_ = false;
    return;
  }

  const uint16_t border = cameraButtonPending_ ? M5.Display.color565(150, 150, 150) : M5.Display.color565(90, 170, 230);
  const uint16_t fill = cameraButtonPending_ ? M5.Display.color565(32, 32, 36) : M5.Display.color565(14, 32, 46);
  const uint16_t icon = cameraButtonPending_ ? M5.Display.color565(145, 150, 155) : M5.Display.color565(205, 232, 255);
  M5.Display.fillRoundRect(x, y, w, h, 8, fill);
  M5.Display.drawRoundRect(x, y, w, h, 8, border);
  M5.Display.fillRect(x + 6, y + 27, 18, 12, icon);
  M5.Display.fillRect(x + 10, y + 23, 8, 5, icon);
  M5.Display.fillCircle(x + 15, y + 33, 5, fill);
  M5.Display.drawCircle(x + 15, y + 33, 5, icon);
  if (cameraButtonPending_) {
    M5.Display.drawArc(x + 15, y + 47, 5, 4, 30, 330, border);
  }
  cameraOverlayDirty_ = false;
}

void FaceController::drawCameraOverlayOnCanvas() {
#if STACKCHAN_ROUND_DISPLAY
  drawRoundCameraOverlayTarget(canvas_);
  return;
#endif
#if !STACKCHAN_HAS_CAMERA
  cameraOverlayDirty_ = false;
  return;
#endif
  const int32_t w = 30;
  const int32_t h = 64;
  const int32_t x = M5.Display.width() - w - 5;
  const int32_t y = M5.Display.height() - 140;
  canvas_.fillRoundRect(x - 2, y - 2, w + 4, h + 4, 8, TFT_BLACK);
  if (!micConnected_) {
    cameraOverlayDirty_ = false;
    return;
  }

  const uint16_t border = cameraButtonPending_ ? M5.Display.color565(150, 150, 150) : M5.Display.color565(90, 170, 230);
  const uint16_t fill = cameraButtonPending_ ? M5.Display.color565(32, 32, 36) : M5.Display.color565(14, 32, 46);
  const uint16_t icon = cameraButtonPending_ ? M5.Display.color565(145, 150, 155) : M5.Display.color565(205, 232, 255);
  canvas_.fillRoundRect(x, y, w, h, 8, fill);
  canvas_.drawRoundRect(x, y, w, h, 8, border);
  canvas_.fillRect(x + 6, y + 27, 18, 12, icon);
  canvas_.fillRect(x + 10, y + 23, 8, 5, icon);
  canvas_.fillCircle(x + 15, y + 33, 5, fill);
  canvas_.drawCircle(x + 15, y + 33, 5, icon);
  if (cameraButtonPending_) {
    canvas_.drawArc(x + 15, y + 47, 5, 4, 30, 330, border);
  }
  cameraOverlayDirty_ = false;
}

void FaceController::drawMicOverlayOnCanvas() {
#if STACKCHAN_ROUND_DISPLAY
  drawRoundMicOverlayTarget(canvas_);
  return;
#endif
#if STACKCHAN_SMALL_DISPLAY
  {
    const int32_t cx = M5.Display.width() - 13;
    const int32_t cy = 13;
    const int32_t r = 9;
    const uint16_t border = !micConnected_
                              ? M5.Display.color565(90, 94, 100)
                              : (micMuted_ ? M5.Display.color565(220, 90, 90) : M5.Display.color565(90, 210, 150));
    const uint16_t fill = !micConnected_
                            ? M5.Display.color565(22, 24, 28)
                            : (micMuted_ ? M5.Display.color565(52, 18, 22) : M5.Display.color565(16, 42, 30));
    const uint16_t icon = !micConnected_
                            ? M5.Display.color565(116, 122, 130)
                            : (micStreaming_ ? M5.Display.color565(120, 255, 175) : M5.Display.color565(210, 220, 210));
    canvas_.fillCircle(cx, cy, r, fill);
    canvas_.drawCircle(cx, cy, r, border);
    canvas_.fillRoundRect(cx - 3, cy - 5, 6, 9, 3, icon);
    canvas_.drawLine(cx, cy + 4, cx, cy + 7, icon);
    canvas_.drawLine(cx - 4, cy + 7, cx + 4, cy + 7, icon);
    if (micMuted_ || !micConnected_) {
      canvas_.drawLine(cx - 6, cy - 6, cx + 6, cy + 7, border);
    }
    micOverlayDirty_ = false;
    return;
  }
#endif
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
#if STACKCHAN_ROUND_DISPLAY
  return drawRoundCachedTalkFace(path);
#else
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
  const unsigned long now = millis();
#if STACKCHAN_SMALL_DISPLAY
  const void* buffer = talkCanvas_[setIndex][index].getBuffer();
  if (canvasReady_ && buffer != nullptr) {
    canvas_.fillScreen(TFT_BLACK);
    canvas_.pushImage(x, y, FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT, static_cast<const uint16_t*>(buffer));
    drawAffectionOverlayOnCanvas(now);
    drawBatteryOverlayOnCanvas();
    drawCameraOverlayOnCanvas();
    drawMicOverlayOnCanvas();
    canvas_.pushSprite(&M5.Display, 0, 0);
  } else {
    talkCanvas_[setIndex][index].pushSprite(&M5.Display, x, y);
    drawAffectionOverlay(now);
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
#else
  talkCanvas_[setIndex][index].pushSprite(&M5.Display, x, y);
  if (overlaysNeedRefresh(now)) {
    drawAffectionOverlay(now);
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
#endif
  return true;
#endif
}

#if STACKCHAN_ROUND_DISPLAY
bool FaceController::drawRoundCachedTalkFace(const char* path) {
  if (state_ != ChanState::Speaking) {
    return false;
  }

  const char* path0 = talkFacePath(0);
  const char* path1 = talkFacePath(1);
  int index = -1;
  if (strcmp(path, path0) == 0) {
    index = 0;
  } else if (strcmp(path, path1) == 0) {
    index = 1;
  } else {
    return false;
  }

  if (!prepareRoundTalkCache(path0, path1) || !roundTalkCacheReady_[index]) {
    return false;
  }

  roundTalkCanvas_[index].pushSprite(&M5.Display, 0, 0);
  const unsigned long now = millis();
  drawAffectionOverlay(now);
  drawBatteryOverlay();
  drawCameraOverlay();
  drawMicOverlay();
  return true;
}

bool FaceController::prepareRoundTalkCache(const char* path0, const char* path1) {
  const char* paths[2] = {path0, path1};
  bool ok = true;

  for (uint8_t i = 0; i < 2; ++i) {
    if (paths[i] == nullptr || paths[i][0] == '\0') {
      roundTalkCacheReady_[i] = false;
      ok = false;
      continue;
    }

    if (roundTalkCacheReady_[i] && roundTalkCachePath_[i] == paths[i]) {
      continue;
    }

    if (!roundTalkCacheAllocated_[i]) {
      roundTalkCanvas_[i].setPsram(true);
      roundTalkCanvas_[i].setColorDepth(16);
      if (roundTalkCanvas_[i].createSprite(M5.Display.width(), M5.Display.height()) == nullptr) {
        Serial.printf("[face] round talk cache %u allocate failed\n", static_cast<unsigned>(i));
        roundTalkCacheReady_[i] = false;
        ok = false;
        continue;
      }
      roundTalkCacheAllocated_[i] = true;
    }

    roundTalkCacheReady_[i] = loadRoundBaseFaceToCanvas(roundTalkCanvas_[i], paths[i]);
    roundTalkCachePath_[i] = paths[i];
    ok &= roundTalkCacheReady_[i];
    Serial.printf("[face] round talk cache %u %s: %s\n",
                  static_cast<unsigned>(i),
                  paths[i],
                  roundTalkCacheReady_[i] ? "ready" : "failed");
  }

  return ok;
}

bool FaceController::loadRoundBaseFaceToCanvas(M5Canvas& canvas, const char* path) {
  if (!LittleFS.exists(path)) {
    Serial.printf("[face] missing round cache file: %s\n", path);
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[face] failed to open round cache file: %s\n", path);
    return false;
  }

  canvas.fillScreen(TFT_BLACK);
  const bool ok = drawFaceImageTarget(canvas, file);
  file.close();
  return ok;
}
#endif

bool FaceController::overlaysNeedRefresh(unsigned long now) const {
  return affectionOverlayDirty_ ||
         batteryOverlayDirty_ ||
         clockOverlayDirty_ ||
         cameraOverlayDirty_ ||
         micOverlayDirty_ ||
         (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_);
}

void FaceController::prepareTalkCache() {
#if STACKCHAN_ROUND_DISPLAY
  prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
  return;
#else
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
#endif
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
