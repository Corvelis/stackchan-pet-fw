#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <M5StackChan.h>
#include <Preferences.h>
#include <math.h>
#include <qrcodegen.h>

#include "AffectionController.h"
#include "AppState.h"
#include "AudioController.h"
#include "CameraManager.h"
#include "FaceController.h"
#include "MotionController.h"
#include "WebSocketServerController.h"
#include "config.h"

FaceController faceController;
MotionController motionController;
WebSocketServerController wsServer;
AudioController audioController;
CameraManager cameraManager;
AffectionController affectionController;
WebServer httpServer(HTTP_PORT);
Preferences preferences;

enum class NetworkMode : uint8_t {
  Sta = 0,
  SoftAp = 1,
};

enum class SettingsPage : uint8_t {
  Network = 0,
  Display = 1,
  Audio = 2,
  Servo = 3,
  Power = 4,
};

enum class NetworkQrType : uint8_t {
  None = 0,
  WifiConnect = 1,
  Setup = 2,
};

enum class ThermalLevel : uint8_t {
  Normal,
  Warm,
  Hot,
};

struct DeviceSettings {
  uint8_t brightness = DISPLAY_BRIGHTNESS_DEFAULT;
  uint8_t volume = AUDIO_SPEAKER_VOLUME;
  bool lowPowerMode = false;
};

struct ThermalStatus {
  float chipTempC = NAN;
  float pmicTempC = NAN;
  float baselineChipTempC = NAN;
  float baselinePmicTempC = NAN;
  ThermalLevel level = ThermalLevel::Normal;
  unsigned long lastSampleMs = 0;
  unsigned long hotSinceMs = 0;
  bool suggestLowPower = false;
};

ChanState currentState = ChanState::Idle;
AuthFaceMode currentAuthFaceMode = AuthFaceMode::Unknown;
NetworkMode networkMode = NetworkMode::Sta;
SettingsPage settingsPage = SettingsPage::Network;
DeviceSettings deviceSettings;
ThermalStatus thermalStatus;
bool vadActive = false;
bool infoScreenVisible = false;
bool displayOn = true;
bool wsStarted = false;
bool httpStarted = false;
bool wsClientConnected = false;
bool usbSerialClientConnected = false;
NetworkQrType activeNetworkQr = NetworkQrType::None;
unsigned long lastWifiCheckMs = 0;
unsigned long lastInfoDrawMs = 0;
bool pendingStateAfterPlayback = false;
ChanState deferredStateAfterPlayback = ChanState::Idle;
unsigned long deferredStateReadyMs = 0;
bool pendingFaceModeNormalAfterPlayback = false;
unsigned long deferredFaceModeReadyMs = 0;
unsigned long wsAudioSettleUntilMs = 0;
int16_t pendingAffectionDelta = 0;
AffectionState pendingAffectionVisualState;
bool pendingAffectionVisualStateValid = false;
unsigned long pendingAffectionDeltaReadyMs = 0;
unsigned long pendingAffectionDeltaQueuedMs = 0;
bool pendingAffectionDeltaSawAudio = false;
unsigned long nextListeningNodMs = 0;
unsigned long listeningNodPhaseEndMs = 0;
uint8_t listeningNodPhase = 0;
bool pettingActive = false;
unsigned long pettingEndMs = 0;
unsigned long nextPetMoveMs = 0;
unsigned long lastPettingRepeatEventMs = 0;
Pose pettingBasePose = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
bool shakeActive = false;
unsigned long shakeEndMs = 0;
unsigned long nextShakeCheckMs = 0;
unsigned long lastShakeTriggerMs = 0;
unsigned long nextShakeMotionMs = 0;
unsigned long lastShakeRepeatEventMs = 0;
uint8_t shakeStrongSamples = 0;
uint32_t cameraButtonEventSeq = 0;
unsigned long lastCameraButtonEventMs = 0;
bool cameraButtonPending = false;
bool backTouchReady = false;
unsigned long backTouchReleasedSinceMs = 0;
unsigned long backTouchCandidateSinceMs = 0;
unsigned long backTouchClearSinceMs = 0;
unsigned long lastInitializeDrawMs = 0;
unsigned long lastFaceUpdateMs = 0;
uint8_t initializeSpinnerFrame = 0;
unsigned long interactionReadyAtMs = 0;
unsigned long usbSerialLastRxMs = 0;
size_t usbSerialLineLength = 0;
bool usbSerialLineOverflow = false;
char usbSerialLineBuffer[USB_SERIAL_LINE_BUFFER_BYTES] = {};
bool usbSerialFramedMode = false;
uint32_t usbSerialTxSeq = 0;
uint8_t usbSerialFrameHeader[16] = {};
uint8_t usbSerialFramePayload[USB_SERIAL_FRAME_MAX_PAYLOAD_BYTES] = {};
uint8_t usbSerialHeaderIndex = 0;
uint32_t usbSerialFrameLength = 0;
uint32_t usbSerialPayloadIndex = 0;
uint8_t usbSerialCrcIndex = 0;
uint8_t usbSerialFrameCrcBytes[4] = {};
uint8_t usbSerialMagicIndex = 0;
uint32_t usbSerialTtsFrameCount = 0;
uint32_t usbSerialTtsTotalBytes = 0;
unsigned long usbSerialSpeakingReceivedMs = 0;
unsigned long usbSerialFirstPcmMs = 0;
unsigned long usbSerialLastPcmMs = 0;
uint32_t usbSerialRxDiagEventCount = 0;
enum class UsbSerialRxState : uint8_t {
  Line,
  Header,
  Payload,
  Crc,
  DropFrame,
};
UsbSerialRxState usbSerialRxState = UsbSerialRxState::Line;
struct WifiCredential {
  String ssid;
  String password;
};

constexpr size_t kMaxWifiCredentials = 5;
constexpr unsigned long CAMERA_BUTTON_COOLDOWN_MS = 2000;
constexpr unsigned long CAMERA_BUTTON_RESPONSE_TIMEOUT_MS = 30000;
constexpr bool AUTH_FACE_BASE_SWITCH_ENABLED = false;
WifiCredential wifiCredentials[kMaxWifiCredentials] = {
  {WIFI_SSID, WIFI_PASSWORD},
  {WIFI_SSID_2, WIFI_PASSWORD_2},
};
size_t wifiCredentialCount = 0;
size_t currentWifiIndex = 0;
uint8_t wifiConnectAttempts = 0;
uint8_t qrCodeBuffer[qrcodegen_BUFFER_LEN_MAX];
uint8_t qrTempBuffer[qrcodegen_BUFFER_LEN_MAX];

void sendAffectionState(const char* requestId = nullptr);
void sendInteractionEvent(const char* event, const char* phase, unsigned long now);
bool sendCameraButtonEvent(unsigned long now);
bool appClientConnected();
void updateUsbSerial(unsigned long now);
bool sendUsbSerialJson(const char* payload);
bool sendUsbSerialFrame(uint8_t type, const uint8_t* payload, size_t length, uint8_t flags = 0);
bool sendUsbSerialMicPacket(const uint8_t* payload, size_t length, void* context);
void handleUsbSerialLine(const uint8_t* payload, size_t length);
void handleUsbSerialFrame(uint8_t type, uint8_t flags, uint32_t seq, const uint8_t* payload, size_t length);
void handleUsbSerialJsonPayload(const uint8_t* payload, size_t length);
void handleUsbSerialCaptureRequest(JsonDocument& doc);
void updateCameraButtonPending(unsigned long now);
void clearCameraButtonPending(const char* reason);
bool interactionsReady(unsigned long now);
void applyListeningPresentation(unsigned long now);
AuthFaceMode displayAuthFaceMode(AuthFaceMode mode);
bool audioBusyForUiEffects();
void updatePendingAffectionDelta();
void drawInfoScreen();
void applyDisplayBrightness();
void applyLowPowerMode(bool enabled, bool persist);
bool audioBusyForServoCalibration();

constexpr uint8_t kUsbSerialMagic[4] = {'S', 'C', 'U', '1'};
constexpr uint8_t kUsbSerialVersion = 0x01;
constexpr uint8_t kUsbSerialTypeJson = 0x01;
constexpr uint8_t kUsbSerialTypeTtsPcm = 0x02;
constexpr uint8_t kUsbSerialTypeMicPcm = 0x03;
constexpr uint8_t kUsbSerialTypeCaptureRequest = 0x04;
constexpr uint8_t kUsbSerialTypeCaptureImageChunk = 0x05;
constexpr uint8_t kUsbSerialTypeAck = 0x06;
constexpr uint8_t kUsbSerialTypeError = 0x07;
constexpr uint8_t kUsbSerialTypePing = 0x08;
constexpr uint8_t kUsbSerialTypePong = 0x09;

const char* chanStateName(ChanState state) {
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

uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

void writeLe32(uint8_t* data, uint32_t value) {
  data[0] = static_cast<uint8_t>(value & 0xff);
  data[1] = static_cast<uint8_t>((value >> 8) & 0xff);
  data[2] = static_cast<uint8_t>((value >> 16) & 0xff);
  data[3] = static_cast<uint8_t>((value >> 24) & 0xff);
}

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return crc;
}

void printHexBytes(const uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    Serial.printf("%02x", data[i]);
    if (i + 1 < length) {
      Serial.print(' ');
    }
  }
}

bool isWifiCredentialConfigured(size_t index) {
  return index < wifiCredentialCount &&
         wifiCredentials[index].ssid.length() > 0;
}

bool selectNextConfiguredWifi() {
  for (size_t offset = 0; offset < wifiCredentialCount; ++offset) {
    currentWifiIndex = (currentWifiIndex + 1) % wifiCredentialCount;
    if (isWifiCredentialConfigured(currentWifiIndex)) {
      return true;
    }
  }
  return false;
}

void applyAffectionResult(const AffectionApplyResult& result, unsigned long now, bool sendState,
                          const char* requestId = nullptr, bool deferDeltaUntilAudioSettles = false) {
  if (!result.applied) {
    return;
  }
  const bool deferVisualUntilAudioSettles = audioBusyForUiEffects() || deferDeltaUntilAudioSettles;
  if (deferVisualUntilAudioSettles) {
    pendingAffectionVisualState = result.state;
    pendingAffectionVisualStateValid = true;
    pendingAffectionDeltaReadyMs = 0;
    pendingAffectionDeltaQueuedMs = now;
    pendingAffectionDeltaSawAudio = pendingAffectionDeltaSawAudio || audioBusyForUiEffects();
    if (result.delta != 0) {
      pendingAffectionDelta = constrain(static_cast<int>(pendingAffectionDelta) + result.delta, -999, 999);
    }
  } else {
    faceController.setAffectionState(result.state);
    if (result.delta != 0) {
      faceController.showAffectionDelta(result.delta, now);
    }
  }
  if (sendState) {
    sendAffectionState(requestId);
  }
  if (result.levelChanged && sendState) {
    sendInteractionEvent(
      result.levelIndex > result.previousLevelIndex ? "level_up" : "level_down",
      "instant",
      now
    );
  }
}

bool audioBusyForUiEffects() {
  return currentState == ChanState::Speaking ||
         audioController.state() == ChanState::Speaking ||
         audioController.isPlaybackDraining();
}

bool appClientConnected() {
  return wsClientConnected || usbSerialClientConnected;
}

void updatePendingAffectionDelta() {
  if (!pendingAffectionVisualStateValid && pendingAffectionDelta == 0) {
    return;
  }
  if (audioBusyForUiEffects()) {
    pendingAffectionDeltaReadyMs = 0;
    pendingAffectionDeltaSawAudio = true;
    return;
  }

  const unsigned long now = millis();
  if (!pendingAffectionDeltaSawAudio && now - pendingAffectionDeltaQueuedMs < 2500) {
    return;
  }
  if (pendingAffectionDeltaReadyMs == 0) {
    pendingAffectionDeltaReadyMs = now + AUDIO_SPEAKER_TO_MIC_DELAY_MS;
    return;
  }
  if (now < pendingAffectionDeltaReadyMs) {
    return;
  }

  if (pendingAffectionVisualStateValid) {
    faceController.setAffectionState(pendingAffectionVisualState);
    pendingAffectionVisualStateValid = false;
  }
  if (pendingAffectionDelta != 0) {
    faceController.showAffectionDelta(pendingAffectionDelta, now);
    Serial.printf("[affection] deferred delta shown %+d\n", pendingAffectionDelta);
  }
  pendingAffectionDelta = 0;
  pendingAffectionDeltaReadyMs = 0;
  pendingAffectionDeltaQueuedMs = 0;
  pendingAffectionDeltaSawAudio = false;
}

void updateAffectionState(unsigned long now) {
  if (currentState == ChanState::Speaking ||
      audioController.state() == ChanState::Speaking ||
      audioController.isPlaybackDraining()) {
    return;
  }
  const AffectionApplyResult result = affectionController.update(now);
  applyAffectionResult(result, now, true);
}

const char* networkModeName() {
  return networkMode == NetworkMode::SoftAp ? "SoftAP" : "STA";
}

String wifiQrEscape(const char* value) {
  String escaped;
  if (value == nullptr) {
    return escaped;
  }
  escaped.reserve(strlen(value) + 4);
  for (const char* p = value; *p != '\0'; ++p) {
    if (*p == '\\' || *p == ';' || *p == ',' || *p == ':' || *p == '"') {
      escaped += '\\';
    }
    escaped += *p;
  }
  return escaped;
}

String wifiConnectQrPayload() {
  String payload = F("WIFI:T:WPA;S:");
  payload += wifiQrEscape(AP_SSID);
  payload += F(";P:");
  payload += wifiQrEscape(AP_PASSWORD);
  payload += F(";;");
  return payload;
}

String wifiSetupUrl() {
  if (networkMode == NetworkMode::SoftAp) {
    String url = F("http://");
    url += WiFi.softAPIP().toString();
    url += F("/wifi");
    return url;
  }
  if (WiFi.status() == WL_CONNECTED) {
    String url = F("http://");
    url += WiFi.localIP().toString();
    url += F("/wifi");
    return url;
  }
  return String();
}

bool setupQrAvailable() {
  return networkMode == NetworkMode::SoftAp || WiFi.status() == WL_CONNECTED;
}

NetworkMode loadNetworkMode() {
  preferences.begin("stackchan", true);
  const uint8_t value = preferences.getUChar("net_mode", static_cast<uint8_t>(NetworkMode::Sta));
  preferences.end();
  return value == static_cast<uint8_t>(NetworkMode::SoftAp) ? NetworkMode::SoftAp : NetworkMode::Sta;
}

void loadWifiCredentials() {
  for (size_t i = 0; i < kMaxWifiCredentials; ++i) {
    wifiCredentials[i].ssid = "";
    wifiCredentials[i].password = "";
  }

  preferences.begin("stackchan", true);
  const uint8_t storedCount = preferences.getUChar("wifi_count", 255);
  if (storedCount != 255) {
    wifiCredentialCount = min<size_t>(storedCount, kMaxWifiCredentials);
    for (size_t i = 0; i < wifiCredentialCount; ++i) {
      char ssidKey[12];
      char passKey[12];
      snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid%u", static_cast<unsigned>(i));
      snprintf(passKey, sizeof(passKey), "wifi_pass%u", static_cast<unsigned>(i));
      wifiCredentials[i].ssid = preferences.getString(ssidKey, "");
      wifiCredentials[i].password = preferences.getString(passKey, "");
    }
    preferences.end();
    return;
  }
  preferences.end();

  wifiCredentialCount = 0;
  if (String(WIFI_SSID).length() > 0 && wifiCredentialCount < kMaxWifiCredentials) {
    wifiCredentials[wifiCredentialCount++] = {WIFI_SSID, WIFI_PASSWORD};
  }
  if (String(WIFI_SSID_2).length() > 0 && wifiCredentialCount < kMaxWifiCredentials) {
    wifiCredentials[wifiCredentialCount++] = {WIFI_SSID_2, WIFI_PASSWORD_2};
  }
}

