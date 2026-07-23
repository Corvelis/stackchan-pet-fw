#include "SpeechBubbleController.h"

#include <cstring>

#include "AudioController.h"
#include "FaceController.h"

namespace {

bool elapsedAtLeast(unsigned long now, unsigned long since, uint32_t durationMs) {
  // `now` is sampled by the main loop, while `since` can be updated a little
  // later by the WebSocket task. Treat that ordering as "not elapsed" instead
  // of letting unsigned subtraction wrap into a multi-billion-ms timeout.
  return static_cast<int32_t>(now - since) >= static_cast<int32_t>(durationMs);
}

}  // namespace

void SpeechBubbleController::begin(AudioController* audioController,
                                   FaceController* faceController) {
  audioController_ = audioController;
  faceController_ = faceController;
  if (stateMutex_ == nullptr) {
    stateMutex_ = xSemaphoreCreateMutex();
    if (stateMutex_ == nullptr) {
      Serial.println("[speech_bubble] failed to create state mutex");
    }
  }
  reset("begin");
}

bool SpeechBubbleController::cue(const char* sequenceId,
                                 uint32_t segmentIndex,
                                 const char* text,
                                 uint32_t pcmBytes,
                                 uint32_t sampleRate,
                                 SpeechBubbleTransport transport,
                                 const char** error) {
  if (audioController_ == nullptr || faceController_ == nullptr) {
    return fail(error, "not_initialized");
  }
  if (transport == SpeechBubbleTransport::None) {
    return fail(error, "invalid_transport");
  }
  if (sequenceId == nullptr || sequenceId[0] == '\0') {
    return fail(error, "missing_sequence_id");
  }
  const size_t sequenceLength = strnlen(sequenceId, SPEECH_BUBBLE_MAX_SEQUENCE_ID_BYTES + 1);
  if (sequenceLength > SPEECH_BUBBLE_MAX_SEQUENCE_ID_BYTES) {
    return fail(error, "sequence_id_too_long");
  }
  if (text == nullptr || text[0] == '\0') {
    return fail(error, "missing_text");
  }
  const size_t textLength = strnlen(text, SPEECH_BUBBLE_MAX_TEXT_BYTES + 1);
  if (textLength > SPEECH_BUBBLE_MAX_TEXT_BYTES) {
    return fail(error, "text_too_long");
  }
  if (pcmBytes == 0 || (pcmBytes & 1U) != 0 || pcmBytes > SPEECH_BUBBLE_MAX_PCM_BYTES) {
    return fail(error, "invalid_pcm_bytes");
  }
  if (sampleRate != AUDIO_SAMPLE_RATE) {
    return fail(error, "unsupported_sample_rate");
  }

  const unsigned long now = millis();
  const bool audioSpeaking = audioController_->state() == ChanState::Speaking;
  bool sequenceReplaced = false;
  bool boundaryResynced = false;
  bool pendingSpeaking = false;
  uint32_t previousExpectedBytes = 0;
  uint32_t receivedBytes = 0;
  uint32_t playbackStartBytes = 0;
  uint8_t queuedCount = 0;

  if (!lockState()) {
    return fail(error, "busy");
  }

  if (!active_ || strcmp(sequenceId_, sequenceId) != 0) {
    sequenceReplaced = active_;
    startSequenceLocked(sequenceId, transport, audioSpeaking, now);
  } else if (transport_ != transport) {
    unlockState();
    return fail(error, "transport_mismatch");
  } else if (awaitingSpeaking_ && audioSpeaking) {
    bindAudioStreamLocked(now);
  }

  if (!audioStreamBound_ && !awaitingSpeaking_) {
    unlockState();
    return fail(error, "audio_not_speaking");
  }

  if (hasLastSegmentIndex_ && segmentIndex <= lastSegmentIndex_) {
    if (segmentIndex == lastSegmentIndex_) {
      pendingSpeaking = awaitingSpeaking_;
      unlockState();
      Serial.printf("[speech_bubble] duplicate cue retained sequence=%s segment=%lu pending_speaking=%d\n",
                    sequenceId,
                    static_cast<unsigned long>(segmentIndex),
                    pendingSpeaking ? 1 : 0);
      return pendingSpeaking ? fail(error, "audio_not_speaking") : true;
    }
    unlockState();
    return fail(error, "segment_index_out_of_order");
  }
  if (queueCount_ >= SPEECH_BUBBLE_MAX_QUEUED_CUES) {
    unlockState();
    return fail(error, "cue_queue_full");
  }

  previousExpectedBytes = expectedReceivedBytes_;
  if (audioStreamBound_) {
    receivedBytes = audioController_->playbackPcmReceivedBytes();
    boundaryResynced = hasLastSegmentIndex_ && receivedBytes != expectedReceivedBytes_;
    if (UINT32_MAX - receivedBytes < pcmBytes) {
      unlockState();
      return fail(error, "pcm_offset_overflow");
    }
    playbackStartBytes = audioController_->playbackPcmAcceptedBytes();
    expectedReceivedBytes_ = receivedBytes + pcmBytes;
  } else {
    if (UINT32_MAX - expectedReceivedBytes_ < pcmBytes) {
      unlockState();
      return fail(error, "pcm_offset_overflow");
    }
    playbackStartBytes = expectedReceivedBytes_;
    expectedReceivedBytes_ += pcmBytes;
  }

  const uint8_t slot = static_cast<uint8_t>((queueHead_ + queueCount_) % SPEECH_BUBBLE_MAX_QUEUED_CUES);
  CueEntry& queuedCue = queue_[slot];
  queuedCue.segmentIndex = segmentIndex;
  queuedCue.playbackStartBytes = playbackStartBytes;
  queuedCue.pcmBytes = pcmBytes;
  memcpy(queuedCue.text, text, textLength);
  queuedCue.text[textLength] = '\0';
  ++queueCount_;

  lastSegmentIndex_ = segmentIndex;
  hasLastSegmentIndex_ = true;
  endReceived_ = false;
  holdStarted_ = false;
  holdUntilMs_ = 0;
  renderRetryNotBeforeMs_ = 0;
  lastProgressMs_ = now;
  pendingSpeaking = awaitingSpeaking_;
  queuedCount = queueCount_;
  unlockState();

  if (sequenceReplaced) {
    Serial.printf("[speech_bubble] reset reason=sequence_replaced next=%s\n", sequenceId);
  }
  if (boundaryResynced) {
    Serial.printf("[speech_bubble] cue boundary resync sequence=%s segment=%lu expected_rx=%lu actual_rx=%lu\n",
                  sequenceId,
                  static_cast<unsigned long>(segmentIndex),
                  static_cast<unsigned long>(previousExpectedBytes),
                  static_cast<unsigned long>(receivedBytes));
  }
  Serial.printf("[speech_bubble] cue queued sequence=%s segment=%lu start=%lu pcm=%lu queued=%u transport=%s pending_speaking=%d\n",
                sequenceId,
                static_cast<unsigned long>(segmentIndex),
                static_cast<unsigned long>(playbackStartBytes),
                static_cast<unsigned long>(pcmBytes),
                static_cast<unsigned>(queuedCount),
                transportName(transport),
                pendingSpeaking ? 1 : 0);

  // Retain an early cue for 500 ms, while preserving the v1 error response so
  // current apps can immediately resend state:speaking and retry the same cue.
  return pendingSpeaking ? fail(error, "audio_not_speaking") : true;
}

