#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
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
  void deferMicCaptureUntil(unsigned long timestampMs);
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
  void stopMicInput(const char* reason);
  void noteMicPause(const char* reason);
  void startSpeakerIfDue();
  void updateMicSend();
  static void micCaptureTaskEntry(void* context);
  void micCaptureTaskLoop();
  bool enqueueMicPacket(const int16_t* samples, size_t sampleCount, unsigned long timestampMs, uint16_t flags);
  bool copyNextMicPacket(uint8_t* out);
  void clearMicPacketQueue();
  void processMicChunk(int16_t* samples, size_t sampleCount);
  void writeMicPacket(uint8_t* packet, const int16_t* samples, size_t sampleCount, unsigned long timestampMs, uint16_t flags);
  void updateMicDiagnostics(unsigned long now);
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
  unsigned long nextMicSendDueMs_ = 0;
  bool micEnabled_ = false;
  bool micMuted_ = false;
  bool speakerEnabled_ = false;
  bool speakerStartPending_ = false;
  bool playbackStarted_ = false;
  bool pendingIdleAfterPlayback_ = false;
  unsigned long idleDrainEmptySinceMs_ = 0;
  unsigned long speakerStartNotBeforeMs_ = 0;
  unsigned long micCaptureNotBeforeMs_ = 0;
  unsigned long lastMicStatsMs_ = 0;
  unsigned long lastMicTxStatsMs_ = 0;
  unsigned long lastMicTxChunkMs_ = 0;
  unsigned long micTxMaxIntervalMs_ = 0;
  unsigned long micCaptureTimestampMs_ = 0;
  unsigned long lastMicCaptureStartMs_ = 0;
  unsigned long micCaptureGapMaxMs_ = 0;
  unsigned long micCaptureGapLastMs_ = 0;
  uint8_t volume_ = AUDIO_SPEAKER_VOLUME;
  uint32_t micStatsSamples_ = 0;
  uint32_t micStatsClipCount_ = 0;
  uint32_t micTxStatsChunks_ = 0;
  uint32_t micTxStatsBytes_ = 0;
  uint32_t micTxStatsDroppedChunks_ = 0;
  uint32_t micTxStatsCaptureOverrun_ = 0;
  uint32_t micTxStatsCaptureUnderrun_ = 0;
  uint32_t micTxStatsRingOverflow_ = 0;
  uint32_t micTxStatsSendQueueOverflow_ = 0;
  uint32_t micTxStatsScheduleMissed_ = 0;
  uint32_t micTxStatsSendFail_ = 0;
  uint32_t micCaptureGapCount_ = 0;
  uint32_t micPausedByState_ = 0;
  uint32_t micPauseIdle_ = 0;
  uint32_t micPauseSpeaking_ = 0;
  uint32_t micPauseMuted_ = 0;
  uint32_t micPauseCamera_ = 0;
  uint32_t micPauseWsSettle_ = 0;
  uint32_t micPauseOther_ = 0;
  int32_t micStatsPeak_ = 0;
  size_t micRecordIndex_ = 2;
  size_t micSendIndex_ = 0;
  size_t micPrimedCount_ = 0;
  size_t micTxQueueReadIndex_ = 0;
  size_t micTxQueueWriteIndex_ = 0;
  size_t micTxQueueCount_ = 0;
  size_t micTxQueueCapacity_ = 0;
  uint32_t micTxSeq_ = 0;
  size_t speakerBufferIndex_ = 0;
  size_t rxReadIndex_ = 0;
  size_t rxWriteIndex_ = 0;
  size_t rxSize_ = 0;
  size_t rxCapacity_ = 0;
  uint8_t* rxRing_ = nullptr;
  uint8_t* micTxRing_ = nullptr;
  TaskHandle_t micCaptureTaskHandle_ = nullptr;
  portMUX_TYPE micMux_ = portMUX_INITIALIZER_UNLOCKED;
  volatile bool micCaptureActive_ = false;
  volatile bool micCaptureRecording_ = false;
  volatile bool micCaptureStartPending_ = true;
  int16_t micBuffers_[AUDIO_MIC_BUFFER_COUNT][AUDIO_CHUNK_SAMPLES] = {};
  uint8_t micSendScratch_[AUDIO_MIC_PACKET_BYTES] = {};
  int16_t speakerBuffers_[AUDIO_SPEAKER_BUFFER_COUNT][AUDIO_PLAYBACK_CHUNK_SAMPLES] = {};
};