void saveWifiCredentials() {
  preferences.begin("stackchan", false);
  preferences.putUChar("wifi_count", static_cast<uint8_t>(wifiCredentialCount));
  for (size_t i = 0; i < kMaxWifiCredentials; ++i) {
    char ssidKey[12];
    char passKey[12];
    snprintf(ssidKey, sizeof(ssidKey), "wifi_ssid%u", static_cast<unsigned>(i));
    snprintf(passKey, sizeof(passKey), "wifi_pass%u", static_cast<unsigned>(i));
    if (i < wifiCredentialCount) {
      preferences.putString(ssidKey, wifiCredentials[i].ssid);
      preferences.putString(passKey, wifiCredentials[i].password);
    } else {
      preferences.remove(ssidKey);
      preferences.remove(passKey);
    }
  }
  preferences.end();
}

bool upsertWifiCredential(const String& ssid, const String& password, size_t preferredIndex) {
  if (ssid.length() == 0) {
    return false;
  }

  size_t index = kMaxWifiCredentials;
  for (size_t i = 0; i < wifiCredentialCount; ++i) {
    if (wifiCredentials[i].ssid == ssid) {
      index = i;
      break;
    }
  }

  if (index == kMaxWifiCredentials) {
    if (wifiCredentialCount >= kMaxWifiCredentials) {
      return false;
    }
    index = wifiCredentialCount++;
  }

  wifiCredentials[index].ssid = ssid;
  if (password.length() > 0 || wifiCredentials[index].password.length() == 0) {
    wifiCredentials[index].password = password;
  }

  preferredIndex = min(preferredIndex, wifiCredentialCount - 1);
  while (index > preferredIndex) {
    WifiCredential tmp = wifiCredentials[index - 1];
    wifiCredentials[index - 1] = wifiCredentials[index];
    wifiCredentials[index] = tmp;
    --index;
  }
  while (index < preferredIndex) {
    WifiCredential tmp = wifiCredentials[index + 1];
    wifiCredentials[index + 1] = wifiCredentials[index];
    wifiCredentials[index] = tmp;
    ++index;
  }
  currentWifiIndex = preferredIndex;
  return true;
}

bool deleteWifiCredential(size_t index) {
  if (index >= wifiCredentialCount) {
    return false;
  }
  for (size_t i = index; i + 1 < wifiCredentialCount; ++i) {
    wifiCredentials[i] = wifiCredentials[i + 1];
  }
  --wifiCredentialCount;
  if (currentWifiIndex >= wifiCredentialCount) {
    currentWifiIndex = 0;
  }
  return true;
}

bool moveWifiCredential(size_t index, int delta) {
  if (index >= wifiCredentialCount) {
    return false;
  }
  if (delta < 0 && index == 0) {
    return false;
  }
  if (delta > 0 && index + 1 >= wifiCredentialCount) {
    return false;
  }
  const size_t other = delta < 0 ? index - 1 : index + 1;
  WifiCredential tmp = wifiCredentials[other];
  wifiCredentials[other] = wifiCredentials[index];
  wifiCredentials[index] = tmp;
  currentWifiIndex = other;
  return true;
}

void saveNetworkMode(NetworkMode mode) {
  preferences.begin("stackchan", false);
  preferences.putUChar("net_mode", static_cast<uint8_t>(mode));
  preferences.end();
}

void loadDeviceSettings() {
  preferences.begin("stackchan", true);
  deviceSettings.brightness = preferences.getUChar("brightness", DISPLAY_BRIGHTNESS_DEFAULT);
  deviceSettings.volume = preferences.getUChar("volume", AUDIO_SPEAKER_VOLUME);
  deviceSettings.lowPowerMode = preferences.getBool("low_power", false);
  preferences.end();

  deviceSettings.brightness = constrain(deviceSettings.brightness, DISPLAY_BRIGHTNESS_MIN, DISPLAY_BRIGHTNESS_MAX);
  deviceSettings.volume = constrain(deviceSettings.volume, AUDIO_SPEAKER_VOLUME_MIN, AUDIO_SPEAKER_VOLUME_MAX);
}

void saveDeviceSettings() {
  preferences.begin("stackchan", false);
  preferences.putUChar("brightness", deviceSettings.brightness);
  preferences.putUChar("volume", deviceSettings.volume);
  preferences.putBool("low_power", deviceSettings.lowPowerMode);
  preferences.end();
}

uint8_t effectiveBrightness() {
  if (!displayOn) {
    return 0;
  }
  if (deviceSettings.lowPowerMode) {
    return min<uint8_t>(deviceSettings.brightness, DISPLAY_LOW_POWER_BRIGHTNESS_MAX);
  }
  if (!infoScreenVisible && appClientConnected() &&
      (currentState == ChanState::Listening || currentState == ChanState::Speaking ||
       audioController.state() == ChanState::Listening || audioController.state() == ChanState::Speaking)) {
    const uint16_t dimmed = static_cast<uint16_t>(deviceSettings.brightness) * DISPLAY_CONVERSATION_BRIGHTNESS_PERCENT / 100;
    return max<uint8_t>(DISPLAY_BRIGHTNESS_MIN, static_cast<uint8_t>(dimmed));
  }
  return deviceSettings.brightness;
}

ThermalFaceMode thermalFaceModeForLevel(ThermalLevel level) {
  if (level == ThermalLevel::Hot) {
    return ThermalFaceMode::Hot;
  }
  if (level == ThermalLevel::Warm && !appClientConnected()) {
    return ThermalFaceMode::Warm;
  }
  return ThermalFaceMode::Normal;
}

void applyThermalFaceMode() {
  faceController.setThermalFaceMode(deviceSettings.lowPowerMode
                                      ? ThermalFaceMode::LowPower
                                      : thermalFaceModeForLevel(thermalStatus.level));
}

uint8_t steppedSettingValue(uint8_t value, int delta, uint8_t minValue, uint8_t maxValue) {
  if (delta > 0) {
    if (value >= maxValue) {
      return maxValue;
    }
    const int next = ((static_cast<int>(value) / SETTINGS_STEP_VALUE) + 1) * SETTINGS_STEP_VALUE;
    return static_cast<uint8_t>(min(next, static_cast<int>(maxValue)));
  }
  if (delta < 0) {
    if (value <= minValue) {
      return minValue;
    }
    const int next = ((static_cast<int>(value) - 1) / SETTINGS_STEP_VALUE) * SETTINGS_STEP_VALUE;
    return static_cast<uint8_t>(max(next, static_cast<int>(minValue)));
  }
  return constrain(value, minValue, maxValue);
}

void applyDisplayBrightness() {
  M5.Display.setBrightness(effectiveBrightness());
}

void setDisplayOn(bool on) {
  if (displayOn == on) {
    return;
  }
  displayOn = on;
  faceController.setEnabled(on && !infoScreenVisible);
  if (displayOn) {
    M5.Display.wakeup();
    applyDisplayBrightness();
    if (infoScreenVisible) {
      drawInfoScreen();
    }
  } else {
    M5.Display.setBrightness(0);
    M5.Display.sleep();
  }
  Serial.printf("[display] %s\n", displayOn ? "on" : "off");
}

void applyLowPowerMode(bool enabled, bool persist) {
  if (deviceSettings.lowPowerMode == enabled && !persist) {
    return;
  }
  deviceSettings.lowPowerMode = enabled;
  applyDisplayBrightness();
  applyThermalFaceMode();
  if (persist) {
    saveDeviceSettings();
  }
  if (infoScreenVisible && displayOn) {
    drawInfoScreen();
  }
  Serial.printf("[power] low_power=%d\n", enabled);
}

void switchNetworkModeAndRestart() {
  const NetworkMode nextMode = networkMode == NetworkMode::SoftAp ? NetworkMode::Sta : NetworkMode::SoftAp;
  saveNetworkMode(nextMode);

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 48);
  M5.Display.printf("Switching to\n%s", nextMode == NetworkMode::SoftAp ? "SoftAP" : "STA");
  M5.Display.setTextSize(1);
  M5.Display.setCursor(12, 112);
  M5.Display.println("Restarting...");
  Serial.printf("[network] switching mode to %s and restarting\n", nextMode == NetworkMode::SoftAp ? "SoftAP" : "STA");
  delay(500);
  ESP.restart();
}

void scheduleNextListeningNod(unsigned long now) {
  if (!LISTENING_NOD_ENABLED) {
    nextListeningNodMs = 0;
    return;
  }
  nextListeningNodMs = now + random(LISTENING_NOD_MIN_INTERVAL_MS, LISTENING_NOD_MAX_INTERVAL_MS + 1);
}

void scheduleFirstListeningNod(unsigned long now) {
  if (!LISTENING_NOD_ENABLED) {
    nextListeningNodMs = 0;
    return;
  }
  nextListeningNodMs = now + random(LISTENING_NOD_FIRST_MIN_INTERVAL_MS, LISTENING_NOD_FIRST_MAX_INTERVAL_MS + 1);
}

void cancelListeningNod(bool recenter) {
  const bool wasActive = listeningNodPhase != 0 || nextListeningNodMs != 0;
  listeningNodPhase = 0;
  nextListeningNodMs = 0;
  listeningNodPhaseEndMs = 0;
  if (recenter && wasActive) {
    motionController.setImmediatePose(SERVO_PAN_CENTER, SERVO_TILT_CENTER);
  }
}

Pose makePettingPoseFromBase(const Pose& base, bool bigMove) {
  const int amplitude = bigMove
                          ? random(PET_BIG_PAN_MIN, PET_BIG_PAN_MAX + 1)
                          : random(PET_SMALL_PAN_MIN, PET_SMALL_PAN_MAX + 1);
  const int direction = random(100) < 50 ? -1 : 1;
  const int panJitter = random(-4, 5);
  const int tiltLift = bigMove
                         ? random(14, 27)
                         : random(8, 18);
  return {
    constrain(base.pan + direction * amplitude + panJitter, SERVO_PAN_MIN, SERVO_PAN_MAX),
    constrain(base.tilt + tiltLift, SERVO_TILT_MIN, SERVO_TILT_MAX)
  };
}

void setPettingActive(bool active, unsigned long now) {
  if (active && !displayOn) {
    return;
  }

  if (active) {
    pettingEndMs = now + PET_TOUCH_RELEASE_GRACE_MS;
    if (!pettingActive) {
      pettingActive = true;
      nextPetMoveMs = 0;
      pettingBasePose = motionController.currentPose();
      cancelListeningNod(false);
      faceController.setPetFaceMode(true);
      const Pose pose = makePettingPoseFromBase(pettingBasePose, false);
      motionController.setTargetPose(pose.pan, pose.tilt);
      applyAffectionResult(affectionController.applyEvent("petting", 1.0f, 1.0f, nullptr, now), now, true);
      sendInteractionEvent("petting", "start", now);
      lastPettingRepeatEventMs = now;
      Serial.println("[pet] start");
    } else if (now - lastPettingRepeatEventMs >= 800) {
      sendInteractionEvent("petting", "repeat", now);
      lastPettingRepeatEventMs = now;
    }
    return;
  }

  if (!pettingActive) {
    return;
  }

  pettingActive = false;
  pettingEndMs = 0;
  nextPetMoveMs = 0;
  lastPettingRepeatEventMs = 0;
  faceController.setPetFaceMode(false);
  if (!shakeActive) {
    if (currentState == ChanState::Listening) {
      applyListeningPresentation(now);
    } else {
      motionController.setMotion("center");
    }
  }
  sendInteractionEvent("petting", "end", now);
  Serial.println("[pet] end");
}

void updatePetting(unsigned long now) {
  if (!displayOn) {
    setPettingActive(false, now);
    return;
  }

  if (!pettingActive) {
    return;
  }

  if (shakeActive) {
    return;
  }

  if (pettingEndMs != 0 && now >= pettingEndMs) {
    setPettingActive(false, now);
    return;
  }

  if (nextPetMoveMs != 0 && now < nextPetMoveMs) {
    return;
  }

  nextPetMoveMs = now + random(PET_MOVE_MIN_INTERVAL_MS, PET_MOVE_MAX_INTERVAL_MS + 1);
  const bool bigMove = random(100) < PET_BIG_MOVE_CHANCE_PERCENT;
  const Pose pose = makePettingPoseFromBase(pettingBasePose, bigMove);
  motionController.setTargetPose(pose.pan, pose.tilt);
}

void setShakeActive(bool active, unsigned long now) {
  if (active && !displayOn) {
    return;
  }

  if (active) {
    shakeEndMs = now + SHAKE_FACE_HOLD_MS;
    lastShakeTriggerMs = now;
    if (!shakeActive) {
      shakeActive = true;
      shakeStrongSamples = 0;
      nextShakeMotionMs = 0;
      cancelListeningNod(false);
      faceController.setShakeFaceMode(true);
      motionController.setTargetPose(SERVO_PAN_CENTER + random(-10, 11), SERVO_TILT_CENTER - random(4, 11));
      applyAffectionResult(affectionController.applyEvent("shake", 1.0f, 1.0f, nullptr, now), now, true);
      sendInteractionEvent("shake", "start", now);
      lastShakeRepeatEventMs = now;
      Serial.println("[shake] start");
    } else if (now - lastShakeRepeatEventMs >= 800) {
      sendInteractionEvent("shake", "repeat", now);
      lastShakeRepeatEventMs = now;
    }
    return;
  }

  if (!shakeActive) {
    return;
  }

  shakeActive = false;
  shakeEndMs = 0;
  nextShakeMotionMs = 0;
  lastShakeRepeatEventMs = 0;
  shakeStrongSamples = 0;
  faceController.setShakeFaceMode(false);
  if (!pettingActive && currentState != ChanState::Listening) {
    motionController.setMotion("center");
  }
  sendInteractionEvent("shake", "end", now);
  Serial.println("[shake] end");
}

void updateShakeMotion(const m5::imu_data_t& data, unsigned long now) {
  if (now < nextShakeMotionMs) {
    return;
  }

  nextShakeMotionMs = now + SHAKE_MOTION_INTERVAL_MS;

  const int panFromAccel = static_cast<int>(data.accel.y * SHAKE_MOTION_PAN_MAX_DEGREES);
  const int panFromGyro = static_cast<int>(data.gyro.z * SHAKE_MOTION_GYRO_PAN_SCALE);
  const int tiltFromAccel = static_cast<int>(-data.accel.x * SHAKE_MOTION_TILT_DOWN_DEGREES);
  const int tiltFromGyro = static_cast<int>(data.gyro.x * SHAKE_MOTION_GYRO_TILT_SCALE);
  const int panOffset = constrain(panFromAccel + panFromGyro, -SHAKE_MOTION_PAN_MAX_DEGREES, SHAKE_MOTION_PAN_MAX_DEGREES);
  const int tiltOffset = constrain(
    tiltFromAccel + tiltFromGyro,
    -SHAKE_MOTION_TILT_UP_DEGREES,
    SHAKE_MOTION_TILT_DOWN_DEGREES
  );

  motionController.setTargetPose(SERVO_PAN_CENTER + panOffset, SERVO_TILT_CENTER + tiltOffset);
}

