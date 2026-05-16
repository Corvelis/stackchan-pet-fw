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

enum class ThermalFaceMode : uint8_t {
  Normal,
  Warm,
  Hot,
  LowPower
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
  void setThermalFaceMode(ThermalFaceMode mode);
  void setBatteryState(int level, bool charging);
  void setMicState(bool connected, bool muted, bool streaming);
  void setAffectionState(const AffectionState& state);
  void showAffectionDelta(int delta, unsigned long now);
  void update(unsigned long now);

private:
  static constexpr uint8_t kTalkCacheSetCount = 16;

  const char* talkFacePath(uint8_t index) const;
  const char* listeningFacePath() const;
  const char* blinkFacePath() const;
  const char* idleFacePath() const;
  const char* thermalFacePath(uint8_t index) const;
  const char* thermalBlinkFacePath() const;
  const char* petFacePath(uint8_t index) const;
  const char* petBlinkFacePath() const;
  const char* shakeFacePath(uint8_t index) const;
  const char* fallbackFacePath(const char* preferred, const char* fallback) const;
  uint8_t visualTierIndex() const;
  void showBaseFace();
  void drawFace(const char* path);
  bool drawCachedTalkFace(const char* path);
  void drawAffectionOverlay(unsigned long now);
  void drawAffectionOverlayOnCanvas(unsigned long now);
  void drawBatteryOverlay();
  void drawBatteryOverlayOnCanvas();
  void drawMicOverlay();
  void drawMicOverlayOnCanvas();
  void drawHeart(M5GFX& target, int32_t cx, int32_t cy, int32_t size, uint16_t color);
  void drawHeart(M5Canvas& target, int32_t cx, int32_t cy, int32_t size, uint16_t color);
  uint16_t affectionColor() const;
  uint16_t blendColor(uint32_t from, uint32_t to, float t) const;
  void prepareTalkCache();
  bool loadPngToCanvas(M5Canvas& canvas, const char* path, int32_t x, int32_t y);
  void scheduleBlink(unsigned long now);
  void scheduleSmile(unsigned long now);

  M5Canvas canvas_;
  M5Canvas talkCanvas_[kTalkCacheSetCount][2];
  ChanState state_ = ChanState::Idle;
  AuthFaceMode authFaceMode_ = AuthFaceMode::Unknown;
  bool photoFaceMode_ = false;
  bool photoMasterFaceMode_ = false;
  bool petFaceMode_ = false;
  bool shakeFaceMode_ = false;
  bool enabled_ = true;
  ThermalFaceMode thermalFaceMode_ = ThermalFaceMode::Normal;
  bool canvasReady_ = false;
  bool talkCacheReady_[kTalkCacheSetCount][2] = {};
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
  int batteryLevel_ = -1;
  bool batteryCharging_ = false;
  bool batteryOverlayDirty_ = true;
  bool micConnected_ = false;
  bool micMuted_ = false;
  bool micStreaming_ = false;
  bool micOverlayDirty_ = true;
  String currentPath_;
};
