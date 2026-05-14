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
  } else if (strcmp(name, "furifuri_0") == 0) {
    drawFace(FACE_FURIFURI_0_PATH);
  } else if (strcmp(name, "furifuri_1") == 0) {
    drawFace(FACE_FURIFURI_1_PATH);
  } else if (strcmp(name, "blink") == 0) {
    drawFace(FACE_BLINK_PATH);
  } else if (strcmp(name, "smile") == 0) {
    drawFace(FACE_SMILE_PATH);
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

void FaceController::setAffectionState(const AffectionState& state) {
  if (affectionState_.affection == state.affection &&
      affectionState_.mood == state.mood &&
      affectionState_.confusion == state.confusion &&
      affectionState_.seq == state.seq) {
    return;
  }

  affectionState_ = state;
  affectionOverlayDirty_ = true;
  if (enabled_) {
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

  if (affectionOverlayDirty_ || (affectionDeltaUntilMs_ != 0 && now >= affectionDeltaUntilMs_)) {
    drawAffectionOverlay(now);
  }

  if (state_ == ChanState::Speaking) {
    if (now - lastLipSyncMs_ >= LIP_SYNC_INTERVAL_MS) {
      lastLipSyncMs_ = now;
      lipOpen_ = !lipOpen_;
      drawFace(talkFacePath(lipOpen_ ? 1 : 0));
    }
    return;
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

  if (!shakeFaceMode_ && !petFaceMode_ && !photoFaceMode_ && !photoMasterFaceMode_ && state_ == ChanState::Idle && now >= nextSmileMs_) {
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
    return index == 0 ? FACE_FURIFURI_0_PATH : FACE_FURIFURI_1_PATH;
  }
  if (petFaceMode_) {
    return index == 0 ? FACE_NADENADE_0_PATH : FACE_NADENADE_1_PATH;
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
  return index == 0 ? FACE_TALK_0_PATH : FACE_TALK_1_PATH;
}

const char* FaceController::listeningFacePath() const {
  if (shakeFaceMode_) {
    return FACE_FURIFURI_0_PATH;
  }
  if (petFaceMode_) {
    return FACE_NADENADE_0_PATH;
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
  return FACE_LISTEN_PATH;
}

const char* FaceController::blinkFacePath() const {
  if (shakeFaceMode_) {
    return FACE_FURIFURI_0_PATH;
  }
  if (petFaceMode_) {
    return FACE_NADENADE_0_PATH;
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
  return FACE_BLINK_PATH;
}

void FaceController::showBaseFace() {
  switch (state_) {
    case ChanState::Idle:
      drawFace(shakeFaceMode_ ? FACE_FURIFURI_0_PATH : (petFaceMode_ ? FACE_NADENADE_0_PATH : (photoMasterFaceMode_ ? FACE_PHOTO_MASTER_0_PATH : (photoFaceMode_ ? FACE_PHOTO_0_PATH : FACE_IDLE_PATH))));
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
  }

  if (setIndex < 0 || index < 0 || !talkCacheReady_[setIndex][index]) {
    return false;
  }

  const int32_t x = (M5.Display.width() - FACE_IMAGE_WIDTH) / 2;
  const int32_t y = (M5.Display.height() - FACE_IMAGE_HEIGHT) / 2;
  talkCanvas_[setIndex][index].pushSprite(&M5.Display, x, y);
  return true;
}

void FaceController::prepareTalkCache() {
  const char* paths[7][2] = {
    {FACE_TALK_0_PATH, FACE_TALK_1_PATH},
    {FACE_BAD_0_PATH, FACE_BAD_1_PATH},
    {FACE_GOOD_0_PATH, FACE_GOOD_1_PATH},
    {FACE_PHOTO_0_PATH, FACE_PHOTO_1_PATH},
    {FACE_PHOTO_MASTER_0_PATH, FACE_PHOTO_MASTER_1_PATH},
    {FACE_NADENADE_0_PATH, FACE_NADENADE_1_PATH},
    {FACE_FURIFURI_0_PATH, FACE_FURIFURI_1_PATH},
  };

  for (int setIndex = 0; setIndex < 7; ++setIndex) {
    for (int i = 0; i < 2; ++i) {
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
