#include "FaceController.h"

#include "config.h"

void FaceController::begin() {
  canvas_.setPsram(true);
  canvas_.setColorDepth(16);
  canvasReady_ = canvas_.createSprite(M5.Display.width(), M5.Display.height()) != nullptr;
  if (!canvasReady_) {
    Serial.println("[face] failed to allocate canvas; direct drawing fallback enabled");
  }
#if STACKCHAN_PET_ANIMATION_ENABLED
  preparePetAnimationCache();
#endif
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
#if STACKCHAN_GURUGURU_FACE_ENABLED
  stopGuruguruDizzyAnimation(false);
#endif
#if STACKCHAN_PET_ANIMATION_ENABLED
  stopPetAnimation(false);
#endif
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
#if STACKCHAN_PET_ANIMATION_ENABLED
  setPetFaceMode(enabled, millis(), false, false);
#else
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
#endif
}

#if STACKCHAN_PET_ANIMATION_ENABLED
void FaceController::setPetFaceMode(bool enabled, unsigned long now, bool animate, bool longPetting) {
  const bool wasPetFaceMode = petFaceMode_;
  if (petFaceMode_ == enabled) {
    if (!enabled && petAnimationActive()) {
      if (animate) {
        finishPetAnimation(now, longPetting);
      } else {
        stopPetAnimation(true);
      }
    }
    return;
  }

#if FACE_DIAG_LOG_ENABLED
  logSpeakingInterference("setPet", enabled ? 1 : 0);
#endif
  petFaceMode_ = enabled;
  const bool useAnimationPath = (enabled && animate) ||
                                (!enabled && animate && wasPetFaceMode && petAnimationActive());
#if STACKCHAN_ROUND_DISPLAY
  if (!useAnimationPath) {
    prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
  }
#endif
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = now;
  scheduleBlink(lastLipSyncMs_);

  if (enabled) {
    if (animate && startPetAnimation(now)) {
      return;
    }
    stopPetAnimation(false);
  } else if (animate && wasPetFaceMode && petAnimationActive()) {
    finishPetAnimation(now, longPetting);
    return;
  } else {
    stopPetAnimation(false);
  }

  if (enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

bool FaceController::petAnimationActive() const {
  return petAnimationPhase_ != PetAnimationPhase::None;
}

void FaceController::setPetAnimationTouchFrame(uint8_t frame, unsigned long now) {
  frame = constrain(frame, static_cast<uint8_t>(2), static_cast<uint8_t>(4));
  if (petAnimationTouchFrame_ == frame) {
    return;
  }

  petAnimationTouchFrame_ = frame;
  if (petAnimationPhase_ != PetAnimationPhase::Loop || !enabled_) {
    return;
  }

  if (!drawPetAnimationFrame(petAnimationTouchFrame_)) {
    Serial.printf("[face] pet animation touch draw failed frame=%u\n",
                  static_cast<unsigned>(petAnimationTouchFrame_));
    stopPetAnimation(true);
    return;
  }
  nextPetAnimationFrameMs_ = now + PET_ANIMATION_LOOP_INTERVAL_MS;
}
#endif

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

void FaceController::setGuruguruFaceMode(bool enabled) {
  if (guruguruFaceMode_ == enabled) {
    return;
  }

#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  if (enabled) {
#if STACKCHAN_DEVICE_CORES3
    releaseTalkCache();
#endif
    releasePetAnimationCache();
    prepareGuruguruFaceCache();
    releaseGuruguruBlinkCache();
    prepareGuruguruBlinkCache(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
    if (!preloadGuruguruDizzyPlaybackRange(false,
                                           0,
                                           GURUGURU_DIZZY_FRAME_COUNT,
                                           kGuruguruDizzyCanvasSlots)) {
      releaseGuruguruBlinkCache();
      preloadGuruguruDizzyPlaybackRange(false,
                                        0,
                                        GURUGURU_DIZZY_FRAME_COUNT,
                                        kGuruguruDizzyCanvasSlots);
      if (prepareGuruguruBlinkCache(STACKCHAN_GURUGURU_FACE_CENTER_INDEX) < 0) {
        releaseGuruguruDizzySourceRange(13, 15);
        prepareGuruguruBlinkCache(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
      }
    }
  } else {
    releaseGuruguruFaceCache();
#if STACKCHAN_DEVICE_CORES3
    prepareTalkCache();
#endif
    preparePetAnimationCache();
  }
#endif

  guruguruFaceMode_ = enabled;
#if STACKCHAN_GURUGURU_FACE_ENABLED
  stopGuruguruDizzyAnimation(false);
#endif
  if (enabled) {
    guruguruFaceDirection_ = STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
  }
  lipOpen_ = false;
  blinking_ = false;
  smiling_ = false;
  lastLipSyncMs_ = millis();
  scheduleBlink(lastLipSyncMs_);
  scheduleSmile(lastLipSyncMs_);

  if (enabled_ && state_ == ChanState::Idle) {
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::setGuruguruFaceDirection(uint8_t direction) {
  direction = constrain(direction,
                        static_cast<uint8_t>(0),
                        static_cast<uint8_t>(STACKCHAN_GURUGURU_FACE_COUNT - 1));
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (guruguruDizzyAnimating_) {
    return;
  }
#endif
  if (guruguruFaceDirection_ == direction) {
    return;
  }

  guruguruFaceDirection_ = direction;
  if (!enabled_ || !guruguruFaceMode_ || state_ != ChanState::Idle) {
    return;
  }

  currentPath_ = "";
  if (blinking_) {
    drawFace(blinkFacePath());
  } else {
    showBaseFace();
  }
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  if (direction == STACKCHAN_GURUGURU_FACE_CENTER_INDEX) {
    prepareGuruguruBlinkCache(direction);
  }
#endif
}

#if STACKCHAN_GURUGURU_FACE_ENABLED
bool FaceController::startGuruguruDizzyAnimation(bool reverse, unsigned long now) {
  if (!enabled_ || !guruguruFaceMode_ || state_ != ChanState::Idle) {
    return false;
  }

  const String firstFramePath = guruguruDizzyFramePath(reverse, 0);
  if (firstFramePath.isEmpty()) {
    Serial.printf("[guruguru] dizzy animation missing reverse=%d\n", reverse ? 1 : 0);
    return false;
  }

#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  guruguruDizzyBlinkReleased_ = false;
  if (!preloadGuruguruDizzyPlaybackRange(reverse,
                                         0,
                                         GURUGURU_DIZZY_FRAME_COUNT,
                                         kGuruguruDizzyCanvasSlots)) {
    releaseGuruguruBlinkCache();
    guruguruDizzyBlinkReleased_ = true;
    if (!preloadGuruguruDizzyPlaybackRange(reverse,
                                           0,
                                           GURUGURU_DIZZY_FRAME_COUNT,
                                           kGuruguruDizzyCanvasSlots)) {
    Serial.printf("[guruguru] dizzy full preload with dir cache failed reverse=%d freePsram=%u\n",
                  reverse ? 1 : 0,
                  static_cast<unsigned>(ESP.getFreePsram()));
    releaseGuruguruDizzyJpegCache();
    releaseGuruguruDirCache();
    if (!preloadGuruguruDizzyPlaybackRange(reverse,
                                           0,
                                           GURUGURU_DIZZY_FRAME_COUNT,
                                           kGuruguruDizzyCanvasSlots)) {
      Serial.printf("[guruguru] dizzy full preload failed reverse=%d freePsram=%u\n",
                    reverse ? 1 : 0,
                    static_cast<unsigned>(ESP.getFreePsram()));
      releaseGuruguruDizzyJpegCache();
      prepareGuruguruFaceCache();
      return false;
    }
    prepareGuruguruFaceCache();
    }
  }
  guruguruDizzySpinReleased_ = false;
  guruguruDizzyKeepSpinCacheOnStop_ = false;
#endif

  guruguruDizzyAnimating_ = true;
  guruguruDizzyReverse_ = reverse;
  guruguruDizzyFrame_ = 0;
  guruguruDizzyStartedMs_ = now;
  nextGuruguruDizzyFrameMs_ = now;
  blinking_ = false;
  smiling_ = false;
  currentPath_ = "";
  Serial.printf("[guruguru] dizzy animation start reverse=%d\n", reverse ? 1 : 0);
  return true;
}

bool FaceController::guruguruDizzyAnimationActive() const {
  return guruguruDizzyAnimating_;
}

void FaceController::stopGuruguruDizzyAnimation(bool restoreFace) {
  if (!guruguruDizzyAnimating_) {
    return;
  }
  guruguruDizzyAnimating_ = false;
  guruguruDizzyFrame_ = 0;
  guruguruDizzyStartedMs_ = 0;
  nextGuruguruDizzyFrameMs_ = 0;
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  const bool keepDizzyCache = guruguruDizzyKeepSpinCacheOnStop_ &&
                              enabled_ &&
                              guruguruFaceMode_ &&
                              state_ == ChanState::Idle;
  if (!keepDizzyCache) {
    releaseGuruguruDizzyJpegCache();
  }
  guruguruDizzyBlinkReleased_ = false;
  guruguruDizzySpinReleased_ = false;
  guruguruDizzyKeepSpinCacheOnStop_ = false;
  if (enabled_ && guruguruFaceMode_ && state_ == ChanState::Idle) {
    prepareGuruguruFaceCache();
    if (prepareGuruguruBlinkCache(STACKCHAN_GURUGURU_FACE_CENTER_INDEX) < 0) {
      releaseGuruguruDizzySourceRange(13, 15);
      prepareGuruguruBlinkCache(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
    }
  }
#endif
  if (restoreFace && enabled_ && state_ == ChanState::Idle) {
    guruguruFaceDirection_ = STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
    currentPath_ = "";
    showBaseFace();
  }
}

void FaceController::drawGuruguruDizzyFrame(unsigned long now) {
  if (!guruguruFaceMode_ || state_ != ChanState::Idle) {
    stopGuruguruDizzyAnimation(true);
    return;
  }

  if (now < nextGuruguruDizzyFrameMs_) {
    return;
  }

  const uint8_t frame = guruguruDizzyFrame_;

  if (frame >= GURUGURU_DIZZY_FRAME_COUNT) {
    stopGuruguruDizzyAnimation(true);
    Serial.println("[guruguru] dizzy animation end");
    return;
  }

  const String framePath = guruguruDizzyFramePath(guruguruDizzyReverse_, frame);
  const char* path = framePath.c_str();
  if (framePath.isEmpty()) {
    Serial.printf("[guruguru] dizzy frame missing reverse=%d frame=%u\n",
                  guruguruDizzyReverse_ ? 1 : 0,
                  static_cast<unsigned>(frame));
    stopGuruguruDizzyAnimation(true);
    return;
  }

  if (!drawGuruguruDizzyFrameDirect(path, frame)) {
    Serial.printf("[guruguru] dizzy frame draw failed: %s\n", path);
    stopGuruguruDizzyAnimation(true);
    return;
  }
  guruguruDizzyFrame_ = frame + 1;
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  serviceGuruguruDizzyCache(frame);
#endif
  if (guruguruDizzyFrame_ >= GURUGURU_DIZZY_FRAME_COUNT) {
    Serial.println("[guruguru] dizzy animation end");
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
    guruguruDizzyKeepSpinCacheOnStop_ = true;
#endif
    stopGuruguruDizzyAnimation(true);
  } else {
    nextGuruguruDizzyFrameMs_ = now + GURUGURU_DIZZY_FRAME_INTERVAL_MS;
  }
}

bool FaceController::drawGuruguruDizzyFrameDirect(const char* path, uint8_t frame) {
  const String imagePath = resolvedImagePath(path);
  if (imagePath.isEmpty()) {
    return false;
  }

#if STACKCHAN_ROUND_DISPLAY
  const int32_t drawSize = faceImageDrawSize();
  const int32_t baseX = (M5.Display.width() - drawSize) / 2;
  const int32_t baseY = (M5.Display.height() - drawSize) / 2;
  const float scale = faceImageScale();
#else
  const int32_t baseX = faceImageDrawX();
  const int32_t baseY = faceImageDrawY();
  const float scale = 1.0f;
#endif

  int32_t x = baseX;
  int32_t y = baseY;
#if STACKCHAN_ROUND_DISPLAY
  int32_t scaledW = drawSize;
  int32_t scaledH = drawSize;
#else
  int32_t scaledW = FACE_IMAGE_WIDTH;
  int32_t scaledH = FACE_IMAGE_HEIGHT;
#endif

  M5.Display.startWrite();
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  const int16_t frameX = frame < GURUGURU_DIZZY_FRAME_COUNT ? guruguruDizzyFrameX_[frame] : 0;
  const int16_t frameY = frame < GURUGURU_DIZZY_FRAME_COUNT ? guruguruDizzyFrameY_[frame] : 0;
  const int16_t metaW = frame < GURUGURU_DIZZY_FRAME_COUNT ? guruguruDizzyFrameW_[frame] : 0;
  const int16_t metaH = frame < GURUGURU_DIZZY_FRAME_COUNT ? guruguruDizzyFrameH_[frame] : 0;
  const int16_t frameW = metaW > 0 ? metaW : FACE_IMAGE_WIDTH;
  const int16_t frameH = metaH > 0 ? metaH : FACE_IMAGE_HEIGHT;
  x = baseX + static_cast<int32_t>(frameX * scale + 0.5f);
  y = baseY + static_cast<int32_t>(frameY * scale + 0.5f);
  scaledW = max<int32_t>(1, static_cast<int32_t>(frameW * scale + 0.5f));
  scaledH = max<int32_t>(1, static_cast<int32_t>(frameH * scale + 0.5f));
  const uint8_t cacheIndex = guruguruDizzySourceFrame(guruguruDizzyReverse_, frame);
  int8_t canvasSlot = findGuruguruDizzyCanvasSlot(cacheIndex);
  if (canvasSlot < 0 && loadGuruguruDizzyCanvasFrame(guruguruDizzyReverse_, frame)) {
    canvasSlot = findGuruguruDizzyCanvasSlot(cacheIndex);
  }
  if (canvasSlot >= 0) {
    const int32_t canvasX = x + (scaledW - GURUGURU_DIZZY_CANVAS_SIZE) / 2;
    const int32_t canvasY = y + (scaledH - GURUGURU_DIZZY_CANVAS_SIZE) / 2;
    const unsigned long overlayNow = millis();
    const void* buffer = guruguruDizzyCanvasCache_[canvasSlot].getBuffer();
    if (canvasReady_ && buffer != nullptr) {
      M5.Display.endWrite();
      canvas_.fillScreen(TFT_BLACK);
      canvas_.pushImage(canvasX,
                        canvasY,
                        GURUGURU_DIZZY_CANVAS_SIZE,
                        GURUGURU_DIZZY_CANVAS_SIZE,
                        static_cast<const uint16_t*>(buffer));
      drawAffectionOverlayOnCanvas(overlayNow);
      drawBatteryOverlayOnCanvas();
      drawCameraOverlayOnCanvas();
      drawMicOverlayOnCanvas();
      canvas_.pushSprite(&M5.Display, 0, 0);
    } else {
#if STACKCHAN_ROUND_DISPLAY
      M5.Display.fillRect(baseX, baseY, drawSize, drawSize, TFT_BLACK);
#else
      M5.Display.fillRect(baseX, baseY, FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT, TFT_BLACK);
#endif
      guruguruDizzyCanvasCache_[canvasSlot].pushSprite(&M5.Display, canvasX, canvasY);
      M5.Display.endWrite();
      drawAffectionOverlay(overlayNow);
      drawBatteryOverlay();
      drawCameraOverlay();
      drawMicOverlay();
    }
    currentPath_ = path;
    return true;
  }
#endif

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    M5.Display.endWrite();
    Serial.printf("[guruguru] dizzy frame open failed: %s\n", imagePath.c_str());
    return false;
  }
#if STACKCHAN_ROUND_DISPLAY
  M5.Display.fillRect(baseX, baseY, drawSize, drawSize, TFT_BLACK);
#else
  M5.Display.fillRect(baseX, baseY, FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT, TFT_BLACK);
#endif
#if STACKCHAN_ROUND_DISPLAY
  const bool ok = isJpegPath(imagePath.c_str())
                    ? M5.Display.drawJpg(&file,
                                         x,
                                         y,
                                         scaledW,
                                         scaledH,
                                         0,
                                         0,
                                         scale,
                                         scale,
                                         datum_t::top_left)
                    : M5.Display.drawPng(&file,
                                         x,
                                         y,
                                         scaledW,
                                         scaledH,
                                         0,
                                         0,
                                         scale,
                                         scale,
                                         datum_t::top_left);
#else
  const bool ok = isJpegPath(imagePath.c_str()) ? M5.Display.drawJpg(&file, x, y)
                                                : M5.Display.drawPng(&file, x, y);
#endif
  M5.Display.endWrite();
  file.close();
  if (ok) {
    const unsigned long overlayNow = millis();
    drawAffectionOverlay(overlayNow);
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
  currentPath_ = path;
  return ok;
}

#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
bool FaceController::preloadGuruguruDizzyAnimation(bool reverse, uint8_t maxFrames) {
  releaseGuruguruBlinkCache();
  return preloadGuruguruDizzyJpegCache(reverse, maxFrames);
}
#endif
#endif

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

  if (enabled_ &&
      (state_ == ChanState::Listening || state_ == ChanState::Speaking)
#if STACKCHAN_PET_ANIMATION_ENABLED
      && !petAnimationActive()
#endif
  ) {
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
	    if (oldTier != newTier &&
	        !photoFaceMode_ &&
	        !photoMasterFaceMode_ &&
	        !guruguruFaceMode_ &&
	        authFaceMode_ == AuthFaceMode::Unknown
#if STACKCHAN_PET_ANIMATION_ENABLED
        && !petAnimationActive()
#endif
    ) {
#if STACKCHAN_ROUND_DISPLAY
	      prepareRoundTalkCache(talkFacePath(0), talkFacePath(1));
#else
	      releaseTalkCache();
	      prepareTalkCache();
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

void FaceController::showGuruguruStep(uint8_t steps, uint8_t difficulty, unsigned long now) {
  guruguruStep_ = steps;
  guruguruStepDifficulty_ = difficulty;
  if (affectionDeltaUntilMs_ == 0 || now + 1800 > affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = now + 1800;
  }
  batteryOverlayDirty_ = true;
  clockOverlayDirty_ = true;
  if (enabled_ && state_ == ChanState::Idle) {
    drawBatteryOverlay();
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

#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (guruguruDizzyAnimating_) {
    drawGuruguruDizzyFrame(now);
    return;
  }
#endif
#if STACKCHAN_PET_ANIMATION_ENABLED
  if (petAnimationActive()) {
    updatePetAnimation(now);
    return;
  }
#endif

  const bool affectionDeltaExpired = affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_;
  if (affectionDeltaExpired && guruguruStep_ > 0) {
    currentPath_ = "";
    showBaseFace();
    return;
  }
  if (affectionOverlayDirty_ ||
      batteryOverlayDirty_ ||
      clockOverlayDirty_ ||
      cameraOverlayDirty_ ||
      micOverlayDirty_ ||
      affectionDeltaExpired) {
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
      !guruguruFaceMode_ &&
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
  if (guruguruFaceMode_ && state_ == ChanState::Idle) {
    return guruguruFacePath(true);
  }
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

const char* FaceController::guruguruFacePath(bool blink) const {
  static const char* dirPaths[25] = {
    "/dir0.png",
    "/dir1.png",
    "/dir2.png",
    "/dir3.png",
    "/dir4.png",
    "/dir5.png",
    "/dir6.png",
    "/dir7.png",
    "/dir8.png",
    "/dir9.png",
    "/dir10.png",
    "/dir11.png",
    "/dir12.png",
    "/dir13.png",
    "/dir14.png",
    "/dir15.png",
    "/dir16.png",
    "/dir17.png",
    "/dir18.png",
    "/dir19.png",
    "/dir20.png",
    "/dir21.png",
    "/dir22.png",
    "/dir23.png",
    "/dir24.png",
  };
  static const char* blinkPaths[25] = {
    "/blink0.png",
    "/blink1.png",
    "/blink2.png",
    "/blink3.png",
    "/blink4.png",
    "/blink5.png",
    "/blink6.png",
    "/blink7.png",
    "/blink8.png",
    "/blink9.png",
    "/blink10.png",
    "/blink11.png",
    "/blink12.png",
    "/blink13.png",
    "/blink14.png",
    "/blink15.png",
    "/blink16.png",
    "/blink17.png",
    "/blink18.png",
    "/blink19.png",
    "/blink20.png",
    "/blink21.png",
    "/blink22.png",
    "/blink23.png",
    "/blink24.png",
  };

  const uint8_t direction = guruguruFaceDirection_ < STACKCHAN_GURUGURU_FACE_COUNT
                                ? guruguruFaceDirection_
                                : STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
  const uint8_t sourceIndex = guruguruFaceSourceIndex(direction);
  const bool useBlink = blink &&
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
                        direction == STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
#else
                        true;
#endif
  const char* preferred = useBlink ? blinkPaths[sourceIndex] : dirPaths[sourceIndex];
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  static char virtualPath[16];
  snprintf(virtualPath,
           sizeof(virtualPath),
           useBlink ? "/blink%u.png" : "/dir%u.png",
           static_cast<unsigned>(direction));
  return virtualPath;
#else
  return fallbackFacePath(preferred, useBlink ? FACE_BLINK_PATH : FACE_IDLE_PATH);
#endif
}

const char* FaceController::fallbackFacePath(const char* preferred, const char* fallback) const {
  return resolvedImagePath(preferred).isEmpty() ? fallback : preferred;
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
      drawFace(guruguruFaceMode_ ? guruguruFacePath(false) : (shakeFaceMode_ ? shakeFacePath(0) : (petFaceMode_ ? petFacePath(0) : (photoMasterFaceMode_ ? FACE_PHOTO_MASTER_0_PATH : (photoFaceMode_ ? FACE_PHOTO_0_PATH : idleFacePath())))));
      break;
    case ChanState::Listening:
      drawFace(listeningFacePath());
      break;
    case ChanState::Speaking:
      drawFace(talkFacePath(0));
      break;
  }
}

void FaceController::drawFace(const char* path, bool drawOverlays) {
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
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  if (drawCachedGuruguruFace(path)) {
    currentPath_ = path;
    return;
  }
#endif

  currentPath_ = path;

  const String imagePath = resolvedImagePath(path);
  if (imagePath.isEmpty()) {
    Serial.printf("[face] missing file: %s\n", path);
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(16, 80);
    M5.Display.printf("Missing:\n%s", path);
    return;
  }

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    Serial.printf("[face] failed to open: %s\n", imagePath.c_str());
    return;
  }

  bool ok = false;
  if (canvasReady_) {
    canvas_.fillScreen(TFT_BLACK);
    ok = drawFaceImageTarget(canvas_, file, imagePath.c_str());
    if (ok && drawOverlays) {
      drawAffectionOverlayOnCanvas(millis());
      drawBatteryOverlayOnCanvas();
      drawCameraOverlayOnCanvas();
      drawMicOverlayOnCanvas();
      canvas_.pushSprite(&M5.Display, 0, 0);
    } else if (ok) {
      canvas_.pushSprite(&M5.Display, 0, 0);
    }
  } else {
    M5.Display.fillScreen(TFT_BLACK);
    ok = drawFaceImageTarget(M5.Display, file, imagePath.c_str());
  }
  file.close();
  if (!ok) {
    Serial.printf("[face] failed to draw: %s\n", path);
  } else if (!canvasReady_ && drawOverlays) {
    drawAffectionOverlay(millis());
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
}

#if STACKCHAN_PET_ANIMATION_ENABLED
String FaceController::petAnimationFramePath(uint8_t frame) const {
  if (frame >= kPetAnimationFrameCount) {
    return String();
  }

  char path[24];
  snprintf(path, sizeof(path), "/pet_anim_%u.png", static_cast<unsigned>(frame));
  return resolvedImagePath(path);
}

bool FaceController::preparePetAnimationCache() {
  if (petAnimationCachePrepared_) {
    return true;
  }

#if STACKCHAN_ROUND_DISPLAY
  releaseRoundTalkCache();
#endif

  const int32_t cacheW = M5.Display.width();
  const int32_t cacheH = M5.Display.height();
  bool ok = true;
  for (uint8_t frame = 0; frame < kPetAnimationFrameCount; ++frame) {
    if (petAnimationCacheReady_[frame]) {
      continue;
    }

    if (petAnimationFramePath(frame).isEmpty()) {
      Serial.printf("[face] pet animation frame missing: %u\n", static_cast<unsigned>(frame));
      ok = false;
      break;
    }

    if (!petAnimationCacheAllocated_[frame]) {
      petAnimationCanvas_[frame].setPsram(true);
      petAnimationCanvas_[frame].setColorDepth(16);
      if (petAnimationCanvas_[frame].createSprite(cacheW, cacheH) == nullptr) {
        Serial.printf("[face] pet animation cache %u allocate failed freePsram=%u\n",
                      static_cast<unsigned>(frame),
                      static_cast<unsigned>(ESP.getFreePsram()));
        ok = false;
        break;
      }
      petAnimationCacheAllocated_[frame] = true;
    }

    petAnimationCacheReady_[frame] = loadPetAnimationFrameToCanvas(petAnimationCanvas_[frame], frame);
    Serial.printf("[face] pet animation cache %u: %s freePsram=%u\n",
                  static_cast<unsigned>(frame),
                  petAnimationCacheReady_[frame] ? "ready" : "failed",
                  static_cast<unsigned>(ESP.getFreePsram()));
    ok &= petAnimationCacheReady_[frame];
    if (!ok) {
      break;
    }
  }

  if (!ok) {
    releasePetAnimationCache();
    return false;
  }

  petAnimationCachePrepared_ = true;
  return true;
}

void FaceController::releasePetAnimationCache() {
  stopPetAnimation(false);
  for (uint8_t frame = 0; frame < kPetAnimationFrameCount; ++frame) {
    if (petAnimationCacheAllocated_[frame]) {
      petAnimationCanvas_[frame].deleteSprite();
    }
    petAnimationCacheAllocated_[frame] = false;
    petAnimationCacheReady_[frame] = false;
  }
  petAnimationCachePrepared_ = false;
}

bool FaceController::loadPetAnimationFrameToCanvas(M5Canvas& canvas, uint8_t frame) {
  const String imagePath = petAnimationFramePath(frame);
  if (imagePath.isEmpty()) {
    return false;
  }

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    Serial.printf("[face] pet animation frame open failed: %s\n", imagePath.c_str());
    return false;
  }

  canvas.fillScreen(TFT_BLACK);
  const bool ok = drawFaceImageTarget(canvas, file, imagePath.c_str());
  file.close();
  return ok;
}

bool FaceController::drawPetAnimationFrame(uint8_t frame) {
  if (frame >= kPetAnimationFrameCount || !preparePetAnimationCache() || !petAnimationCacheReady_[frame]) {
    return false;
  }

  const void* buffer = petAnimationCanvas_[frame].getBuffer();
  const int32_t w = M5.Display.width();
  const int32_t h = M5.Display.height();
  const unsigned long now = millis();
#if STACKCHAN_SMALL_DISPLAY
  (void)buffer;
  (void)w;
  (void)h;
  petAnimationCanvas_[frame].pushSprite(&M5.Display, 0, 0);
  drawAffectionOverlay(now);
  drawBatteryOverlay();
  drawCameraOverlay();
  drawMicOverlay();
#else
  if (canvasReady_ && buffer != nullptr) {
    canvas_.pushImage(0, 0, w, h, static_cast<const uint16_t*>(buffer));
    drawAffectionOverlayOnCanvas(now);
    drawBatteryOverlayOnCanvas();
    drawCameraOverlayOnCanvas();
    drawMicOverlayOnCanvas();
    canvas_.pushSprite(&M5.Display, 0, 0);
  } else {
    petAnimationCanvas_[frame].pushSprite(&M5.Display, 0, 0);
    drawAffectionOverlay(now);
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
#endif
  currentPath_ = "";
  return true;
}

bool FaceController::startPetAnimation(unsigned long now) {
  if (!enabled_ || state_ == ChanState::Speaking || !preparePetAnimationCache()) {
    return false;
  }

  petAnimationPhase_ = PetAnimationPhase::Start;
  petAnimationSequenceIndex_ = 0;
  petAnimationTouchFrame_ = 3;
  petAnimationLong_ = false;
  nextPetAnimationFrameMs_ = now;
  blinking_ = false;
  smiling_ = false;
  currentPath_ = "";
  updatePetAnimation(now);
  return petAnimationActive();
}

void FaceController::finishPetAnimation(unsigned long now, bool longPetting) {
  if (!petAnimationActive() || !preparePetAnimationCache()) {
    stopPetAnimation(true);
    return;
  }

  petAnimationPhase_ = PetAnimationPhase::End;
  petAnimationSequenceIndex_ = 0;
  petAnimationLong_ = longPetting;
  nextPetAnimationFrameMs_ = now;
  blinking_ = false;
  smiling_ = false;
  currentPath_ = "";
  updatePetAnimation(now);
}

void FaceController::stopPetAnimation(bool restoreFace) {
  const bool wasActive = petAnimationActive();
  petAnimationPhase_ = PetAnimationPhase::None;
  petAnimationSequenceIndex_ = 0;
  petAnimationLong_ = false;
  nextPetAnimationFrameMs_ = 0;

  if (restoreFace && wasActive && enabled_) {
    currentPath_ = "";
    showBaseFace();
  }
}

const uint8_t* FaceController::petAnimationSequence(uint8_t& length, unsigned long& intervalMs) const {
  static const uint8_t kStart[] = {0, 1};
  static const uint8_t kLoop[] = {3, 2, 3, 4};
  static const uint8_t kEnd[] = {3, 2};
  static const uint8_t kLongAfter[] = {1, 0, 5, 6, 5, 6, 5, 6, 5};
  static const uint8_t kShortAfter[] = {1, 0, 7, 8, 7, 8, 7, 8, 7};

  switch (petAnimationPhase_) {
    case PetAnimationPhase::Start:
      length = static_cast<uint8_t>(sizeof(kStart) / sizeof(kStart[0]));
      intervalMs = PET_ANIMATION_START_INTERVAL_MS;
      return kStart;
    case PetAnimationPhase::Loop:
#if STACKCHAN_DEVICE_STOPWATCH
      length = 0;
      intervalMs = PET_ANIMATION_LOOP_INTERVAL_MS;
      return nullptr;
#else
      length = static_cast<uint8_t>(sizeof(kLoop) / sizeof(kLoop[0]));
      intervalMs = PET_ANIMATION_LOOP_INTERVAL_MS;
      return kLoop;
#endif
    case PetAnimationPhase::End:
      length = static_cast<uint8_t>(sizeof(kEnd) / sizeof(kEnd[0]));
      intervalMs = PET_ANIMATION_END_INTERVAL_MS;
      return kEnd;
    case PetAnimationPhase::After:
      if (petAnimationLong_) {
        length = static_cast<uint8_t>(sizeof(kLongAfter) / sizeof(kLongAfter[0]));
        intervalMs = PET_ANIMATION_AFTER_INTERVAL_MS;
        return kLongAfter;
      }
      length = static_cast<uint8_t>(sizeof(kShortAfter) / sizeof(kShortAfter[0]));
      intervalMs = PET_ANIMATION_AFTER_INTERVAL_MS;
      return kShortAfter;
    case PetAnimationPhase::None:
    default:
      length = 0;
      intervalMs = 0;
      return nullptr;
  }
}

void FaceController::updatePetAnimation(unsigned long now) {
  if (!petAnimationActive()) {
    return;
  }
  if (!enabled_) {
    stopPetAnimation(false);
    return;
  }
  if (state_ == ChanState::Speaking) {
    stopPetAnimation(true);
    return;
  }
  if (now < nextPetAnimationFrameMs_) {
    return;
  }

#if STACKCHAN_DEVICE_STOPWATCH
  if (petAnimationPhase_ == PetAnimationPhase::Loop) {
    if (!drawPetAnimationFrame(petAnimationTouchFrame_)) {
      Serial.printf("[face] pet animation loop draw failed frame=%u\n",
                    static_cast<unsigned>(petAnimationTouchFrame_));
      stopPetAnimation(true);
      return;
    }
    nextPetAnimationFrameMs_ = now + PET_ANIMATION_LOOP_INTERVAL_MS;
    return;
  }
#endif

  uint8_t length = 0;
  unsigned long intervalMs = 0;
  const uint8_t* sequence = petAnimationSequence(length, intervalMs);
  if (sequence == nullptr || length == 0 || intervalMs == 0) {
    stopPetAnimation(true);
    return;
  }

  if (petAnimationSequenceIndex_ >= length) {
    switch (petAnimationPhase_) {
      case PetAnimationPhase::Start:
        petAnimationPhase_ = PetAnimationPhase::Loop;
        petAnimationSequenceIndex_ = 0;
        break;
      case PetAnimationPhase::Loop:
        petAnimationSequenceIndex_ = 0;
        break;
      case PetAnimationPhase::End:
        petAnimationPhase_ = PetAnimationPhase::After;
        petAnimationSequenceIndex_ = 0;
        break;
      case PetAnimationPhase::After:
        stopPetAnimation(true);
        return;
      case PetAnimationPhase::None:
      default:
        stopPetAnimation(true);
        return;
    }

#if STACKCHAN_DEVICE_STOPWATCH
    if (petAnimationPhase_ == PetAnimationPhase::Loop) {
      if (!drawPetAnimationFrame(petAnimationTouchFrame_)) {
        Serial.printf("[face] pet animation loop draw failed frame=%u\n",
                      static_cast<unsigned>(petAnimationTouchFrame_));
        stopPetAnimation(true);
        return;
      }
      nextPetAnimationFrameMs_ = now + PET_ANIMATION_LOOP_INTERVAL_MS;
      return;
    }
#endif

    sequence = petAnimationSequence(length, intervalMs);
    if (sequence == nullptr || length == 0 || intervalMs == 0) {
      stopPetAnimation(true);
      return;
    }
  }

  const uint8_t frame = sequence[petAnimationSequenceIndex_];
  if (!drawPetAnimationFrame(frame)) {
    Serial.printf("[face] pet animation draw failed frame=%u\n", static_cast<unsigned>(frame));
    stopPetAnimation(true);
    return;
  }

  ++petAnimationSequenceIndex_;
  nextPetAnimationFrameMs_ = now + intervalMs;
}
#endif

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

bool FaceController::isJpegPath(const char* path) const {
  if (path == nullptr) {
    return false;
  }
  const char* ext = strrchr(path, '.');
  return ext != nullptr && (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0);
}

bool FaceController::isPngPath(const char* path) const {
  if (path == nullptr) {
    return false;
  }
  const char* ext = strrchr(path, '.');
  return ext != nullptr && strcasecmp(ext, ".png") == 0;
}

String FaceController::resolvedImagePath(const char* path) const {
  if (path == nullptr || path[0] == '\0') {
    return String();
  }

  const size_t pathLen = strlen(path);
  const bool cacheable = pathLen < kImagePathCacheMaxLen;
  if (cacheable) {
    for (uint8_t i = 0; i < kImagePathCacheCount; ++i) {
      if (imagePathCacheReady_[i] && strcmp(imagePathCacheRequest_[i], path) == 0) {
        return imagePathCacheResolved_[i][0] == '\0' ? String() : String(imagePathCacheResolved_[i]);
      }
    }
  }

  String resolved;
  if (LittleFS.exists(path)) {
    resolved = path;
  } else {
    const char* ext = strrchr(path, '.');
    if (ext != nullptr) {
      String jpegPath(path);
      jpegPath.remove(static_cast<unsigned int>(ext - path));
      jpegPath += ".jpg";
      if (LittleFS.exists(jpegPath)) {
        resolved = jpegPath;
      }
    }
  }

  if (cacheable && (resolved.isEmpty() || resolved.length() < kImagePathCacheMaxLen)) {
    const uint8_t slot = imagePathCacheNext_;
    snprintf(imagePathCacheRequest_[slot], kImagePathCacheMaxLen, "%s", path);
    snprintf(imagePathCacheResolved_[slot], kImagePathCacheMaxLen, "%s", resolved.c_str());
    imagePathCacheReady_[slot] = true;
    imagePathCacheNext_ = static_cast<uint8_t>((imagePathCacheNext_ + 1) % kImagePathCacheCount);
  }
  return resolved;
}

template <typename Target>
bool FaceController::drawFaceImageTarget(Target& target, File& file, const char* path) const {
#if STACKCHAN_ROUND_DISPLAY
  const int32_t drawSize = faceImageDrawSize();
  const int32_t x = (M5.Display.width() - drawSize) / 2;
  const int32_t y = (M5.Display.height() - drawSize) / 2;
  if (isJpegPath(path)) {
    return target.drawJpg(&file,
                          x,
                          y,
                          drawSize,
                          drawSize,
                          0,
                          0,
                          faceImageScale(),
                          faceImageScale(),
                          datum_t::top_left);
  }
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
  if (isJpegPath(path)) {
    return target.drawJpg(&file, faceImageDrawX(), faceImageDrawY());
  }
  return target.drawPng(&file, faceImageDrawX(), faceImageDrawY());
#endif
}

template <typename Target>
void FaceController::drawRoundAffectionOverlayTarget(Target& target, unsigned long now) {
  lastAffectionOverlayMs_ = now;
  if (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_) {
    affectionDeltaUntilMs_ = 0;
    affectionDelta_ = 0;
    guruguruStep_ = 0;
    guruguruStepDifficulty_ = 0;
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
  const unsigned long now = millis();
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
  const int32_t comboW = 70;
  const int32_t comboH = 16;
  const int32_t comboY = statusCy - 28;

  target.fillRect(clearX, clearY, clearW, clearH, TFT_BLACK);

  if (!clockText_.isEmpty()) {
    const uint16_t clockColor = clockValid_ ? M5.Display.color565(235, 240, 238) : M5.Display.color565(112, 116, 118);
    target.setTextDatum(middle_left);
    target.setTextColor(clockColor, TFT_BLACK);
    target.drawString(clockText_, startX, statusCy);
    target.setTextDatum(top_left);
  }

  if (guruguruStep_ > 0 && affectionDeltaUntilMs_ != 0) {
    const String stepText = String("STEP ") + String(guruguruStep_);
    uint16_t stepBg = M5.Display.color565(34, 26, 10);
    uint16_t stepBorder = M5.Display.color565(112, 82, 28);
    uint16_t stepColor = M5.Display.color565(255, 210, 90);
    if (guruguruStepDifficulty_ >= 15) {
      stepBg = M5.Display.color565(64, 10, 16);
      stepBorder = M5.Display.color565(230, 68, 78);
      stepColor = M5.Display.color565(255, 225, 210);
    } else if (guruguruStepDifficulty_ >= 14) {
      stepBg = M5.Display.color565(58, 24, 8);
      stepBorder = M5.Display.color565(220, 105, 44);
      stepColor = M5.Display.color565(255, 222, 130);
    } else if (guruguruStepDifficulty_ >= 12) {
      stepBg = M5.Display.color565(46, 34, 8);
      stepBorder = M5.Display.color565(184, 136, 38);
      stepColor = M5.Display.color565(255, 220, 96);
    } else if (guruguruStepDifficulty_ >= 10) {
      stepBg = M5.Display.color565(32, 38, 16);
      stepBorder = M5.Display.color565(128, 150, 50);
      stepColor = M5.Display.color565(230, 235, 110);
    }
    target.fillRoundRect(cx - comboW / 2, comboY - comboH / 2, comboW, comboH, 5, stepBg);
    target.drawRoundRect(cx - comboW / 2, comboY - comboH / 2, comboW, comboH, 5, stepBorder);
    target.setFont(&fonts::Font0);
    target.setTextSize(1);
    target.setTextDatum(middle_center);
    target.setTextColor(stepColor, stepBg);
    target.drawString(stepText, cx, comboY);
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
    guruguruStep_ = 0;
    guruguruStepDifficulty_ = 0;
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

  const int32_t stepCx = M5.Display.width() / 2;
  const int32_t stepBadgeW = 74;
  const int32_t stepBadgeH = 17;
  const int32_t stepBadgeY = M5.Display.height() - 18;
  if (guruguruStep_ > 0 && affectionDeltaUntilMs_ != 0) {
    const String stepText = String("STEP ") + String(guruguruStep_);
    uint16_t stepBg = M5.Display.color565(34, 26, 10);
    uint16_t stepBorder = M5.Display.color565(112, 82, 28);
    uint16_t stepColor = M5.Display.color565(255, 210, 90);
    if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS) {
      stepBg = M5.Display.color565(64, 10, 16);
      stepBorder = M5.Display.color565(230, 68, 78);
      stepColor = M5.Display.color565(255, 225, 210);
    } else if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS - 2) {
      stepBg = M5.Display.color565(58, 24, 8);
      stepBorder = M5.Display.color565(220, 105, 44);
      stepColor = M5.Display.color565(255, 222, 130);
    } else if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS - 6) {
      stepBg = M5.Display.color565(46, 34, 8);
      stepBorder = M5.Display.color565(184, 136, 38);
      stepColor = M5.Display.color565(255, 220, 96);
    } else if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS - 10) {
      stepBg = M5.Display.color565(32, 38, 16);
      stepBorder = M5.Display.color565(128, 150, 50);
      stepColor = M5.Display.color565(230, 235, 110);
    }
    M5.Display.fillRoundRect(stepCx - stepBadgeW / 2, stepBadgeY - stepBadgeH / 2, stepBadgeW, stepBadgeH, 5, stepBg);
    M5.Display.drawRoundRect(stepCx - stepBadgeW / 2, stepBadgeY - stepBadgeH / 2, stepBadgeW, stepBadgeH, 5, stepBorder);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(stepColor, stepBg);
    M5.Display.drawString(stepText, stepCx, stepBadgeY);
    M5.Display.setTextDatum(top_left);
  }

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
    guruguruStep_ = 0;
    guruguruStepDifficulty_ = 0;
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

  const int32_t stepCx = M5.Display.width() / 2;
  const int32_t stepBadgeW = 74;
  const int32_t stepBadgeH = 17;
  const int32_t stepBadgeY = M5.Display.height() - 18;
  if (guruguruStep_ > 0 && affectionDeltaUntilMs_ != 0) {
    const String stepText = String("STEP ") + String(guruguruStep_);
    uint16_t stepBg = M5.Display.color565(34, 26, 10);
    uint16_t stepBorder = M5.Display.color565(112, 82, 28);
    uint16_t stepColor = M5.Display.color565(255, 210, 90);
    if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS) {
      stepBg = M5.Display.color565(64, 10, 16);
      stepBorder = M5.Display.color565(230, 68, 78);
      stepColor = M5.Display.color565(255, 225, 210);
    } else if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS - 2) {
      stepBg = M5.Display.color565(58, 24, 8);
      stepBorder = M5.Display.color565(220, 105, 44);
      stepColor = M5.Display.color565(255, 222, 130);
    } else if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS - 6) {
      stepBg = M5.Display.color565(46, 34, 8);
      stepBorder = M5.Display.color565(184, 136, 38);
      stepColor = M5.Display.color565(255, 220, 96);
    } else if (guruguruStepDifficulty_ >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS - 10) {
      stepBg = M5.Display.color565(32, 38, 16);
      stepBorder = M5.Display.color565(128, 150, 50);
      stepColor = M5.Display.color565(230, 235, 110);
    }
    canvas_.fillRoundRect(stepCx - stepBadgeW / 2, stepBadgeY - stepBadgeH / 2, stepBadgeW, stepBadgeH, 5, stepBg);
    canvas_.drawRoundRect(stepCx - stepBadgeW / 2, stepBadgeY - stepBadgeH / 2, stepBadgeW, stepBadgeH, 5, stepBorder);
    canvas_.setFont(&fonts::Font0);
    canvas_.setTextSize(1);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(stepColor, stepBg);
    canvas_.drawString(stepText, stepCx, stepBadgeY);
    canvas_.setTextDatum(top_left);
  }

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
  const char* tierTalk0 = FACE_TALK_0_PATH;
  const char* tierTalk1 = FACE_TALK_1_PATH;
  const char* tierIdle = FACE_IDLE_PATH;
  const char* tierListen = FACE_LISTEN_PATH;
  const char* tierBlink = FACE_BLINK_PATH;
  if (visualTierIndex() == 1) {
    tierTalk0 = fallbackFacePath(FACE_TALK_GUARDED_0_PATH,
                                 fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_TALK_0_PATH));
    tierTalk1 = fallbackFacePath(FACE_TALK_GUARDED_1_PATH, FACE_TALK_1_PATH);
    tierIdle = fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_IDLE_PATH);
    tierListen = fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_LISTEN_PATH);
    tierBlink = fallbackFacePath(FACE_BLINK_GUARDED_0_PATH, FACE_BLINK_PATH);
  } else if (visualTierIndex() == 3) {
    tierTalk0 = fallbackFacePath(FACE_TALK_ATTACHED_0_PATH,
                                 fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_TALK_0_PATH));
    tierTalk1 = fallbackFacePath(FACE_TALK_ATTACHED_1_PATH, FACE_TALK_1_PATH);
    tierIdle = fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_IDLE_PATH);
    tierListen = fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_LISTEN_PATH);
    tierBlink = fallbackFacePath(FACE_BLINK_ATTACHED_0_PATH, FACE_BLINK_PATH);
  }

  if (strcmp(path, tierTalk0) == 0) {
    setIndex = 0;
    index = 0;
  } else if (strcmp(path, tierTalk1) == 0) {
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
	  } else if (strcmp(path, FACE_FURIFURI_0_PATH) == 0) {
	    setIndex = 5;
	    index = 0;
	  } else if (strcmp(path, FACE_FURIFURI_1_PATH) == 0) {
	    setIndex = 5;
	    index = 1;
		  } else if (strcmp(path, tierIdle) == 0) {
		    setIndex = 6;
		    index = 0;
		  } else if (strcmp(path, tierListen) == 0) {
		    setIndex = 6;
		    index = 1;
		  } else if (strcmp(path, tierBlink) == 0) {
		    setIndex = 7;
		    index = 0;
		  } else if (strcmp(path, FACE_SMILE_PATH) == 0) {
		    setIndex = 7;
		    index = 1;
		  } else if (strcmp(path, FACE_GOOD_BLINK_PATH) == 0) {
		    setIndex = 8;
		    index = 0;
		  } else if (strcmp(path, FACE_PHOTO_BLINK_PATH) == 0) {
		    setIndex = 8;
		    index = 1;
		  } else if (strcmp(path, FACE_PHOTO_BLINK_TALK_PATH) == 0) {
		    setIndex = 9;
		    index = 0;
		  } else if (strcmp(path, FACE_TIRED_BLINK_PATH) == 0) {
		    setIndex = 9;
		    index = 1;
		  } else if (strcmp(path, FACE_EXHAUSTED_BLINK_PATH) == 0) {
		    setIndex = 10;
		    index = 0;
		  } else if (strcmp(path, FACE_LOW_POWER_BLINK_PATH) == 0) {
		    setIndex = 10;
		    index = 1;
		  } else if (strcmp(path, FACE_TIRED_0_PATH) == 0) {
		    setIndex = 11;
		    index = 0;
		  } else if (strcmp(path, FACE_TIRED_TALK_PATH) == 0) {
		    setIndex = 11;
		    index = 1;
		  } else if (strcmp(path, FACE_EXHAUSTED_0_PATH) == 0) {
		    setIndex = 12;
		    index = 0;
		  } else if (strcmp(path, FACE_EXHAUSTED_TALK_PATH) == 0) {
		    setIndex = 12;
		    index = 1;
		  } else if (strcmp(path, FACE_LOW_POWER_0_PATH) == 0) {
		    setIndex = 13;
		    index = 0;
		  } else if (strcmp(path, FACE_LOW_POWER_TALK_PATH) == 0) {
		    setIndex = 13;
		    index = 1;
		  } else if (strcmp(path, FACE_NADENADE_0_PATH) == 0) {
		    setIndex = 14;
		    index = 0;
		  } else if (strcmp(path, FACE_NADENADE_1_PATH) == 0) {
		    setIndex = 14;
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
  drawAffectionOverlay(now);
  drawBatteryOverlay();
  drawCameraOverlay();
  drawMicOverlay();
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

void FaceController::releaseRoundTalkCache() {
  for (uint8_t i = 0; i < 2; ++i) {
    if (roundTalkCacheAllocated_[i]) {
      roundTalkCanvas_[i].deleteSprite();
    }
    roundTalkCacheAllocated_[i] = false;
    roundTalkCacheReady_[i] = false;
    roundTalkCachePath_[i] = "";
  }
}

bool FaceController::loadRoundBaseFaceToCanvas(M5Canvas& canvas, const char* path) {
  const String imagePath = resolvedImagePath(path);
  if (imagePath.isEmpty()) {
    Serial.printf("[face] missing round cache file: %s\n", path);
    return false;
  }

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    Serial.printf("[face] failed to open round cache file: %s\n", imagePath.c_str());
    return false;
  }

  canvas.fillScreen(TFT_BLACK);
  const bool ok = drawFaceImageTarget(canvas, file, imagePath.c_str());
  file.close();
  return ok;
}
#endif

