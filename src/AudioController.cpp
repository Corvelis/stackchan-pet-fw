#include "AudioController.h"

#include <cstring>
#include <math.h>

namespace {
const char* audioStateName(ChanState state) {
  switch (state) {
    case ChanState::Idle:
      return "Idle";
    case ChanState::Listening:
      return "Listening";
    case ChanState::Speaking:
      return "Speaking";
  }
  return "Unknown";
}

#if STACKCHAN_DEVICE_STOPWATCH
constexpr uint8_t kStopwatchM5Ioe1Addr = 0x4F;
constexpr uint8_t kStopwatchEs8311Addr = 0x18;
constexpr uint32_t kStopwatchAudioI2cFreq = 100000;
constexpr uint8_t kStopwatchAudioPowerMask = 0b00000100;
constexpr uint8_t kStopwatchPaMask = 0b00000010;

uint8_t readStopwatchAudioReg(uint8_t addr, uint8_t reg) {
  return M5.In_I2C.readRegister8(addr, reg, kStopwatchAudioI2cFreq);
}

bool writeStopwatchAudioReg(uint8_t addr, uint8_t reg, uint8_t value, const char* label) {
  bool ok = false;
  for (uint8_t attempt = 0; attempt < 4 && !ok; ++attempt) {
    ok = M5.In_I2C.writeRegister8(addr, reg, value, kStopwatchAudioI2cFreq);
    if (!ok) {
      delay(1);
    }
  }
  const uint8_t actual = readStopwatchAudioReg(addr, reg);
  if (!ok || actual != value) {
    Serial.printf("[audio] stopwatch %s reg=0x%02x write=0x%02x ok=%d read=0x%02x\n",
                  label,
                  reg,
                  value,
                  ok ? 1 : 0,
                  actual);
  }
  return ok && actual == value;
}

bool setStopwatchAudioBit(uint8_t reg, uint8_t mask, bool enabled, const char* label) {
  bool ok = false;
  for (uint8_t attempt = 0; attempt < 4 && !ok; ++attempt) {
    ok = enabled
      ? M5.In_I2C.bitOn(kStopwatchM5Ioe1Addr, reg, mask, kStopwatchAudioI2cFreq)
      : M5.In_I2C.bitOff(kStopwatchM5Ioe1Addr, reg, mask, kStopwatchAudioI2cFreq);
    if (!ok) {
      delay(1);
    }
  }
  const uint8_t actual = readStopwatchAudioReg(kStopwatchM5Ioe1Addr, reg);
  const bool bitOk = enabled ? ((actual & mask) == mask) : ((actual & mask) == 0);
  if (!ok || !bitOk) {
    Serial.printf("[audio] stopwatch %s reg=0x%02x mask=0x%02x enabled=%d ok=%d read=0x%02x\n",
                  label,
                  reg,
                  mask,
                  enabled ? 1 : 0,
                  ok ? 1 : 0,
                  actual);
  }
  return ok && bitOk;
}

bool configureStopwatchSpeakerHardware(bool enabled, const char* reason) {
  m5gfx::i2c::i2c_temporary_switcher_t backupI2c(1, GPIO_NUM_47, GPIO_NUM_48);
  bool ok = true;

  ok &= writeStopwatchAudioReg(kStopwatchM5Ioe1Addr, 0x23, 0x00, "ioe-i2c");
  ok &= setStopwatchAudioBit(0x13, kStopwatchAudioPowerMask, false, "ioe-g3-pushpull");
  ok &= setStopwatchAudioBit(0x14, kStopwatchPaMask, false, "ioe-g10-pushpull");
  ok &= setStopwatchAudioBit(0x03, kStopwatchAudioPowerMask, true, "ioe-g3-output");
  ok &= setStopwatchAudioBit(0x04, kStopwatchPaMask, true, "ioe-g10-output");

  if (enabled) {
    ok &= setStopwatchAudioBit(0x05, kStopwatchAudioPowerMask, true, "ioe-audio-power");
    delay(20);

    static constexpr uint8_t codecRegs[][2] = {
      {0x00, 0x80}, // reset / CSM power on
      {0x01, 0xB5}, // MCLK = BCLK
      {0x02, 0x18}, // MULT_PRE = 3
      {0x0D, 0x01}, // power up analog circuitry
      {0x12, 0x00}, // power up DAC
      {0x13, 0x10}, // enable headphone/output driver
      {0x32, 0xDF}, // DAC volume, louder StopWatch output while staying below full scale
      {0x37, 0x08}, // bypass DAC equalizer
    };
    for (const auto& reg : codecRegs) {
      ok &= writeStopwatchAudioReg(kStopwatchEs8311Addr, reg[0], reg[1], "es8311");
    }

    ok &= setStopwatchAudioBit(0x06, kStopwatchPaMask, true, "ioe-pa");
  } else {
    ok &= setStopwatchAudioBit(0x06, kStopwatchPaMask, false, "ioe-pa");
    ok &= setStopwatchAudioBit(0x05, kStopwatchAudioPowerMask, false, "ioe-audio-power");
  }

  const uint8_t ioe05 = readStopwatchAudioReg(kStopwatchM5Ioe1Addr, 0x05);
  const uint8_t ioe06 = readStopwatchAudioReg(kStopwatchM5Ioe1Addr, 0x06);
  const uint8_t es00 = readStopwatchAudioReg(kStopwatchEs8311Addr, 0x00);
  const uint8_t es0d = readStopwatchAudioReg(kStopwatchEs8311Addr, 0x0D);
  const uint8_t es12 = readStopwatchAudioReg(kStopwatchEs8311Addr, 0x12);
  const uint8_t es13 = readStopwatchAudioReg(kStopwatchEs8311Addr, 0x13);
  const uint8_t es32 = readStopwatchAudioReg(kStopwatchEs8311Addr, 0x32);
  backupI2c.restore();

  Serial.printf("[audio] stopwatch speaker hw reason=%s enabled=%d ok=%d ioe05=0x%02x ioe06=0x%02x es00=0x%02x es0d=0x%02x es12=0x%02x es13=0x%02x es32=0x%02x\n",
                reason == nullptr ? "" : reason,
                enabled ? 1 : 0,
                ok ? 1 : 0,
                ioe05,
                ioe06,
                es00,
                es0d,
                es12,
                es13,
                es32);
  return ok;
}
#endif

#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_HAS_ATOMIC_ECHO_BASE
constexpr uint8_t kAtomicVoiceEs8311Addr = 0x18;
constexpr uint8_t kAtomicVoicePi4ioeAddr = 0x43;
constexpr uint32_t kAtomicVoiceI2cFreq = 100000;

struct CodecRegValue {
  uint8_t reg;
  uint8_t value;
};

bool writeAtomicVoiceCodecReg(uint8_t reg, uint8_t value) {
  bool ok = false;
  for (uint8_t attempt = 0; attempt < 4 && !ok; ++attempt) {
    ok = M5.In_I2C.writeRegister8(kAtomicVoiceEs8311Addr, reg, value, kAtomicVoiceI2cFreq);
    if (!ok) {
      delay(1);
    }
  }
  return ok;
}

bool writeAtomicVoicePi4ioeReg(uint8_t reg, uint8_t value) {
  bool ok = false;
  for (uint8_t attempt = 0; attempt < 4 && !ok; ++attempt) {
    ok = M5.In_I2C.writeRegister8(kAtomicVoicePi4ioeAddr, reg, value, 400000);
    if (!ok) {
      delay(1);
    }
  }
  return ok;
}

bool configureAtomicVoiceMicCodec(const char* label, const MicDiagnosticConfig* diagConfig = nullptr) {
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  m5gfx::i2c::i2c_temporary_switcher_t i2cSwitch(1, GPIO_NUM_38, GPIO_NUM_39);
#endif
  bool ok = true;
  ok = writeAtomicVoicePi4ioeReg(0x03, 0xFF) && ok;
  ok = writeAtomicVoicePi4ioeReg(0x05, 0xFF) && ok;
  ok = writeAtomicVoicePi4ioeReg(0x07, 0x00) && ok;
  ok = writeAtomicVoicePi4ioeReg(0x0B, 0x00) && ok;

  static constexpr CodecRegValue regs[] = {
    {0x00, 0x1F},
    {0x00, 0x00},
    {0x01, 0xBF},
    {0x02, 0x18},
    {0x03, 0x10},
    {0x04, 0x10},
    {0x05, 0x00},
    {0x06, 0x03},
    {0x07, 0x00},
    {0x08, 0xFF},
    {0x09, 0x0C},
    {0x0A, 0x0C},
    {0x14, 0x5A},
    {0x16, 0x07},
    {0x17, 0xFF},
    {0x32, 0xBF},
    {0x0D, 0x01},
    {0x0E, 0x02},
    {0x12, 0x00},
    {0x13, 0x10},
    {0x1C, 0x6A},
    {0x37, 0x08},
    {0x00, 0x80},
  };

  for (const auto& item : regs) {
    ok = writeAtomicVoiceCodecReg(item.reg, item.value) && ok;
    if (item.reg == 0x00) {
      delay(2);
    }
  }
  if (diagConfig != nullptr) {
    if (diagConfig->codecReg14 >= 0) {
      ok = writeAtomicVoiceCodecReg(0x14, static_cast<uint8_t>(diagConfig->codecReg14)) && ok;
    }
    if (diagConfig->codecReg16 >= 0) {
      ok = writeAtomicVoiceCodecReg(0x16, static_cast<uint8_t>(diagConfig->codecReg16)) && ok;
    }
    if (diagConfig->codecReg17 >= 0) {
      ok = writeAtomicVoiceCodecReg(0x17, static_cast<uint8_t>(diagConfig->codecReg17)) && ok;
    }
  }

  const uint8_t chipId1 = M5.In_I2C.readRegister8(kAtomicVoiceEs8311Addr, 0xFD, kAtomicVoiceI2cFreq);
  const uint8_t chipId2 = M5.In_I2C.readRegister8(kAtomicVoiceEs8311Addr, 0xFE, kAtomicVoiceI2cFreq);
  const uint8_t reg14 = M5.In_I2C.readRegister8(kAtomicVoiceEs8311Addr, 0x14, kAtomicVoiceI2cFreq);
  Serial.printf("[audio] atomic voice codec %s ok=%d id=%02x%02x reg14=0x%02x\n",
                label == nullptr ? "" : label,
                ok ? 1 : 0,
                chipId1,
                chipId2,
                reg14);
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  i2cSwitch.restore();
#endif
  return ok;
}
#else
bool configureAtomicVoiceMicCodec(const char*, const MicDiagnosticConfig* = nullptr) {
  return true;
}
#endif
}

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