bool SpeechBubbleController::end(const char* sequenceId,
                                 uint32_t holdMs,
                                 SpeechBubbleTransport transport,
                                 const char** error) {
  if (sequenceId == nullptr) {
    return fail(error, "sequence_mismatch");
  }
  if (!lockState()) {
    return fail(error, "busy");
  }
  if (!active_) {
    unlockState();
    return fail(error, "no_active_sequence");
  }
  if (strcmp(sequenceId_, sequenceId) != 0) {
    unlockState();
    return fail(error, "sequence_mismatch");
  }
  if (transport_ != transport) {
    unlockState();
    return fail(error, "transport_mismatch");
  }

  const bool streamBound = audioStreamBound_;
  const uint32_t receivedBytes = streamBound ? audioController_->playbackPcmReceivedBytes() : 0;
  const uint32_t expectedBytes = expectedReceivedBytes_;
  const bool pcmMismatch = streamBound && hasLastSegmentIndex_ && receivedBytes != expectedBytes;
  holdMs_ = min<uint32_t>(holdMs, SPEECH_BUBBLE_MAX_HOLD_MS);
  endReceived_ = true;
  holdStarted_ = false;
  holdUntilMs_ = 0;
  lastProgressMs_ = millis();
  const uint32_t effectiveHoldMs = holdMs_;
  unlockState();

  if (pcmMismatch) {
    Serial.printf("[speech_bubble] end pcm mismatch sequence=%s expected_rx=%lu actual_rx=%lu\n",
                  sequenceId,
                  static_cast<unsigned long>(expectedBytes),
                  static_cast<unsigned long>(receivedBytes));
  }
  Serial.printf("[speech_bubble] end queued sequence=%s hold_ms=%lu\n",
                sequenceId,
                static_cast<unsigned long>(effectiveHoldMs));
  return true;
}