uint8_t FaceController::guruguruFaceSourceIndex(uint8_t direction) const {
#if STACKCHAN_DEVICE_STOPWATCH
  if (direction >= STACKCHAN_GURUGURU_FACE_CENTER_INDEX) {
    return 16;
  }
  return static_cast<uint8_t>(direction * 2);
#else
  return direction;
#endif
}

#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
bool FaceController::parseGuruguruFacePath(const char* path, bool& blink, uint8_t& direction) const {
  if (path == nullptr) {
    return false;
  }

  const char* prefix = nullptr;
  blink = false;
  if (strncmp(path, "/dir", 4) == 0) {
    prefix = path + 4;
  } else if (strncmp(path, "/blink", 6) == 0) {
    prefix = path + 6;
    blink = true;
  } else {
    return false;
  }

  char* end = nullptr;
  const long value = strtol(prefix, &end, 10);
  if (end == prefix || strcmp(end, ".png") != 0 || value < 0 || value >= STACKCHAN_GURUGURU_FACE_COUNT) {
    return false;
  }

  direction = static_cast<uint8_t>(value);
  return true;
}

bool FaceController::loadGuruguruFaceToCanvas(M5Canvas& canvas, const char* path) {
  const String imagePath = resolvedImagePath(path);
  if (imagePath.isEmpty()) {
    Serial.printf("[face] missing guruguru cache file: %s\n", path);
    return false;
  }

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    Serial.printf("[face] failed to open guruguru cache file: %s\n", imagePath.c_str());
    return false;
  }

  canvas.fillScreen(TFT_BLACK);
  const bool ok = isJpegPath(imagePath.c_str()) ? canvas.drawJpg(&file, 0, 0) : canvas.drawPng(&file, 0, 0);
  file.close();
  return ok;
}

