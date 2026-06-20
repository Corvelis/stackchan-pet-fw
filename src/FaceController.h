#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <M5Unified.h>

#include "AffectionController.h"
#include "AppState.h"
#include "config.h"

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
  void restartSpeakingAnimation();
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
  void setClockText(const String& text, bool valid);
  void setMicState(bool connected, bool muted, bool streaming);
  void setCameraButtonPending(bool pending);
  void setAffectionState(const AffectionState& state);
  void showAffectionDelta(int delta, unsigned long now);
  void prepareSpeakingCache(AuthFaceMode authMode);
  void startSpeaking(AuthFaceMode authMode);
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
  void logSpeakingInterference(const char* source, int value) const;
  void showBaseFace();
  void drawFace(const char* path);
  bool drawCachedTalkFace(const char* path);
#if STACKCHAN_ROUND_DISPLAY
  bool drawRoundCachedTalkFace(const char* path);
  bool prepareRoundTalkCache(const char* path0, const char* path1);
  bool loadRoundBaseFaceToCanvas(M5Canvas& canvas, const char* path);
#endif
  int32_t faceImageDrawSize() const;
  int32_t faceImageDrawX() const;
  int32_t faceImageDrawY() const;
  float faceImageScale() const;
  template <typename Target>
  bool drawFaceImageTarget(Target& target, File& file) const;
  void drawAffectionOverlay(unsigned long now);
  void drawAffectionOverlayOnCanvas(unsigned long now);
  void drawBatteryOverlay();
  void drawBatteryOverlayOnCanvas();
  void drawCameraOverlay();
  void drawCameraOverlayOnCanvas();
  void drawMicOverlay();
  void drawMicOverlayOnCanvas();
  template <typename Target>
  void drawRoundAffectionOverlayTarget(Target& target, unsigned long now);
  template <typename Target>
  void drawRoundBatteryOverlayTarget(Target& target);
  template <typename Target>
  void drawRoundCameraOverlayTarget(Target& target);
  template <typename Target>
  void drawRoundMicOverlayTarget(Target& target);
  bool overlaysNeedRefresh(unsigned long now) const;
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
#if STACKCHAN_ROUND_DISPLAY
  M5Canvas roundTalkCanvas_[2];
  String roundTalkCachePath_[2];
  bool roundTalkCacheReady_[2] = {};
  bool roundTalkCacheAllocated_[2] = {};
#endif
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
  unsigned long lastSpeakingUpdateMs_ = 0;
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
  String clockText_;
  bool clockValid_ = false;
#if CLOCK_DISPLAY_ENABLED
  bool clockOverlayDirty_ = true;
#else
  bool clockOverlayDirty_ = false;
#endif
  bool micConnected_ = false;
  bool micMuted_ = false;
  bool micStreaming_ = false;
  bool micOverlayDirty_ = true;
  bool cameraButtonPending_ = false;
  bool cameraOverlayDirty_ = true;
  String currentPath_;
};