void updateShake(unsigned long now) {
  if (!displayOn) {
    setShakeActive(false, now);
    shakeStrongSamples = 0;
    nextShakeMotionMs = 0;
    return;
  }

  if (now < nextShakeCheckMs) {
    return;
  }
  nextShakeCheckMs = now + SHAKE_UPDATE_INTERVAL_MS;

  m5::imu_data_t data;
  const bool imuUpdated = M5.Imu.update();
  if (imuUpdated) {
    M5.Imu.getImuData(&data);
  }

  if (shakeActive) {
    if (imuUpdated) {
      updateShakeMotion(data, now);
    }
    if (now - lastShakeRepeatEventMs >= 800) {
      sendInteractionEvent("shake", "repeat", now);
      lastShakeRepeatEventMs = now;
    }
    if (now < shakeEndMs) {
      return;
    }
    if (currentState == ChanState::Speaking && audioController.state() != ChanState::Idle) {
      return;
    }
    setShakeActive(false, now);
    return;
  }

  if (infoScreenVisible || now - lastShakeTriggerMs < SHAKE_COOLDOWN_MS) {
    return;
  }

  if (!imuUpdated) {
    return;
  }

  if (!interactionsReady(now)) {
    shakeStrongSamples = 0;
    return;
  }

  const float ax = data.accel.x;
  const float ay = data.accel.y;
  const float az = data.accel.z;
  const float gx = data.gyro.x;
  const float gy = data.gyro.y;
  const float gz = data.gyro.z;
  const float accMag = sqrtf(ax * ax + ay * ay + az * az);
  const float shakeAcc = fabsf(accMag - 1.0f);
  const float gyroMag = sqrtf(gx * gx + gy * gy + gz * gz);
  const bool strongShake = shakeAcc > SHAKE_ACC_THRESHOLD_G || gyroMag > SHAKE_GYRO_THRESHOLD_DPS;

  if (strongShake) {
    ++shakeStrongSamples;
    if (shakeStrongSamples >= SHAKE_REQUIRED_SAMPLES) {
      Serial.printf("[shake] detected acc=%.2f gyro=%.2f\n", shakeAcc, gyroMag);
      setShakeActive(true, now);
    }
    return;
  }

  if (shakeStrongSamples > 0) {
    --shakeStrongSamples;
  }
}

void applyListeningPresentation(unsigned long now) {
  if (currentState != ChanState::Listening) {
    return;
  }

  cancelListeningNod(vadActive == false);
  if (!vadActive) {
    faceController.setAuthFaceMode(AuthFaceMode::Unknown);
    motionController.setMotion("center");
    return;
  }

  faceController.setAuthFaceMode(displayAuthFaceMode(currentAuthFaceMode));
  if (currentAuthFaceMode == AuthFaceMode::NotMaster) {
    motionController.setMotion("not_master");
    return;
  }

  motionController.setMotion("center");
  scheduleFirstListeningNod(now);
}

AuthFaceMode displayAuthFaceMode(AuthFaceMode mode) {
  return AUTH_FACE_BASE_SWITCH_ENABLED ? mode : AuthFaceMode::Unknown;
}

void drawBootScreen(const char* message) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(16, 32);
  M5.Display.println("Stack-chan");
  M5.Display.setCursor(16, 72);
  M5.Display.println(message);
}

void drawInitializeScreen(unsigned long now) {
  if (lastInitializeDrawMs != 0 && now - lastInitializeDrawMs < 160) {
    return;
  }
  lastInitializeDrawMs = now;

  static const char spinner[] = {'|', '/', '-', '\\'};
  const unsigned long remainingMs = now >= interactionReadyAtMs ? 0 : interactionReadyAtMs - now;

  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(16, 32);
  M5.Display.println("Stack-chan");
  M5.Display.setCursor(16, 76);
  M5.Display.printf("%c Initialize", spinner[initializeSpinnerFrame % 4]);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(16, 116);
  M5.Display.printf("Ready in %lu.%lus", remainingMs / 1000, (remainingMs % 1000) / 100);
  initializeSpinnerFrame = (initializeSpinnerFrame + 1) % 4;
}

bool interactionsReady(unsigned long now) {
  return interactionReadyAtMs != 0 && now >= interactionReadyAtMs;
}

const char* thermalLevelName(ThermalLevel level) {
  switch (level) {
    case ThermalLevel::Warm:
      return "Warm";
    case ThermalLevel::Hot:
      return "Hot";
    case ThermalLevel::Normal:
    default:
      return "Normal";
  }
}

float maxValidTemperature(float a, float b) {
  if (isnan(a)) {
    return b;
  }
  if (isnan(b)) {
    return a;
  }
  return max(a, b);
}

bool externalPowerPresent() {
  if (M5.Power.isCharging() == m5::Power_Class::is_charging) {
    return true;
  }
  const int16_t vbusMv = M5.Power.getVBUSVoltage();
  return vbusMv >= 4500;
}

void updateBatteryStatus() {
  const int batteryLevel = M5.Power.getBatteryLevel();
  faceController.setBatteryState(batteryLevel, externalPowerPresent());
}

void updateMicStatusOverlay() {
  faceController.setMicState(appClientConnected(), audioController.micMuted(), audioController.isMicStreaming());
}

void updateThermalStatus(unsigned long now) {
  if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
    return;
  }
  if (thermalStatus.lastSampleMs != 0 && now - thermalStatus.lastSampleMs < THERMAL_SAMPLE_INTERVAL_MS) {
    return;
  }
  thermalStatus.lastSampleMs = now;

  thermalStatus.chipTempC = temperatureRead();
  if (M5.Power.getType() == m5::Power_Class::pmic_axp2101) {
    thermalStatus.pmicTempC = M5.Power.Axp2101.getInternalTemperature();
  } else {
    thermalStatus.pmicTempC = NAN;
  }

  if (now >= THERMAL_BASELINE_CAPTURE_MS) {
    if (isnan(thermalStatus.baselineChipTempC) && !isnan(thermalStatus.chipTempC)) {
      thermalStatus.baselineChipTempC = thermalStatus.chipTempC;
    }
    if (isnan(thermalStatus.baselinePmicTempC) && !isnan(thermalStatus.pmicTempC)) {
      thermalStatus.baselinePmicTempC = thermalStatus.pmicTempC;
    }
  }

  const float hottest = maxValidTemperature(thermalStatus.chipTempC, thermalStatus.pmicTempC);
  const bool externallyPowered = externalPowerPresent();
  const float warmAbsoluteC = externallyPowered ? THERMAL_CHARGING_WARM_ABSOLUTE_C : THERMAL_WARM_ABSOLUTE_C;
  const float hotAbsoluteC = externallyPowered ? THERMAL_CHARGING_HOT_ABSOLUTE_C : THERMAL_HOT_ABSOLUTE_C;
  float maxDelta = NAN;
  if (!isnan(thermalStatus.chipTempC) && !isnan(thermalStatus.baselineChipTempC)) {
    maxDelta = thermalStatus.chipTempC - thermalStatus.baselineChipTempC;
  }
  if (!isnan(thermalStatus.pmicTempC) && !isnan(thermalStatus.baselinePmicTempC)) {
    const float pmicDelta = thermalStatus.pmicTempC - thermalStatus.baselinePmicTempC;
    maxDelta = isnan(maxDelta) ? pmicDelta : max(maxDelta, pmicDelta);
  }

  ThermalLevel nextLevel = ThermalLevel::Normal;
  if ((!isnan(hottest) && hottest >= hotAbsoluteC) ||
      (!isnan(maxDelta) && maxDelta >= THERMAL_HOT_DELTA_C)) {
    nextLevel = ThermalLevel::Hot;
  } else if ((!isnan(hottest) && hottest >= warmAbsoluteC) ||
             (!externallyPowered && !isnan(maxDelta) && maxDelta >= THERMAL_WARM_DELTA_C)) {
    nextLevel = ThermalLevel::Warm;
  }

  if (nextLevel == ThermalLevel::Hot) {
    if (thermalStatus.hotSinceMs == 0) {
      thermalStatus.hotSinceMs = now;
    }
    thermalStatus.suggestLowPower = now - thermalStatus.hotSinceMs >= THERMAL_LOW_POWER_SUGGEST_MS;
  } else {
    thermalStatus.hotSinceMs = 0;
    thermalStatus.suggestLowPower = false;
  }

  if (thermalStatus.level != nextLevel) {
    thermalStatus.level = nextLevel;
    if (!deviceSettings.lowPowerMode) {
      applyThermalFaceMode();
    }
    Serial.printf("[thermal] level=%s chip=%.1f pmic=%.1f\n",
                  thermalLevelName(thermalStatus.level),
                  thermalStatus.chipTempC,
                  thermalStatus.pmicTempC);
  }

  updateBatteryStatus();
}

void addCorsHeaders() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  httpServer.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  httpServer.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendJson(int code, const char* body) {
  addCorsHeaders();
  httpServer.sendHeader("Cache-Control", "no-store");
  httpServer.send(code, "application/json", body);
}

String htmlEscape(const String& value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (c == '&') {
      escaped += F("&amp;");
    } else if (c == '<') {
      escaped += F("&lt;");
    } else if (c == '>') {
      escaped += F("&gt;");
    } else if (c == '"') {
      escaped += F("&quot;");
    } else if (c == '\'') {
      escaped += F("&#39;");
    } else {
      escaped += c;
    }
  }
  return escaped;
}

void sendWifiPage(const String& messageJa = "", const String& messageEn = "") {
  addCorsHeaders();
  httpServer.sendHeader("Cache-Control", "no-store");

  String body;
  body.reserve(13000);
  body += F("<!doctype html><html lang=\"ja\"><head><meta charset=\"utf-8\">");
  body += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  body += F("<title>Stack-chan Wi-Fi Setup</title>");
  body += F("<style>");
  body += F("body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;margin:0;background:#f7f7f4;color:#1f2328}");
  body += F("main{max-width:720px;margin:0 auto;padding:20px}h1{font-size:24px;margin:0 0 16px}");
  body += F("section{background:#fff;border:1px solid #ddd;border-radius:8px;padding:16px;margin:14px 0}");
  body += F("label{display:block;font-weight:600;margin:12px 0 6px}input,select{box-sizing:border-box;width:100%;padding:10px;border:1px solid #bbb;border-radius:6px;font-size:16px}");
  body += F("button,a.btn{display:inline-block;margin:8px 6px 0 0;padding:9px 12px;border:1px solid #777;border-radius:6px;background:#222;color:#fff;text-decoration:none;font-size:14px}");
  body += F("button.secondary{background:#fff;color:#222}.row{border-top:1px solid #eee;padding:10px 0}.muted{color:#666;font-size:13px}.msg{background:#eaf6ef;border-color:#b9dfc8}.lang{float:right}.lang button{margin-top:0}.lang button.active{background:#0b6bcb;color:#fff;border-color:#0b6bcb}");
  body += F("</style></head><body><main><h1>Stack-chan Wi-Fi Setup</h1>");
  body += F("<div class=\"lang\"><button class=\"secondary\" id=\"lang-ja\" type=\"button\" onclick=\"setLang('ja')\">日本語</button><button class=\"secondary\" id=\"lang-en\" type=\"button\" onclick=\"setLang('en')\">English</button></div>");
  body += F("<p class=\"muted\" data-ja=\"SSIDを選択または入力して保存します。保存済みWi-Fiは上から順に接続を試します。\" data-en=\"Select or enter an SSID and save it. Saved Wi-Fi networks are tried from top to bottom.\">SSIDを選択または入力して保存します。保存済みWi-Fiは上から順に接続を試します。</p>");

  if (messageJa.length() > 0 || messageEn.length() > 0) {
    body += F("<section class=\"msg\" data-ja=\"");
    body += htmlEscape(messageJa);
    body += F("\" data-en=\"");
    body += htmlEscape(messageEn.length() > 0 ? messageEn : messageJa);
    body += F("\">");
    body += htmlEscape(messageJa);
    body += F("</section>");
  }

  body += F("<section><h2 data-ja=\"見つかったSSID\" data-en=\"Found SSIDs\">見つかったSSID</h2>");
  body += F("<button class=\"secondary\" type=\"button\" onclick=\"scanWifi()\" data-ja=\"再スキャン\" data-en=\"Rescan\">再スキャン</button>");
  body += F("<div id=\"scan\" class=\"muted\">読み込み中...</div></section>");

  body += F("<section><h2 data-ja=\"保存 / 変更\" data-en=\"Save / Edit\">保存 / 変更</h2>");
  body += F("<form method=\"post\" action=\"/wifi/save\">");
  body += F("<label for=\"ssid\">SSID</label><input id=\"ssid\" name=\"ssid\" autocomplete=\"off\" required>");
  body += F("<label for=\"password\">Password</label><input id=\"password\" name=\"password\" type=\"password\" autocomplete=\"current-password\">");
  body += F("<p class=\"muted\" data-ja=\"保存済みSSIDを選んでパスワード欄を空にすると、既存パスワードを維持します。\" data-en=\"Select a saved SSID and leave Password empty to keep the existing password.\">保存済みSSIDを選んでパスワード欄を空にすると、既存パスワードを維持します。</p>");
  body += F("<label for=\"priority\" data-ja=\"優先度\" data-en=\"Priority\">優先度</label><select id=\"priority\" name=\"priority\">");
  for (size_t i = 0; i < kMaxWifiCredentials; ++i) {
    body += F("<option value=\"");
    body += String(i);
    body += F("\">");
    body += String(i + 1);
    body += F("</option>");
  }
  body += F("</select><button type=\"submit\" data-ja=\"保存\" data-en=\"Save\">保存</button></form></section>");

  body += F("<section><h2 data-ja=\"保存済みWi-Fi\" data-en=\"Saved Wi-Fi\">保存済みWi-Fi</h2>");
  if (wifiCredentialCount == 0) {
    body += F("<p class=\"muted\" data-ja=\"保存済みWi-Fiはありません。\" data-en=\"No saved Wi-Fi networks.\">保存済みWi-Fiはありません。</p>");
  }
  for (size_t i = 0; i < wifiCredentialCount; ++i) {
    body += F("<div class=\"row\"><strong>");
    body += String(i + 1);
    body += F(". ");
    body += htmlEscape(wifiCredentials[i].ssid);
    body += F("</strong><div>");
    body += F("<button class=\"secondary\" data-ssid=\"");
    body += htmlEscape(wifiCredentials[i].ssid);
    body += F("\" data-index=\"");
    body += String(i);
    body += F("\" onclick=\"editSaved(this.dataset.ssid,this.dataset.index)\" data-ja=\"編集\" data-en=\"Edit\">編集</button>");
    body += F("<form method=\"post\" action=\"/wifi/move\" style=\"display:inline\"><input type=\"hidden\" name=\"index\" value=\"");
    body += String(i);
    body += F("\"><input type=\"hidden\" name=\"dir\" value=\"up\"><button class=\"secondary\" type=\"submit\" data-ja=\"上へ\" data-en=\"Up\">上へ</button></form>");
    body += F("<form method=\"post\" action=\"/wifi/move\" style=\"display:inline\"><input type=\"hidden\" name=\"index\" value=\"");
    body += String(i);
    body += F("\"><input type=\"hidden\" name=\"dir\" value=\"down\"><button class=\"secondary\" type=\"submit\" data-ja=\"下へ\" data-en=\"Down\">下へ</button></form>");
    body += F("<form method=\"post\" action=\"/wifi/delete\" style=\"display:inline\"><input type=\"hidden\" name=\"index\" value=\"");
    body += String(i);
    body += F("\"><button class=\"secondary\" type=\"submit\" data-ja=\"削除\" data-en=\"Delete\">削除</button></form>");
    body += F("</div></div>");
  }
  body += F("</section>");

  body += F("<section><h2 data-ja=\"接続\" data-en=\"Connect\">接続</h2>");
  body += F("<p class=\"muted\" data-ja=\"設定を保存したら、再起動してSTA接続を試します。\" data-en=\"After saving settings, restart to try STA connection.\">設定を保存したら、再起動してSTA接続を試します。</p>");
  body += F("<form method=\"post\" action=\"/wifi/restart\"><button type=\"submit\" data-ja=\"保存済みWi-Fiで再起動\" data-en=\"Restart with saved Wi-Fi\">保存済みWi-Fiで再起動</button></form>");
  body += F("</section>");

  body += F("<script>");
  body += F("let lang=localStorage.getItem('stackchanWifiLang')||((navigator.language||'').toLowerCase().startsWith('ja')?'ja':'en');");
  body += F("const txt={ja:{loading:'読み込み中...',scanning:'スキャン中...',none:'SSIDが見つかりませんでした',failed:'スキャンに失敗しました',select:'選択'},en:{loading:'Loading...',scanning:'Scanning...',none:'No SSIDs found',failed:'Scan failed',select:'Select'}};");
  body += F("function setLang(l){lang=l;localStorage.setItem('stackchanWifiLang',l);document.documentElement.lang=l;document.querySelectorAll('[data-ja][data-en]').forEach(function(e){e.textContent=e.dataset[l]});document.getElementById('lang-ja').classList.toggle('active',l==='ja');document.getElementById('lang-en').classList.toggle('active',l==='en');}");
  body += F("function esc(s){return String(s).replace(/[&<>\"']/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]})}");
  body += F("function selectSsid(s){document.getElementById('ssid').value=s;document.getElementById('password').focus()}");
  body += F("function editSaved(s,i){document.getElementById('ssid').value=s;document.getElementById('priority').value=i;document.getElementById('password').focus()}");
  body += F("async function scanWifi(){let box=document.getElementById('scan');box.textContent=txt[lang].scanning;try{let r=await fetch('/wifi/scan');let d=await r.json();if(!d.networks.length){box.textContent=txt[lang].none;return}box.innerHTML=d.networks.map(n=>'<div class=\"row\"><button class=\"secondary\" onclick=\"selectSsid(\\''+String(n.ssid).replace(/\\\\/g,'\\\\\\\\').replace(/'/g,\"\\\\'\")+'\\')\">'+txt[lang].select+'</button> '+esc(n.ssid)+' <span class=\"muted\">'+n.rssi+' dBm '+esc(n.auth)+'</span></div>').join('')}catch(e){box.textContent=txt[lang].failed}}");
  body += F("setLang(lang);scanWifi();</script></main></body></html>");

  httpServer.send(200, "text/html; charset=utf-8", body);
}