bool FaceController::prepareGuruguruFaceCache() {
  if (guruguruCachePrepared_) {
    return true;
  }

#if STACKCHAN_ROUND_DISPLAY
  releaseRoundTalkCache();
#endif

  bool ok = true;
  for (uint8_t direction = 0; direction < STACKCHAN_GURUGURU_FACE_COUNT; ++direction) {
    if (guruguruDirCacheReady_[direction]) {
      continue;
    }

    char path[16];
    snprintf(path,
             sizeof(path),
             "/dir%u.png",
             static_cast<unsigned>(guruguruFaceSourceIndex(direction)));
    if (resolvedImagePath(path).isEmpty()) {
      guruguruDirCacheReady_[direction] = false;
      ok = false;
      continue;
    }

    if (!guruguruDirCacheAllocated_[direction]) {
      guruguruDirCache_[direction].setPsram(true);
      guruguruDirCache_[direction].setColorDepth(16);
      if (guruguruDirCache_[direction].createSprite(FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT) == nullptr) {
        Serial.printf("[face] guruguru dir cache %u allocate failed freePsram=%u\n",
                      static_cast<unsigned>(direction),
                      static_cast<unsigned>(ESP.getFreePsram()));
        ok = false;
        break;
      }
      guruguruDirCacheAllocated_[direction] = true;
    }

    guruguruDirCacheReady_[direction] = loadGuruguruFaceToCanvas(guruguruDirCache_[direction], path);
    Serial.printf("[face] guruguru dir cache %u: %s freePsram=%u\n",
                  static_cast<unsigned>(direction),
                  guruguruDirCacheReady_[direction] ? "ready" : "failed",
                  static_cast<unsigned>(ESP.getFreePsram()));
    ok &= guruguruDirCacheReady_[direction];
  }

  guruguruCachePrepared_ = ok;
  return ok;
}