bool SpeechBubbleController::cancel(const char* sequenceId,
                                    SpeechBubbleTransport transport,
                                    const char** error) {
  if (!lockState()) {
    return fail(error, "busy");
  }
  if (!active_) {
    unlockState();
    return true;
  }
  if (sequenceId == nullptr || strcmp(sequenceId_, sequenceId) != 0) {
    unlockState();
    return fail(error, "sequence_mismatch");
  }
  if (transport_ != transport) {
    unlockState();
    return fail(error, "transport_mismatch");
  }
  const bool hadPresentation = resetStateLocked();
  unlockState();
  if (hadPresentation) {
    Serial.println("[speech_bubble] reset reason=cancel");
  }
  return true;
}

void SpeechBubbleController::onPcmReceived(SpeechBubbleTransport transport) {
  // This runs immediately before PCM ingestion. Never wait behind display work.
  if (!tryLockState()) {
    return;
  }
  const bool mismatch = active_ && transport_ != transport;
  const bool hadPresentation = mismatch ? resetStateLocked() : false;
  unlockState();
  if (mismatch && hadPresentation) {
    Serial.println("[speech_bubble] reset reason=pcm_transport_mismatch");
  }
}

void SpeechBubbleController::update(unsigned long now) {
  if (audioController_ == nullptr || faceController_ == nullptr) {
    return;
  }

  applyPendingFaceClear();

  enum class UpdateAction : uint8_t {
    None,
    Render,
    Reset,
  };

  const ChanState audioState = audioController_->state();
  const bool draining = audioController_->isPlaybackDraining();
  const bool playbackStarted = audioController_->hasPlaybackStarted() || draining;
  const uint32_t streamId = audioController_->playbackStreamId();
  const uint32_t receivedBytes = audioController_->playbackPcmReceivedBytes();
  const uint32_t playbackBytes = audioController_->playbackPcmDequeuedBytes();

  UpdateAction action = UpdateAction::None;
  CueEntry renderCue;
  uint32_t renderGeneration = 0;
  uint8_t renderQueueHead = 0;
  char sequenceForLog[SPEECH_BUBBLE_MAX_SEQUENCE_ID_BYTES + 1] = {};
  const char* resetReason = nullptr;
  bool boundPendingSequence = false;
  bool holdStarted = false;
  uint32_t effectiveHoldMs = 0;
  bool endWasReceived = false;

  if (!lockState()) {
    return;
  }
  if (!active_) {
    unlockState();
    return;
  }

  if (awaitingSpeaking_) {
    if (audioState == ChanState::Speaking) {
      bindAudioStreamLocked(now);
      boundPendingSequence = true;
    } else if (static_cast<long>(now - preSpeakingDeadlineMs_) >= 0) {
      resetReason = "pre_speaking_timeout";
      resetStateLocked();
      action = UpdateAction::Reset;
    } else {
      unlockState();
      return;
    }
  }

  if (action == UpdateAction::None &&
      (!audioStreamBound_ || streamId != audioStreamId_)) {
    resetReason = "audio_stream_changed";
    resetStateLocked();
    action = UpdateAction::Reset;
  }

  if (action == UpdateAction::None) {
    if (receivedBytes != lastProgressReceivedBytes_ ||
        playbackBytes != lastProgressDequeuedBytes_) {
      lastProgressReceivedBytes_ = receivedBytes;
      lastProgressDequeuedBytes_ = playbackBytes;
      lastProgressMs_ = now;
    } else if (!holdStarted_ &&
               elapsedAtLeast(now, lastProgressMs_, SPEECH_BUBBLE_STALL_TIMEOUT_MS)) {
      resetReason = "audio_stalled";
      resetStateLocked();
      action = UpdateAction::Reset;
    }
  }

  if (action == UpdateAction::None && queueCount_ > 0) {
    CueEntry& cueAtHead = queue_[queueHead_];
    const bool retryReady = renderRetryNotBeforeMs_ == 0 ||
                            static_cast<long>(now - renderRetryNotBeforeMs_) >= 0;
    // The user permits subtitles to lead audio. Once any matching PCM has
    // arrived, rendering may begin without waiting for the speaker prebuffer.
    const bool presentationReady = playbackStarted ||
                                   playbackBytes > 0 ||
                                   receivedBytes > cueAtHead.playbackStartBytes;
    if (retryReady && presentationReady && playbackBytes >= cueAtHead.playbackStartBytes) {
      renderCue = cueAtHead;
      renderGeneration = generation_;
      renderQueueHead = queueHead_;
      strlcpy(sequenceForLog, sequenceId_, sizeof(sequenceForLog));
      action = UpdateAction::Render;
    }
  }

  if (action == UpdateAction::None) {
    const bool playbackFinished = audioState != ChanState::Speaking && !draining;
    const bool queueBoundaryGraceActive = queueCount_ > 0 &&
                                          !elapsedAtLeast(now,
                                                          lastProgressMs_,
                                                          SPEECH_BUBBLE_PRE_SPEAKING_HOLD_MS);
    if (playbackFinished && !holdStarted_ && !queueBoundaryGraceActive) {
      clearQueue();
      holdStarted_ = true;
      effectiveHoldMs = endReceived_ ? holdMs_ : SPEECH_BUBBLE_DEFAULT_HOLD_MS;
      holdUntilMs_ = now + effectiveHoldMs;
      holdStarted = true;
      endWasReceived = endReceived_;
      strlcpy(sequenceForLog, sequenceId_, sizeof(sequenceForLog));
      if (effectiveHoldMs == 0) {
        resetReason = "hold_zero";
        resetStateLocked();
        action = UpdateAction::Reset;
      }
    }
  }

  if (action == UpdateAction::None &&
      holdStarted_ && static_cast<long>(now - holdUntilMs_) >= 0) {
    resetReason = "hold_elapsed";
    resetStateLocked();
    action = UpdateAction::Reset;
  }
  unlockState();

  if (boundPendingSequence) {
    Serial.printf("[speech_bubble] pending cue bound stream=%lu\n",
                  static_cast<unsigned long>(audioController_->playbackStreamId()));
  }
  if (holdStarted) {
    Serial.printf("[speech_bubble] hold started sequence=%s hold_ms=%lu end_received=%d\n",
                  sequenceForLog,
                  static_cast<unsigned long>(effectiveHoldMs),
                  endWasReceived ? 1 : 0);
  }
  if (action == UpdateAction::Reset) {
    Serial.printf("[speech_bubble] reset reason=%s\n",
                  resetReason != nullptr ? resetReason : "unknown");
    applyPendingFaceClear();
    return;
  }
  if (action != UpdateAction::Render) {
    return;
  }

  const uint32_t renderStartedUs = micros();
  const bool rendered = faceController_->setSpeechBubbleText(renderCue.text);
  const uint32_t renderUs = micros() - renderStartedUs;
  bool committed = false;
  bool stale = false;
  if (lockState()) {
    const bool stillHead = active_ &&
                           generation_ == renderGeneration &&
                           queueCount_ > 0 &&
                           queueHead_ == renderQueueHead &&
                           queue_[queueHead_].segmentIndex == renderCue.segmentIndex;
    if (stillHead && rendered) {
      queueHead_ = static_cast<uint8_t>((queueHead_ + 1) % SPEECH_BUBBLE_MAX_QUEUED_CUES);
      --queueCount_;
      renderRetryNotBeforeMs_ = 0;
      committed = true;
    } else if (stillHead) {
      renderRetryNotBeforeMs_ = now + SPEECH_BUBBLE_RENDER_RETRY_MS;
    } else {
      faceClearPending_ = true;
      stale = true;
    }
    unlockState();
  }

  if (committed) {
    Serial.printf("[speech_bubble] cue shown sequence=%s segment=%lu cursor=%lu render_us=%lu\n",
                  sequenceForLog,
                  static_cast<unsigned long>(renderCue.segmentIndex),
                  static_cast<unsigned long>(playbackBytes),
                  static_cast<unsigned long>(renderUs));
  } else if (!rendered) {
    Serial.printf("[speech_bubble] render failed sequence=%s segment=%lu retry_ms=%u render_us=%lu\n",
                  sequenceForLog,
                  static_cast<unsigned long>(renderCue.segmentIndex),
                  SPEECH_BUBBLE_RENDER_RETRY_MS,
                  static_cast<unsigned long>(renderUs));
  } else if (stale) {
    Serial.printf("[speech_bubble] render discarded stale sequence=%s segment=%lu\n",
                  sequenceForLog,
                  static_cast<unsigned long>(renderCue.segmentIndex));
    applyPendingFaceClear();
  }
}

