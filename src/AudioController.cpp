#include "AudioController.h"

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

  M5.Speaker.setVolume(volume_);
  M5.Speaker.setChannelVolume(AUDIO_SPEAKER_CHANNEL, 255);
  M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
  M5.Mic.end();
  Serial.printf("[audio] ready: %d Hz, %u samples/chunk\n", AUDIO_SAMPLE_RATE, static_cast<unsigned>(AUDIO_CHUNK_SAMPLES));
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

  M5.Mic.end();
  micEnabled_ = false;
  resetMicBuffers();
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
    if (micEnabled_) {
      M5.Mic.end();
      micEnabled_ = false;
    }
    resetMicBuffers();
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
    if (now - lastMicSendMs_ < MIC_SEND_INTERVAL_MS) {
      return;
    }
    lastMicSendMs_ = now;
    updateMicCapture();
  }
}

void AudioController::enterIdle() {
  if (micEnabled_) {
    M5.Mic.end();
    micEnabled_ = false;
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
    M5.Mic.end();
    micEnabled_ = false;
    resetMicBuffers();
    Serial.println("[audio] listening: mic muted");
  } else {
    startMicInput();
  }
}

void AudioController::enterSpeaking() {
  if (micEnabled_) {
    Serial.println("[audio] stopping mic before speaker start");
    M5.Mic.end();
    micEnabled_ = false;
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
    Serial.printf("[audio] listening: mic on, mag=%u, nf=%u, oversampling=%u\n",
                  micCfg.magnification,
                  micCfg.noise_filter_level,
                  micCfg.over_sampling);
  } else {
    Serial.println("[audio] failed to start mic");
  }
}

void AudioController::updateMicCapture() {
  if (micMuted_ || !micEnabled_ || wsServer_ == nullptr || !wsServer_->hasClient()) {
    return;
  }

  int16_t* recordBuffer = micBuffers_[micRecordIndex_];
  if (!M5.Mic.record(recordBuffer, AUDIO_CHUNK_SAMPLES, AUDIO_SAMPLE_RATE, false)) {
    return;
  }

  if (micPrimedCount_ >= 2) {
    processMicChunk(micBuffers_[micSendIndex_], AUDIO_CHUNK_SAMPLES);
    wsServer_->sendBinary(reinterpret_cast<const uint8_t*>(micBuffers_[micSendIndex_]), AUDIO_CHUNK_BYTES);
  } else {
    ++micPrimedCount_;
  }

  micRecordIndex_ = (micRecordIndex_ + 1) % AUDIO_MIC_BUFFER_COUNT;
  micSendIndex_ = (micSendIndex_ + 1) % AUDIO_MIC_BUFFER_COUNT;
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
    const float clipPercent = micStatsSamples_ == 0
                                ? 0.0f
                                : (100.0f * static_cast<float>(micStatsClipCount_) / static_cast<float>(micStatsSamples_));
    Serial.printf("[audio] mic peak=%d clip=%.2f%% dc=%ld\n",
                  micStatsPeak_,
                  clipPercent,
                  static_cast<long>(dc));
    micStatsSamples_ = 0;
    micStatsClipCount_ = 0;
    micStatsPeak_ = 0;
    lastMicStatsMs_ = now;
  }
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

  while (M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) < 2 && rxAvailable() >= AUDIO_PLAYBACK_CHUNK_BYTES) {
    int16_t* buffer = speakerBuffers_[speakerBufferIndex_];
    if (!readRxBytes(reinterpret_cast<uint8_t*>(buffer), AUDIO_PLAYBACK_CHUNK_BYTES)) {
      break;
    }
    playPcmChunk(buffer, AUDIO_PLAYBACK_CHUNK_SAMPLES);
  }

  if (pendingIdleAfterPlayback_ && rxAvailable() > 0 && M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) < 2) {
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

  if (M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL) >= 2) {
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
  micRecordIndex_ = 2 % AUDIO_MIC_BUFFER_COUNT;
  micSendIndex_ = 0;
  micPrimedCount_ = 0;
  lastMicSendMs_ = 0;
  lastMicStatsMs_ = 0;
  micStatsSamples_ = 0;
  micStatsClipCount_ = 0;
  micStatsPeak_ = 0;
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
