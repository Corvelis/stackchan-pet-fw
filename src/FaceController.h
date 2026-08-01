#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <M5Unified.h>

#include "AffectionController.h"
#include "AppState.h"
#include "FaceAssetProfile.h"
#include "PhoneCameraRemoteController.h"
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
  void setStepCount(uint32_t steps, bool valid);
  void setMicState(bool connected, bool muted, bool streaming);
  void setCameraButtonPending(bool pending);
  void setCameraCaptureActive(bool active);
  void setPhoneCameraState(PhoneCameraState state,
                           PhoneCameraLens lens = PhoneCameraLens::Unknown,
                           PhoneCameraOperation operation = PhoneCameraOperation::None);
  void setAffectionState(const AffectionState& state);
  void showAffectionDelta(int delta, unsigned long now);
  void showGuruguruStep(uint8_t steps, uint8_t difficulty, unsigned long now);
  void prepareSpeakingCache(AuthFaceMode authMode);
  void startSpeaking(AuthFaceMode authMode);
  void setVoiceMouthLevel(uint8_t level, unsigned long now);
  void setVoicePettingActive(bool active, unsigned long now);
  bool voicePettingAnimationActive() const;
  bool setSpeechBubbleText(const char* text);
  void clearSpeechBubble();
  bool speechBubbleVisible() const;
  const FaceAssetStatus& faceAssetStatus() const;
  void update(unsigned long now);

private:
#if STACKCHAN_DEVICE_CORES3
  static constexpr uint8_t kTalkCacheSetCount = 15;
#else
  static constexpr uint8_t kTalkCacheSetCount = 15;
#endif
  static constexpr uint8_t kImagePathCacheCount = 128;
  static constexpr size_t kImagePathCacheMaxLen = 40;
  static constexpr uint8_t kShakeAnimationFrameCount = 4;

  const char* talkFacePath(uint8_t index) const;
  const char* listeningFacePath() const;
  const char* blinkFacePath() const;
  const char* idleFacePath() const;
  const char* thermalFacePath(uint8_t index) const;
  const char* thermalBlinkFacePath() const;
  const char* petFacePath(uint8_t index) const;
  const char* petBlinkFacePath() const;
  const char* shakeFacePath(uint8_t index) const;
  const char* shakeAnimationFramePath(uint8_t frame) const;
  void resetShakeAnimation(unsigned long now);
  void updateShakeAnimation(unsigned long now);
  void startShakeRecoveryAnimation(unsigned long now);
  bool updateShakeRecoveryAnimation(unsigned long now);
  bool drawShakeRecoveryFrame(uint8_t frame);
  bool prepareShakeAnimationCache();
  void releaseShakeAnimationCache();
  bool loadShakeAnimationFrameToCanvas(M5Canvas& canvas, uint8_t frame);
  bool drawShakeAnimationFrame(uint8_t frame);
  const char* guruguruFacePath(bool blink) const;
  const char* fallbackFacePath(const char* preferred, const char* fallback) const;
  uint8_t visualTierIndex() const;
  void logSpeakingInterference(const char* source, int value) const;
#if STACKCHAN_CLASSIC_FACE_ENABLED
  void updateClassicFace(unsigned long now);
  void drawClassicFace();
  template <typename Target>
  void drawClassicFaceTarget(Target& target);
#endif
  void showBaseFace();
  void drawFace(const char* path, bool drawOverlays = true);
  void drawEmergencyFace();
  template <typename Target>
  void drawEmergencyFaceTarget(Target& target);
#if STACKCHAN_GURUGURU_FACE_ENABLED
  void drawGuruguruDizzyFrame(unsigned long now);
  bool drawGuruguruDizzyFrameDirect(const char* path, uint8_t frame);
  void stopGuruguruDizzyAnimation(bool restoreFace);
#endif
  bool drawCachedTalkFace(const char* path);
#if STACKCHAN_VOICE_SPRITE_ANIMATION_ENABLED
  bool voiceSpriteAnimationAllowed() const;
  bool voiceSpritePresentationActive() const;
  bool voiceSpriteCacheReadyFor(bool full) const;
  bool prepareVoiceSpriteCache(bool full);
  void releaseVoiceSpriteCache();
  void releaseVoiceSpriteOpenMouthCache();
  void applyVoiceSpriteConnectionCachePolicy(bool connected);
  void clearVoiceSpriteBlockingModes(unsigned long now);
  bool loadVoiceSpriteFrameToCanvas(M5Canvas& canvas, uint8_t mouth, uint8_t eye);
  bool drawCachedVoiceSpriteFrame(uint8_t mouth, uint8_t eye);
  bool drawVoiceSpriteFrame(uint8_t mouth, uint8_t eye);
  void resetVoiceSpriteAnimation(unsigned long now);
  void updateVoiceSpriteAnimation(unsigned long now);
  void startVoiceBlinkAnimation(unsigned long now);
  void updateVoicePettingEyeAnimation(unsigned long now, bool& changed);
  String voiceSpriteFramePath(uint8_t mouth, uint8_t eye) const;