void SpeechBubbleController::reset(const char* reason) {
  if (!lockState()) {
    return;
  }
  const bool hadPresentation = resetStateLocked();
  unlockState();
  if (hadPresentation) {
    Serial.printf("[speech_bubble] reset reason=%s\n",
                  reason != nullptr ? reason : "unknown");
  }
}

bool SpeechBubbleController::active() const {
  if (!lockState()) {
    return false;
  }
  const bool value = active_;
  unlockState();
  return value;
}

bool SpeechBubbleController::activeForTransport(SpeechBubbleTransport transport) const {
  if (!lockState()) {
    return false;
  }
  const bool value = active_ && transport_ == transport;
  unlockState();
  return value;
}

bool SpeechBubbleController::fail(const char** error, const char* value) const {
  if (error != nullptr) {
    *error = value;
  }
  return false;
}

bool SpeechBubbleController::lockState() const {
  return stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, portMAX_DELAY) == pdTRUE;
}

bool SpeechBubbleController::tryLockState() const {
  return stateMutex_ == nullptr || xSemaphoreTake(stateMutex_, 0) == pdTRUE;
}

void SpeechBubbleController::unlockState() const {
  if (stateMutex_ != nullptr) {
    xSemaphoreGive(stateMutex_);
  }
}

void SpeechBubbleController::clearQueue() {
  queueHead_ = 0;
  queueCount_ = 0;
}

