#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <M5Unified.h>

#include "AppState.h"
#include "WebSocketServerController.h"
#include "config.h"

class AudioController {
public:
  void begin(WebSocketServerController* wsServer);
  void setState(ChanState state);
  ChanState state() const;
  bool isPlaybackDraining() const;
  bool pauseMicForCapture();
  void resumeMicAfterCapture(bool wasPaused);
  void deferNextSpeakerStartUntil(unsigned long timestampMs);
  void setVolume(uint8_t volume);
  uint8_t volume() const;
  void setMicMuted(bool muted);
  bool micMuted() const;
  bool isMicStreaming() const;
  void onBinaryReceived(uint8_t* payload, size_t length);
  void update(unsigned long now);

private:
  void enterIdle();
  void enterListening();
  void enterSpeaking();
  void finishSpeakingPlayback();
  void startMicInput();
  void startSpeakerIfDue();
  void updateMicCapture();
  void processMicChunk(int16_t* samples, size_t sampleCount);
  void updateSpeakerPlayback();
  void playPcmChunk(const int16_t* samples, size_t sampleCount);
  void resetMicBuffers();
  void clearRxRing();
  size_t rxAvailable() const;
  size_t rxFree() const;
  size_t rxCapacity() const;
  size_t appendRxBytes(const uint8_t* data, size_t length);
  bool readRxBytes(uint8_t* out, size_t length);

  WebSocketServerController* wsServer_ = nullptr;
  ChanState state_ = ChanState::Idle;
  unsigned long lastMicSendMs_ = 0;
  bool micEnabled_ = false;
  bool micMuted_ = false;
  bool speakerEnabled_ = false;
  bool speakerStartPending_ = false;
  bool playbackStarted_ = false;
  bool pendingIdleAfterPlayback_ = false;
  unsigned long idleDrainEmptySinceMs_ = 0;
  unsigned long speakerStartNotBeforeMs_ = 0;
  unsigned long lastMicStatsMs_ = 0;
  uint8_t volume_ = AUDIO_SPEAKER_VOLUME;
  uint32_t micStatsSamples_ = 0;
  uint32_t micStatsClipCount_ = 0;
  int32_t micStatsPeak_ = 0;
  size_t micRecordIndex_ = 2;
  size_t micSendIndex_ = 0;
  size_t micPrimedCount_ = 0;
  size_t speakerBufferIndex_ = 0;
  size_t rxReadIndex_ = 0;
  size_t rxWriteIndex_ = 0;
  size_t rxSize_ = 0;
  size_t rxCapacity_ = 0;
  uint8_t* rxRing_ = nullptr;
  int16_t micBuffers_[AUDIO_MIC_BUFFER_COUNT][AUDIO_CHUNK_SAMPLES] = {};
  int16_t speakerBuffers_[AUDIO_SPEAKER_BUFFER_COUNT][AUDIO_PLAYBACK_CHUNK_SAMPLES] = {};
};
