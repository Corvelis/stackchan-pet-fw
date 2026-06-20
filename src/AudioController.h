#pragma once

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <M5Unified.h>

#include "AppState.h"
#include "WebSocketServerController.h"
#include "config.h"

using MicPacketSender = bool (*)(const uint8_t* payload, size_t length, void* context);

struct MicDiagnosticResult {
  bool beginOk = false;
  uint32_t durationMs = 0;
  uint32_t chunks = 0;
  uint32_t underruns = 0;
  uint32_t sampleCount = 0;
  int32_t peak = 0;
  int32_t rms = 0;
  int32_t dc = 0;
  uint32_t clipCount = 0;
};

enum class MicDiagnosticChannelMode : uint8_t {
  Default,
  Right,
  Left,
  Stereo,
};

struct MicDiagnosticConfig {
  uint32_t sampleRate = AUDIO_SAMPLE_RATE;
  int pinDataIn = -1000;
  int pinBck = -1000;
  int pinWs = -1000;
  int pinMck = -1000;
  int i2sPort = -1000;
  int codecReg14 = -1;
  int codecReg16 = -1;
  int codecReg17 = -1;
  int magnification = -1;
  int noiseFilterLevel = -1;
  int overSampling = -1;
  MicDiagnosticChannelMode channelMode = MicDiagnosticChannelMode::Default;
};

struct MicRuntimeStats {
  bool enabled = false;
  bool captureActive = false;
  bool captureRecording = false;
  bool hasClient = false;
  bool muted = false;
  size_t queuedPackets = 0;
  size_t queueCapacity = 0;
  uint32_t capturedChunks = 0;
  uint32_t enqueuedChunks = 0;
  uint32_t sentChunks = 0;
  uint32_t sentBytes = 0;
  uint32_t wsSentChunks = 0;
  uint32_t usbSentChunks = 0;
  uint32_t droppedChunks = 0;
  uint32_t captureUnderruns = 0;
  uint32_t captureOverruns = 0;
  uint32_t queueOverflows = 0;
  uint32_t sendFails = 0;
  uint32_t txSeq = 0;
  int32_t lastPeak = 0;
  unsigned long lastCaptureMs = 0;
  unsigned long lastProcessMs = 0;
  unsigned long lastEnqueueMs = 0;
  unsigned long lastSendMs = 0;
  unsigned long lastWsSendMs = 0;
  unsigned long lastUsbSendMs = 0;
};

struct AudioPlaybackDiagnostic {
  ChanState state = ChanState::Idle;
  bool draining = false;
  bool playbackStarted = false;
  bool speakerEnabled = false;
  bool speakerStartPending = false;
  size_t rxAvailable = 0;
  size_t rxCapacity = 0;
  uint32_t pcmFramesReceived = 0;
  uint32_t pcmBytesReceived = 0;
  uint32_t pcmBytesAccepted = 0;
  uint32_t pcmBytesDropped = 0;
  uint32_t rxOverflowEvents = 0;
  uint32_t dropNotSpeakingEvents = 0;
  uint32_t dropOddSizeEvents = 0;
  uint32_t idleRequests = 0;
  uint32_t playbackStarts = 0;
  uint32_t playbackFinishes = 0;
  uint32_t underflowResets = 0;
  uint32_t speakerQueueFullEvents = 0;
  uint32_t playRawFailEvents = 0;
  uint32_t speakerChunksQueued = 0;
  uint32_t speakerBytesQueued = 0;
  size_t maxBufferedBytes = 0;
  size_t lastIdleRequestBufferedBytes = 0;
  size_t lastUnderflowBufferedBytes = 0;
  size_t finishBufferedBytes = 0;
  uint32_t finishQueuedChunks = 0;
  unsigned long lastPcmMs = 0;
  unsigned long lastFinishMs = 0;
};

class AudioController {
public:
  void begin(WebSocketServerController* wsServer);
  void setMicPacketSender(MicPacketSender sender, void* context);
  void setUsbSerialClientConnected(bool connected);
  void setState(ChanState state);
  ChanState state() const;
  bool isPlaybackDraining() const;
  bool hasPlaybackStarted() const;
  bool pauseMicForCapture();
  void resumeMicAfterCapture(bool wasPaused);
  void deferNextSpeakerStartUntil(unsigned long timestampMs);
  void deferMicCaptureUntil(unsigned long timestampMs);
  void setVolume(uint8_t volume);
  uint8_t volume() const;
  void setMicMuted(bool muted);
  bool micMuted() const;
  bool isMicStreaming() const;
  void setRemoteVadActive(bool active);
  bool playDiagnosticTone(uint32_t durationMs = 350);
  bool measureMicDiagnostic(uint32_t durationMs, MicDiagnosticResult& result, const MicDiagnosticConfig& config = MicDiagnosticConfig{});
  MicRuntimeStats micRuntimeStats() const;
  AudioPlaybackDiagnostic playbackDiagnostic() const;
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
  bool hasMicClient() const;
  static void micCaptureTaskEntry(void* context);
  void micCaptureTaskLoop();
  bool enqueueMicPacket(const int16_t* samples, size_t sampleCount, unsigned long timestampMs, uint16_t flags);
  bool copyNextMicPacket(uint8_t* out);
  void clearMicPacketQueue();
  void storeMicPreRollChunk(const int16_t* samples, unsigned long timestampMs);
  bool enqueueMicPreRoll(uint16_t firstFlags);
  void clearMicPreRoll();
  bool recordMicChunkBlocking(int16_t* samples, size_t sampleCount, uint32_t sampleRate, uint32_t timeoutMs);
  int32_t processMicChunk(int16_t* samples, size_t sampleCount);
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
#if STACKCHAN_DEVICE_STOPWATCH
  size_t appendRxBytesWithPlaybackBackpressure(const uint8_t* data, size_t length);
#endif
  bool readRxBytes(uint8_t* out, size_t length);