void FaceController::releaseGuruguruDirCache() {
  for (uint8_t i = 0; i < STACKCHAN_GURUGURU_FACE_COUNT; ++i) {
    if (guruguruDirCacheAllocated_[i]) {
      guruguruDirCache_[i].deleteSprite();
    }
    guruguruDirCacheAllocated_[i] = false;
    guruguruDirCacheReady_[i] = false;
  }
  guruguruCachePrepared_ = false;
}

void FaceController::releaseGuruguruFaceCache() {
  releaseGuruguruDirCache();
  releaseGuruguruBlinkCache();
  releaseGuruguruDizzyJpegCache();
}

void FaceController::releaseGuruguruBlinkCache() {
  for (uint8_t i = 0; i < kGuruguruBlinkCacheSlots; ++i) {
    if (guruguruBlinkCacheAllocated_[i]) {
      guruguruBlinkCache_[i].deleteSprite();
    }
    guruguruBlinkCacheAllocated_[i] = false;
    guruguruBlinkCacheReady_[i] = false;
    guruguruBlinkCacheDirection_[i] = 0;
    guruguruBlinkCacheLastUseMs_[i] = 0;
  }
}

void FaceController::releaseGuruguruDizzyJpegCache() {
  for (uint8_t i = 0; i < kGuruguruDizzyCanvasSlots; ++i) {
    if (guruguruDizzyCanvasCacheAllocated_[i]) {
      guruguruDizzyCanvasCache_[i].deleteSprite();
    }
    guruguruDizzyCanvasCacheAllocated_[i] = false;
    guruguruDizzyCanvasCacheReady_[i] = false;
    guruguruDizzyCanvasCacheSource_[i] = 0;
  }

  for (uint8_t i = 0; i < GURUGURU_DIZZY_FRAME_COUNT; ++i) {
    if (guruguruDizzyJpegCache_[i] != nullptr) {
      free(guruguruDizzyJpegCache_[i]);
    }
    guruguruDizzyJpegCache_[i] = nullptr;
    guruguruDizzyJpegCacheSize_[i] = 0;
    guruguruDizzyFrameX_[i] = 0;
    guruguruDizzyFrameY_[i] = 0;
    guruguruDizzyFrameW_[i] = FACE_IMAGE_WIDTH;
    guruguruDizzyFrameH_[i] = FACE_IMAGE_HEIGHT;
  }
  guruguruDizzyJpegCacheReady_ = false;
  guruguruDizzyFrameMetaReady_ = false;
}

