#include "AudioController.h"

#include <cstring>

void AudioController::begin(WebSocketServerController* wsServer) {
  wsServer_ = wsServer;
  rxCapacity_ = AUDIO_RX_RING_BYTES;
  rxRing_ = static_cast<uint8_t*>(heap_caps_malloc(rxCapacity_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (rxRing_ == nullptr) {
    rxCapacity_ = AUDIO_CHUNK_BYTES * 128;
    rxRing_ = static_cast<uint8_t*>(heap_caps_malloc(rxCapacity_, MALLOC_CAP_8BIT));
  }
  if (rxRing_ == nullptr) {
    rxCapacity_ = 0;
    Serial.println("[audio] failed to allocate rx ring");
  } else {
    Serial.printf("[audio] rx ring allocated: %u bytes\n", static_cast<unsigned>(rxCapacity_));
  }

  micTxQueueCapacity_ = AUDIO_MIC_TX_BUFFER_COUNT;
  micTxRing_ = static_cast<uint8_t*>(heap_caps_malloc(AUDIO_MIC_PACKET_BYTES * micTxQueueCapacity_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (micTxRing_ == nullptr) {
    micTxQueueCapacity_ = 32;
    micTxRing_ = static_cast<uint8_t*>(heap_caps_malloc(AUDIO_MIC_PACKET_BYTES * micTxQueueCapacity_, MALLOC_CAP_8BIT));
  }
  if (micTxRing_ == nullptr) {
    micTxQueueCapacity_ = 0;
    Serial.println("[audio] failed to allocate mic tx ring");
  } else {
    Serial.printf("[audio] mic tx ring allocated: %u packets, %u bytes\n",
                  static_cast<unsigned>(micTxQueueCapacity_),
                  static_cast<unsigned>(AUDIO_MIC_PACKET_BYTES * micTxQueueCapacity_));
  }

  M5.Speaker.setVolume(volume_);
  M5.Speaker.setChannelVolume(AUDIO_SPEAKER_CHANNEL, 255);
  M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
  M5.Mic.end();
  Serial.printf("[audio] ready: %d Hz, %u ms mic chunks, %u samples/chunk\n",
                AUDIO_SAMPLE_RATE,
                static_cast<unsigned>(AUDIO_CHUNK_MS),
                static_cast<unsigned>(AUDIO_CHUNK_SAMPLES));

  xTaskCreatePinnedToCore(AudioController::micCaptureTaskEntry,
                          "mic_capture",
                          AUDIO_MIC_CAPTURE_TASK_STACK_WORDS,
                          this,
                          AUDIO_MIC_CAPTURE_TASK_PRIORITY,
                          &micCaptureTaskHandle_,
                          0);
  if (micCaptureTaskHandle_ == nullptr) {
    Serial.println("[audio] failed to start mic capture task");
  }
}

void AudioController::setState(ChanState state) {
  if (state == ChanState::Speaking && state_ == ChanState::Speaking && pendingIdleAfterPlayback_) {
    pendingIdleAfterPlayback_ = false;
    idleDrainEmptySinceMs_ = 0;
    Serial.println("[audio] speaking resumed during drain");
    return;
  }

  if (state_ == state) {
    return;
  }

  if (state == ChanState::Idle && state_ == ChanState::Speaking && speakerEnabled_) {
    pendingIdleAfterPlayback_ = true;
    idleDrainEmptySinceMs_ = 0;
    Serial.printf("[audio] idle requested; draining playback buffer (%u bytes)\n", static_cast<unsigned>(rxAvailable()));
    return;
  }

  state_ = state;
  switch (state_) {
    case ChanState::Idle:
      enterIdle();
      break;
    case ChanState::Listening:
      enterListening();
      break;
    case ChanState::Speaking:
      enterSpeaking();
      break;
  }
}

ChanState AudioController::state() const {
  return state_;
}

bool AudioController::isPlaybackDraining() const {
  return pendingIdleAfterPlayback_;
}

bool AudioController::pauseMicForCapture() {
  if (!micEnabled_) {
    return false;
  }

  stopMicInput("camera capture");
  Serial.println("[audio] mic paused for camera capture");
  return true;
}

void AudioController::resumeMicAfterCapture(bool wasPaused) {
  if (!wasPaused || state_ != ChanState::Listening || micEnabled_) {
    return;
  }

  startMicInput();
  Serial.println("[audio] mic resumed after camera capture");
}

void AudioController::deferNextSpeakerStartUntil(unsigned long timestampMs) {
  speakerStartNotBeforeMs_ = timestampMs;
}

void AudioController::deferMicCaptureUntil(unsigned long timestampMs) {
  micCaptureNotBeforeMs_ = timestampMs;
  if (nextMicSendDueMs_ < timestampMs) {
    nextMicSendDueMs_ = timestampMs;
  }
  micCaptureStartPending_ = true;
  ++micPausedByState_;
  ++micPauseWsSettle_;
}

void AudioController::setVolume(uint8_t volume) {
  volume_ = volume;
  M5.Speaker.setVolume(volume_);
  if (speakerEnabled_) {
    M5.Speaker.setChannelVolume(AUDIO_SPEAKER_CHANNEL, 255);
  }
  Serial.printf("[audio] volume=%u\n", volume_);
}

uint8_t AudioController::volume() const {
  return volume_;
}

void AudioController::setMicMuted(bool muted) {
  if (micMuted_ == muted) {
    return;
  }

  micMuted_ = muted;
  if (micMuted_) {
    stopMicInput("muted");
  } else if (state_ == ChanState::Listening && !micEnabled_) {
    startMicInput();
  }
  Serial.printf("[audio] mic_muted=%d\n", micMuted_);
}

bool AudioController::micMuted() const {
  return micMuted_;
}

bool AudioController::isMicStreaming() const {
  return state_ == ChanState::Listening && micEnabled_ && !micMuted_ && wsServer_ != nullptr && wsServer_->hasClient();
}

void AudioController::onBinaryReceived(uint8_t* payload, size_t length) {
  if (state_ != ChanState::Speaking || (!speakerEnabled_ && !speakerStartPending_)) {
    Serial.printf("[audio] dropped %u bytes; not speaking\n", static_cast<unsigned>(length));
    return;
  }

  if ((length & 1) != 0) {
    Serial.printf("[audio] dropped odd-sized pcm packet: %u bytes\n", static_cast<unsigned>(length));
    return;
  }

  size_t written = appendRxBytes(payload, length);
  if (written > 0 && pendingIdleAfterPlayback_) {
    idleDrainEmptySinceMs_ = 0;
  }
  if (written < length) {
    Serial.printf("[audio] rx ring overflow; dropped %u bytes, buffered=%u/%u\n",
                  static_cast<unsigned>(length - written),
                  static_cast<unsigned>(rxAvailable()),
                  static_cast<unsigned>(rxCapacity()));
  }
}

void AudioController::update(unsigned long now) {
  if (state_ == ChanState::Speaking) {
    updateSpeakerPlayback();
  } else if (state_ == ChanState::Listening) {
    updateMicDiagnostics(now);
    updateMicSend();
  }
}

void AudioController::enterIdle() {
  if (micEnabled_) {
    stopMicInput("idle");
  }
  if (speakerEnabled_) {
    M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
    M5.Speaker.end();
    speakerEnabled_ = false;
  }
  speakerStartPending_ = false;
  playbackStarted_ = false;
  pendingIdleAfterPlayback_ = false;
  idleDrainEmptySinceMs_ = 0;
  clearRxRing();
  Serial.println("[audio] idle: mic off, speaker stopped");
}

void AudioController::enterListening() {
  if (speakerEnabled_) {
    M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
    M5.Speaker.end();
    speakerEnabled_ = false;
  }
  speakerStartPending_ = false;
  playbackStarted_ = false;
  pendingIdleAfterPlayback_ = false;
  idleDrainEmptySinceMs_ = 0;
  clearRxRing();

  if (micMuted_) {
    stopMicInput("muted");
    Serial.println("[audio] listening: mic muted");
  } else {
    startMicInput();
  }
}

void AudioController::enterSpeaking() {
  if (micEnabled_) {
    Serial.println("[audio] stopping mic before speaker start");
    stopMicInput("speaker start");
  }

  playbackStarted_ = false;
  pendingIdleAfterPlayback_ = false;
  idleDrainEmptySinceMs_ = 0;
  clearRxRing();

  const unsigned long now = millis();
  if (speakerStartNotBeforeMs_ != 0 && static_cast<long>(speakerStartNotBeforeMs_ - now) > 0) {
    speakerStartPending_ = true;
    speakerEnabled_ = false;
    Serial.printf("[audio] speaking: speaker start deferred by %ld ms\n",
                  static_cast<long>(speakerStartNotBeforeMs_ - now));
    return;
  }

  startSpeakerIfDue();
}

void AudioController::startSpeakerIfDue() {
  if (speakerEnabled_) {
    return;
  }

  const unsigned long now = millis();
  if (speakerStartNotBeforeMs_ != 0 && static_cast<long>(speakerStartNotBeforeMs_ - now) > 0) {
    speakerStartPending_ = true;
    return;
  }

  speakerStartNotBeforeMs_ = 0;
  speakerStartPending_ = false;
  speakerEnabled_ = M5.Speaker.begin();
  if (speakerEnabled_) {
    M5.Speaker.setVolume(volume_);
    M5.Speaker.setChannelVolume(AUDIO_SPEAKER_CHANNEL, 255);
    playbackStarted_ = false;
    idleDrainEmptySinceMs_ = 0;
    Serial.println("[audio] speaking: speaker on");
  } else {
    Serial.println("[audio] failed to start speaker");
  }
}

void AudioController::startMicInput() {
  resetMicBuffers();
  auto micCfg = M5.Mic.config();
  micCfg.sample_rate = AUDIO_SAMPLE_RATE;
  micCfg.magnification = AUDIO_MIC_MAGNIFICATION;
  micCfg.noise_filter_level = AUDIO_MIC_NOISE_FILTER_LEVEL;
  micCfg.over_sampling = AUDIO_MIC_OVERSAMPLING;
  M5.Mic.config(micCfg);

  micEnabled_ = M5.Mic.begin();
  if (micEnabled_) {
    M5.Mic.setSampleRate(AUDIO_SAMPLE_RATE);
    micCaptureActive_ = true;
    micCaptureStartPending_ = true;
    Serial.printf("[audio] listening: mic on, mag=%u, nf=%u, oversampling=%u\n",
                  micCfg.magnification,
                  micCfg.noise_filter_level,
                  micCfg.over_sampling);
  } else {
    Serial.println("[audio] failed to start mic");
  }
}

void AudioController::stopMicInput(const char* reason) {
  if (!micEnabled_ && !micCaptureActive_) {
    resetMicBuffers();
    return;
  }

  noteMicPause(reason);
  micCaptureActive_ = false;
  const unsigned long waitStartMs = millis();
  while (micCaptureRecording_ && millis() - waitStartMs < (AUDIO_CHUNK_MS + 40)) {
    delay(1);
  }

  if (micEnabled_) {
    M5.Mic.end();
    micEnabled_ = false;
  }
  micCaptureStartPending_ = true;
  resetMicBuffers();
  if (reason != nullptr) {
    Serial.printf("[audio] mic stopped: %s\n", reason);
  }
}

void AudioController::noteMicPause(const char* reason) {
  ++micPausedByState_;
  if (reason == nullptr) {
    ++micPauseOther_;
    return;
  }
  if (strcmp(reason, "idle") == 0) {
    ++micPauseIdle_;
  } else if (strcmp(reason, "speaker start") == 0) {
    ++micPauseSpeaking_;
  } else if (strcmp(reason, "muted") == 0) {
    ++micPauseMuted_;
  } else if (strcmp(reason, "camera capture") == 0) {
    ++micPauseCamera_;
  } else {
    ++micPauseOther_;
  }
}

void AudioController::updateMicSend() {
  if (micMuted_ || !micEnabled_ || wsServer_ == nullptr || !wsServer_->hasClient()) {
    return;
  }

  while (copyNextMicPacket(micSendScratch_)) {
    const unsigned long now = millis();
    processMicChunk(reinterpret_cast<int16_t*>(micSendScratch_ + AUDIO_MIC_PACKET_HEADER_BYTES), AUDIO_CHUNK_SAMPLES);
    if (wsServer_->sendBinary(micSendScratch_, AUDIO_MIC_PACKET_BYTES)) {
      ++micTxStatsChunks_;
      micTxStatsBytes_ += AUDIO_CHUNK_BYTES;
      if (lastMicTxChunkMs_ != 0) {
        const unsigned long interval = now - lastMicTxChunkMs_;
        if (interval > micTxMaxIntervalMs_) {
          micTxMaxIntervalMs_ = interval;
        }
      }
      lastMicTxChunkMs_ = now;
    } else {
      ++micTxStatsDroppedChunks_;
      ++micTxStatsSendFail_;
      break;
    }
  }
}

void AudioController::writeMicPacket(uint8_t* packet, const int16_t* samples, size_t sampleCount, unsigned long timestampMs, uint16_t flags) {
  packet[0] = 'M';
  packet[1] = 'I';
  packet[2] = 'C';
  packet[3] = '1';

  const uint32_t seq = micTxSeq_++;
  packet[4] = static_cast<uint8_t>(seq & 0xff);
  packet[5] = static_cast<uint8_t>((seq >> 8) & 0xff);
  packet[6] = static_cast<uint8_t>((seq >> 16) & 0xff);
  packet[7] = static_cast<uint8_t>((seq >> 24) & 0xff);

  const uint32_t timestamp = static_cast<uint32_t>(timestampMs);
  packet[8] = static_cast<uint8_t>(timestamp & 0xff);
  packet[9] = static_cast<uint8_t>((timestamp >> 8) & 0xff);
  packet[10] = static_cast<uint8_t>((timestamp >> 16) & 0xff);
  packet[11] = static_cast<uint8_t>((timestamp >> 24) & 0xff);

  const uint16_t samples16 = static_cast<uint16_t>(sampleCount);
  packet[12] = static_cast<uint8_t>(samples16 & 0xff);
  packet[13] = static_cast<uint8_t>((samples16 >> 8) & 0xff);
  packet[14] = static_cast<uint8_t>(flags & 0xff);
  packet[15] = static_cast<uint8_t>((flags >> 8) & 0xff);

  memcpy(packet + AUDIO_MIC_PACKET_HEADER_BYTES, samples, sampleCount * sizeof(int16_t));
}

void AudioController::micCaptureTaskEntry(void* context) {
  auto* controller = static_cast<AudioController*>(context);
  if (controller != nullptr) {
    controller->micCaptureTaskLoop();
  }
  vTaskDelete(nullptr);
}

void AudioController::micCaptureTaskLoop() {
  int16_t captureBuffer[AUDIO_CHUNK_SAMPLES] = {};

  for (;;) {
    if (!micCaptureActive_) {
      micCaptureRecording_ = false;
      micCaptureStartPending_ = true;
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    const unsigned long now = millis();
    if (micCaptureNotBeforeMs_ != 0 && static_cast<long>(micCaptureNotBeforeMs_ - now) > 0) {
      micCaptureStartPending_ = true;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    micCaptureNotBeforeMs_ = 0;

    micCaptureRecording_ = true;
    const unsigned long recordStartMs = millis();
    const bool recorded = M5.Mic.record(captureBuffer, AUDIO_CHUNK_SAMPLES, AUDIO_SAMPLE_RATE, false);
    const unsigned long recordEndMs = millis();
    micCaptureRecording_ = false;

    if (!micCaptureActive_) {
      continue;
    }

    if (!recorded) {
      ++micTxStatsCaptureUnderrun_;
      ++micTxStatsDroppedChunks_;
      micCaptureTimestampMs_ = recordEndMs;
      micCaptureStartPending_ = true;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    if (recordEndMs - recordStartMs > AUDIO_CHUNK_MS + 20) {
      ++micTxStatsCaptureOverrun_;
    }

    if (!micCaptureStartPending_ && lastMicCaptureStartMs_ != 0) {
      const unsigned long startDeltaMs = recordStartMs - lastMicCaptureStartMs_;
      if (startDeltaMs > AUDIO_CHUNK_MS + AUDIO_MIC_CAPTURE_GAP_TOLERANCE_MS) {
        ++micCaptureGapCount_;
        micCaptureGapLastMs_ = startDeltaMs;
        if (startDeltaMs > micCaptureGapMaxMs_) {
          micCaptureGapMaxMs_ = startDeltaMs;
        }
        const uint32_t missed = static_cast<uint32_t>((startDeltaMs - AUDIO_CHUNK_MS) / AUDIO_CHUNK_MS);
        micTxStatsScheduleMissed_ += missed == 0 ? 1 : missed;
      }
    }
    lastMicCaptureStartMs_ = recordStartMs;

    if (micCaptureStartPending_ || micCaptureTimestampMs_ == 0) {
      micCaptureTimestampMs_ = recordStartMs;
    }

    uint16_t flags = 0;
    if (micCaptureStartPending_) {
      flags |= AUDIO_MIC_PACKET_FLAG_START;
      micCaptureStartPending_ = false;
    }

    if (wsServer_ != nullptr && wsServer_->hasClient()) {
      if (!enqueueMicPacket(captureBuffer, AUDIO_CHUNK_SAMPLES, micCaptureTimestampMs_, flags)) {
        ++micTxStatsRingOverflow_;
        ++micTxStatsSendQueueOverflow_;
        ++micTxStatsDroppedChunks_;
        micCaptureStartPending_ = true;
      }
    } else {
      clearMicPacketQueue();
      micCaptureStartPending_ = true;
    }

    micCaptureTimestampMs_ += AUDIO_CHUNK_MS;
    taskYIELD();
  }
}

bool AudioController::enqueueMicPacket(const int16_t* samples, size_t sampleCount, unsigned long timestampMs, uint16_t flags) {
  if (micTxRing_ == nullptr || micTxQueueCapacity_ == 0) {
    return false;
  }

  portENTER_CRITICAL(&micMux_);
  if (micTxQueueCount_ >= micTxQueueCapacity_) {
    portEXIT_CRITICAL(&micMux_);
    return false;
  }

  uint8_t* packet = micTxRing_ + (micTxQueueWriteIndex_ * AUDIO_MIC_PACKET_BYTES);
  writeMicPacket(packet, samples, sampleCount, timestampMs, flags);
  micTxQueueWriteIndex_ = (micTxQueueWriteIndex_ + 1) % micTxQueueCapacity_;
  ++micTxQueueCount_;
  portEXIT_CRITICAL(&micMux_);
  return true;
}

bool AudioController::copyNextMicPacket(uint8_t* out) {
  if (out == nullptr || micTxRing_ == nullptr || micTxQueueCapacity_ == 0) {
    return false;
  }

  portENTER_CRITICAL(&micMux_);
  if (micTxQueueCount_ == 0) {
    portEXIT_CRITICAL(&micMux_);
    return false;
  }

  const uint8_t* packet = micTxRing_ + (micTxQueueReadIndex_ * AUDIO_MIC_PACKET_BYTES);
  memcpy(out, packet, AUDIO_MIC_PACKET_BYTES);
  micTxQueueReadIndex_ = (micTxQueueReadIndex_ + 1) % micTxQueueCapacity_;
  --micTxQueueCount_;
  portEXIT_CRITICAL(&micMux_);
  return true;
}

void AudioController::clearMicPacketQueue() {
  portENTER_CRITICAL(&micMux_);
  micTxQueueReadIndex_ = 0;
  micTxQueueWriteIndex_ = 0;
  micTxQueueCount_ = 0;
  portEXIT_CRITICAL(&micMux_);
}

void AudioController::processMicChunk(int16_t* samples, size_t sampleCount) {
  if (sampleCount == 0) {
    return;
  }

  int64_t sum = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    sum += samples[i];
  }
  const int32_t dc = static_cast<int32_t>(sum / static_cast<int64_t>(sampleCount));

  for (size_t i = 0; i < sampleCount; ++i) {
    int32_t value = samples[i] - dc;
    value = (value * AUDIO_MIC_SOFTWARE_GAIN_Q8) >> 8;
    if (value > 32767) {
      value = 32767;
    } else if (value < -32768) {
      value = -32768;
    }

    const int32_t absValue = abs(value);
    if (absValue > micStatsPeak_) {
      micStatsPeak_ = absValue;
    }
    if (absValue >= AUDIO_MIC_CLIP_THRESHOLD) {
      ++micStatsClipCount_;
    }
    ++micStatsSamples_;
    samples[i] = static_cast<int16_t>(value);
  }

  const unsigned long now = millis();
  if (now - lastMicStatsMs_ >= 1000) {
#if AUDIO_DIAG_LOG_ENABLED
    const float clipPercent = micStatsSamples_ == 0
                                ? 0.0f
                                : (100.0f * static_cast<float>(micStatsClipCount_) / static_cast<float>(micStatsSamples_));
    Serial.printf("[audio] mic peak=%d clip=%.2f%% dc=%ld\n",
                  micStatsPeak_,
                  clipPercent,
                  static_cast<long>(dc));
#endif
    micStatsSamples_ = 0;
    micStatsClipCount_ = 0;
    micStatsPeak_ = 0;
    lastMicStatsMs_ = now;
  }
}

void AudioController::updateMicDiagnostics(unsigned long now) {
  if (lastMicTxStatsMs_ == 0) {
    lastMicTxStatsMs_ = now;
    return;
  }

  const unsigned long elapsedMs = now - lastMicTxStatsMs_;
  if (elapsedMs < AUDIO_MIC_DIAG_INTERVAL_MS) {
    return;
  }

#if AUDIO_DIAG_LOG_ENABLED
  const float scale = 1000.0f / static_cast<float>(elapsedMs);
  Serial.printf("[audio] mic tx chunks/s=%.1f bytes/s=%.0f dropped=%lu capture_overrun=%lu capture_underrun=%lu capture_gap_count=%lu capture_gap_max=%lu ms capture_gap_last=%lu ms ring_overflow=%lu send_queue_overflow=%lu ws_send_fail=%lu schedule_missed=%lu mic_paused_by_state=%lu pause_idle=%lu pause_speaking=%lu pause_muted=%lu pause_camera=%lu pause_ws_settle=%lu pause_other=%lu max_interval=%lu ms queued=%u\n",
                static_cast<float>(micTxStatsChunks_) * scale,
                static_cast<float>(micTxStatsBytes_) * scale,
                static_cast<unsigned long>(micTxStatsDroppedChunks_),
                static_cast<unsigned long>(micTxStatsCaptureOverrun_),
                static_cast<unsigned long>(micTxStatsCaptureUnderrun_),
                static_cast<unsigned long>(micCaptureGapCount_),
                static_cast<unsigned long>(micCaptureGapMaxMs_),
                static_cast<unsigned long>(micCaptureGapLastMs_),
                static_cast<unsigned long>(micTxStatsRingOverflow_),
                static_cast<unsigned long>(micTxStatsSendQueueOverflow_),
                static_cast<unsigned long>(micTxStatsSendFail_),
                static_cast<unsigned long>(micTxStatsScheduleMissed_),
                static_cast<unsigned long>(micPausedByState_),
                static_cast<unsigned long>(micPauseIdle_),
                static_cast<unsigned long>(micPauseSpeaking_),
                static_cast<unsigned long>(micPauseMuted_),
                static_cast<unsigned long>(micPauseCamera_),
                static_cast<unsigned long>(micPauseWsSettle_),
                static_cast<unsigned long>(micPauseOther_),
                static_cast<unsigned long>(micTxMaxIntervalMs_),
                static_cast<unsigned>(micTxQueueCount_));
#else
  if (micTxStatsDroppedChunks_ != 0 ||
      micTxStatsCaptureOverrun_ != 0 ||
      micTxStatsCaptureUnderrun_ != 0 ||
      micCaptureGapCount_ != 0 ||
      micTxStatsRingOverflow_ != 0 ||
      micTxStatsSendQueueOverflow_ != 0 ||
      micTxStatsSendFail_ != 0 ||
      micTxStatsScheduleMissed_ != 0) {
    Serial.printf("[audio] mic error dropped=%lu capture_overrun=%lu capture_underrun=%lu capture_gap_count=%lu ring_overflow=%lu send_queue_overflow=%lu ws_send_fail=%lu schedule_missed=%lu queued=%u\n",
                  static_cast<unsigned long>(micTxStatsDroppedChunks_),
                  static_cast<unsigned long>(micTxStatsCaptureOverrun_),
                  static_cast<unsigned long>(micTxStatsCaptureUnderrun_),
                  static_cast<unsigned long>(micCaptureGapCount_),
                  static_cast<unsigned long>(micTxStatsRingOverflow_),
                  static_cast<unsigned long>(micTxStatsSendQueueOverflow_),
                  static_cast<unsigned long>(micTxStatsSendFail_),
                  static_cast<unsigned long>(micTxStatsScheduleMissed_),
                  static_cast<unsigned>(micTxQueueCount_));
  }
#endif

  micTxStatsChunks_ = 0;
  micTxStatsBytes_ = 0;
  micTxStatsDroppedChunks_ = 0;
  micTxStatsCaptureOverrun_ = 0;
  micTxStatsCaptureUnderrun_ = 0;
  micCaptureGapCount_ = 0;
  micCaptureGapMaxMs_ = 0;
  micCaptureGapLastMs_ = 0;
  micTxStatsRingOverflow_ = 0;
  micTxStatsSendQueueOverflow_ = 0;
  micTxStatsScheduleMissed_ = 0;
  micTxStatsSendFail_ = 0;
  micPausedByState_ = 0;
  micPauseIdle_ = 0;
  micPauseSpeaking_ = 0;
  micPauseMuted_ = 0;
  micPauseCamera_ = 0;
  micPauseWsSettle_ = 0;
  micPauseOther_ = 0;
  micTxMaxIntervalMs_ = 0;
  lastMicTxStatsMs_ = now;
}

void AudioController::updateSpeakerPlayback() {
  if (speakerStartPending_) {
    startSpeakerIfDue();
  }

  if (!speakerEnabled_) {
    return;
  }

  if (!playbackStarted_) {
    if (rxAvailable() >= AUDIO_PLAYBACK_PREBUFFER_BYTES || (pendingIdleAfterPlayback_ && rxAvailable() > 0)) {
      playbackStarted_ = true;
      Serial.printf("[audio] playback started with %u bytes buffered\n", static_cast<unsigned>(rxAvailable()));
    } else if (pendingIdleAfterPlayback_ && rxAvailable() == 0 && M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) == 0) {
      const unsigned long now = millis();
      if (idleDrainEmptySinceMs_ == 0) {
        idleDrainEmptySinceMs_ = now;
        return;
      }
      if (now - idleDrainEmptySinceMs_ >= AUDIO_IDLE_DRAIN_GRACE_MS) {
        finishSpeakingPlayback();
      }
      return;
    } else {
      return;
    }
  }

  uint8_t queuedThisUpdate = 0;
  while (queuedThisUpdate < AUDIO_SPEAKER_MAX_QUEUE_PER_UPDATE &&
         M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) < AUDIO_SPEAKER_QUEUE_TARGET_CHUNKS &&
         rxAvailable() >= AUDIO_PLAYBACK_CHUNK_BYTES) {
    int16_t* buffer = speakerBuffers_[speakerBufferIndex_];
    if (!readRxBytes(reinterpret_cast<uint8_t*>(buffer), AUDIO_PLAYBACK_CHUNK_BYTES)) {
      break;
    }
    playPcmChunk(buffer, AUDIO_PLAYBACK_CHUNK_SAMPLES);
    ++queuedThisUpdate;
  }

  if (pendingIdleAfterPlayback_ && rxAvailable() > 0 &&
      queuedThisUpdate < AUDIO_SPEAKER_MAX_QUEUE_PER_UPDATE &&
      M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) < AUDIO_SPEAKER_QUEUE_TARGET_CHUNKS) {
    int16_t* buffer = speakerBuffers_[speakerBufferIndex_];
    const size_t remaining = rxAvailable();
    memset(buffer, 0, AUDIO_PLAYBACK_CHUNK_BYTES);
    readRxBytes(reinterpret_cast<uint8_t*>(buffer), remaining);
    playPcmChunk(buffer, AUDIO_PLAYBACK_CHUNK_SAMPLES);
  }

  if (pendingIdleAfterPlayback_ && rxAvailable() == 0 && M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) == 0) {
    const unsigned long now = millis();
    if (idleDrainEmptySinceMs_ == 0) {
      idleDrainEmptySinceMs_ = now;
      return;
    }
    if (now - idleDrainEmptySinceMs_ >= AUDIO_IDLE_DRAIN_GRACE_MS) {
      finishSpeakingPlayback();
    }
    return;
  }

  if (!pendingIdleAfterPlayback_ && rxAvailable() < AUDIO_PLAYBACK_CHUNK_BYTES && M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) == 0) {
    playbackStarted_ = false;
  }
}

void AudioController::finishSpeakingPlayback() {
  M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
  M5.Speaker.end();
  speakerEnabled_ = false;
  speakerStartPending_ = false;
  playbackStarted_ = false;
  pendingIdleAfterPlayback_ = false;
  idleDrainEmptySinceMs_ = 0;
  clearRxRing();
  state_ = ChanState::Idle;
  Serial.println("[audio] playback drained; speaker stopped");
}

void AudioController::playPcmChunk(const int16_t* samples, size_t sampleCount) {
  if (sampleCount == 0) {
    return;
  }

  if (M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) >= AUDIO_SPEAKER_QUEUE_TARGET_CHUNKS) {
    Serial.println("[audio] speaker queue full; dropped pcm chunk");
    return;
  }

  int16_t* buffer = speakerBuffers_[speakerBufferIndex_];
  memcpy(buffer, samples, sampleCount * sizeof(int16_t));
  speakerBufferIndex_ = (speakerBufferIndex_ + 1) % AUDIO_SPEAKER_BUFFER_COUNT;

  if (!M5.Speaker.playRaw(buffer, sampleCount, AUDIO_SAMPLE_RATE, false, 1, AUDIO_SPEAKER_CHANNEL, false)) {
    Serial.println("[audio] playRaw failed");
  }
}