  WebSocketServerController* wsServer_ = nullptr;
  MicPacketSender micPacketSender_ = nullptr;
  void* micPacketSenderContext_ = nullptr;
  ChanState state_ = ChanState::Idle;
  unsigned long nextMicSendDueMs_ = 0;
  bool micEnabled_ = false;
  bool micMuted_ = false;
  bool usbSerialClientConnected_ = false;
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
  unsigned long micGateOpenUntilMs_ = 0;
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
  uint32_t micRuntimeCapturedChunks_ = 0;
  uint32_t micRuntimeEnqueuedChunks_ = 0;
  uint32_t micRuntimeSentChunks_ = 0;
  uint32_t micRuntimeSentBytes_ = 0;
  uint32_t micRuntimeWsSentChunks_ = 0;
  uint32_t micRuntimeUsbSentChunks_ = 0;
  uint32_t micRuntimeDroppedChunks_ = 0;
  uint32_t micRuntimeCaptureUnderruns_ = 0;
  uint32_t micRuntimeCaptureOverruns_ = 0;
  uint32_t micRuntimeQueueOverflows_ = 0;
  uint32_t micRuntimeSendFails_ = 0;
  uint32_t speakerRxPacketCount_ = 0;
  size_t speakerLastBufferLogBytes_ = 0;
  uint32_t playbackDiagPcmFramesReceived_ = 0;
  uint32_t playbackDiagPcmBytesReceived_ = 0;
  uint32_t playbackDiagPcmBytesAccepted_ = 0;
  uint32_t playbackDiagPcmBytesDropped_ = 0;
  uint32_t playbackDiagRxOverflowEvents_ = 0;
  uint32_t playbackDiagDropNotSpeakingEvents_ = 0;
  uint32_t playbackDiagDropOddSizeEvents_ = 0;
  uint32_t playbackDiagIdleRequests_ = 0;
  uint32_t playbackDiagStarts_ = 0;
  uint32_t playbackDiagFinishes_ = 0;
  uint32_t playbackDiagUnderflowResets_ = 0;
  uint32_t playbackDiagSpeakerQueueFullEvents_ = 0;
  uint32_t playbackDiagPlayRawFailEvents_ = 0;
  uint32_t playbackDiagSpeakerChunksQueued_ = 0;
  uint32_t playbackDiagSpeakerBytesQueued_ = 0;
  size_t playbackDiagMaxBufferedBytes_ = 0;
  size_t playbackDiagLastIdleRequestBufferedBytes_ = 0;
  size_t playbackDiagLastUnderflowBufferedBytes_ = 0;
  size_t playbackDiagFinishBufferedBytes_ = 0;
  uint32_t playbackDiagFinishQueuedChunks_ = 0;
  unsigned long playbackDiagLastPcmMs_ = 0;
  unsigned long playbackDiagLastFinishMs_ = 0;
  int32_t micStatsPeak_ = 0;
  int32_t micRuntimeLastPeak_ = 0;
  unsigned long micRuntimeLastCaptureMs_ = 0;
  unsigned long micRuntimeLastProcessMs_ = 0;
  unsigned long micRuntimeLastEnqueueMs_ = 0;
  unsigned long micRuntimeLastSendMs_ = 0;
  unsigned long micRuntimeLastWsSendMs_ = 0;
  unsigned long micRuntimeLastUsbSendMs_ = 0;
  size_t micRecordIndex_ = 2;
  size_t micSendIndex_ = 0;
  size_t micPrimedCount_ = 0;
  size_t micTxQueueReadIndex_ = 0;
  size_t micTxQueueWriteIndex_ = 0;
  size_t micTxQueueCount_ = 0;
  size_t micTxQueueCapacity_ = 0;
  size_t micPreRollWriteIndex_ = 0;
  size_t micPreRollCount_ = 0;
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
  volatile bool remoteVadActive_ = false;
  bool micGateSending_ = false;
  bool micLastRemoteVadActive_ = false;
  int16_t micBuffers_[AUDIO_MIC_BUFFER_COUNT][AUDIO_CHUNK_SAMPLES] = {};
  int16_t micPreRollBuffers_[AUDIO_MIC_GATE_PREROLL_CHUNKS][AUDIO_CHUNK_SAMPLES] = {};
  unsigned long micPreRollTimestamps_[AUDIO_MIC_GATE_PREROLL_CHUNKS] = {};
  uint8_t micSendScratch_[AUDIO_MIC_PACKET_BYTES] = {};
  int16_t speakerBuffers_[AUDIO_SPEAKER_BUFFER_COUNT][AUDIO_PLAYBACK_CHUNK_SAMPLES] = {};
};
