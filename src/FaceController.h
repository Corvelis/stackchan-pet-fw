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
#if STACKCHAN_PET_ANIMATION_ENABLED
  void setPetFaceMode(bool enabled, unsigned long now, bool animate, bool longPetting = false);
  void setPetAnimationTouchFrame(uint8_t frame, unsigned long now);
  bool petAnimationActive() const;
#endif
  void setShakeFaceMode(bool enabled);
  void setGuruguruFaceMode(bool enabled);
  void setGuruguruFaceDirection(uint8_t direction);
#if STACKCHAN_GURUGURU_FACE_ENABLED
  bool startGuruguruDizzyAnimation(bool reverse, unsigned long now);
  bool guruguruDizzyAnimationActive() const;
#endif
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  bool preloadGuruguruDizzyAnimation(bool reverse, uint8_t maxFrames);
#endif
  void setEnabled(bool enabled);
  void setThermalFaceMode(ThermalFaceMode mode);
  void setBatteryState(int level, bool charging);
  void setClockText(const String& text, bool valid);
  void setMicState(bool connected, bool muted, bool streaming);
  void setCameraButtonPending(bool pending);
  void setAffectionState(const AffectionState& state);
  void showAffectionDelta(int delta, unsigned long now);
  void showGuruguruStep(uint8_t steps, uint8_t difficulty, unsigned long now);
  void prepareSpeakingCache(AuthFaceMode authMode);
  void startSpeaking(AuthFaceMode authMode);
  void update(unsigned long now);

private:
#if STACKCHAN_DEVICE_CORES3
  static constexpr uint8_t kTalkCacheSetCount = 15;
#else
  static constexpr uint8_t kTalkCacheSetCount = 15;
#endif
  static constexpr uint8_t kImagePathCacheCount = 128;
  static constexpr size_t kImagePathCacheMaxLen = 40;

  const char* talkFacePath(uint8_t index) const;
  const char* listeningFacePath() const;
  const char* blinkFacePath() const;
  const char* idleFacePath() const;
  const char* thermalFacePath(uint8_t index) const;
  const char* thermalBlinkFacePath() const;
  const char* petFacePath(uint8_t index) const;
  const char* petBlinkFacePath() const;
  const char* shakeFacePath(uint8_t index) const;
  const char* guruguruFacePath(bool blink) const;
  const char* fallbackFacePath(const char* preferred, const char* fallback) const;
  uint8_t visualTierIndex() const;
  void logSpeakingInterference(const char* source, int value) const;
  void showBaseFace();
  void drawFace(const char* path, bool drawOverlays = true);
#if STACKCHAN_GURUGURU_FACE_ENABLED
  void drawGuruguruDizzyFrame(unsigned long now);
  bool drawGuruguruDizzyFrameDirect(const char* path, uint8_t frame);
  void stopGuruguruDizzyAnimation(bool restoreFace);
#endif
  bool drawCachedTalkFace(const char* path);
#if STACKCHAN_ROUND_DISPLAY
  bool drawRoundCachedTalkFace(const char* path);
  bool prepareRoundTalkCache(const char* path0, const char* path1);
  void releaseRoundTalkCache();
  bool loadRoundBaseFaceToCanvas(M5Canvas& canvas, const char* path);
#endif
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  static constexpr uint8_t kGuruguruBlinkCacheSlots = 1;
  static constexpr uint8_t kGuruguruDizzyCanvasSlots = 15;
#endif
#if STACKCHAN_PET_ANIMATION_ENABLED
  enum class PetAnimationPhase : uint8_t {
    None,
    Start,
    Loop,
    End,
    After
  };
  static constexpr uint8_t kPetAnimationFrameCount = 9;
  bool preparePetAnimationCache();
  void releasePetAnimationCache();
  bool loadPetAnimationFrameToCanvas(M5Canvas& canvas, uint8_t frame);
  bool drawPetAnimationFrame(uint8_t frame);
  bool startPetAnimation(unsigned long now);
  void finishPetAnimation(unsigned long now, bool longPetting);
  void stopPetAnimation(bool restoreFace);
  void updatePetAnimation(unsigned long now);
  const uint8_t* petAnimationSequence(uint8_t& length, unsigned long& intervalMs) const;
  String petAnimationFramePath(uint8_t frame) const;
#endif
  uint8_t guruguruFaceSourceIndex(uint8_t direction) const;
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  bool prepareGuruguruFaceCache();
  void releaseGuruguruFaceCache();
  void releaseGuruguruDirCache();
  void releaseGuruguruBlinkCache();
  bool drawCachedGuruguruFace(const char* path);
  bool parseGuruguruFacePath(const char* path, bool& blink, uint8_t& direction) const;
  bool loadGuruguruFaceToCanvas(M5Canvas& canvas, const char* path);
  int8_t findGuruguruBlinkCacheSlot(uint8_t direction) const;
  int8_t prepareGuruguruBlinkCache(uint8_t direction);
  bool preloadGuruguruDizzyJpegCache(bool reverse, uint8_t maxFrames);
  void releaseGuruguruDizzyJpegCache();
  void releaseGuruguruDizzySourceRange(uint8_t firstSourceFrame, uint8_t lastSourceFrame);
  bool preloadGuruguruDizzyPlaybackRange(bool reverse, uint8_t firstFrame, uint8_t endFrame, uint8_t maxLoads);
  bool loadGuruguruDizzyCacheFrame(bool reverse, uint8_t frame);
  void serviceGuruguruDizzyCache(uint8_t displayedFrame);
  int8_t findGuruguruDizzyCanvasSlot(uint8_t sourceFrame) const;
  int8_t prepareGuruguruDizzyCanvasSlot(uint8_t sourceFrame);
  bool loadGuruguruDizzyCanvasFrame(bool reverse, uint8_t frame);
  bool loadGuruguruDizzyFrameMeta(bool reverse);
  uint8_t guruguruDizzySourceFrame(bool reverse, uint8_t frame) const;
  String guruguruDizzyFramePath(bool reverse, uint8_t frame) const;