void AudioController::resetMicBuffers() {
  memset(micBuffers_, 0, sizeof(micBuffers_));
  clearMicPacketQueue();
  micRecordIndex_ = 2 % AUDIO_MIC_BUFFER_COUNT;
  micSendIndex_ = 0;
  micPrimedCount_ = 0;
  nextMicSendDueMs_ = 0;
  lastMicStatsMs_ = 0;
  lastMicTxStatsMs_ = 0;
  lastMicTxChunkMs_ = 0;
  micTxMaxIntervalMs_ = 0;
  micCaptureTimestampMs_ = 0;
  lastMicCaptureStartMs_ = 0;
  micCaptureGapMaxMs_ = 0;
  micCaptureGapLastMs_ = 0;
  micCaptureStartPending_ = true;
  micStatsSamples_ = 0;
  micStatsClipCount_ = 0;
  micStatsPeak_ = 0;
  micTxStatsChunks_ = 0;
  micTxStatsBytes_ = 0;
  micTxStatsDroppedChunks_ = 0;
  micTxStatsCaptureOverrun_ = 0;
  micTxStatsCaptureUnderrun_ = 0;
  micCaptureGapCount_ = 0;
  micTxStatsRingOverflow_ = 0;
  micTxStatsSendQueueOverflow_ = 0;
  micTxStatsScheduleMissed_ = 0;
  micTxStatsSendFail_ = 0;
  micPausedByState_ = 0;
  micPauseIdle_ = 0;
  micPauseSpeaking_ = 0;
  micPauseMuted_ = 0;
  micPauseCamera_ = 0;
  micPauseWsSettle_ = 0;
  micPauseOther_ = 0;
}