void FaceController::releaseGuruguruDizzySourceRange(uint8_t firstSourceFrame, uint8_t lastSourceFrame) {
  for (uint8_t i = 0; i < kGuruguruDizzyCanvasSlots; ++i) {
    const uint8_t source = guruguruDizzyCanvasCacheSource_[i];
    if (source >= firstSourceFrame && source <= lastSourceFrame) {
      if (guruguruDizzyCanvasCacheAllocated_[i]) {
        guruguruDizzyCanvasCache_[i].deleteSprite();
      }
      guruguruDizzyCanvasCacheAllocated_[i] = false;
      guruguruDizzyCanvasCacheReady_[i] = false;
      guruguruDizzyCanvasCacheSource_[i] = 0;
    }
  }

  for (uint8_t i = firstSourceFrame; i <= lastSourceFrame && i < GURUGURU_DIZZY_FRAME_COUNT; ++i) {
    if (guruguruDizzyJpegCache_[i] != nullptr) {
      free(guruguruDizzyJpegCache_[i]);
    }
    guruguruDizzyJpegCache_[i] = nullptr;
    guruguruDizzyJpegCacheSize_[i] = 0;
  }
}

bool FaceController::preloadGuruguruDizzyJpegCache(bool reverse, uint8_t maxFrames) {
  if (maxFrames == 0) {
    return guruguruDizzyJpegCacheReady_ && guruguruDizzyJpegCacheReverse_ == reverse;
  }
  return preloadGuruguruDizzyPlaybackRange(reverse, 0, GURUGURU_DIZZY_FRAME_COUNT, maxFrames);
}

