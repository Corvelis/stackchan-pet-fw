#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"

class AudioController;
class FaceController;

enum class SpeechBubbleTransport : uint8_t {
  None,
  WebSocket,
  UsbSerial,
};

class SpeechBubbleController {
public:
  void begin(AudioController* audioController, FaceController* faceController);
  bool cue(const char* sequenceId,
           uint32_t segmentIndex,
           const char* text,
           uint32_t pcmBytes,
           uint32_t sampleRate,
           SpeechBubbleTransport transport,
           const char** error = nullptr);
  bool end(const char* sequenceId,
           uint32_t holdMs,
           SpeechBubbleTransport transport,
           const char** error = nullptr);
  bool cancel(const char* sequenceId,
              SpeechBubbleTransport transport,
              const char** error = nullptr);
  void onPcmReceived(SpeechBubbleTransport transport);
  void update(unsigned long now);
  void reset(const char* reason);
  bool active() const;
  bool activeForTransport(SpeechBubbleTransport transport) const;

private:
  struct CueEntry {
    uint32_t segmentIndex = 0;
    uint32_t playbackStartBytes = 0;
    uint32_t pcmBytes = 0;
    char text[SPEECH_BUBBLE_MAX_TEXT_BYTES + 1] = {};
  };

  bool fail(const char** error, const char* value) const;
  bool lockState() const;
  bool tryLockState() const;
  void unlockState() const;
  void clearQueue();
  bool resetStateLocked();
  void startSequenceLocked(const char* sequenceId,
                           SpeechBubbleTransport transport,
                           bool bindAudioStream,
                           unsigned long now);
  void bindAudioStreamLocked(unsigned long now);
  void applyPendingFaceClear();
  const char* transportName(SpeechBubbleTransport transport) const;

  AudioController* audioController_ = nullptr;
  FaceController* faceController_ = nullptr;
  mutable SemaphoreHandle_t stateMutex_ = nullptr;
  CueEntry queue_[SPEECH_BUBBLE_MAX_QUEUED_CUES];
  uint8_t queueHead_ = 0;
  uint8_t queueCount_ = 0;
  char sequenceId_[SPEECH_BUBBLE_MAX_SEQUENCE_ID_BYTES + 1] = {};
  SpeechBubbleTransport transport_ = SpeechBubbleTransport::None;
  uint32_t audioStreamId_ = 0;
  uint32_t generation_ = 0;
  uint32_t expectedReceivedBytes_ = 0;
  uint32_t lastSegmentIndex_ = 0;
  bool hasLastSegmentIndex_ = false;
  bool active_ = false;
  bool audioStreamBound_ = false;
  bool awaitingSpeaking_ = false;
  bool faceClearPending_ = false;
  bool endReceived_ = false;
  bool holdStarted_ = false;
  uint32_t holdMs_ = SPEECH_BUBBLE_DEFAULT_HOLD_MS;
  unsigned long holdUntilMs_ = 0;
  uint32_t lastProgressReceivedBytes_ = 0;
  uint32_t lastProgressDequeuedBytes_ = 0;
  unsigned long lastProgressMs_ = 0;
  unsigned long preSpeakingDeadlineMs_ = 0;
  unsigned long renderRetryNotBeforeMs_ = 0;
};