void handleWifiScanRequest() {
  WiFiMode_t previousMode = WiFi.getMode();
  if (previousMode == WIFI_AP) {
    WiFi.mode(WIFI_AP_STA);
  }
  const int count = WiFi.scanNetworks(false, true);

  JsonDocument doc;
  JsonArray networks = doc["networks"].to<JsonArray>();
  for (int i = 0; i < count; ++i) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) {
      continue;
    }
    JsonObject item = networks.add<JsonObject>();
    item["ssid"] = ssid;
    item["rssi"] = WiFi.RSSI(i);
    item["auth"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured";
  }
  WiFi.scanDelete();
  if (previousMode == WIFI_AP) {
    WiFi.mode(WIFI_AP);
  }

  String body;
  serializeJson(doc, body);
  sendJson(200, body.c_str());
}

void handleWifiSaveRequest() {
  String ssid = httpServer.arg("ssid");
  ssid.trim();
  String password = httpServer.arg("password");
  const size_t priority = static_cast<size_t>(httpServer.arg("priority").toInt());
  if (!upsertWifiCredential(ssid, password, priority)) {
    sendWifiPage("保存できませんでした。SSIDが空、または保存件数が上限です。",
                 "Could not save. SSID is empty or the saved network list is full.");
    return;
  }
  saveWifiCredentials();
  sendWifiPage("Wi-Fi設定を保存しました。必要なら再起動して接続を試してください。",
               "Wi-Fi settings saved. Restart to try connecting if needed.");
}

void handleWifiDeleteRequest() {
  if (deleteWifiCredential(static_cast<size_t>(httpServer.arg("index").toInt()))) {
    saveWifiCredentials();
    sendWifiPage("Wi-Fi設定を削除しました。",
                 "Wi-Fi settings deleted.");
    return;
  }
  sendWifiPage("削除できませんでした。",
               "Could not delete.");
}

void handleWifiMoveRequest() {
  const size_t index = static_cast<size_t>(httpServer.arg("index").toInt());
  const int delta = httpServer.arg("dir") == "up" ? -1 : 1;
  if (moveWifiCredential(index, delta)) {
    saveWifiCredentials();
    sendWifiPage("優先度を変更しました。",
                 "Priority changed.");
    return;
  }
  sendWifiPage("優先度を変更できませんでした。",
               "Could not change priority.");
}

void handleWifiRestartRequest() {
  saveNetworkMode(NetworkMode::Sta);
  sendWifiPage("再起動します。保存済みWi-Fiへの接続を試します。",
               "Restarting. The device will try saved Wi-Fi networks.");
  delay(500);
  ESP.restart();
}

void handleCaptureRequest() {
  faceController.setPhotoFaceMode(true);
  faceController.showFace("photo_0");
  const bool micPausedForCapture = CAMERA_PAUSE_MIC_DURING_CAPTURE
                                     ? audioController.pauseMicForCapture()
                                     : false;
  if (VERBOSE_LOG_ENABLED && !CAMERA_PAUSE_MIC_DURING_CAPTURE && audioController.isMicStreaming()) {
    Serial.println("[camera] capture while mic streaming");
  }

  if (!cameraManager.isReady() && !cameraManager.init()) {
    audioController.resumeMicAfterCapture(micPausedForCapture);
    sendJson(503, "{\"error\":\"camera_not_ready\"}");
    return;
  }

  uint8_t* jpg = nullptr;
  size_t jpgLen = 0;
  if (!cameraManager.captureJpeg(&jpg, &jpgLen)) {
    cameraManager.deinit();
    audioController.resumeMicAfterCapture(micPausedForCapture);
    sendJson(500, "{\"error\":\"capture_failed\"}");
    return;
  }

  addCorsHeaders();
  httpServer.sendHeader("Cache-Control", "no-store");
  httpServer.setContentLength(jpgLen);
  httpServer.send(200, "image/jpeg", "");
  WiFiClient client = httpServer.client();
  client.write(jpg, jpgLen);
  cameraManager.releaseBuffer(jpg);
  cameraManager.deinit();
  delay(80);
  audioController.resumeMicAfterCapture(micPausedForCapture);
  audioController.deferNextSpeakerStartUntil(millis() + AUDIO_AFTER_CAPTURE_SPEAKER_DELAY_MS);
}

void handleStatusRequest() {
  JsonDocument doc;
  const AffectionState& affection = affectionController.state();
  doc["cameraReady"] = cameraManager.isReady();
  doc["networkMode"] = networkModeName();
  doc["wsClientConnected"] = wsClientConnected;
  doc["usbSerialClientConnected"] = usbSerialClientConnected;
  doc["affection"] = affection.affection;
  doc["mood"] = affection.mood;
  doc["confusion"] = affection.confusion;
  doc["affectionSeq"] = affection.seq;
  doc["affectionLevel"] = affectionController.level();
  doc["levelIndex"] = affectionController.levelIndex();
  doc["visualTier"] = affectionController.visualTier();
  doc["styleId"] = affectionController.styleId();
  doc["timestampMs"] = millis();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["freePsram"] = ESP.getFreePsram();
  doc["displayOn"] = displayOn;
  doc["brightness"] = deviceSettings.brightness;
  doc["volume"] = deviceSettings.volume;
  doc["micMuted"] = audioController.micMuted();
  doc["micStreaming"] = audioController.isMicStreaming();
  doc["lowPowerMode"] = deviceSettings.lowPowerMode;
  doc["thermalLevel"] = thermalLevelName(thermalStatus.level);
  doc["chipTempC"] = thermalStatus.chipTempC;
  doc["pmicTempC"] = thermalStatus.pmicTempC;
  doc["batteryLevel"] = M5.Power.getBatteryLevel();
  doc["charging"] = externalPowerPresent();
  if (networkMode == NetworkMode::SoftAp) {
    doc["ip"] = WiFi.softAPIP().toString();
    doc["stations"] = WiFi.softAPgetStationNum();
  } else {
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.localIP().toString();
  }

  String body;
  serializeJson(doc, body);
  sendJson(200, body.c_str());
}

void startHttpServer() {
  if (httpStarted) {
    return;
  }

  httpServer.on("/capture", HTTP_OPTIONS, []() {
    addCorsHeaders();
    httpServer.send(204);
  });
  httpServer.on("/capture", HTTP_POST, handleCaptureRequest);
  httpServer.on("/status", HTTP_OPTIONS, []() {
    addCorsHeaders();
    httpServer.send(204);
  });
  httpServer.on("/status", HTTP_GET, handleStatusRequest);
  httpServer.on("/wifi", HTTP_GET, []() {
    sendWifiPage();
  });
  httpServer.on("/setup", HTTP_GET, []() {
    sendWifiPage();
  });
  httpServer.on("/wifi/scan", HTTP_GET, handleWifiScanRequest);
  httpServer.on("/wifi/save", HTTP_POST, handleWifiSaveRequest);
  httpServer.on("/wifi/delete", HTTP_POST, handleWifiDeleteRequest);
  httpServer.on("/wifi/move", HTTP_POST, handleWifiMoveRequest);
  httpServer.on("/wifi/restart", HTTP_POST, handleWifiRestartRequest);
  httpServer.onNotFound([]() {
    sendJson(404, "{\"error\":\"not_found\"}");
  });
  httpServer.begin();
  httpStarted = true;
  Serial.printf("[http] server started on port %u\n", HTTP_PORT);
}

void startServers() {
  if (!wsStarted) {
    wsServer.begin(WS_PORT);
    wsStarted = true;
  }
  startHttpServer();
}

const char* settingsPageName(SettingsPage page) {
  switch (page) {
    case SettingsPage::Display:
      return "Display";
    case SettingsPage::Audio:
      return "Audio";
    case SettingsPage::Servo:
      return "Servo";
    case SettingsPage::Power:
      return "Power";
    case SettingsPage::Network:
    default:
      return "Network";
  }
}

void drawButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* label, bool active = false) {
  const uint16_t border = active ? M5.Display.color565(90, 210, 150) : M5.Display.color565(110, 120, 128);
  const uint16_t fill = active ? M5.Display.color565(20, 52, 38) : TFT_BLACK;
  M5.Display.fillRoundRect(x, y, w, h, 5, fill);
  M5.Display.drawRoundRect(x, y, w, h, 5, border);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, fill);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, x + w / 2, y + h / 2);
  M5.Display.setTextDatum(top_left);
}

void drawSlider(int32_t x, int32_t y, int32_t w, const char* label, int value, int minValue, int maxValue) {
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(x, y);
  M5.Display.printf("%s: %d", label, value);
  const int32_t trackY = y + 22;
  M5.Display.drawRoundRect(x, trackY, w, 12, 4, M5.Display.color565(88, 96, 104));
  const int32_t fillW = map(constrain(value, minValue, maxValue), minValue, maxValue, 0, w - 4);
  if (fillW > 0) {
    M5.Display.fillRoundRect(x + 2, trackY + 2, fillW, 8, 3, M5.Display.color565(90, 190, 245));
  }
}

void drawSettingsTabs() {
  drawButton(6, 206, 58, 26, "Net", settingsPage == SettingsPage::Network);
  drawButton(68, 206, 58, 26, "Disp", settingsPage == SettingsPage::Display);
  drawButton(130, 206, 58, 26, "Audio", settingsPage == SettingsPage::Audio);
  drawButton(192, 206, 58, 26, "Servo", settingsPage == SettingsPage::Servo);
  drawButton(254, 206, 60, 26, "Pwr", settingsPage == SettingsPage::Power);
}

void drawNetworkSettingsPage() {
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(12, 56);
  M5.Display.printf("Mode: %s\n", networkModeName());

  if (networkMode == NetworkMode::SoftAp) {
    const String ip = WiFi.softAPIP().toString();
    M5.Display.printf("SSID: %s\n", AP_SSID);
    M5.Display.printf("PASS: %s\n", AP_PASSWORD);
    M5.Display.printf("IP: %s\n", ip.c_str());
    M5.Display.printf("Setup: http://%s/wifi\n", ip.c_str());
    M5.Display.printf("WS: ws://%s:%d%s\n", ip.c_str(), WS_PORT, WS_PATH);
    M5.Display.printf("Stations: %d\n", WiFi.softAPgetStationNum());
    drawButton(12, 154, 136, 30, "Wi-Fi QR");
    drawButton(172, 154, 136, 30, "Setup QR");
  } else if (WiFi.status() == WL_CONNECTED) {
    const String ip = WiFi.localIP().toString();
    M5.Display.printf("SSID: %s\n", wifiCredentials[currentWifiIndex].ssid.c_str());
    M5.Display.printf("IP: %s\n", ip.c_str());
    M5.Display.printf("WS: ws://%s:%d%s\n", ip.c_str(), WS_PORT, WS_PATH);
    M5.Display.printf("Setup: http://%s/wifi\n", ip.c_str());
    drawButton(172, 154, 136, 30, "Setup QR");
  } else {
    M5.Display.printf("SSID: %s\n", wifiCredentials[currentWifiIndex].ssid.c_str());
    M5.Display.println("IP: not connected");
    M5.Display.println("WS: not ready");
  }

  M5.Display.printf("Client: %s\n", appClientConnected() ? "connected" : "disconnected");
  M5.Display.printf("USB: %s\n", usbSerialClientConnected ? "connected" : "waiting");
  M5.Display.println();
  M5.Display.printf("Hold: switch to %s\n", networkMode == NetworkMode::SoftAp ? "STA" : "SoftAP");
}

void drawNetworkQrScreen() {
  const bool isWifiQr = activeNetworkQr == NetworkQrType::WifiConnect;
  const String payload = isWifiQr ? wifiConnectQrPayload() : wifiSetupUrl();
  const bool encoded = payload.length() > 0 &&
                       qrcodegen_encodeText(payload.c_str(), qrTempBuffer, qrCodeBuffer,
                                            qrcodegen_Ecc_MEDIUM,
                                            qrcodegen_VERSION_MIN,
                                            qrcodegen_VERSION_MAX,
                                            qrcodegen_Mask_AUTO, true);

  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 12);
  M5.Display.println(isWifiQr ? "Wi-Fi QR" : "Setup QR");

  if (!encoded) {
    M5.Display.setTextSize(1);
    M5.Display.setCursor(12, 72);
    M5.Display.println("QR unavailable");
    M5.Display.setCursor(12, 96);
    M5.Display.println("Tap to return");
    return;
  }

  const int size = qrcodegen_getSize(qrCodeBuffer);
  const int quietModules = 4;
  const int totalModules = size + quietModules * 2;
  const int maxQrPixels = min(M5.Display.width() - 28, M5.Display.height() - 78);
  const int scale = max(1, maxQrPixels / totalModules);
  const int qrPixels = totalModules * scale;
  const int originX = (M5.Display.width() - qrPixels) / 2;
  const int originY = 40;

  M5.Display.fillRect(originX, originY, qrPixels, qrPixels, TFT_WHITE);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      if (qrcodegen_getModule(qrCodeBuffer, x, y)) {
        M5.Display.fillRect(originX + (x + quietModules) * scale,
                            originY + (y + quietModules) * scale,
                            scale, scale, TFT_BLACK);
      }
    }
  }

  const int textY = min(originY + qrPixels + 8, M5.Display.height() - 34);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(12, textY);
  if (isWifiQr) {
    M5.Display.printf("SSID: %s\n", AP_SSID);
    M5.Display.printf("PASS: %s", AP_PASSWORD);
  } else {
    M5.Display.println(payload);
    if (networkMode == NetworkMode::Sta) {
      M5.Display.print("Same Wi-Fi only");
    }
  }
  M5.Display.setCursor(220, M5.Display.height() - 14);
  M5.Display.print("Tap: back");
}