#endif
  int32_t faceImageDrawSize() const;
  int32_t faceImageDrawX() const;
  int32_t faceImageDrawY() const;
  float faceImageScale() const;
  bool isJpegPath(const char* path) const;
  bool isPngPath(const char* path) const;
  String resolvedImagePath(const char* path) const;
  template <typename Target>
  bool drawFaceImageTarget(Target& target, File& file, const char* path) const;
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
  void releaseTalkCache();
  bool loadImageToCanvas(M5Canvas& canvas, const char* path, int32_t x, int32_t y);
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
#if STACKCHAN_GURUGURU_CANVAS_CACHE_ENABLED
  M5Canvas guruguruDirCache_[STACKCHAN_GURUGURU_FACE_COUNT];
  M5Canvas guruguruBlinkCache_[kGuruguruBlinkCacheSlots];
  M5Canvas guruguruDizzyCanvasCache_[kGuruguruDizzyCanvasSlots];
  bool guruguruDirCacheReady_[STACKCHAN_GURUGURU_FACE_COUNT] = {};
  bool guruguruDirCacheAllocated_[STACKCHAN_GURUGURU_FACE_COUNT] = {};
  bool guruguruBlinkCacheReady_[kGuruguruBlinkCacheSlots] = {};
  bool guruguruBlinkCacheAllocated_[kGuruguruBlinkCacheSlots] = {};
  bool guruguruDizzyCanvasCacheReady_[kGuruguruDizzyCanvasSlots] = {};
  bool guruguruDizzyCanvasCacheAllocated_[kGuruguruDizzyCanvasSlots] = {};
  uint8_t guruguruBlinkCacheDirection_[kGuruguruBlinkCacheSlots] = {};
  uint8_t guruguruDizzyCanvasCacheSource_[kGuruguruDizzyCanvasSlots] = {};
  unsigned long guruguruBlinkCacheLastUseMs_[kGuruguruBlinkCacheSlots] = {};
  bool guruguruCachePrepared_ = false;
  uint8_t* guruguruDizzyJpegCache_[GURUGURU_DIZZY_FRAME_COUNT] = {};
  size_t guruguruDizzyJpegCacheSize_[GURUGURU_DIZZY_FRAME_COUNT] = {};
  int16_t guruguruDizzyFrameX_[GURUGURU_DIZZY_FRAME_COUNT] = {};
  int16_t guruguruDizzyFrameY_[GURUGURU_DIZZY_FRAME_COUNT] = {};
  int16_t guruguruDizzyFrameW_[GURUGURU_DIZZY_FRAME_COUNT] = {};
  int16_t guruguruDizzyFrameH_[GURUGURU_DIZZY_FRAME_COUNT] = {};
  bool guruguruDizzyJpegCacheReady_ = false;
  bool guruguruDizzyJpegCacheReverse_ = false;
  bool guruguruDizzyFrameMetaReady_ = false;
  bool guruguruDizzyBlinkReleased_ = false;
  bool guruguruDizzySpinReleased_ = false;
  bool guruguruDizzyKeepSpinCacheOnStop_ = false;
#endif
#if STACKCHAN_PET_ANIMATION_ENABLED
  M5Canvas petAnimationCanvas_[kPetAnimationFrameCount];
  bool petAnimationCacheReady_[kPetAnimationFrameCount] = {};
  bool petAnimationCacheAllocated_[kPetAnimationFrameCount] = {};
  bool petAnimationCachePrepared_ = false;
  PetAnimationPhase petAnimationPhase_ = PetAnimationPhase::None;
  uint8_t petAnimationSequenceIndex_ = 0;
  uint8_t petAnimationTouchFrame_ = 3;
  bool petAnimationLong_ = false;
  unsigned long nextPetAnimationFrameMs_ = 0;
#endif
  ChanState state_ = ChanState::Idle;
  AuthFaceMode authFaceMode_ = AuthFaceMode::Unknown;
  bool photoFaceMode_ = false;
  bool photoMasterFaceMode_ = false;
  bool petFaceMode_ = false;
  bool shakeFaceMode_ = false;
  bool guruguruFaceMode_ = false;
  uint8_t guruguruFaceDirection_ = STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
#if STACKCHAN_GURUGURU_FACE_ENABLED
  bool guruguruDizzyAnimating_ = false;
  bool guruguruDizzyReverse_ = false;
  uint8_t guruguruDizzyFrame_ = 0;
  unsigned long guruguruDizzyStartedMs_ = 0;
  unsigned long nextGuruguruDizzyFrameMs_ = 0;
#endif
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
  uint8_t guruguruStep_ = 0;
  uint8_t guruguruStepDifficulty_ = 0;
  unsigned long lastAffectionOverlayMs_ = 0;
  bool affectionOverlayDirty_ = true;
  int batteryLevel_ = -1;
  bool batteryCharging_ = false;
  bool batteryOverlayDirty_ = true;
  mutable char imagePathCacheRequest_[kImagePathCacheCount][kImagePathCacheMaxLen] = {};
  mutable char imagePathCacheResolved_[kImagePathCacheCount][kImagePathCacheMaxLen] = {};
  mutable bool imagePathCacheReady_[kImagePathCacheCount] = {};
  mutable uint8_t imagePathCacheNext_ = 0;
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