void AudioController::clearRxRing() {
  rxReadIndex_ = 0;
  rxWriteIndex_ = 0;
  rxSize_ = 0;
}

size_t AudioController::rxAvailable() const {
  return rxSize_;
}

size_t AudioController::rxFree() const {
  return rxCapacity_ - rxSize_;
}

size_t AudioController::rxCapacity() const {
  return rxCapacity_;
}

size_t AudioController::appendRxBytes(const uint8_t* data, size_t length) {
  if (rxRing_ == nullptr || rxCapacity_ == 0) {
    return 0;
  }

  const size_t writable = min(length, rxFree());
  size_t copied = 0;
  while (copied < writable) {
    const size_t contiguous = min(writable - copied, rxCapacity_ - rxWriteIndex_);
    memcpy(rxRing_ + rxWriteIndex_, data + copied, contiguous);
    rxWriteIndex_ = (rxWriteIndex_ + contiguous) % rxCapacity_;
    copied += contiguous;
  }
  rxSize_ += writable;
  return writable;
}

bool AudioController::readRxBytes(uint8_t* out, size_t length) {
  if (rxRing_ == nullptr || rxAvailable() < length) {
    return false;
  }

  size_t copied = 0;
  while (copied < length) {
    const size_t contiguous = min(length - copied, rxCapacity_ - rxReadIndex_);
    memcpy(out + copied, rxRing_ + rxReadIndex_, contiguous);
    rxReadIndex_ = (rxReadIndex_ + contiguous) % rxCapacity_;
    copied += contiguous;
  }
  rxSize_ -= length;
  return true;
}