#endif
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
  static constexpr uint8_t kPetAnimationFrameCount = 16;
  bool preparePetAnimationCache();
  void releasePetAnimationCache();
  bool loadPetAnimationFrameToCanvas(M5Canvas& canvas, uint8_t frame);
  bool drawPetAnimationFrame(uint8_t frame);
  bool startPetAnimation(unsigned long now);
  void finishPetAnimation(unsigned long now, bool longPetting);
  void stopPetAnimation(bool restoreFace);
  void releasePetAnimationAfterCache();
  void updatePetAnimation(unsigned long now);
  const uint8_t* petAnimationSequence(uint8_t& length, unsigned long& intervalMs) const;
  unsigned long petAnimationFrameInterval(uint8_t frame, unsigned long defaultIntervalMs) const;
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
  void drawSpeechBubbleOverlay();
  void drawSpeechBubbleOverlayOnCanvas();
  bool prepareSpeechBubbleCanvas();
  bool rebuildSpeechBubbleCanvas(const char* text);
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
  M5Canvas speechBubbleCanvas_;
  M5Canvas talkCanvas_[kTalkCacheSetCount][2];
  M5Canvas shakeAnimationCanvas_[kShakeAnimationFrameCount];
  bool shakeAnimationCacheReady_[kShakeAnimationFrameCount] = {};
  bool shakeAnimationCacheAllocated_[kShakeAnimationFrameCount] = {};
  bool shakeAnimationCachePrepared_ = false;
#if STACKCHAN_VOICE_SPRITE_ANIMATION_ENABLED
  M5Canvas voiceSpriteCanvas_[VOICE_SPRITE_MOUTH_FRAME_COUNT][VOICE_SPRITE_EYE_FRAME_COUNT];
  bool voiceSpriteCacheReady_[VOICE_SPRITE_MOUTH_FRAME_COUNT][VOICE_SPRITE_EYE_FRAME_COUNT] = {};
  bool voiceSpriteCacheAllocated_[VOICE_SPRITE_MOUTH_FRAME_COUNT][VOICE_SPRITE_EYE_FRAME_COUNT] = {};
  bool voiceBlinkAnimating_ = false;
  uint8_t voiceEyeIndex_ = 0;
  uint8_t voiceMouthIndex_ = 0;
  uint8_t voiceMouthTargetIndex_ = 0;
  uint8_t voiceBlinkSequenceIndex_ = 0;
  uint8_t voiceMouthSequenceIndex_ = 0;
  unsigned long nextVoiceBlinkFrameMs_ = 0;
  unsigned long nextVoiceMouthFrameMs_ = 0;
  unsigned long nextVoicePettingEyeFrameMs_ = 0;
  unsigned long lastVoiceMouthLevelMs_ = 0;
  uint16_t voiceBlinkFrameIntervalMs_ = 1000 / VOICE_SPRITE_BLINK_MAX_FPS;
  bool voicePettingActive_ = false;
  bool voicePettingEyeTransitioning_ = false;
#endif
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
  FaceAssetStatus faceAssetStatus_;
  AuthFaceMode authFaceMode_ = AuthFaceMode::Unknown;
  bool photoFaceMode_ = false;
  bool photoMasterFaceMode_ = false;
  bool petFaceMode_ = false;
  bool shakeFaceMode_ = false;
  uint8_t shakeAnimationFrame_ = 0;
  unsigned long nextShakeAnimationFrameMs_ = 0;
  bool shakeRecoveryAnimating_ = false;
  uint8_t shakeRecoveryFrame_ = 0;
  unsigned long nextShakeRecoveryFrameMs_ = 0;
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
  bool speechBubbleCanvasAllocated_ = false;
  bool speechBubbleCanvasReady_ = false;
  bool speechBubbleVisible_ = false;
  int32_t speechBubbleX_ = 0;
  int32_t speechBubbleY_ = 0;
  int32_t speechBubbleWidth_ = 0;
  int32_t speechBubbleHeight_ = 0;
  String speechBubbleText_;
  bool talkCacheReady_[kTalkCacheSetCount][2] = {};
  bool lipOpen_ = false;
  bool blinking_ = false;
  bool smiling_ = false;
#if STACKCHAN_CLASSIC_FACE_ENABLED
  uint8_t classicMouthLevel_ = 0;
  uint8_t classicMouthTargetLevel_ = 0;
  uint8_t classicMouthSameFrameCount_ = 0;
  unsigned long nextClassicMouthFrameMs_ = 0;
  bool classicFaceDirty_ = true;
#endif
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
  uint32_t stepCount_ = 0;
  bool stepCountValid_ = false;
#if STEP_COUNTER_ENABLED
  bool stepOverlayDirty_ = true;
#else
  bool stepOverlayDirty_ = false;
#endif
  bool micConnected_ = false;
  bool micMuted_ = false;
  bool micStreaming_ = false;
  bool micOverlayDirty_ = true;
  bool cameraButtonPending_ = false;
  bool cameraCaptureActive_ = false;
  bool cameraOverlayDirty_ = true;
  PhoneCameraState phoneCameraState_ = PhoneCameraState::Unavailable;
  PhoneCameraLens phoneCameraLens_ = PhoneCameraLens::Unknown;
  PhoneCameraOperation phoneCameraOperation_ = PhoneCameraOperation::None;
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  bool phoneCameraOverlayDirty_ = true;
#else
  bool phoneCameraOverlayDirty_ = false;
#endif
  String currentPath_;
};