bool FaceController::preloadGuruguruDizzyPlaybackRange(bool reverse,
                                                       uint8_t firstFrame,
                                                       uint8_t endFrame,
                                                       uint8_t maxLoads) {
#if !STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  if (guruguruDizzyJpegCacheReady_ && guruguruDizzyJpegCacheReverse_ != reverse) {
    releaseGuruguruDizzyJpegCache();
  }
#endif
  if (!guruguruDizzyJpegCacheReady_) {
    guruguruDizzyJpegCacheReverse_ = reverse;
    guruguruDizzyJpegCacheReady_ = true;
  }

  const String firstFramePath = guruguruDizzyFramePath(reverse, 0);
  const bool useJpegFrames = isJpegPath(firstFramePath.c_str());
  const bool usePngFrames = isPngPath(firstFramePath.c_str());
  if (!useJpegFrames && !usePngFrames) {
    guruguruDizzyJpegCacheReady_ = false;
    return false;
  }
  for (uint8_t i = 0; i < GURUGURU_DIZZY_FRAME_COUNT; ++i) {
    guruguruDizzyFrameX_[i] = 0;
    guruguruDizzyFrameY_[i] = 0;
    guruguruDizzyFrameW_[i] = FACE_IMAGE_WIDTH;
    guruguruDizzyFrameH_[i] = FACE_IMAGE_HEIGHT;
  }
  guruguruDizzyFrameMetaReady_ = false;

  uint8_t loadedThisCall = 0;
  for (uint8_t frame = firstFrame; frame < endFrame && frame < GURUGURU_DIZZY_FRAME_COUNT; ++frame) {
    const uint8_t cacheIndex = guruguruDizzySourceFrame(reverse, frame);
    if (cacheIndex >= GURUGURU_DIZZY_FRAME_COUNT) {
      continue;
    }
    if (findGuruguruDizzyCanvasSlot(cacheIndex) >= 0 ||
        (guruguruDizzyJpegCache_[cacheIndex] != nullptr && guruguruDizzyJpegCacheSize_[cacheIndex] > 0)) {
      continue;
    }
    if (loadedThisCall >= maxLoads) {
      continue;
    }

    if (!loadGuruguruDizzyCacheFrame(reverse, frame)) {
      return false;
    }
    ++loadedThisCall;
  }

  bool allLoaded = true;
  size_t totalBytes = 0;
  bool countedSources[GURUGURU_DIZZY_FRAME_COUNT] = {};
  for (uint8_t frame = firstFrame; frame < endFrame && frame < GURUGURU_DIZZY_FRAME_COUNT; ++frame) {
    const uint8_t cacheIndex = guruguruDizzySourceFrame(reverse, frame);
    if (cacheIndex >= GURUGURU_DIZZY_FRAME_COUNT ||
        findGuruguruDizzyCanvasSlot(cacheIndex) < 0) {
      allLoaded = false;
      break;
    }
    if (!countedSources[cacheIndex]) {
      countedSources[cacheIndex] = true;
      totalBytes += static_cast<size_t>(GURUGURU_DIZZY_CANVAS_SIZE) * static_cast<size_t>(GURUGURU_DIZZY_CANVAS_SIZE) * sizeof(uint16_t);
    }
  }

  Serial.printf("[guruguru] dizzy preload reverse=%d range=%u-%u jpg=%d png=%d loaded=%u bytes=%u freePsram=%u\n",
                reverse ? 1 : 0,
                static_cast<unsigned>(firstFrame),
                static_cast<unsigned>(endFrame),
                useJpegFrames ? 1 : 0,
                usePngFrames ? 1 : 0,
                static_cast<unsigned>(loadedThisCall),
                static_cast<unsigned>(totalBytes),
                static_cast<unsigned>(ESP.getFreePsram()));
  return allLoaded;
}

bool FaceController::loadGuruguruDizzyCacheFrame(bool reverse, uint8_t frame) {
  return loadGuruguruDizzyCanvasFrame(reverse, frame);
}

