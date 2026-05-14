#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <M5Unified.h>

#include "AffectionController.h"
#include "AppState.h"

enum class AuthFaceMode {
  Unknown,
  Master,
  NotMaster
};

class FaceController {
public:
  void begin();
  void setState(ChanState state);
  void showFace(const char* name);
  void setBadSpeakingFace(bool enabled);
  void setAuthFaceMode(AuthFaceMode mode);
  void setPhotoFaceMode(bool enabled);
  void setPhotoMasterFaceMode(bool enabled);
  void setPetFaceMode(bool enabled);
  void setShakeFaceMode(bool enabled);
  void setEnabled(bool enabled);
  void setAffectionState(const AffectionState& state);
  void showAffectionDelta(int delta, unsigned long now);
  void update(unsigned long now);

private:
  const char* talkFacePath(uint8_t index) const;
  const char* listeningFacePath() const;
  const char* blinkFacePath() const;
  void showBaseFace();
  void drawFace(const char* path);
  bool drawCachedTalkFace(const char* path);
  void drawAffectionOverlay(unsigned long now);
  void drawAffectionOverlayOnCanvas(unsigned long now);
  void drawHeart(M5GFX& target, int32_t cx, int32_t cy, int32_t size, uint16_t color);
  void drawHeart(M5Canvas& target, int32_t cx, int32_t cy, int32_t size, uint16_t color);
  uint16_t affectionColor() const;
  uint16_t blendColor(uint32_t from, uint32_t to, float t) const;
  void prepareTalkCache();
  bool loadPngToCanvas(M5Canvas& canvas, const char* path, int32_t x, int32_t y);
  void scheduleBlink(unsigned long now);
  void scheduleSmile(unsigned long now);

  M5Canvas canvas_;
  M5Canvas talkCanvas_[7][2];
  ChanState state_ = ChanState::Idle;
  AuthFaceMode authFaceMode_ = AuthFaceMode::Unknown;
  bool photoFaceMode_ = false;
  bool photoMasterFaceMode_ = false;
  bool petFaceMode_ = false;
  bool shakeFaceMode_ = false;
  bool enabled_ = true;
  bool canvasReady_ = false;
  bool talkCacheReady_[7][2] = {{false, false}, {false, false}, {false, false}, {false, false}, {false, false}, {false, false}, {false, false}};
  bool lipOpen_ = false;
  bool blinking_ = false;
  bool smiling_ = false;
  unsigned long lastLipSyncMs_ = 0;
  unsigned long nextBlinkMs_ = 0;
  unsigned long blinkEndMs_ = 0;
  unsigned long nextSmileMs_ = 0;
  unsigned long smileEndMs_ = 0;
  AffectionState affectionState_;
  int16_t affectionDelta_ = 0;
  unsigned long affectionDeltaUntilMs_ = 0;
  unsigned long lastAffectionOverlayMs_ = 0;
  bool affectionOverlayDirty_ = true;
  String currentPath_;
};