void AudioController::setMicPacketSender(MicPacketSender sender, void* context) {
  micPacketSender_ = sender;
  micPacketSenderContext_ = context;
}

void AudioController::setUsbSerialClientConnected(bool connected) {
  usbSerialClientConnected_ = connected;
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
    playbackDiagIdleRequests_++;
    playbackDiagLastIdleRequestBufferedBytes_ = rxAvailable();
    Serial.printf("[audio] idle requested; draining playback buffer (%u bytes)\n", static_cast<unsigned>(playbackDiagLastIdleRequestBufferedBytes_));
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

bool AudioController::hasPlaybackStarted() const {
  return state_ == ChanState::Speaking && playbackStarted_;
}

AudioPlaybackDiagnostic AudioController::playbackDiagnostic() const {
  AudioPlaybackDiagnostic diag;
  diag.state = state_;
  diag.draining = pendingIdleAfterPlayback_;
  diag.playbackStarted = playbackStarted_;
  diag.speakerEnabled = speakerEnabled_;
  diag.speakerStartPending = speakerStartPending_;
  diag.rxAvailable = rxAvailable();
  diag.rxCapacity = rxCapacity();
  diag.pcmFramesReceived = playbackDiagPcmFramesReceived_;
  diag.pcmBytesReceived = playbackDiagPcmBytesReceived_;
  diag.pcmBytesAccepted = playbackDiagPcmBytesAccepted_;
  diag.pcmBytesDropped = playbackDiagPcmBytesDropped_;
  diag.rxOverflowEvents = playbackDiagRxOverflowEvents_;
  diag.dropNotSpeakingEvents = playbackDiagDropNotSpeakingEvents_;
  diag.dropOddSizeEvents = playbackDiagDropOddSizeEvents_;
  diag.idleRequests = playbackDiagIdleRequests_;
  diag.playbackStarts = playbackDiagStarts_;
  diag.playbackFinishes = playbackDiagFinishes_;
  diag.underflowResets = playbackDiagUnderflowResets_;
  diag.speakerQueueFullEvents = playbackDiagSpeakerQueueFullEvents_;
  diag.playRawFailEvents = playbackDiagPlayRawFailEvents_;
  diag.speakerChunksQueued = playbackDiagSpeakerChunksQueued_;
  diag.speakerBytesQueued = playbackDiagSpeakerBytesQueued_;
  diag.maxBufferedBytes = playbackDiagMaxBufferedBytes_;
  diag.lastIdleRequestBufferedBytes = playbackDiagLastIdleRequestBufferedBytes_;
  diag.lastUnderflowBufferedBytes = playbackDiagLastUnderflowBufferedBytes_;
  diag.finishBufferedBytes = playbackDiagFinishBufferedBytes_;
  diag.finishQueuedChunks = playbackDiagFinishQueuedChunks_;
  diag.lastPcmMs = playbackDiagLastPcmMs_;
  diag.lastFinishMs = playbackDiagLastFinishMs_;
  return diag;
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
  return state_ == ChanState::Listening && micEnabled_ && !micMuted_ && hasMicClient();
}

void AudioController::setRemoteVadActive(bool active) {
  remoteVadActive_ = active;
}

bool AudioController::playDiagnosticTone(uint32_t durationMs) {
  if (state_ == ChanState::Speaking || pendingIdleAfterPlayback_) {
    Serial.printf("[audio] speaker test skipped: state=%s draining=%d\n",
                  audioStateName(state_),
                  pendingIdleAfterPlayback_ ? 1 : 0);
    return false;
  }

  const bool resumeMic = state_ == ChanState::Listening && micEnabled_ && !micMuted_;
  if (resumeMic) {
    stopMicInput("speaker test");
  }

  const auto cfg = M5.Speaker.config();
  Serial.printf("[audio] speaker test cfg dout=%d bck=%d ws=%d mck=%d port=%d stereo=%d mag=%u sample=%lu volume=%u\n",
                cfg.pin_data_out,
                cfg.pin_bck,
                cfg.pin_ws,
                cfg.pin_mck,
                static_cast<int>(cfg.i2s_port),
                cfg.stereo ? 1 : 0,
                cfg.magnification,
                static_cast<unsigned long>(cfg.sample_rate),
                volume_);

  bool ok = M5.Speaker.begin();
  Serial.printf("[audio] speaker test begin=%d\n", ok ? 1 : 0);
  if (ok) {
#if STACKCHAN_DEVICE_STOPWATCH
    const bool hwOk = configureStopwatchSpeakerHardware(true, "speaker-test");
    Serial.printf("[audio] speaker test stopwatch_hw=%d\n", hwOk ? 1 : 0);
#endif
#if STACKCHAN_DEVICE_STOPWATCH
    const uint8_t testVolume = volume_ < 220 ? 220 : volume_;
#else
    const uint8_t testVolume = volume_;
#endif
    const uint32_t testDurationMs = durationMs < 1200 ? 1200 : durationMs;
    M5.Speaker.setVolume(testVolume);
    M5.Speaker.setChannelVolume(AUDIO_SPEAKER_CHANNEL, 255);
    ok = M5.Speaker.tone(880.0f, testDurationMs, AUDIO_SPEAKER_CHANNEL, true);
    Serial.printf("[audio] speaker test tone=%d duration=%lu volume=%u\n",
                  ok ? 1 : 0,
                  static_cast<unsigned long>(testDurationMs),
                  testVolume);
    delay(testDurationMs + 80);
    M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
    M5.Speaker.end();
#if STACKCHAN_DEVICE_STOPWATCH
    configureStopwatchSpeakerHardware(false, "speaker-test-end");
#endif
  }

  if (resumeMic && state_ == ChanState::Listening && !micMuted_) {
    startMicInput();
  }

  return ok;
}

bool AudioController::measureMicDiagnostic(uint32_t durationMs, MicDiagnosticResult& result, const MicDiagnosticConfig& config) {
  result = MicDiagnosticResult{};
  result.durationMs = durationMs;

  if (state_ == ChanState::Speaking || pendingIdleAfterPlayback_) {
    Serial.printf("[audio] mic test skipped: state=%s draining=%d\n",
                  audioStateName(state_),
                  pendingIdleAfterPlayback_ ? 1 : 0);
    return false;
  }

  const bool restoreListening = state_ == ChanState::Listening && micEnabled_ && !micMuted_;
  if (micEnabled_ || micCaptureActive_) {
    stopMicInput("mic diagnostic");
  }

  resetMicBuffers();
  const uint32_t sampleRate = (config.sampleRate >= 8000 && config.sampleRate <= 48000)
                                ? config.sampleRate
                                : AUDIO_SAMPLE_RATE;
  auto micCfg = M5.Mic.config();
  micCfg.sample_rate = sampleRate;
  micCfg.magnification = AUDIO_MIC_MAGNIFICATION;
  micCfg.noise_filter_level = AUDIO_MIC_NOISE_FILTER_LEVEL;
  micCfg.over_sampling = AUDIO_MIC_OVERSAMPLING;
  if (config.magnification >= 1 && config.magnification <= 255) {
    micCfg.magnification = static_cast<uint8_t>(config.magnification);
  }
  if (config.noiseFilterLevel >= 0 && config.noiseFilterLevel <= 255) {
    micCfg.noise_filter_level = static_cast<uint8_t>(config.noiseFilterLevel);
  }
  if (config.overSampling >= 1 && config.overSampling <= 8) {
    micCfg.over_sampling = static_cast<uint8_t>(config.overSampling);
  }
  if (config.pinDataIn != -1000) {
    micCfg.pin_data_in = config.pinDataIn;
  }
  if (config.pinBck != -1000) {
    micCfg.pin_bck = config.pinBck;
  }
  if (config.pinWs != -1000) {
    micCfg.pin_ws = config.pinWs;
  }
  if (config.pinMck != -1000) {
    micCfg.pin_mck = config.pinMck;
  }
  if (config.i2sPort >= 0) {
    micCfg.i2s_port = static_cast<i2s_port_t>(config.i2sPort);
  }
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_HAS_ATOMIC_ECHO_BASE
  micCfg.stereo = false;
  micCfg.left_channel = true;
#endif
  if (config.channelMode == MicDiagnosticChannelMode::Right) {
    micCfg.stereo = false;
    micCfg.left_channel = false;
  } else if (config.channelMode == MicDiagnosticChannelMode::Left) {
    micCfg.stereo = false;
    micCfg.left_channel = true;
  } else if (config.channelMode == MicDiagnosticChannelMode::Stereo) {
    micCfg.stereo = true;
  }
  M5.Mic.config(micCfg);

  Serial.printf("[audio] mic test cfg din=%d bck=%d ws=%d mck=%d port=%d mag=%u nf=%u oversampling=%u sample=%lu duration=%lu\n",
                micCfg.pin_data_in,
                micCfg.pin_bck,
                micCfg.pin_ws,
                micCfg.pin_mck,
                static_cast<int>(micCfg.i2s_port),
                micCfg.magnification,
                micCfg.noise_filter_level,
                micCfg.over_sampling,
                static_cast<unsigned long>(sampleRate),
                static_cast<unsigned long>(durationMs));

  result.beginOk = M5.Mic.begin();
  Serial.printf("[audio] mic test begin=%d\n", result.beginOk ? 1 : 0);
  if (!result.beginOk) {
    if (restoreListening && state_ == ChanState::Listening && !micMuted_) {
      startMicInput();
    }
    return false;
  }

  M5.Mic.setSampleRate(sampleRate);
  uint32_t chunkTarget = (durationMs + AUDIO_CHUNK_MS - 1) / AUDIO_CHUNK_MS;
  if (chunkTarget == 0) {
    chunkTarget = 1;
  }
  int16_t captureBuffer[AUDIO_CHUNK_SAMPLES] = {};
  int64_t rawSum = 0;
  int64_t processedSquareSum = 0;
  const uint32_t recordTimeoutMs = ((AUDIO_CHUNK_SAMPLES * 1000UL) / sampleRate) + 120;

  for (uint32_t chunk = 0; chunk < chunkTarget; ++chunk) {
    const bool recorded = recordMicChunkBlocking(captureBuffer,
                                                 AUDIO_CHUNK_SAMPLES,
                                                 sampleRate,
                                                 recordTimeoutMs);
    if (!recorded) {
      ++result.underruns;
      delay(1);
      continue;
    }

    int64_t chunkSum = 0;
    for (size_t i = 0; i < AUDIO_CHUNK_SAMPLES; ++i) {
      chunkSum += captureBuffer[i];
    }
    const int32_t chunkDc = static_cast<int32_t>(chunkSum / static_cast<int64_t>(AUDIO_CHUNK_SAMPLES));

    for (size_t i = 0; i < AUDIO_CHUNK_SAMPLES; ++i) {
      int32_t value = captureBuffer[i] - chunkDc;
      value = (value * AUDIO_MIC_SOFTWARE_GAIN_Q8) >> 8;
      if (value > 32767) {
        value = 32767;
      } else if (value < -32768) {
        value = -32768;
      }

      const int32_t absValue = abs(value);
      if (absValue > result.peak) {
        result.peak = absValue;
      }
      if (absValue >= AUDIO_MIC_CLIP_THRESHOLD) {
        ++result.clipCount;
      }
      processedSquareSum += static_cast<int64_t>(value) * static_cast<int64_t>(value);
    }

    rawSum += chunkSum;
    result.sampleCount += AUDIO_CHUNK_SAMPLES;
    ++result.chunks;
  }

  M5.Mic.end();

  if (result.sampleCount != 0) {
    result.dc = static_cast<int32_t>(rawSum / static_cast<int64_t>(result.sampleCount));
    result.rms = static_cast<int32_t>(sqrt(static_cast<double>(processedSquareSum) /
                                          static_cast<double>(result.sampleCount)));
  }

  Serial.printf("[audio] mic test result ok=%d chunks=%lu underruns=%lu samples=%lu peak=%ld rms=%ld dc=%ld clip=%lu\n",
                result.chunks > 0 ? 1 : 0,
                static_cast<unsigned long>(result.chunks),
                static_cast<unsigned long>(result.underruns),
                static_cast<unsigned long>(result.sampleCount),
                static_cast<long>(result.peak),
                static_cast<long>(result.rms),
                static_cast<long>(result.dc),
                static_cast<unsigned long>(result.clipCount));

  if (restoreListening && state_ == ChanState::Listening && !micMuted_) {
    startMicInput();
  }

  return result.chunks > 0;
}

MicRuntimeStats AudioController::micRuntimeStats() const {
  MicRuntimeStats stats;
  stats.enabled = micEnabled_;
  stats.captureActive = micCaptureActive_;
  stats.captureRecording = micCaptureRecording_;
  stats.hasClient = hasMicClient();
  stats.muted = micMuted_;
  stats.queuedPackets = micTxQueueCount_;
  stats.queueCapacity = micTxQueueCapacity_;
  stats.capturedChunks = micRuntimeCapturedChunks_;
  stats.enqueuedChunks = micRuntimeEnqueuedChunks_;
  stats.sentChunks = micRuntimeSentChunks_;
  stats.sentBytes = micRuntimeSentBytes_;
  stats.wsSentChunks = micRuntimeWsSentChunks_;
  stats.usbSentChunks = micRuntimeUsbSentChunks_;
  stats.droppedChunks = micRuntimeDroppedChunks_;
  stats.captureUnderruns = micRuntimeCaptureUnderruns_;
  stats.captureOverruns = micRuntimeCaptureOverruns_;
  stats.queueOverflows = micRuntimeQueueOverflows_;
  stats.sendFails = micRuntimeSendFails_;
  stats.txSeq = micTxSeq_;
  stats.lastPeak = micRuntimeLastPeak_;
  stats.lastCaptureMs = micRuntimeLastCaptureMs_;
  stats.lastProcessMs = micRuntimeLastProcessMs_;
  stats.lastEnqueueMs = micRuntimeLastEnqueueMs_;
  stats.lastSendMs = micRuntimeLastSendMs_;
  stats.lastWsSendMs = micRuntimeLastWsSendMs_;
  stats.lastUsbSendMs = micRuntimeLastUsbSendMs_;
  return stats;
}

void AudioController::onBinaryReceived(uint8_t* payload, size_t length) {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  Serial.printf("AudioController::onBinaryReceived bytes=%u state=%s speaker_enabled=%d speaker_pending=%d\n",
                static_cast<unsigned>(length),
                audioStateName(state_),
                speakerEnabled_ ? 1 : 0,
                speakerStartPending_ ? 1 : 0);
#endif
  if (state_ != ChanState::Speaking || (!speakerEnabled_ && !speakerStartPending_)) {
    playbackDiagDropNotSpeakingEvents_++;
    playbackDiagPcmBytesDropped_ += static_cast<uint32_t>(length);
    Serial.printf("AudioController drop binary reason=not_speaking_or_speaker_off state=%s bytes=%u speaker_enabled=%d speaker_pending=%d\n",
                  audioStateName(state_),
                  static_cast<unsigned>(length),
                  speakerEnabled_ ? 1 : 0,
                  speakerStartPending_ ? 1 : 0);
    return;
  }

  if ((length & 1) != 0) {
    playbackDiagDropOddSizeEvents_++;
    playbackDiagPcmBytesDropped_ += static_cast<uint32_t>(length);
    Serial.printf("AudioController drop binary reason=odd_sized_pcm state=%s bytes=%u\n",
                  audioStateName(state_),
                  static_cast<unsigned>(length));
    return;
  }

  playbackDiagPcmFramesReceived_++;
  playbackDiagPcmBytesReceived_ += static_cast<uint32_t>(length);
  playbackDiagLastPcmMs_ = millis();
#if STACKCHAN_DEVICE_STOPWATCH
  size_t written = appendRxBytesWithPlaybackBackpressure(payload, length);
#else
  size_t written = appendRxBytes(payload, length);
#endif
  playbackDiagPcmBytesAccepted_ += static_cast<uint32_t>(written);
  if (rxAvailable() > playbackDiagMaxBufferedBytes_) {
    playbackDiagMaxBufferedBytes_ = rxAvailable();
  }
  ++speakerRxPacketCount_;
  if (written > 0 && pendingIdleAfterPlayback_) {
    idleDrainEmptySinceMs_ = 0;
  }
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  Serial.printf("AudioController accepted pcm bytes=%u buffered_bytes=%u\n",
                static_cast<unsigned>(written),
                static_cast<unsigned>(rxAvailable()));
  if (speakerRxPacketCount_ <= 6 || rxAvailable() >= AUDIO_PLAYBACK_PREBUFFER_BYTES) {
    Serial.printf("TTS buffer bytes=%u\n", static_cast<unsigned>(rxAvailable()));
  }
#endif
  if (written < length) {
    playbackDiagRxOverflowEvents_++;
    playbackDiagPcmBytesDropped_ += static_cast<uint32_t>(length - written);
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
#if STACKCHAN_DEVICE_STOPWATCH
    configureStopwatchSpeakerHardware(false, "idle");
#endif
    speakerEnabled_ = false;
  }
  speakerStartPending_ = false;
  playbackStarted_ = false;
  pendingIdleAfterPlayback_ = false;
  idleDrainEmptySinceMs_ = 0;
  speakerRxPacketCount_ = 0;
  speakerLastBufferLogBytes_ = 0;
  playbackDiagPcmFramesReceived_ = 0;
  playbackDiagPcmBytesReceived_ = 0;
  playbackDiagPcmBytesAccepted_ = 0;
  playbackDiagPcmBytesDropped_ = 0;
  playbackDiagRxOverflowEvents_ = 0;
  playbackDiagDropNotSpeakingEvents_ = 0;
  playbackDiagDropOddSizeEvents_ = 0;
  playbackDiagIdleRequests_ = 0;
  playbackDiagStarts_ = 0;
  playbackDiagFinishes_ = 0;
  playbackDiagUnderflowResets_ = 0;
  playbackDiagSpeakerQueueFullEvents_ = 0;
  playbackDiagPlayRawFailEvents_ = 0;
  playbackDiagSpeakerChunksQueued_ = 0;
  playbackDiagSpeakerBytesQueued_ = 0;
  playbackDiagMaxBufferedBytes_ = 0;
  playbackDiagLastIdleRequestBufferedBytes_ = 0;
  playbackDiagLastUnderflowBufferedBytes_ = 0;
  playbackDiagFinishBufferedBytes_ = 0;
  playbackDiagFinishQueuedChunks_ = 0;
  playbackDiagLastPcmMs_ = 0;
  playbackDiagLastFinishMs_ = 0;
  clearRxRing();
  Serial.println("[audio] idle: mic off, speaker stopped");
}

void AudioController::enterListening() {
  if (speakerEnabled_) {
    M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
    M5.Speaker.end();
#if STACKCHAN_DEVICE_STOPWATCH
    configureStopwatchSpeakerHardware(false, "listening");
#endif
    speakerEnabled_ = false;
  }
  speakerStartPending_ = false;
  playbackStarted_ = false;
  pendingIdleAfterPlayback_ = false;
  idleDrainEmptySinceMs_ = 0;
  speakerRxPacketCount_ = 0;
  speakerLastBufferLogBytes_ = 0;
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
  speakerRxPacketCount_ = 0;
  speakerLastBufferLogBytes_ = 0;
  playbackDiagPcmFramesReceived_ = 0;
  playbackDiagPcmBytesReceived_ = 0;
  playbackDiagPcmBytesAccepted_ = 0;
  playbackDiagPcmBytesDropped_ = 0;
  playbackDiagRxOverflowEvents_ = 0;
  playbackDiagDropNotSpeakingEvents_ = 0;
  playbackDiagDropOddSizeEvents_ = 0;
  playbackDiagIdleRequests_ = 0;
  playbackDiagStarts_ = 0;
  playbackDiagFinishes_ = 0;
  playbackDiagUnderflowResets_ = 0;
  playbackDiagSpeakerQueueFullEvents_ = 0;
  playbackDiagPlayRawFailEvents_ = 0;
  playbackDiagSpeakerChunksQueued_ = 0;
  playbackDiagSpeakerBytesQueued_ = 0;
  playbackDiagMaxBufferedBytes_ = 0;
  playbackDiagLastIdleRequestBufferedBytes_ = 0;
  playbackDiagLastUnderflowBufferedBytes_ = 0;
  playbackDiagFinishBufferedBytes_ = 0;
  playbackDiagFinishQueuedChunks_ = 0;
  playbackDiagLastPcmMs_ = 0;
  playbackDiagLastFinishMs_ = 0;
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
#if STACKCHAN_DEVICE_STOPWATCH
    configureStopwatchSpeakerHardware(true, "speaking");
#endif
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
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_HAS_ATOMIC_ECHO_BASE
  micCfg.stereo = false;
  micCfg.left_channel = true;
#endif
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
  if (micMuted_ || !micEnabled_ || !hasMicClient()) {
    return;
  }

  while (copyNextMicPacket(micSendScratch_)) {
    const unsigned long now = millis();
    bool wsSent = false;
    bool usbSent = false;
    if (wsServer_ != nullptr && wsServer_->hasClient()) {
      wsSent = wsServer_->sendBinary(micSendScratch_, AUDIO_MIC_PACKET_BYTES);
    }
    if (usbSerialClientConnected_ && micPacketSender_ != nullptr) {
      usbSent = micPacketSender_(micSendScratch_, AUDIO_MIC_PACKET_BYTES, micPacketSenderContext_);
    }
    const bool sent = wsSent || usbSent;
    if (sent) {
      ++micTxStatsChunks_;
      micTxStatsBytes_ += AUDIO_CHUNK_BYTES;
      ++micRuntimeSentChunks_;
      micRuntimeSentBytes_ += AUDIO_CHUNK_BYTES;
      micRuntimeLastSendMs_ = now;
      if (wsSent) {
        ++micRuntimeWsSentChunks_;
        micRuntimeLastWsSendMs_ = now;
      }
      if (usbSent) {
        ++micRuntimeUsbSentChunks_;
        micRuntimeLastUsbSendMs_ = now;
      }
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
      ++micRuntimeDroppedChunks_;
      ++micRuntimeSendFails_;
      break;
    }
  }
}

bool AudioController::hasMicClient() const {
  return (wsServer_ != nullptr && wsServer_->hasClient()) ||
         (usbSerialClientConnected_ && micPacketSender_ != nullptr);
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
  const uint32_t recordTimeoutMs = AUDIO_CHUNK_MS + 80;

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
    const bool recorded = recordMicChunkBlocking(captureBuffer,
                                                 AUDIO_CHUNK_SAMPLES,
                                                 AUDIO_SAMPLE_RATE,
                                                 recordTimeoutMs);
    const unsigned long recordEndMs = millis();
    micCaptureRecording_ = false;

    if (!micCaptureActive_) {
      continue;
    }

    if (!recorded) {
      ++micTxStatsCaptureUnderrun_;
      ++micTxStatsDroppedChunks_;
      ++micRuntimeCaptureUnderruns_;
      ++micRuntimeDroppedChunks_;
      micCaptureTimestampMs_ = recordEndMs;
      micCaptureStartPending_ = true;
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    ++micRuntimeCapturedChunks_;
    micRuntimeLastCaptureMs_ = recordEndMs;

    if (recordEndMs - recordStartMs > AUDIO_CHUNK_MS + 20) {
      ++micTxStatsCaptureOverrun_;
      ++micRuntimeCaptureOverruns_;
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

    if (hasMicClient()) {
      const int32_t chunkPeak = processMicChunk(captureBuffer, AUDIO_CHUNK_SAMPLES);
#if AUDIO_MIC_GATE_ENABLED
      const bool remoteVadActive = remoteVadActive_;
      if (remoteVadActive != micLastRemoteVadActive_) {
        micLastRemoteVadActive_ = remoteVadActive;
        clearMicPreRoll();
        micGateOpenUntilMs_ = 0;
        if (remoteVadActive) {
          micGateSending_ = true;
        } else {
          micGateSending_ = false;
          micCaptureStartPending_ = true;
        }
      }

      bool shouldSend = remoteVadActive;

      if (!remoteVadActive) {
        if (chunkPeak >= AUDIO_MIC_GATE_PEAK_THRESHOLD) {
          micGateOpenUntilMs_ = recordEndMs + AUDIO_MIC_GATE_OPEN_HOLD_MS;
        }
        shouldSend = micGateOpenUntilMs_ != 0 && static_cast<long>(micGateOpenUntilMs_ - recordEndMs) >= 0;
      }

      if (!shouldSend) {
        micGateSending_ = false;
        micGateOpenUntilMs_ = 0;
        micCaptureStartPending_ = true;
        storeMicPreRollChunk(captureBuffer, micCaptureTimestampMs_);
      } else if (!remoteVadActive && !micGateSending_) {
        storeMicPreRollChunk(captureBuffer, micCaptureTimestampMs_);
        const uint16_t flags = micCaptureStartPending_ ? AUDIO_MIC_PACKET_FLAG_START : 0;
        if (!enqueueMicPreRoll(flags)) {
          ++micTxStatsRingOverflow_;
          ++micTxStatsSendQueueOverflow_;
          ++micTxStatsDroppedChunks_;
          ++micRuntimeQueueOverflows_;
          ++micRuntimeDroppedChunks_;
          micCaptureStartPending_ = true;
        } else {
          micCaptureStartPending_ = false;
          micGateSending_ = true;
        }
      } else {
        uint16_t flags = 0;
        if (micCaptureStartPending_) {
          flags |= AUDIO_MIC_PACKET_FLAG_START;
          micCaptureStartPending_ = false;
        }
        if (!enqueueMicPacket(captureBuffer, AUDIO_CHUNK_SAMPLES, micCaptureTimestampMs_, flags)) {
          ++micTxStatsRingOverflow_;
          ++micTxStatsSendQueueOverflow_;
          ++micTxStatsDroppedChunks_;
          ++micRuntimeQueueOverflows_;
          ++micRuntimeDroppedChunks_;
          micCaptureStartPending_ = true;
        }
      }
#else
      (void)chunkPeak;
      uint16_t flags = 0;
      if (micCaptureStartPending_) {
        flags |= AUDIO_MIC_PACKET_FLAG_START;
        micCaptureStartPending_ = false;
      }
      if (!enqueueMicPacket(captureBuffer, AUDIO_CHUNK_SAMPLES, micCaptureTimestampMs_, flags)) {
        ++micTxStatsRingOverflow_;
        ++micTxStatsSendQueueOverflow_;
        ++micTxStatsDroppedChunks_;
        ++micRuntimeQueueOverflows_;
        ++micRuntimeDroppedChunks_;
        micCaptureStartPending_ = true;
      }
#endif
    } else {
      clearMicPacketQueue();
      clearMicPreRoll();
      micGateSending_ = false;
      micGateOpenUntilMs_ = 0;
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
  ++micRuntimeEnqueuedChunks_;
  micRuntimeLastEnqueueMs_ = millis();
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

void AudioController::storeMicPreRollChunk(const int16_t* samples, unsigned long timestampMs) {
  if (samples == nullptr || AUDIO_MIC_GATE_PREROLL_CHUNKS == 0) {
    return;
  }

  memcpy(micPreRollBuffers_[micPreRollWriteIndex_], samples, AUDIO_CHUNK_BYTES);
  micPreRollTimestamps_[micPreRollWriteIndex_] = timestampMs;
  micPreRollWriteIndex_ = (micPreRollWriteIndex_ + 1) % AUDIO_MIC_GATE_PREROLL_CHUNKS;
  if (micPreRollCount_ < AUDIO_MIC_GATE_PREROLL_CHUNKS) {
    ++micPreRollCount_;
  }
}

bool AudioController::enqueueMicPreRoll(uint16_t firstFlags) {
  if (micPreRollCount_ == 0) {
    return true;
  }

  const size_t start = (micPreRollWriteIndex_ + AUDIO_MIC_GATE_PREROLL_CHUNKS - micPreRollCount_) % AUDIO_MIC_GATE_PREROLL_CHUNKS;
  for (size_t i = 0; i < micPreRollCount_; ++i) {
    const size_t index = (start + i) % AUDIO_MIC_GATE_PREROLL_CHUNKS;
    const uint16_t flags = i == 0 ? firstFlags : 0;
    if (!enqueueMicPacket(micPreRollBuffers_[index], AUDIO_CHUNK_SAMPLES, micPreRollTimestamps_[index], flags)) {
      clearMicPreRoll();
      return false;
    }
  }

  clearMicPreRoll();
  return true;
}

void AudioController::clearMicPreRoll() {
  micPreRollWriteIndex_ = 0;
  micPreRollCount_ = 0;
}

bool AudioController::recordMicChunkBlocking(int16_t* samples,
                                             size_t sampleCount,
                                             uint32_t sampleRate,
                                             uint32_t timeoutMs) {
  if (samples == nullptr || sampleCount == 0) {
    return false;
  }

  memset(samples, 0, sampleCount * sizeof(int16_t));
  if (!M5.Mic.record(samples, sampleCount, sampleRate, false)) {
    return false;
  }

  const unsigned long startedMs = millis();
  uint32_t expectedMs = static_cast<uint32_t>((sampleCount * 1000UL + sampleRate - 1) / sampleRate);
  if (expectedMs == 0) {
    expectedMs = 1;
  }
  vTaskDelay(pdMS_TO_TICKS(expectedMs + 2));

  while (static_cast<unsigned long>(millis() - startedMs) < timeoutMs) {
    if (M5.Mic.isRecording() == 0) {
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return M5.Mic.isRecording() == 0;
}

int32_t AudioController::processMicChunk(int16_t* samples, size_t sampleCount) {
  if (sampleCount == 0) {
    return 0;
  }

  int64_t sum = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    sum += samples[i];
  }
  const int32_t dc = static_cast<int32_t>(sum / static_cast<int64_t>(sampleCount));

  int32_t chunkPeak = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    int32_t value = samples[i] - dc;
    value = (value * AUDIO_MIC_SOFTWARE_GAIN_Q8) >> 8;
    if (value > 32767) {
      value = 32767;
    } else if (value < -32768) {
      value = -32768;
    }

    const int32_t absValue = abs(value);
    if (absValue > chunkPeak) {
      chunkPeak = absValue;
    }
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
  micRuntimeLastPeak_ = chunkPeak;
  micRuntimeLastProcessMs_ = now;
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

  return chunkPeak;
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
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
    const size_t buffered = rxAvailable();
    if (buffered != speakerLastBufferLogBytes_ &&
        (buffered >= AUDIO_PLAYBACK_PREBUFFER_BYTES ||
         buffered >= speakerLastBufferLogBytes_ + AUDIO_PLAYBACK_CHUNK_BYTES ||
         buffered < speakerLastBufferLogBytes_)) {
      speakerLastBufferLogBytes_ = buffered;
      Serial.printf("TTS buffer bytes=%u\n", static_cast<unsigned>(buffered));
    }
#endif
    if (rxAvailable() >= AUDIO_PLAYBACK_PREBUFFER_BYTES || (pendingIdleAfterPlayback_ && rxAvailable() > 0)) {
      playbackStarted_ = true;
      playbackDiagStarts_++;
      Serial.printf("TTS prebuffer ready threshold=%u buffered=%u\n",
                    static_cast<unsigned>(AUDIO_PLAYBACK_PREBUFFER_BYTES),
                    static_cast<unsigned>(rxAvailable()));
      Serial.println("TTS playback start");
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
#if VERBOSE_LOG_ENABLED
    Serial.printf("I2S write bytes=%u\n", static_cast<unsigned>(AUDIO_PLAYBACK_CHUNK_BYTES));
#endif
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
#if VERBOSE_LOG_ENABLED
    Serial.printf("I2S write bytes=%u\n", static_cast<unsigned>(AUDIO_PLAYBACK_CHUNK_BYTES));
#endif
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
    playbackDiagUnderflowResets_++;
    playbackDiagLastUnderflowBufferedBytes_ = rxAvailable();
    playbackStarted_ = false;
  }
}

void AudioController::finishSpeakingPlayback() {
  playbackDiagFinishes_++;
  playbackDiagFinishBufferedBytes_ = rxAvailable();
  playbackDiagFinishQueuedChunks_ = static_cast<uint32_t>(M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL));
  playbackDiagLastFinishMs_ = millis();
  M5.Speaker.stop(AUDIO_SPEAKER_CHANNEL);
  M5.Speaker.end();
#if STACKCHAN_DEVICE_STOPWATCH
  configureStopwatchSpeakerHardware(false, "playback-drained");
#endif
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
    playbackDiagSpeakerQueueFullEvents_++;
    Serial.println("[audio] speaker queue full; dropped pcm chunk");
    return;
  }

  int16_t* buffer = speakerBuffers_[speakerBufferIndex_];
  memcpy(buffer, samples, sampleCount * sizeof(int16_t));
  speakerBufferIndex_ = (speakerBufferIndex_ + 1) % AUDIO_SPEAKER_BUFFER_COUNT;

  const unsigned long playRawStartedAt = millis();
  const bool ok = M5.Speaker.playRaw(buffer, sampleCount, AUDIO_SAMPLE_RATE, false, 1, AUDIO_SPEAKER_CHANNEL, false);
#if STACKCHAN_DEVICE_STOPWATCH && USB_SERIAL_TTS_DIAG_LOG_ENABLED
  const unsigned long playRawElapsedMs = millis() - playRawStartedAt;
  if (playRawElapsedMs > 20) {
    Serial.printf("[audio] playRaw blocked %lu ms queued=%u rx=%u\n",
                  playRawElapsedMs,
                  static_cast<unsigned>(M5.Speaker.isPlaying(AUDIO_SPEAKER_CHANNEL)),
                  static_cast<unsigned>(rxAvailable()));
  }
#endif
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  Serial.printf("I2S write result=%d\n", ok ? 1 : 0);
#endif
  if (!ok) {
    playbackDiagPlayRawFailEvents_++;
    Serial.println("[audio] playRaw failed");
  } else {
    playbackDiagSpeakerChunksQueued_++;
    playbackDiagSpeakerBytesQueued_ += static_cast<uint32_t>(sampleCount * sizeof(int16_t));
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
  micGateOpenUntilMs_ = 0;
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
  micGateSending_ = false;
  micLastRemoteVadActive_ = false;
  remoteVadActive_ = false;
  clearMicPreRoll();
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

#if STACKCHAN_DEVICE_STOPWATCH
size_t AudioController::appendRxBytesWithPlaybackBackpressure(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return 0;
  }

  size_t copied = 0;
  unsigned long lastProgressMs = millis();
  while (copied < length) {
    const size_t written = appendRxBytes(data + copied, length - copied);
    if (written > 0) {
      copied += written;
      lastProgressMs = millis();
      continue;
    }

    if (!speakerEnabled_ && !speakerStartPending_) {
      break;
    }

    updateSpeakerPlayback();

    const unsigned long now = millis();
    if (now - lastProgressMs >= AUDIO_RX_BACKPRESSURE_TIMEOUT_MS) {
      break;
    }
    delay(1);
  }
  return copied;
}
#endif

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