int8_t FaceController::findGuruguruDizzyCanvasSlot(uint8_t sourceFrame) const {
  if (sourceFrame == 0 || sourceFrame >= GURUGURU_DIZZY_FRAME_COUNT) {
    return -1;
  }
  for (uint8_t i = 0; i < kGuruguruDizzyCanvasSlots; ++i) {
    if (guruguruDizzyCanvasCacheReady_[i] &&
        guruguruDizzyCanvasCacheSource_[i] == sourceFrame) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

int8_t FaceController::prepareGuruguruDizzyCanvasSlot(uint8_t sourceFrame) {
  const int8_t existing = findGuruguruDizzyCanvasSlot(sourceFrame);
  if (existing >= 0) {
    return existing;
  }

  int8_t slot = -1;
  for (uint8_t i = 0; i < kGuruguruDizzyCanvasSlots; ++i) {
    if (!guruguruDizzyCanvasCacheReady_[i]) {
      slot = static_cast<int8_t>(i);
      break;
    }
  }
  if (slot < 0) {
    return -1;
  }

  if (!guruguruDizzyCanvasCacheAllocated_[slot]) {
    guruguruDizzyCanvasCache_[slot].setPsram(true);
    guruguruDizzyCanvasCache_[slot].setColorDepth(16);
    if (guruguruDizzyCanvasCache_[slot].createSprite(GURUGURU_DIZZY_CANVAS_SIZE, GURUGURU_DIZZY_CANVAS_SIZE) == nullptr) {
      Serial.printf("[guruguru] dizzy canvas alloc failed source=%u freePsram=%u\n",
                    static_cast<unsigned>(sourceFrame),
                    static_cast<unsigned>(ESP.getFreePsram()));
      return -1;
    }
    guruguruDizzyCanvasCacheAllocated_[slot] = true;
  }
  return slot;
}

bool FaceController::loadGuruguruDizzyCanvasFrame(bool reverse, uint8_t frame) {
  const uint8_t sourceFrame = guruguruDizzySourceFrame(reverse, frame);
  if (sourceFrame == 0 || sourceFrame >= GURUGURU_DIZZY_FRAME_COUNT) {
    return false;
  }
  if (findGuruguruDizzyCanvasSlot(sourceFrame) >= 0) {
    return true;
  }

  const int8_t slot = prepareGuruguruDizzyCanvasSlot(sourceFrame);
  if (slot < 0) {
    return false;
  }

  const String imagePath = guruguruDizzyFramePath(reverse, frame);
  if (imagePath.isEmpty()) {
    return false;
  }

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    Serial.printf("[guruguru] dizzy canvas open failed: %s\n", imagePath.c_str());
    return false;
  }

  guruguruDizzyCanvasCache_[slot].fillScreen(TFT_BLACK);
  const float drawScale = static_cast<float>(GURUGURU_DIZZY_CANVAS_SIZE) / static_cast<float>(FACE_IMAGE_WIDTH);
  const int32_t drawW = GURUGURU_DIZZY_CANVAS_SIZE;
  const int32_t drawH = GURUGURU_DIZZY_CANVAS_SIZE;
  const int32_t drawX = 0;
  const int32_t drawY = 0;
  const bool ok = isJpegPath(imagePath.c_str())
                    ? guruguruDizzyCanvasCache_[slot].drawJpg(&file,
                                                              drawX,
                                                              drawY,
                                                              drawW,
                                                              drawH,
                                                              0,
                                                              0,
                                                              drawScale,
                                                              drawScale,
                                                              datum_t::top_left)
                    : guruguruDizzyCanvasCache_[slot].drawPng(&file,
                                                              drawX,
                                                              drawY,
                                                              drawW,
                                                              drawH,
                                                              0,
                                                              0,
                                                              drawScale,
                                                              drawScale,
                                                              datum_t::top_left);
  file.close();

  if (!ok) {
    Serial.printf("[guruguru] dizzy canvas decode failed source=%u path=%s\n",
                  static_cast<unsigned>(sourceFrame),
                  imagePath.c_str());
    guruguruDizzyCanvasCacheReady_[slot] = false;
    guruguruDizzyCanvasCacheSource_[slot] = 0;
    return false;
  }

  guruguruDizzyCanvasCacheReady_[slot] = true;
  guruguruDizzyCanvasCacheSource_[slot] = sourceFrame;
  Serial.printf("[guruguru] dizzy canvas source=%u slot=%d freePsram=%u\n",
                static_cast<unsigned>(sourceFrame),
                static_cast<int>(slot),
                static_cast<unsigned>(ESP.getFreePsram()));
  return true;
}

void FaceController::serviceGuruguruDizzyCache(uint8_t displayedFrame) {
  if (kGuruguruDizzyCanvasSlots >= 15) {
    return;
  }

  if (!guruguruDizzyBlinkReleased_) {
    releaseGuruguruBlinkCache();
    guruguruDizzyBlinkReleased_ = true;
  }

  const uint8_t firstFutureFrame = displayedFrame + 1;
  if (firstFutureFrame >= GURUGURU_DIZZY_FRAME_COUNT) {
    return;
  }
  uint8_t endFutureFrame = firstFutureFrame + kGuruguruDizzyCanvasSlots;
  if (endFutureFrame > GURUGURU_DIZZY_FRAME_COUNT) {
    endFutureFrame = GURUGURU_DIZZY_FRAME_COUNT;
  }

  for (uint8_t slot = 0; slot < kGuruguruDizzyCanvasSlots; ++slot) {
    if (!guruguruDizzyCanvasCacheReady_[slot]) {
      continue;
    }
    const uint8_t cachedSource = guruguruDizzyCanvasCacheSource_[slot];
    bool needed = false;
    for (uint8_t frame = firstFutureFrame; frame < endFutureFrame; ++frame) {
      if (guruguruDizzySourceFrame(guruguruDizzyReverse_, frame) == cachedSource) {
        needed = true;
        break;
      }
    }
    if (!needed) {
      guruguruDizzyCanvasCacheReady_[slot] = false;
      guruguruDizzyCanvasCacheSource_[slot] = 0;
    }
  }

  for (uint8_t frame = firstFutureFrame; frame < endFutureFrame; ++frame) {
    if (!loadGuruguruDizzyCacheFrame(guruguruDizzyReverse_, frame)) {
      Serial.printf("[guruguru] dizzy lookahead failed frame=%u freePsram=%u\n",
                    static_cast<unsigned>(frame),
                    static_cast<unsigned>(ESP.getFreePsram()));
      break;
    }
  }
}

uint8_t FaceController::guruguruDizzySourceFrame(bool reverse, uint8_t frame) const {
  if (frame >= GURUGURU_DIZZY_FRAME_COUNT) {
    return GURUGURU_DIZZY_FRAME_COUNT;
  }

  if (frame < 16) {
    const uint8_t spinFrame = frame % 8;
    return reverse ? static_cast<uint8_t>(8 - spinFrame)
                   : static_cast<uint8_t>(1 + spinFrame);
  }
  return static_cast<uint8_t>(9 + (frame - 16));
}

String FaceController::guruguruDizzyFramePath(bool reverse, uint8_t frame) const {
  const uint8_t sourceFrame = guruguruDizzySourceFrame(reverse, frame);
  if (sourceFrame >= GURUGURU_DIZZY_FRAME_COUNT) {
    return String();
  }

  char pngPath[24];
  snprintf(pngPath, sizeof(pngPath), "/dizzy_%02u.png", static_cast<unsigned>(sourceFrame));
  if (LittleFS.exists(pngPath)) {
    return String(pngPath);
  }

  const String jpgPath = resolvedImagePath(pngPath);
  if (!jpgPath.isEmpty()) {
    return jpgPath;
  }

  char legacyPath[32];
  snprintf(legacyPath,
           sizeof(legacyPath),
           reverse ? "/dizzy_ccw_%02u.png" : "/dizzy_cw_%02u.png",
           static_cast<unsigned>(frame));
  return resolvedImagePath(legacyPath);
}

bool FaceController::loadGuruguruDizzyFrameMeta(bool reverse) {
  if (guruguruDizzyFrameMetaReady_ && guruguruDizzyJpegCacheReverse_ == reverse) {
    return true;
  }

  for (uint8_t i = 0; i < GURUGURU_DIZZY_FRAME_COUNT; ++i) {
    guruguruDizzyFrameX_[i] = 0;
    guruguruDizzyFrameY_[i] = 0;
    guruguruDizzyFrameW_[i] = FACE_IMAGE_WIDTH;
    guruguruDizzyFrameH_[i] = FACE_IMAGE_HEIGHT;
  }

  const char* path = reverse ? "/dizzy_ccw_meta.txt" : "/dizzy_cw_meta.txt";
  if (!LittleFS.exists(path)) {
    guruguruDizzyFrameMetaReady_ = false;
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("[guruguru] dizzy meta open failed: %s\n", path);
    guruguruDizzyFrameMetaReady_ = false;
    return false;
  }

  uint8_t frame = 0;
  while (file.available() && frame < GURUGURU_DIZZY_FRAME_COUNT) {
    const String line = file.readStringUntil('\n');
    int x = 0;
    int y = 0;
    int w = FACE_IMAGE_WIDTH;
    int h = FACE_IMAGE_HEIGHT;
    if (sscanf(line.c_str(), "%d %d %d %d", &x, &y, &w, &h) == 4) {
      guruguruDizzyFrameX_[frame] = constrain(x, 0, FACE_IMAGE_WIDTH - 1);
      guruguruDizzyFrameY_[frame] = constrain(y, 0, FACE_IMAGE_HEIGHT - 1);
      guruguruDizzyFrameW_[frame] = constrain(w, 1, FACE_IMAGE_WIDTH);
      guruguruDizzyFrameH_[frame] = constrain(h, 1, FACE_IMAGE_HEIGHT);
    }
    ++frame;
  }
  file.close();

  guruguruDizzyFrameMetaReady_ = frame == GURUGURU_DIZZY_FRAME_COUNT;
  Serial.printf("[guruguru] dizzy meta %s frames=%u ready=%d\n",
                path,
                static_cast<unsigned>(frame),
                guruguruDizzyFrameMetaReady_ ? 1 : 0);
  return guruguruDizzyFrameMetaReady_;
}

int8_t FaceController::findGuruguruBlinkCacheSlot(uint8_t direction) const {
  for (uint8_t i = 0; i < kGuruguruBlinkCacheSlots; ++i) {
    if (guruguruBlinkCacheReady_[i] && guruguruBlinkCacheDirection_[i] == direction) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;
}

int8_t FaceController::prepareGuruguruBlinkCache(uint8_t direction) {
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  if (direction != STACKCHAN_GURUGURU_FACE_CENTER_INDEX) {
    return -1;
  }
#endif
  const int8_t existing = findGuruguruBlinkCacheSlot(direction);
  if (existing >= 0) {
    guruguruBlinkCacheLastUseMs_[existing] = millis();
    return existing;
  }

  uint8_t slot = 0;
  bool foundFree = false;
  for (uint8_t i = 0; i < kGuruguruBlinkCacheSlots; ++i) {
    if (!guruguruBlinkCacheAllocated_[i]) {
      slot = i;
      foundFree = true;
      break;
    }
  }
  if (!foundFree) {
    unsigned long oldest = ~0UL;
    bool foundEvictable = false;
    for (uint8_t i = 0; i < kGuruguruBlinkCacheSlots; ++i) {
      if (guruguruBlinkCacheReady_[i] &&
          guruguruBlinkCacheDirection_[i] == STACKCHAN_GURUGURU_FACE_CENTER_INDEX) {
        continue;
      }
      if (!foundEvictable || guruguruBlinkCacheLastUseMs_[i] < oldest) {
        oldest = guruguruBlinkCacheLastUseMs_[i];
        slot = i;
        foundEvictable = true;
      }
    }
  }

  if (!guruguruBlinkCacheAllocated_[slot]) {
    guruguruBlinkCache_[slot].setPsram(true);
    guruguruBlinkCache_[slot].setColorDepth(16);
    if (guruguruBlinkCache_[slot].createSprite(FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT) == nullptr) {
      Serial.printf("[face] guruguru blink cache allocate failed dir=%u freePsram=%u\n",
                    static_cast<unsigned>(direction),
                    static_cast<unsigned>(ESP.getFreePsram()));
      return -1;
    }
    guruguruBlinkCacheAllocated_[slot] = true;
  }

  char path[18];
  snprintf(path,
           sizeof(path),
           "/blink%u.png",
           static_cast<unsigned>(guruguruFaceSourceIndex(direction)));
  if (!loadGuruguruFaceToCanvas(guruguruBlinkCache_[slot], path)) {
    guruguruBlinkCacheReady_[slot] = false;
    return -1;
  }

  guruguruBlinkCacheReady_[slot] = true;
  guruguruBlinkCacheDirection_[slot] = direction;
  guruguruBlinkCacheLastUseMs_[slot] = millis();
  Serial.printf("[face] guruguru blink cache slot=%u dir=%u freePsram=%u\n",
                static_cast<unsigned>(slot),
                static_cast<unsigned>(direction),
                static_cast<unsigned>(ESP.getFreePsram()));
  return static_cast<int8_t>(slot);
}

bool FaceController::drawCachedGuruguruFace(const char* path) {
  if (!guruguruFaceMode_ || state_ != ChanState::Idle) {
    return false;
  }
  if (!prepareGuruguruFaceCache()) {
    return false;
  }

  bool blink = false;
  uint8_t direction = 0;
  if (!parseGuruguruFacePath(path, blink, direction)) {
    return false;
  }

  M5Canvas* source = nullptr;
  if (blink) {
    const int8_t slot = prepareGuruguruBlinkCache(direction);
    if (slot < 0 || !guruguruBlinkCacheReady_[slot]) {
      if (!guruguruDirCacheReady_[direction]) {
        return true;
      }
      source = &guruguruDirCache_[direction];
    } else {
      source = &guruguruBlinkCache_[slot];
    }
  } else {
    if (!guruguruDirCacheReady_[direction]) {
      return false;
    }
    source = &guruguruDirCache_[direction];
  }

  const void* buffer = source->getBuffer();
  if (buffer == nullptr) {
    return false;
  }

  const int32_t x = (M5.Display.width() - FACE_IMAGE_WIDTH) / 2;
  const int32_t y = (M5.Display.height() - FACE_IMAGE_HEIGHT) / 2;
  const unsigned long now = millis();
  if (canvasReady_) {
    canvas_.fillScreen(TFT_BLACK);
    canvas_.pushImage(x, y, FACE_IMAGE_WIDTH, FACE_IMAGE_HEIGHT, static_cast<const uint16_t*>(buffer));
    drawAffectionOverlayOnCanvas(now);
    drawBatteryOverlayOnCanvas();
    drawCameraOverlayOnCanvas();
    drawMicOverlayOnCanvas();
    canvas_.pushSprite(&M5.Display, 0, 0);
  } else {
    M5.Display.fillScreen(TFT_BLACK);
    source->pushSprite(&M5.Display, x, y);
    drawAffectionOverlay(now);
    drawBatteryOverlay();
    drawCameraOverlay();
    drawMicOverlay();
  }
  return true;
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
  const char* tierTalk0 = FACE_TALK_0_PATH;
  const char* tierTalk1 = FACE_TALK_1_PATH;
  const char* tierIdle = FACE_IDLE_PATH;
  const char* tierListen = FACE_LISTEN_PATH;
  const char* tierBlink = FACE_BLINK_PATH;
  if (visualTierIndex() == 1) {
    tierTalk0 = fallbackFacePath(FACE_TALK_GUARDED_0_PATH,
                                 fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_TALK_0_PATH));
    tierTalk1 = fallbackFacePath(FACE_TALK_GUARDED_1_PATH, FACE_TALK_1_PATH);
    tierIdle = fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_IDLE_PATH);
    tierListen = fallbackFacePath(FACE_IDLE_GUARDED_0_PATH, FACE_LISTEN_PATH);
    tierBlink = fallbackFacePath(FACE_BLINK_GUARDED_0_PATH, FACE_BLINK_PATH);
  } else if (visualTierIndex() == 3) {
    tierTalk0 = fallbackFacePath(FACE_TALK_ATTACHED_0_PATH,
                                 fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_TALK_0_PATH));
    tierTalk1 = fallbackFacePath(FACE_TALK_ATTACHED_1_PATH, FACE_TALK_1_PATH);
    tierIdle = fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_IDLE_PATH);
    tierListen = fallbackFacePath(FACE_IDLE_ATTACHED_0_PATH, FACE_LISTEN_PATH);
    tierBlink = fallbackFacePath(FACE_BLINK_ATTACHED_0_PATH, FACE_BLINK_PATH);
  }

  const char* paths[kTalkCacheSetCount][2] = {
	    {tierTalk0, tierTalk1},
	    {FACE_BAD_0_PATH, FACE_BAD_1_PATH},
	    {FACE_GOOD_0_PATH, FACE_GOOD_1_PATH},
	    {FACE_PHOTO_0_PATH, FACE_PHOTO_1_PATH},
	    {FACE_PHOTO_MASTER_0_PATH, FACE_PHOTO_MASTER_1_PATH},
	    {FACE_FURIFURI_0_PATH, FACE_FURIFURI_1_PATH},
	    {tierIdle, tierListen},
	    {tierBlink, FACE_SMILE_PATH},
	    {FACE_GOOD_BLINK_PATH, FACE_PHOTO_BLINK_PATH},
	    {FACE_PHOTO_BLINK_TALK_PATH, FACE_TIRED_BLINK_PATH},
	    {FACE_EXHAUSTED_BLINK_PATH, FACE_LOW_POWER_BLINK_PATH},
	    {FACE_TIRED_0_PATH, FACE_TIRED_TALK_PATH},
	    {FACE_EXHAUSTED_0_PATH, FACE_EXHAUSTED_TALK_PATH},
	    {FACE_LOW_POWER_0_PATH, FACE_LOW_POWER_TALK_PATH},
	    {FACE_NADENADE_0_PATH, FACE_NADENADE_1_PATH},
	  };

  for (int setIndex = 0; setIndex < kTalkCacheSetCount; ++setIndex) {
    for (int i = 0; i < 2; ++i) {
      if (talkCacheReady_[setIndex][i]) {
        continue;
      }
      if (resolvedImagePath(paths[setIndex][i]).isEmpty()) {
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
      talkCacheReady_[setIndex][i] = loadImageToCanvas(talkCanvas_[setIndex][i], paths[setIndex][i], 0, 0);
      Serial.printf("[face] talk cache %d:%d: %s\n", setIndex, i, talkCacheReady_[setIndex][i] ? "ready" : "failed");
    }
  }
#endif
}