void drawDisplaySettingsPage() {
  drawSlider(24, 58, 210, "Brightness", deviceSettings.brightness, DISPLAY_BRIGHTNESS_MIN, DISPLAY_BRIGHTNESS_MAX);
  drawButton(246, 70, 54, 28, "-");
  drawButton(246, 108, 54, 28, "+");
  drawButton(24, 150, 128, 32, displayOn ? "Screen Off" : "Screen On", !displayOn);
}

void drawAudioSettingsPage() {
  drawSlider(24, 72, 210, "Volume", deviceSettings.volume, AUDIO_SPEAKER_VOLUME_MIN, AUDIO_SPEAKER_VOLUME_MAX);
  drawButton(246, 84, 54, 28, "-");
  drawButton(246, 122, 54, 28, "+");
}

void drawServoSettingsPage() {
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(20, 64);
  M5.Display.println("Face direction");
  M5.Display.setCursor(20, 86);
  M5.Display.printf("Saved yaw: %+d\n", motionController.savedYawOffset());
  M5.Display.setCursor(20, 104);
  M5.Display.printf("Saved pitch: %+d\n", motionController.savedPitchOffset());
  M5.Display.setCursor(20, 124);
  M5.Display.println(audioBusyForServoCalibration() ? "Save disabled during audio." : "Adjust face by hand, then save.");
  drawButton(24, 150, 132, 32, "Go to Saved");
  drawButton(164, 150, 132, 32, "Save Direction", audioBusyForServoCalibration());
}

void drawPowerSettingsPage() {
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(20, 56);
  M5.Display.printf("Thermal: %s\n", thermalLevelName(thermalStatus.level));
  M5.Display.printf("Chip: %.1f C\n", thermalStatus.chipTempC);
  if (!isnan(thermalStatus.pmicTempC)) {
    M5.Display.printf("PMIC: %.1f C\n", thermalStatus.pmicTempC);
  } else {
    M5.Display.println("PMIC: n/a");
  }
  M5.Display.printf("Battery: %d %%\n", M5.Power.getBatteryLevel());
  M5.Display.printf("Charging: %s\n", externalPowerPresent() ? "yes" : "no");
  M5.Display.printf("Suggest: %s\n", thermalStatus.suggestLowPower ? "Low Power" : "none");
  drawButton(150, 144, 150, 28, deviceSettings.lowPowerMode ? "Low Power On" : "Low Power Off", deviceSettings.lowPowerMode);
}

void drawInfoScreen() {
  lastInfoDrawMs = millis();
  M5.Display.fillScreen(TFT_BLACK);
  if (activeNetworkQr != NetworkQrType::None) {
    drawNetworkQrScreen();
    return;
  }
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 16);
  M5.Display.println(settingsPageName(settingsPage));

  switch (settingsPage) {
    case SettingsPage::Display:
      drawDisplaySettingsPage();
      break;
    case SettingsPage::Audio:
      drawAudioSettingsPage();
      break;
    case SettingsPage::Servo:
      drawServoSettingsPage();
      break;
    case SettingsPage::Power:
      drawPowerSettingsPage();
      break;
    case SettingsPage::Network:
    default:
      drawNetworkSettingsPage();
      break;
  }

  drawSettingsTabs();
}

bool settingsPageNeedsPeriodicRefresh() {
  return settingsPage == SettingsPage::Network || settingsPage == SettingsPage::Power;
}

void setInfoScreenVisible(bool visible) {
  if (infoScreenVisible == visible) {
    return;
  }
  if (!visible) {
    activeNetworkQr = NetworkQrType::None;
  }
  infoScreenVisible = visible;
  faceController.setEnabled(displayOn && !visible);
  applyDisplayBrightness();
  if (visible) {
    drawInfoScreen();
  }
}

bool touchIn(const m5::touch_detail_t& touch, int32_t x, int32_t y, int32_t w, int32_t h) {
  return touch.x >= x && touch.x < x + w && touch.y >= y && touch.y < y + h;
}

bool isSettingsSwipe(const m5::touch_detail_t& touch) {
  if (!touch.wasFlicked()) {
    return false;
  }
  const int32_t edge = 44;
  return touch.x <= edge || touch.x >= M5.Display.width() - edge ||
         touch.y <= edge || touch.y >= M5.Display.height() - edge;
}

void adjustBrightness(int delta) {
  const uint8_t next = steppedSettingValue(deviceSettings.brightness, delta, DISPLAY_BRIGHTNESS_MIN, DISPLAY_BRIGHTNESS_MAX);
  if (next == deviceSettings.brightness) {
    return;
  }
  deviceSettings.brightness = next;
  applyDisplayBrightness();
  saveDeviceSettings();
  drawInfoScreen();
}

void adjustVolume(int delta) {
  const uint8_t next = steppedSettingValue(deviceSettings.volume, delta, AUDIO_SPEAKER_VOLUME_MIN, AUDIO_SPEAKER_VOLUME_MAX);
  if (next == deviceSettings.volume) {
    return;
  }
  deviceSettings.volume = next;
  audioController.setVolume(deviceSettings.volume);
  saveDeviceSettings();
  drawInfoScreen();
}

bool audioBusyForServoCalibration() {
  return audioController.state() != ChanState::Idle || audioController.isPlaybackDraining();
}