bool SpeechBubbleController::resetStateLocked() {
  const bool hadPresentation = active_;
  clearQueue();
  sequenceId_[0] = '\0';
  transport_ = SpeechBubbleTransport::None;
  audioStreamId_ = 0;
  ++generation_;
  if (generation_ == 0) {
    ++generation_;
  }
  expectedReceivedBytes_ = 0;
  lastSegmentIndex_ = 0;
  hasLastSegmentIndex_ = false;
  active_ = false;
  audioStreamBound_ = false;
  awaitingSpeaking_ = false;
  faceClearPending_ = true;
  endReceived_ = false;
  holdStarted_ = false;
  holdMs_ = SPEECH_BUBBLE_DEFAULT_HOLD_MS;
  holdUntilMs_ = 0;
  lastProgressReceivedBytes_ = 0;
  lastProgressDequeuedBytes_ = 0;
  lastProgressMs_ = 0;
  preSpeakingDeadlineMs_ = 0;
  renderRetryNotBeforeMs_ = 0;
  return hadPresentation;
}

void SpeechBubbleController::startSequenceLocked(const char* sequenceId,
                                                 SpeechBubbleTransport transport,
                                                 bool bindAudioStream,
                                                 unsigned long now) {
  resetStateLocked();
  strlcpy(sequenceId_, sequenceId, sizeof(sequenceId_));
  transport_ = transport;
  active_ = true;
  lastProgressMs_ = now;
  if (bindAudioStream) {
    bindAudioStreamLocked(now);
  } else {
    audioStreamBound_ = false;
    awaitingSpeaking_ = true;
    preSpeakingDeadlineMs_ = now + SPEECH_BUBBLE_PRE_SPEAKING_HOLD_MS;
  }
}