void FaceController::releaseTalkCache() {
#if STACKCHAN_ROUND_DISPLAY
  releaseRoundTalkCache();
#else
  for (int setIndex = 0; setIndex < kTalkCacheSetCount; ++setIndex) {
    for (int i = 0; i < 2; ++i) {
      talkCanvas_[setIndex][i].deleteSprite();
      talkCacheReady_[setIndex][i] = false;
    }
  }
#endif
}

bool FaceController::loadImageToCanvas(M5Canvas& canvas, const char* path, int32_t x, int32_t y) {
  const String imagePath = resolvedImagePath(path);
  if (imagePath.isEmpty()) {
    Serial.printf("[face] missing cache file: %s\n", path);
    return false;
  }

  File file = LittleFS.open(imagePath, "r");
  if (!file) {
    Serial.printf("[face] failed to open cache file: %s\n", imagePath.c_str());
    return false;
  }

  bool ok = isJpegPath(imagePath.c_str()) ? canvas.drawJpg(&file, x, y) : canvas.drawPng(&file, x, y);
  file.close();
  return ok;
}

void FaceController::scheduleBlink(unsigned long now) {
  const unsigned long minInterval = guruguruFaceMode_ ? GURUGURU_BLINK_MIN_INTERVAL_MS : BLINK_MIN_INTERVAL_MS;
  const unsigned long maxInterval = guruguruFaceMode_ ? GURUGURU_BLINK_MAX_INTERVAL_MS : BLINK_MAX_INTERVAL_MS;
  nextBlinkMs_ = now + random(minInterval, maxInterval + 1);
}

void FaceController::scheduleSmile(unsigned long now) {
  nextSmileMs_ = now + random(IDLE_SMILE_MIN_INTERVAL_MS, IDLE_SMILE_MAX_INTERVAL_MS + 1);
}