bool handleSettingsTouch(const m5::touch_detail_t& touch) {
  if (activeNetworkQr != NetworkQrType::None) {
    activeNetworkQr = NetworkQrType::None;
    drawInfoScreen();
    return true;
  }

  if (touchIn(touch, 6, 206, 58, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Network;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 68, 206, 58, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Display;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 130, 206, 58, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Audio;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 192, 206, 58, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Servo;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 254, 206, 60, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Power;
    drawInfoScreen();
    return true;
  }

  if (settingsPage == SettingsPage::Network) {
    if (networkMode == NetworkMode::SoftAp && touchIn(touch, 12, 154, 136, 30)) {
      activeNetworkQr = NetworkQrType::WifiConnect;
      drawInfoScreen();
      return true;
    }
    if (setupQrAvailable() && touchIn(touch, 172, 154, 136, 30)) {
      activeNetworkQr = NetworkQrType::Setup;
      drawInfoScreen();
      return true;
    }
  } else if (settingsPage == SettingsPage::Display) {
    if (touchIn(touch, 246, 70, 54, 28)) {
      adjustBrightness(-20);
      return true;
    }
    if (touchIn(touch, 246, 108, 54, 28)) {
      adjustBrightness(20);
      return true;
    }
    if (touchIn(touch, 24, 150, 128, 32)) {
      setDisplayOn(!displayOn);
      return true;
    }
  } else if (settingsPage == SettingsPage::Audio) {
    if (touchIn(touch, 246, 84, 54, 28)) {
      adjustVolume(-20);
      return true;
    }
    if (touchIn(touch, 246, 122, 54, 28)) {
      adjustVolume(20);
      return true;
    }
  } else if (settingsPage == SettingsPage::Servo) {
    if (touchIn(touch, 24, 150, 132, 32)) {
      motionController.moveToSavedHome();
      return true;
    }
    if (touchIn(touch, 164, 150, 132, 32)) {
      if (audioBusyForServoCalibration()) {
        Serial.println("[motion] servo home save ignored during audio");
        return true;
      }
      motionController.saveCurrentPoseAsHome();
      drawInfoScreen();
      return true;
    }
  } else if (settingsPage == SettingsPage::Power) {
    if (touchIn(touch, 150, 144, 150, 28)) {
      applyLowPowerMode(!deviceSettings.lowPowerMode, true);
      return true;
    }
  }

  return false;
}

void drawLowPowerPrompt() {
  if (!displayOn || infoScreenVisible || deviceSettings.lowPowerMode || !thermalStatus.suggestLowPower) {
    return;
  }
  drawButton(M5.Display.width() - 132, M5.Display.height() - 44, 66, 30, "LOW", true);
}

void setState(ChanState state) {
  if (currentState == state) {
    if (state == ChanState::Speaking && audioController.state() != ChanState::Speaking) {
      pendingStateAfterPlayback = false;
      deferredStateReadyMs = 0;
      wsAudioSettleUntilMs = 0;
      faceController.restartSpeakingAnimation();
      audioController.setState(ChanState::Speaking);
      applyDisplayBrightness();
      Serial.println("[state] speaking resynced");
    }
    return;
  }
  const ChanState previousState = currentState;

  if (state != ChanState::Speaking && previousState == ChanState::Speaking) {
    audioController.setState(ChanState::Idle);
    if (audioController.isPlaybackDraining()) {
      pendingStateAfterPlayback = true;
      deferredStateAfterPlayback = state;
      deferredStateReadyMs = 0;
      Serial.printf("[state] %d deferred until audio playback drains\n", static_cast<int>(state));
      return;
    }
  }

  currentState = state;
  pendingStateAfterPlayback = false;
  deferredStateReadyMs = 0;
  if (state == ChanState::Speaking && wsAudioSettleUntilMs != 0 &&
      static_cast<long>(wsAudioSettleUntilMs - millis()) > 0) {
    audioController.deferNextSpeakerStartUntil(wsAudioSettleUntilMs);
    Serial.printf("[audio] first speaker start deferred until websocket settles (%ld ms)\n",
                  static_cast<long>(wsAudioSettleUntilMs - millis()));
    wsAudioSettleUntilMs = 0;
  }
  faceController.setState(state);
  audioController.setState(state);
  applyDisplayBrightness();

  if (state == ChanState::Idle) {
    pendingFaceModeNormalAfterPlayback = false;
    deferredFaceModeReadyMs = 0;
    faceController.setPhotoFaceMode(false);
    currentAuthFaceMode = AuthFaceMode::Unknown;
    vadActive = false;
    audioController.setRemoteVadActive(false);
    cancelListeningNod(true);
    motionController.setMotion("center");
  } else if (state == ChanState::Listening) {
    if (previousState == ChanState::Speaking) {
      pendingFaceModeNormalAfterPlayback = false;
      deferredFaceModeReadyMs = 0;
      faceController.setPhotoFaceMode(false);
    }
    vadActive = false;
    audioController.setRemoteVadActive(false);
    applyListeningPresentation(millis());
  } else if (state == ChanState::Speaking) {
    vadActive = false;
    audioController.setRemoteVadActive(false);
    cancelListeningNod(false);
    motionController.holdCurrentPose();
    faceController.setAuthFaceMode(displayAuthFaceMode(currentAuthFaceMode));
  }

  Serial.printf("[state] changed to %d\n", static_cast<int>(state));
}

void updateDeferredFaceState() {
  if (!pendingStateAfterPlayback) {
    return;
  }
  if (audioController.state() != ChanState::Idle) {
    return;
  }
  const unsigned long now = millis();
  if (deferredStateReadyMs == 0) {
    deferredStateReadyMs = now + AUDIO_SPEAKER_TO_MIC_DELAY_MS;
    return;
  }
  if (now < deferredStateReadyMs) {
    return;
  }

  const ChanState state = deferredStateAfterPlayback;
  pendingStateAfterPlayback = false;
  deferredStateReadyMs = 0;
  currentState = state;
  if (state == ChanState::Idle || state == ChanState::Listening) {
    faceController.setPhotoFaceMode(false);
  }
  faceController.setState(state);
  applyDisplayBrightness();

  if (state == ChanState::Idle) {
    currentAuthFaceMode = AuthFaceMode::Unknown;
    vadActive = false;
    audioController.setRemoteVadActive(false);
    cancelListeningNod(true);
    motionController.setMotion("center");
  } else if (state == ChanState::Listening) {
    currentAuthFaceMode = AuthFaceMode::Unknown;
    vadActive = false;
    audioController.setRemoteVadActive(false);
    applyListeningPresentation(now);
    audioController.setState(ChanState::Listening);
  }

  Serial.printf("[state] deferred %d applied after audio playback\n", static_cast<int>(state));
}

void updateDeferredFaceMode() {
  if (!pendingFaceModeNormalAfterPlayback) {
    return;
  }
  if (audioController.state() != ChanState::Idle) {
    return;
  }

  const unsigned long now = millis();
  if (deferredFaceModeReadyMs == 0) {
    deferredFaceModeReadyMs = now + AUDIO_SPEAKER_TO_MIC_DELAY_MS;
    return;
  }
  if (now < deferredFaceModeReadyMs) {
    return;
  }

  pendingFaceModeNormalAfterPlayback = false;
  deferredFaceModeReadyMs = 0;
  faceController.setPhotoFaceMode(false);
  faceController.setPetFaceMode(false);
  setShakeActive(false, now);
  Serial.println("[face] deferred mode=normal applied after audio playback");
}

void handleStateCommand(const char* value) {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  const ChanState stateBefore = currentState;
  const ChanState audioBefore = audioController.state();
#endif
  if (strcmp(value, "idle") == 0) {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
    Serial.printf("USB JSON state idle received state before=%s audio_before=%s\n",
                  chanStateName(stateBefore),
                  chanStateName(audioBefore));
#endif
    setState(ChanState::Idle);
  } else if (strcmp(value, "listening") == 0) {
    setState(ChanState::Listening);
  } else if (strcmp(value, "speaking") == 0) {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
    usbSerialSpeakingReceivedMs = millis();
    usbSerialFirstPcmMs = 0;
    usbSerialLastPcmMs = 0;
    usbSerialTtsFrameCount = 0;
    usbSerialTtsTotalBytes = 0;
    Serial.printf("USB JSON state speaking received state before=%s audio_before=%s\n",
                  chanStateName(stateBefore),
                  chanStateName(audioBefore));
#endif
    setState(ChanState::Speaking);
  } else {
    Serial.printf("[json] unsupported state: %s\n", value);
    return;
  }
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  if (strcmp(value, "idle") == 0 || strcmp(value, "speaking") == 0) {
    Serial.printf("USB JSON state %s processed state after=%s audio_after=%s\n",
                  value,
                  chanStateName(currentState),
                  chanStateName(audioController.state()));
    if (strcmp(value, "idle") == 0 && usbSerialLastPcmMs != 0) {
      Serial.printf("TTS timing last_pcm_to_idle_ms=%lu\n",
                    static_cast<unsigned long>(millis() - usbSerialLastPcmMs));
    }
  }
#endif
}

void handleFaceModeCommand(const char* value) {
  if (strcmp(value, "photo") == 0) {
    pendingFaceModeNormalAfterPlayback = false;
    deferredFaceModeReadyMs = 0;
    faceController.setPhotoFaceMode(true);
    Serial.println("[face] mode=photo");
  } else if (strcmp(value, "photo_master") == 0) {
    pendingFaceModeNormalAfterPlayback = false;
    deferredFaceModeReadyMs = 0;
    faceController.setPhotoMasterFaceMode(true);
    Serial.println("[face] mode=photo_master");
  } else if (strcmp(value, "normal") == 0) {
    if (currentState == ChanState::Speaking && audioController.state() != ChanState::Idle) {
      pendingFaceModeNormalAfterPlayback = true;
      deferredFaceModeReadyMs = 0;
      Serial.println("[face] mode=normal deferred until audio playback drains");
      return;
    }
    faceController.setPhotoFaceMode(false);
    faceController.setPetFaceMode(false);
    setShakeActive(false, millis());
    Serial.println("[face] mode=normal");
  } else if (strcmp(value, "nadenade") == 0 || strcmp(value, "pet") == 0) {
    if (!displayOn) {
      Serial.println("[face] mode=nadenade ignored while display off");
      return;
    }
    setPettingActive(true, millis());
    Serial.println("[face] mode=nadenade");
  } else if (strcmp(value, "furifuri") == 0 || strcmp(value, "shake") == 0) {
    if (!displayOn) {
      Serial.println("[face] mode=furifuri ignored while display off");
      return;
    }
    setShakeActive(true, millis());
    Serial.println("[face] mode=furifuri");
  } else {
    Serial.printf("[face] unsupported mode: %s\n", value);
  }
}

void handlePetCommand(JsonDocument& doc) {
  const bool active = doc["active"] | doc["value"] | false;
  setPettingActive(active, millis());
}

void handleAuthCommand(const char* result) {
  if (strcmp(result, "master") == 0) {
    currentAuthFaceMode = AuthFaceMode::Master;
    applyListeningPresentation(millis());
#if VERBOSE_LOG_ENABLED
    Serial.println("[auth] master");
#endif
  } else if (strcmp(result, "not_master") == 0) {
    currentAuthFaceMode = AuthFaceMode::NotMaster;
    applyListeningPresentation(millis());
#if VERBOSE_LOG_ENABLED
    Serial.println("[auth] not_master");
#endif
  } else if (strcmp(result, "unknown") == 0 || strcmp(result, "none") == 0) {
    currentAuthFaceMode = AuthFaceMode::Unknown;
    applyListeningPresentation(millis());
#if VERBOSE_LOG_ENABLED
    Serial.println("[auth] unknown");
#endif
  } else {
    Serial.printf("[auth] unsupported result: %s\n", result);
  }
}

void handleVadCommand(bool active) {
  vadActive = active;
  audioController.setRemoteVadActive(active);
  applyListeningPresentation(millis());
#if VERBOSE_LOG_ENABLED
  Serial.printf("[vad] %s\n", vadActive ? "active" : "inactive");
#endif
}

void writeAffectionSnapshot(JsonDocument& doc, unsigned long now) {
  const AffectionState& affection = affectionController.state();
  doc["seq"] = affection.seq;
  doc["timestampMs"] = now;
  doc["affection"] = affection.affection;
  doc["mood"] = affection.mood;
  doc["confusion"] = affection.confusion;
  doc["level"] = affectionController.level();
  doc["levelIndex"] = affectionController.levelIndex();
  doc["visualTier"] = affectionController.visualTier();
  doc["styleId"] = affectionController.styleId();
}

void writeAffectionState(JsonDocument& doc, const char* requestId, unsigned long now) {
  doc["type"] = "affection.state";
  if (requestId != nullptr && requestId[0] != '\0') {
    doc["requestId"] = requestId;
  }
  writeAffectionSnapshot(doc, now);
}

bool sendUsbSerialJson(const char* payload) {
#if USB_SERIAL_PROTOCOL_ENABLED
  if (!usbSerialClientConnected || payload == nullptr) {
    return false;
  }
  if (usbSerialFramedMode) {
    const bool sent = sendUsbSerialFrame(kUsbSerialTypeJson,
                                         reinterpret_cast<const uint8_t*>(payload),
                                         strlen(payload));
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
    Serial.printf("USB TX JSON framed length=%u ok=%d at_ms=%lu\n",
                  static_cast<unsigned>(strlen(payload)),
                  sent ? 1 : 0,
                  static_cast<unsigned long>(millis()));
#endif
    return sent;
  }
  const size_t payloadWritten = Serial.write(reinterpret_cast<const uint8_t*>(payload), strlen(payload));
  const size_t newlineWritten = Serial.write('\n');
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
  Serial.printf("USB TX JSON line length=%u wrote=%u ok=%d at_ms=%lu\n",
                static_cast<unsigned>(strlen(payload)),
                static_cast<unsigned>(payloadWritten + newlineWritten),
                (payloadWritten == strlen(payload) && newlineWritten == 1) ? 1 : 0,
                static_cast<unsigned long>(millis()));
#endif
  return true;
#else
  (void)payload;
  return false;
#endif
}

bool sendUsbSerialFrame(uint8_t type, const uint8_t* payload, size_t length, uint8_t flags) {
#if USB_SERIAL_PROTOCOL_ENABLED
  if (!usbSerialClientConnected || length > USB_SERIAL_FRAME_MAX_PAYLOAD_BYTES ||
      (length > 0 && payload == nullptr)) {
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
    Serial.printf("USB TX frame skipped type=0x%02x length=%u connected=%d\n",
                  type,
                  static_cast<unsigned>(length),
                  usbSerialClientConnected ? 1 : 0);
#endif
    return false;
  }

  uint8_t header[16] = {
    'S', 'C', 'U', '1',
    kUsbSerialVersion,
    type,
    flags,
    0,
  };
  writeLe32(header + 8, usbSerialTxSeq++);
  writeLe32(header + 12, static_cast<uint32_t>(length));

  uint32_t crc = 0xFFFFFFFFUL;
  crc = crc32Update(crc, header + 4, 12);
  if (length > 0) {
    crc = crc32Update(crc, payload, length);
  }
  crc = ~crc;

  uint8_t crcBytes[4];
  writeLe32(crcBytes, crc);
  const size_t headerWritten = Serial.write(header, sizeof(header));
  size_t payloadWritten = 0;
  if (length > 0) {
    payloadWritten = Serial.write(payload, length);
  }
  const size_t crcWritten = Serial.write(crcBytes, sizeof(crcBytes));
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
  if (type == kUsbSerialTypePong || type == kUsbSerialTypeJson || type == kUsbSerialTypeError) {
    Serial.printf("USB TX frame type=0x%02x seq=%lu length=%u wrote=%u/%u at_ms=%lu\n",
                  type,
                  static_cast<unsigned long>(usbSerialTxSeq - 1),
                  static_cast<unsigned>(length),
                  static_cast<unsigned>(headerWritten + payloadWritten + crcWritten),
                  static_cast<unsigned>(sizeof(header) + length + sizeof(crcBytes)),
                  static_cast<unsigned long>(millis()));
  }
#endif
  return true;
#else
  (void)type;
  (void)payload;
  (void)length;
  (void)flags;
  return false;
#endif
}

bool sendUsbSerialMicPacket(const uint8_t* payload, size_t length, void* context) {
  (void)context;
  return sendUsbSerialFrame(kUsbSerialTypeMicPcm, payload, length);
}

void sendAffectionState(const char* requestId) {
  if (!wsServer.hasClient() && !usbSerialClientConnected) {
    return;
  }

  JsonDocument doc;
  writeAffectionState(doc, requestId, millis());

  String body;
  serializeJson(doc, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

void sendInteractionEvent(const char* event, const char* phase, unsigned long now) {
  if ((!wsServer.hasClient() && !usbSerialClientConnected) || event == nullptr || phase == nullptr) {
    return;
  }

  JsonDocument doc;
  doc["type"] = "interaction.event";
  doc["event"] = event;
  doc["phase"] = phase;
  doc["source"] = "device";
  writeAffectionSnapshot(doc, now);

  String body;
  serializeJson(doc, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

bool sendCameraButtonEvent(unsigned long now) {
  if (!appClientConnected()) {
    return false;
  }
  updateCameraButtonPending(now);
  if (cameraButtonPending) {
    return false;
  }
  if (lastCameraButtonEventMs != 0 && now - lastCameraButtonEventMs < CAMERA_BUTTON_COOLDOWN_MS) {
    return false;
  }

  lastCameraButtonEventMs = now;
  cameraButtonPending = true;
  faceController.setCameraButtonPending(true);

  JsonDocument doc;
  doc["type"] = "interaction.event";
  doc["event"] = "camera_button";
  doc["phase"] = "pressed";
  doc["source"] = "device";
  doc["seq"] = ++cameraButtonEventSeq;

  String body;
  serializeJson(doc, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
  Serial.printf("[interaction] camera_button pressed seq=%lu\n",
                static_cast<unsigned long>(cameraButtonEventSeq));
  return true;
}

void updateCameraButtonPending(unsigned long now) {
  if (!cameraButtonPending) {
    return;
  }
  if (!appClientConnected()) {
    clearCameraButtonPending("disconnect");
    return;
  }
  if (now - lastCameraButtonEventMs >= CAMERA_BUTTON_RESPONSE_TIMEOUT_MS) {
    clearCameraButtonPending("timeout");
  }
}

void clearCameraButtonPending(const char* reason) {
  if (!cameraButtonPending) {
    return;
  }
  cameraButtonPending = false;
  faceController.setCameraButtonPending(false);
  Serial.printf("[interaction] camera_button ready: %s\n", reason == nullptr ? "response" : reason);
}

const char* normalizeAffectionSource(const char* source) {
  if (source == nullptr || source[0] == '\0') {
    return "phone";
  }
  if (strcmp(source, "app") == 0 || strcmp(source, "phone") == 0) {
    return "phone";
  }
  if (strcmp(source, "device") == 0) {
    return "device";
  }
  if (strcmp(source, "debug") == 0) {
    return "debug";
  }
  return "phone";
}

void handleAffectionEventCommand(JsonDocument& doc) {
  const char* eventName = doc["event"] | "";
  const char* eventId = doc["id"] | "";
  const char* source = normalizeAffectionSource(doc["source"] | "phone");
  const float confidence = doc["confidence"] | 1.0f;
  const float intensity = doc["intensity"] | 1.0f;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.applyEvent(eventName, confidence, intensity, eventId, now);
  const bool deferDelta = strcmp(source, "phone") == 0 &&
                          (strcmp(eventName, "talk") == 0 ||
                           strcmp(eventName, "positive_talk") == 0 ||
                           strcmp(eventName, "negative_talk") == 0);
  applyAffectionResult(result, now, true, doc["requestId"] | "", deferDelta);
  if (result.duplicate) {
    sendAffectionState(doc["requestId"] | "");
  }
}

void handleAffectionGetCommand(JsonDocument& doc) {
  sendAffectionState(doc["requestId"] | "");
}

void handleAffectionResetCommand(JsonDocument& doc) {
  const int value = doc["value"] | 500;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.reset(value);
  applyAffectionResult(result, now, true, doc["requestId"] | "");
}

void handleAffectionDebugAdjustCommand(JsonDocument& doc) {
  const int delta = doc["delta"] | 0;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.debugAdjust(delta);
  applyAffectionResult(result, now, true, doc["requestId"] | "");
}

void handleAffectionDebugSetCommand(JsonDocument& doc) {
  const bool hasLevelIndex = !doc["levelIndex"].isNull();
  const bool hasAffection = !doc["affection"].isNull() || !doc["value"].isNull();
  const bool hasMood = !doc["mood"].isNull();
  const bool hasConfusion = !doc["confusion"].isNull();
  const uint8_t levelIndex = doc["levelIndex"] | 3;
  const int affection = doc["affection"] | doc["value"] | 500;
  const int mood = doc["mood"] | 0;
  const int confusion = doc["confusion"] | 0;
  const bool persist = doc["persist"] | false;
  const unsigned long now = millis();

  const AffectionApplyResult result = affectionController.debugSet(
    hasAffection,
    affection,
    hasLevelIndex,
    levelIndex,
    hasMood,
    mood,
    hasConfusion,
    confusion,
    persist
  );
  applyAffectionResult(result, now, true, doc["requestId"] | "");
  if (!result.applied) {
    sendAffectionState(doc["requestId"] | "");
  }
}

void handleJsonCommand(const uint8_t* payload, size_t length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.printf("[json] parse error: %s\n", error.c_str());
    return;
  }

  const char* type = doc["type"] | "";
#if FACE_DIAG_LOG_ENABLED
  if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
    if (strcmp(type, "state") == 0 || strcmp(type, "face_mode") == 0 ||
        strcmp(type, "face") == 0 || strcmp(type, "motion") == 0 ||
        strcmp(type, "pet") == 0 || strcmp(type, "nadenade") == 0 ||
        strcmp(type, "auth") == 0 || strcmp(type, "vad") == 0) {
      const char* value = doc["value"] | doc["name"] | doc["result"] | "";
      Serial.printf("[face_diag] ws type=%s value=%s current=%d audio=%d\n",
                    type,
                    value,
                    static_cast<int>(currentState),
                    static_cast<int>(audioController.state()));
    }
  }
#endif
  if (strcmp(type, "state") == 0) {
    const char* value = doc["value"] | "";
    handleStateCommand(value);
  } else if (strcmp(type, "affection.event") == 0) {
    handleAffectionEventCommand(doc);
  } else if (strcmp(type, "affection.get") == 0) {
    handleAffectionGetCommand(doc);
  } else if (strcmp(type, "affection.reset") == 0) {
    handleAffectionResetCommand(doc);
  } else if (strcmp(type, "affection.debug_adjust") == 0) {
    handleAffectionDebugAdjustCommand(doc);
  } else if (strcmp(type, "affection.debug_set") == 0) {
    handleAffectionDebugSetCommand(doc);
  } else if (strcmp(type, "auth") == 0) {
    const char* result = doc["result"] | "";
    handleAuthCommand(result);
  } else if (strcmp(type, "vad") == 0) {
    bool active = doc["active"] | false;
    handleVadCommand(active);
  } else if (strcmp(type, "face_mode") == 0) {
    const char* value = doc["value"] | "";
    handleFaceModeCommand(value);
  } else if (strcmp(type, "pet") == 0 || strcmp(type, "nadenade") == 0) {
    handlePetCommand(doc);
  } else if (strcmp(type, "face") == 0) {
    const char* value = doc["value"] | "";
    faceController.showFace(value);
  } else if (strcmp(type, "motion") == 0) {
    const char* name = doc["name"] | "";
    if (strcmp(name, "look_away") == 0 || strcmp(name, "not_master") == 0) {
      currentAuthFaceMode = AuthFaceMode::NotMaster;
      if (currentState == ChanState::Listening) {
        vadActive = true;
        audioController.setRemoteVadActive(true);
        applyListeningPresentation(millis());
      } else {
        cancelListeningNod(false);
        faceController.setAuthFaceMode(displayAuthFaceMode(AuthFaceMode::NotMaster));
      }
    }
    motionController.setMotion(name);
  } else if (strcmp(type, "pose") == 0) {
    int pan = doc["pan"] | SERVO_PAN_CENTER;
    int tilt = doc["tilt"] | SERVO_TILT_CENTER;
    motionController.setTargetPose(pan, tilt);
  } else {
    Serial.printf("[json] unsupported type: %s\n", type);
  }
}

void handleUsbSerialLine(const uint8_t* payload, size_t length) {
#if USB_SERIAL_PROTOCOL_ENABLED
  if (payload == nullptr || length == 0) {
    return;
  }
  usbSerialFramedMode = false;
  handleUsbSerialJsonPayload(payload, length);
#else
  (void)payload;
  (void)length;
#endif
}

void handleUsbSerialJsonPayload(const uint8_t* payload, size_t length) {
#if USB_SERIAL_PROTOCOL_ENABLED
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    if (usbSerialClientConnected) {
      JsonDocument err;
      err["type"] = "error";
      err["source"] = "usb_serial";
      err["error"] = "json_parse";
      err["detail"] = error.c_str();
      String body;
      serializeJson(err, body);
      sendUsbSerialJson(body.c_str());
    }
    return;
  }

  usbSerialClientConnected = true;
  usbSerialLastRxMs = millis();
  audioController.setUsbSerialClientConnected(true);
  updateMicStatusOverlay();
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
  Serial.printf("USB client connected at_ms=%lu framed=%d\n",
                static_cast<unsigned long>(usbSerialLastRxMs),
                usbSerialFramedMode ? 1 : 0);
#endif

  const char* type = doc["type"] | "";
  if (strcmp(type, "ping") == 0) {
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
    Serial.printf("USB ping JSON received id=%s at_ms=%lu framed=%d\n",
                  doc["id"] | "",
                  static_cast<unsigned long>(millis()),
                  usbSerialFramedMode ? 1 : 0);
#endif
    JsonDocument pong;
    pong["type"] = "pong";
    const char* id = doc["id"] | "";
    if (id != nullptr && id[0] != '\0') {
      pong["id"] = id;
    }
    pong["timestampMs"] = millis();

    String body;
    serializeJson(pong, body);
    const bool sent = sendUsbSerialJson(body.c_str());
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
    Serial.printf("USB pong JSON sent ok=%d at_ms=%lu\n",
                  sent ? 1 : 0,
                  static_cast<unsigned long>(millis()));
#endif
    return;
  }
  if (strcmp(type, "capture.request") == 0) {
    handleUsbSerialCaptureRequest(doc);
    return;
  }

  clearCameraButtonPending("usb_serial");
  handleJsonCommand(payload, length);
#else
  (void)payload;
  (void)length;
#endif
}

void handleUsbSerialCaptureRequest(JsonDocument& doc) {
#if USB_SERIAL_PROTOCOL_ENABLED
  const char* requestId = doc["id"] | "";
  JsonDocument start;
  start["type"] = "capture.start";
  if (requestId[0] != '\0') {
    start["id"] = requestId;
  }

  faceController.setPhotoFaceMode(true);
  faceController.showFace("photo_0");
  const bool micPausedForCapture = CAMERA_PAUSE_MIC_DURING_CAPTURE
                                     ? audioController.pauseMicForCapture()
                                     : false;

  if (!cameraManager.isReady() && !cameraManager.init()) {
    audioController.resumeMicAfterCapture(micPausedForCapture);
    JsonDocument end;
    end["type"] = "capture.end";
    if (requestId[0] != '\0') {
      end["id"] = requestId;
    }
    end["ok"] = false;
    end["error"] = "camera_not_ready";
    String body;
    serializeJson(end, body);
    sendUsbSerialJson(body.c_str());
    return;
  }

  uint8_t* jpg = nullptr;
  size_t jpgLen = 0;
  if (!cameraManager.captureJpeg(&jpg, &jpgLen)) {
    cameraManager.deinit();
    audioController.resumeMicAfterCapture(micPausedForCapture);
    JsonDocument end;
    end["type"] = "capture.end";
    if (requestId[0] != '\0') {
      end["id"] = requestId;
    }
    end["ok"] = false;
    end["error"] = "capture_failed";
    String body;
    serializeJson(end, body);
    sendUsbSerialJson(body.c_str());
    return;
  }

  start["contentType"] = "image/jpeg";
  start["length"] = static_cast<uint32_t>(jpgLen);
  String body;
  serializeJson(start, body);
  sendUsbSerialJson(body.c_str());

  size_t offset = 0;
  while (offset < jpgLen) {
    const size_t chunk = min(static_cast<size_t>(USB_SERIAL_CAPTURE_CHUNK_BYTES), jpgLen - offset);
    sendUsbSerialFrame(kUsbSerialTypeCaptureImageChunk, jpg + offset, chunk);
    offset += chunk;
    delay(1);
  }

  cameraManager.releaseBuffer(jpg);
  cameraManager.deinit();
  delay(80);
  audioController.resumeMicAfterCapture(micPausedForCapture);
  audioController.deferNextSpeakerStartUntil(millis() + AUDIO_AFTER_CAPTURE_SPEAKER_DELAY_MS);

  JsonDocument end;
  end["type"] = "capture.end";
  if (requestId[0] != '\0') {
    end["id"] = requestId;
  }
  end["ok"] = true;
  body = "";
  serializeJson(end, body);
  sendUsbSerialJson(body.c_str());
#else
  (void)doc;
#endif
}

void handleUsbSerialFrame(uint8_t type, uint8_t flags, uint32_t seq, const uint8_t* payload, size_t length) {
#if USB_SERIAL_PROTOCOL_ENABLED
  (void)flags;
  usbSerialClientConnected = true;
  usbSerialFramedMode = true;
  usbSerialLastRxMs = millis();
  audioController.setUsbSerialClientConnected(true);
  updateMicStatusOverlay();

#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  if (type == kUsbSerialTypeJson || type == kUsbSerialTypeTtsPcm || type == kUsbSerialTypePing) {
    Serial.printf("SCU1 rx type=0x%02x seq=%lu length=%u crc_ok=1\n",
                  type,
                  static_cast<unsigned long>(seq),
                  static_cast<unsigned>(length));
  }
#endif

  switch (type) {
    case kUsbSerialTypeJson:
      handleUsbSerialJsonPayload(payload, length);
      break;
    case kUsbSerialTypeTtsPcm: {
      clearCameraButtonPending("usb_audio");
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
      const unsigned long now = millis();
      if (usbSerialFirstPcmMs == 0) {
        usbSerialFirstPcmMs = now;
        if (usbSerialSpeakingReceivedMs != 0) {
          Serial.printf("TTS timing speaking_to_first_pcm_ms=%lu\n",
                        static_cast<unsigned long>(now - usbSerialSpeakingReceivedMs));
        }
      }
      usbSerialLastPcmMs = now;
      ++usbSerialTtsFrameCount;
      usbSerialTtsTotalBytes += static_cast<uint32_t>(length);
      uint32_t nonZeroSamples = 0;
      int32_t peakAbs = 0;
      const size_t sampleCount = length / sizeof(int16_t);
      for (size_t i = 0; i < sampleCount; ++i) {
        const int16_t sample = static_cast<int16_t>(
          static_cast<uint16_t>(payload[i * 2]) |
          (static_cast<uint16_t>(payload[i * 2 + 1]) << 8)
        );
        if (sample != 0) {
          ++nonZeroSamples;
        }
        const int32_t absSample = sample == INT16_MIN ? 32768 : abs(sample);
        if (absSample > peakAbs) {
          peakAbs = absSample;
        }
      }
      Serial.printf("USB TTS PCM received length=%u state=%s audio_state=%s total_usb_tts_bytes=%lu frame_count=%lu\n",
                    static_cast<unsigned>(length),
                    chanStateName(currentState),
                    chanStateName(audioController.state()),
                    static_cast<unsigned long>(usbSerialTtsTotalBytes),
                    static_cast<unsigned long>(usbSerialTtsFrameCount));
      if (audioController.state() != ChanState::Speaking) {
        Serial.printf("USB TTS PCM dropped reason=not_speaking state=%s audio_state=%s\n",
                      chanStateName(currentState),
                      chanStateName(audioController.state()));
      }
      if (usbSerialTtsFrameCount <= 6 || (usbSerialTtsFrameCount % 10) == 0) {
        Serial.printf("USB TTS PCM stats length=%u non_zero_samples=%lu peak_abs=%ld\n",
                      static_cast<unsigned>(length),
                      static_cast<unsigned long>(nonZeroSamples),
                      static_cast<long>(peakAbs));
      }
#endif
      audioController.onBinaryReceived(const_cast<uint8_t*>(payload), length);
      break;
    }
    case kUsbSerialTypeCaptureRequest:
      if (length > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error) {
          handleUsbSerialCaptureRequest(doc);
        }
      }
      break;
    case kUsbSerialTypePing: {
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
      Serial.printf("USB ping frame received seq=%lu length=%u at_ms=%lu\n",
                    static_cast<unsigned long>(seq),
                    static_cast<unsigned>(length),
                    static_cast<unsigned long>(millis()));
#endif
      JsonDocument pong;
      pong["type"] = "pong";
      if (length > 0) {
        JsonDocument ping;
        if (!deserializeJson(ping, payload, length)) {
          const char* id = ping["id"] | "";
          if (id[0] != '\0') {
            pong["id"] = id;
          }
        }
      }
      pong["timestampMs"] = millis();
      String body;
      serializeJson(pong, body);
      const bool sent = sendUsbSerialFrame(kUsbSerialTypePong, reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
      Serial.printf("USB pong frame sent ok=%d at_ms=%lu\n",
                    sent ? 1 : 0,
                    static_cast<unsigned long>(millis()));
#endif
      break;
    }
    default: {
      JsonDocument err;
      err["type"] = "error";
      err["source"] = "usb_serial";
      err["error"] = "unsupported_frame_type";
      err["frameType"] = type;
      String body;
      serializeJson(err, body);
      sendUsbSerialJson(body.c_str());
      break;
    }
  }
#else
  (void)type;
  (void)flags;
  (void)seq;
  (void)payload;
  (void)length;
#endif
}

void updateUsbSerial(unsigned long now) {
#if USB_SERIAL_PROTOCOL_ENABLED
  if (usbSerialClientConnected && now - usbSerialLastRxMs > USB_SERIAL_CLIENT_TIMEOUT_MS) {
    usbSerialClientConnected = false;
    usbSerialFramedMode = false;
    audioController.setUsbSerialClientConnected(false);
    updateMicStatusOverlay();
    clearCameraButtonPending("usb_timeout");
  }

  uint8_t rxDiagFirstBytes[16] = {};
  size_t rxDiagFirstLength = 0;
  size_t rxDiagBytesRead = 0;
  size_t budget = USB_SERIAL_READ_BUDGET_BYTES;
  while (budget-- > 0 && Serial.available() > 0) {
    const int value = Serial.read();
    if (value < 0) {
      break;
    }
    if (rxDiagFirstLength < sizeof(rxDiagFirstBytes)) {
      rxDiagFirstBytes[rxDiagFirstLength++] = static_cast<uint8_t>(value);
    }
    ++rxDiagBytesRead;

    const char ch = static_cast<char>(value);
    switch (usbSerialRxState) {
      case UsbSerialRxState::Line:
        if (static_cast<uint8_t>(value) == kUsbSerialMagic[usbSerialMagicIndex]) {
          if (usbSerialMagicIndex == 0) {
            usbSerialLineLength = 0;
            usbSerialLineOverflow = false;
          }
          usbSerialFrameHeader[usbSerialMagicIndex++] = static_cast<uint8_t>(value);
          if (usbSerialMagicIndex == sizeof(kUsbSerialMagic)) {
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
            Serial.printf("USB RX SCU1 magic detected at_ms=%lu\n",
                          static_cast<unsigned long>(millis()));
#endif
            usbSerialRxState = UsbSerialRxState::Header;
            usbSerialHeaderIndex = sizeof(kUsbSerialMagic);
            usbSerialMagicIndex = 0;
          }
          continue;
        }
        if (usbSerialMagicIndex != 0) {
          for (uint8_t i = 0; i < usbSerialMagicIndex; ++i) {
            if (usbSerialLineLength + 1 < USB_SERIAL_LINE_BUFFER_BYTES && !usbSerialLineOverflow) {
              usbSerialLineBuffer[usbSerialLineLength++] = static_cast<char>(kUsbSerialMagic[i]);
              usbSerialLineBuffer[usbSerialLineLength] = '\0';
            }
          }
          usbSerialMagicIndex = 0;
        }
        if (ch == '\r') {
          continue;
        }
        if (ch == '\n') {
          if (!usbSerialLineOverflow && usbSerialLineLength > 0) {
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
            Serial.printf("USB RX raw JSON line length=%u at_ms=%lu\n",
                          static_cast<unsigned>(usbSerialLineLength),
                          static_cast<unsigned long>(millis()));
#endif
            handleUsbSerialLine(reinterpret_cast<const uint8_t*>(usbSerialLineBuffer), usbSerialLineLength);
          }
          usbSerialLineLength = 0;
          usbSerialLineOverflow = false;
          continue;
        }

        if (usbSerialLineLength + 1 >= USB_SERIAL_LINE_BUFFER_BYTES) {
          usbSerialLineOverflow = true;
          continue;
        }
        if (!usbSerialLineOverflow) {
          usbSerialLineBuffer[usbSerialLineLength++] = ch;
          usbSerialLineBuffer[usbSerialLineLength] = '\0';
        }
        break;

      case UsbSerialRxState::Header:
        usbSerialFrameHeader[usbSerialHeaderIndex++] = static_cast<uint8_t>(value);
        if (usbSerialHeaderIndex >= sizeof(usbSerialFrameHeader)) {
          usbSerialFrameLength = readLe32(usbSerialFrameHeader + 12);
          usbSerialPayloadIndex = 0;
          usbSerialCrcIndex = 0;
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
          Serial.printf("SCU1 header version=0x%02x type=0x%02x seq=%lu length=%lu at_ms=%lu\n",
                        usbSerialFrameHeader[4],
                        usbSerialFrameHeader[5],
                        static_cast<unsigned long>(readLe32(usbSerialFrameHeader + 8)),
                        static_cast<unsigned long>(usbSerialFrameLength),
                        static_cast<unsigned long>(millis()));
#endif
          if (usbSerialFrameHeader[4] != kUsbSerialVersion ||
              usbSerialFrameLength > USB_SERIAL_FRAME_MAX_PAYLOAD_BYTES) {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
            Serial.printf("SCU1 invalid header version=0x%02x seq=%lu length=%lu max=%u\n",
                          usbSerialFrameHeader[4],
                          static_cast<unsigned long>(readLe32(usbSerialFrameHeader + 8)),
                          static_cast<unsigned long>(usbSerialFrameLength),
                          static_cast<unsigned>(USB_SERIAL_FRAME_MAX_PAYLOAD_BYTES));
#endif
            usbSerialRxState = UsbSerialRxState::DropFrame;
          } else {
            usbSerialRxState = usbSerialFrameLength == 0 ? UsbSerialRxState::Crc : UsbSerialRxState::Payload;
          }
        }
        break;

      case UsbSerialRxState::Payload:
        usbSerialFramePayload[usbSerialPayloadIndex++] = static_cast<uint8_t>(value);
        if (usbSerialPayloadIndex >= usbSerialFrameLength) {
          usbSerialCrcIndex = 0;
          usbSerialRxState = UsbSerialRxState::Crc;
        }
        break;

      case UsbSerialRxState::Crc:
        usbSerialFrameCrcBytes[usbSerialCrcIndex++] = static_cast<uint8_t>(value);
        if (usbSerialCrcIndex >= sizeof(usbSerialFrameCrcBytes)) {
          uint32_t crc = 0xFFFFFFFFUL;
          crc = crc32Update(crc, usbSerialFrameHeader + 4, 12);
          if (usbSerialFrameLength > 0) {
            crc = crc32Update(crc, usbSerialFramePayload, usbSerialFrameLength);
          }
          crc = ~crc;
          const uint32_t actualCrc = readLe32(usbSerialFrameCrcBytes);
          if (crc == actualCrc) {
            handleUsbSerialFrame(usbSerialFrameHeader[5],
                                 usbSerialFrameHeader[6],
                                 readLe32(usbSerialFrameHeader + 8),
                                 usbSerialFramePayload,
                                 usbSerialFrameLength);
          } else {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
            Serial.printf("SCU1 crc mismatch seq=%lu expected=0x%08lx actual=0x%08lx length=%lu\n",
                          static_cast<unsigned long>(readLe32(usbSerialFrameHeader + 8)),
                          static_cast<unsigned long>(crc),
                          static_cast<unsigned long>(actualCrc),
                          static_cast<unsigned long>(usbSerialFrameLength));
#endif
          }
          usbSerialRxState = UsbSerialRxState::Line;
          usbSerialHeaderIndex = 0;
          usbSerialPayloadIndex = 0;
          usbSerialCrcIndex = 0;
        }
        break;

      case UsbSerialRxState::DropFrame:
        usbSerialRxState = UsbSerialRxState::Line;
        usbSerialMagicIndex = 0;
        usbSerialLineLength = 0;
        usbSerialLineOverflow = false;
        break;
      }
  }
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
  if (rxDiagBytesRead > 0 &&
      (usbSerialRxDiagEventCount < 40 || rxDiagFirstLength >= sizeof(kUsbSerialMagic))) {
    ++usbSerialRxDiagEventCount;
    Serial.printf("USB RX bytes length=%u first%u=",
                  static_cast<unsigned>(rxDiagBytesRead),
                  static_cast<unsigned>(rxDiagFirstLength));
    printHexBytes(rxDiagFirstBytes, rxDiagFirstLength);
    Serial.printf(" state=%u at_ms=%lu\n",
                  static_cast<unsigned>(usbSerialRxState),
                  static_cast<unsigned long>(millis()));
  }
#endif
#else
  (void)now;
#endif
}

void onWsText(uint8_t clientId, const uint8_t* payload, size_t length) {
  (void)clientId;
#if VERBOSE_LOG_ENABLED
  Serial.printf("[ws] text %u bytes\n", static_cast<unsigned>(length));
#endif
  clearCameraButtonPending("text");
  handleJsonCommand(payload, length);
}

void onWsBinary(uint8_t clientId, uint8_t* payload, size_t length) {
  (void)clientId;
  clearCameraButtonPending("binary");
  audioController.onBinaryReceived(payload, length);
}

void onWsConnection(uint8_t clientId, bool connected) {
  (void)clientId;
  wsClientConnected = connected;
  updateMicStatusOverlay();
  applyDisplayBrightness();
  applyThermalFaceMode();
  if (connected) {
    const unsigned long now = millis();
    wsAudioSettleUntilMs = now + AUDIO_WS_CONNECT_SETTLE_MS;
    audioController.deferMicCaptureUntil(wsAudioSettleUntilMs);
    Serial.printf("[ws] audio settle %u ms\n", AUDIO_WS_CONNECT_SETTLE_MS);
    applyAffectionResult(affectionController.resetTransient(), now, false);
    sendAffectionState();
    sendInteractionEvent("session_start", "instant", now);
  } else {
    wsAudioSettleUntilMs = 0;
    clearCameraButtonPending("disconnect");
  }
  if (infoScreenVisible) {
    drawInfoScreen();
  }
}

void connectWiFi() {
  if (networkMode == NetworkMode::SoftAp) {
    IPAddress localIP(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
    IPAddress gateway(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    const bool configOk = WiFi.softAPConfig(localIP, gateway, subnet);
    const bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);

    if (!configOk || !apOk) {
      Serial.printf("[wifi] SoftAP failed config=%d ap=%d\n", configOk, apOk);
      drawBootScreen("SoftAP failed");
      return;
    }

    Serial.printf("[wifi] SoftAP started ssid=%s ip=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    startServers();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (!isWifiCredentialConfigured(currentWifiIndex) && !selectNextConfiguredWifi()) {
    Serial.println("[wifi] no STA credentials configured");
    drawBootScreen("WiFi setup AP");
    networkMode = NetworkMode::SoftAp;
    connectWiFi();
    return;
  }

  WiFi.begin(wifiCredentials[currentWifiIndex].ssid.c_str(), wifiCredentials[currentWifiIndex].password.c_str());
  Serial.printf("[wifi] connecting to %s\n", wifiCredentials[currentWifiIndex].ssid.c_str());
}

void updateWiFi(unsigned long now) {
  if (now - lastWifiCheckMs < 1000) {
    return;
  }
  lastWifiCheckMs = now;

  if (networkMode == NetworkMode::SoftAp) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectAttempts = 0;
    if (!wsStarted) {
      Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
      startServers();
    }
    return;
  }

  ++wifiConnectAttempts;
  if (wifiConnectAttempts >= 8) {
    wifiConnectAttempts = 0;
    if (!selectNextConfiguredWifi()) {
      Serial.println("[wifi] no STA credentials configured");
      return;
    }
    Serial.printf("[wifi] switching candidate to %s\n", wifiCredentials[currentWifiIndex].ssid.c_str());
  }

  Serial.println("[wifi] disconnected, reconnecting");
  WiFi.disconnect();
  WiFi.begin(wifiCredentials[currentWifiIndex].ssid.c_str(), wifiCredentials[currentWifiIndex].password.c_str());
}

void updateTouch(unsigned long now) {
  if (!interactionsReady(now)) {
    return;
  }

  auto touch = M5.Touch.getDetail();
  if (!displayOn) {
    return;
  }

  if (infoScreenVisible && settingsPage == SettingsPage::Network &&
      activeNetworkQr == NetworkQrType::None && touch.wasHold()) {
    switchNetworkModeAndRestart();
    return;
  }

  if (touch.wasClicked()) {
    if (!infoScreenVisible && appClientConnected() &&
        touchIn(touch, M5.Display.width() - 40, M5.Display.height() - 144, 40, 72)) {
      sendCameraButtonEvent(now);
      return;
    }
    if (!infoScreenVisible && appClientConnected() &&
        touchIn(touch, M5.Display.width() - 40, M5.Display.height() - 80, 40, 80)) {
      audioController.setMicMuted(!audioController.micMuted());
      updateMicStatusOverlay();
      return;
    }
    if (!infoScreenVisible && thermalStatus.suggestLowPower && !deviceSettings.lowPowerMode &&
        touchIn(touch, M5.Display.width() - 132, M5.Display.height() - 44, 66, 30)) {
      applyLowPowerMode(true, true);
      return;
    }
    if (infoScreenVisible) {
      handleSettingsTouch(touch);
      return;
    }
  }

  if (isSettingsSwipe(touch)) {
    setInfoScreenVisible(!infoScreenVisible);
    return;
  }
}

void updateButtons(unsigned long now) {
  (void)now;
  if (M5.BtnPWR.wasClicked()) {
    setDisplayOn(!displayOn);
  }
}

void updateBackTouch(unsigned long now) {
  if (!displayOn) {
    setPettingActive(false, now);
    backTouchReady = false;
    backTouchReleasedSinceMs = 0;
    backTouchCandidateSinceMs = 0;
    backTouchClearSinceMs = 0;
    return;
  }

  if (infoScreenVisible) {
    return;
  }

  // This is the physical touch sensor on Stack-chan's back, not the CoreS3 screen.
  auto& touchSensor = M5StackChan.TouchSensor;
  const auto& intensities = touchSensor.getIntensities();
  const uint8_t maxIntensity = max(intensities[0], max(intensities[1], intensities[2]));
  const bool backTouchDetected = maxIntensity >= BACK_TOUCH_INTENSITY_THRESHOLD;

  if (!backTouchReady) {
    backTouchCandidateSinceMs = 0;
    backTouchClearSinceMs = 0;
    if (!interactionsReady(now) || backTouchDetected) {
      backTouchReleasedSinceMs = 0;
      return;
    }
    if (backTouchReleasedSinceMs == 0) {
      backTouchReleasedSinceMs = now;
      return;
    }
    if (now - backTouchReleasedSinceMs < BACK_TOUCH_STARTUP_RELEASE_MS) {
      return;
    }
    backTouchReady = true;
    Serial.println("[touch] back touch ready");
    return;
  }

  if (backTouchDetected) {
    backTouchClearSinceMs = 0;
    if (backTouchCandidateSinceMs == 0) {
      backTouchCandidateSinceMs = now;
      return;
    }
    if (now - backTouchCandidateSinceMs >= BACK_TOUCH_REQUIRED_MS) {
      setPettingActive(true, now);
    }
    return;
  }

  backTouchCandidateSinceMs = 0;
  if (backTouchClearSinceMs == 0) {
    backTouchClearSinceMs = now;
  }
  if (now - backTouchClearSinceMs >= BACK_TOUCH_RELEASE_MS) {
    backTouchClearSinceMs = 0;
  }
}

void updateListeningNod(unsigned long now) {
  if (!LISTENING_NOD_ENABLED) {
    cancelListeningNod(false);
    return;
  }
  if (shakeActive || pettingActive || currentState != ChanState::Listening || !vadActive || currentAuthFaceMode == AuthFaceMode::NotMaster) {
    cancelListeningNod(currentState == ChanState::Idle);
    return;
  }

  if (nextListeningNodMs == 0) {
    scheduleNextListeningNod(now);
  }

  if (listeningNodPhase == 0) {
    if (now < nextListeningNodMs) {
      return;
    }
    listeningNodPhase = 1;
    listeningNodPhaseEndMs = now + LISTENING_NOD_DOWN_MS;
    motionController.setTargetPose(SERVO_PAN_CENTER, SERVO_TILT_CENTER - LISTENING_NOD_TILT_DELTA);
    return;
  }

  if (now < listeningNodPhaseEndMs) {
    return;
  }

  if (listeningNodPhase == 1) {
    listeningNodPhase = 2;
    listeningNodPhaseEndMs = now + LISTENING_NOD_UP_MS;
    motionController.setTargetPose(SERVO_PAN_CENTER, SERVO_TILT_CENTER);
    return;
  }

  listeningNodPhase = 0;
  scheduleNextListeningNod(now);
}

void setup() {
  M5StackChan.begin();
  Serial.setRxBufferSize(USB_SERIAL_RX_BUFFER_BYTES);
  Serial.begin(USB_SERIAL_BAUD);
  Serial.printf("[boot] reset_reason=%d usb_baud=%lu rx_buffer=%u at_ms=%lu\n",
                static_cast<int>(esp_reset_reason()),
                static_cast<unsigned long>(USB_SERIAL_BAUD),
                static_cast<unsigned>(USB_SERIAL_RX_BUFFER_BYTES),
                static_cast<unsigned long>(millis()));

  randomSeed(esp_random());
  networkMode = loadNetworkMode();
  loadWifiCredentials();
  loadDeviceSettings();
  applyDisplayBrightness();
  drawBootScreen("Starting...");
  Serial.printf("[network] mode=%s\n", networkModeName());

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] LittleFS mount failed");
    drawBootScreen("LittleFS failed");
  } else {
    Serial.println("[fs] LittleFS mounted");
  }

  faceController.begin();
  if (deviceSettings.lowPowerMode) {
    faceController.setThermalFaceMode(ThermalFaceMode::LowPower);
  }
  affectionController.begin(&preferences);
  faceController.setAffectionState(affectionController.state());
  motionController.begin();
  audioController.begin(&wsServer);
  audioController.setMicPacketSender(sendUsbSerialMicPacket, nullptr);
  audioController.setVolume(deviceSettings.volume);

  wsServer.onText(onWsText);
  wsServer.onBinary(onWsBinary);
  wsServer.onConnection(onWsConnection);

  connectWiFi();
  interactionReadyAtMs = millis() + INTERACTION_STARTUP_IGNORE_MS;
  lastInitializeDrawMs = 0;
  drawInitializeScreen(millis());
}

void loop() {
  M5StackChan.update();

  unsigned long now = millis();
  const bool diagSpeakingAtLoopStart =
    currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking;
#if FACE_DIAG_LOG_ENABLED
  auto logFaceDiagStep = [&](const char* name, unsigned long startedAt) {
    const unsigned long elapsed = millis() - startedAt;
    if (diagSpeakingAtLoopStart && elapsed > 100) {
      Serial.printf("[face_diag] loop step %s took=%lu current=%d audio=%d\n",
                    name,
                    elapsed,
                    static_cast<int>(currentState),
                    static_cast<int>(audioController.state()));
    }
  };
#endif
  updateButtons(now);
  updateThermalStatus(now);
  updateMicStatusOverlay();
  updateTouch(now);
  updateBackTouch(now);
  updateShake(now);
  updateUsbSerial(now);
  updateWiFi(now);

  if (wsStarted) {
#if FACE_DIAG_LOG_ENABLED
    const unsigned long stepStartedAt = millis();
#endif
    wsServer.loop();
#if FACE_DIAG_LOG_ENABLED
    logFaceDiagStep("wsServer.loop", stepStartedAt);
#endif
  }
  if (httpStarted) {
#if FACE_DIAG_LOG_ENABLED
    const unsigned long stepStartedAt = millis();
#endif
    httpServer.handleClient();
#if FACE_DIAG_LOG_ENABLED
    logFaceDiagStep("httpServer.handleClient", stepStartedAt);
#endif
  }
  updateCameraButtonPending(now);

  if (!interactionsReady(now)) {
    if (displayOn) {
      drawInitializeScreen(now);
    }
    audioController.update(now);
    updateAffectionState(now);
    motionController.update(now);
    return;
  }

  if (infoScreenVisible && displayOn) {
#if FACE_DIAG_LOG_ENABLED
    static bool loggedInfoScreenSpeakingSkip = false;
    if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
      if (!loggedInfoScreenSpeakingSkip) {
        Serial.printf("[face_diag] face update skipped infoScreen current=%d audio=%d\n",
                      static_cast<int>(currentState),
                      static_cast<int>(audioController.state()));
        loggedInfoScreenSpeakingSkip = true;
      }
    } else {
      loggedInfoScreenSpeakingSkip = false;
    }
#endif
    if (settingsPage == SettingsPage::Power && now - lastInfoDrawMs >= 3000) {
      drawInfoScreen();
    }
    audioController.update(now);
    updateAffectionState(now);
    updateDeferredFaceState();
    updateDeferredFaceMode();
    updatePendingAffectionDelta();
    updatePetting(now);
    updateListeningNod(now);
    motionController.update(now);
    return;
  }

#if FACE_DIAG_LOG_ENABLED
  unsigned long stepStartedAt = millis();
#endif
  audioController.update(now);
#if FACE_DIAG_LOG_ENABLED
  logFaceDiagStep("audioController.update", stepStartedAt);
  stepStartedAt = millis();
#endif
  updateAffectionState(now);
  updateDeferredFaceState();
  updateDeferredFaceMode();
  updatePendingAffectionDelta();
#if FACE_DIAG_LOG_ENABLED
  logFaceDiagStep("deferred/affection", stepStartedAt);
#endif
  if (displayOn) {
    const bool speaking = currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking;
    if (speaking || !deviceSettings.lowPowerMode || now - lastFaceUpdateMs >= LOW_POWER_FACE_UPDATE_INTERVAL_MS) {
      lastFaceUpdateMs = now;
      faceController.update(now);
      drawLowPowerPrompt();
    }
#if FACE_DIAG_LOG_ENABLED
  } else if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
    static unsigned long lastDisplayOffFaceSkipLogMs = 0;
    if (now - lastDisplayOffFaceSkipLogMs > 700) {
      Serial.printf("[face_diag] face update skipped displayOff current=%d audio=%d\n",
                    static_cast<int>(currentState),
                    static_cast<int>(audioController.state()));
      lastDisplayOffFaceSkipLogMs = now;
    }
#endif
  }
  updatePetting(now);
  if (deviceSettings.lowPowerMode) {
    cancelListeningNod(false);
  } else {
    updateListeningNod(now);
  }
  motionController.update(now);
}