void SpeechBubbleController::bindAudioStreamLocked(unsigned long now) {
  audioStreamId_ = audioController_->playbackStreamId();
  const uint32_t receivedBytes = audioController_->playbackPcmReceivedBytes();
  const uint32_t acceptedBytes = audioController_->playbackPcmAcceptedBytes();
  const uint32_t playbackBytes = audioController_->playbackPcmDequeuedBytes();

  if (queueCount_ > 0) {
    uint32_t playbackOffset = receivedBytes > 0 ? 0 : acceptedBytes;
    uint32_t expectedBytes = playbackOffset;
    for (uint8_t index = 0; index < queueCount_; ++index) {
      CueEntry& cue = queue_[(queueHead_ + index) % SPEECH_BUBBLE_MAX_QUEUED_CUES];
      cue.playbackStartBytes = playbackOffset;
      playbackOffset += cue.pcmBytes;
      expectedBytes += cue.pcmBytes;
    }
    expectedReceivedBytes_ = max<uint32_t>(receivedBytes, expectedBytes);
  } else {
    expectedReceivedBytes_ = receivedBytes;
  }

  audioStreamBound_ = true;
  awaitingSpeaking_ = false;
  preSpeakingDeadlineMs_ = 0;
  lastProgressReceivedBytes_ = receivedBytes;
  lastProgressDequeuedBytes_ = playbackBytes;
  lastProgressMs_ = now;
}

void SpeechBubbleController::applyPendingFaceClear() {
  bool shouldClear = false;
  if (lockState()) {
    shouldClear = faceClearPending_;
    faceClearPending_ = false;
    unlockState();
  }
  if (shouldClear && faceController_ != nullptr) {
    faceController_->clearSpeechBubble();
  }
}

const char* SpeechBubbleController::transportName(SpeechBubbleTransport transport) const {
  switch (transport) {
    case SpeechBubbleTransport::WebSocket:
      return "websocket";
    case SpeechBubbleTransport::UsbSerial:
      return "usb_serial";
    case SpeechBubbleTransport::None:
    default:
      return "none";
  }
}
