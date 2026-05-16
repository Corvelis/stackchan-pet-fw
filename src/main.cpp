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
  Power = 3,
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
NetworkQrType activeNetworkQr = NetworkQrType::None;
unsigned long lastWifiCheckMs = 0;
unsigned long lastInfoDrawMs = 0;
bool pendingStateAfterPlayback = false;
ChanState deferredStateAfterPlayback = ChanState::Idle;
unsigned long deferredStateReadyMs = 0;
bool pendingFaceModeNormalAfterPlayback = false;
unsigned long deferredFaceModeReadyMs = 0;
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
bool backTouchReady = false;
unsigned long backTouchReleasedSinceMs = 0;
unsigned long backTouchCandidateSinceMs = 0;
unsigned long backTouchClearSinceMs = 0;
unsigned long lastInitializeDrawMs = 0;
unsigned long lastFaceUpdateMs = 0;
uint8_t initializeSpinnerFrame = 0;
unsigned long interactionReadyAtMs = 0;
struct WifiCredential {
  String ssid;
  String password;
};

constexpr size_t kMaxWifiCredentials = 5;
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
bool interactionsReady(unsigned long now);
void applyListeningPresentation(unsigned long now);
void drawInfoScreen();
void applyDisplayBrightness();
void applyLowPowerMode(bool enabled, bool persist);

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

void applyAffectionResult(const AffectionApplyResult& result, unsigned long now, bool sendState, const char* requestId = nullptr) {
  if (!result.applied) {
    return;
  }
  faceController.setAffectionState(result.state);
  if (result.delta != 0) {
    faceController.showAffectionDelta(result.delta, now);
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
  return deviceSettings.brightness;
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
  if (enabled) {
    faceController.setThermalFaceMode(ThermalFaceMode::LowPower);
  } else {
    faceController.setThermalFaceMode(thermalStatus.level == ThermalLevel::Hot
                                        ? ThermalFaceMode::Hot
                                        : (thermalStatus.level == ThermalLevel::Warm ? ThermalFaceMode::Warm : ThermalFaceMode::Normal));
  }
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
  nextListeningNodMs = now + random(LISTENING_NOD_MIN_INTERVAL_MS, LISTENING_NOD_MAX_INTERVAL_MS + 1);
}

void scheduleFirstListeningNod(unsigned long now) {
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

  faceController.setAuthFaceMode(currentAuthFaceMode);
  if (currentAuthFaceMode == AuthFaceMode::NotMaster) {
    motionController.setMotion("not_master");
    return;
  }

  motionController.setMotion("center");
  scheduleFirstListeningNod(now);
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

void updateBatteryStatus() {
  const int batteryLevel = M5.Power.getBatteryLevel();
  const bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  faceController.setBatteryState(batteryLevel, charging);
}

void updateMicStatusOverlay() {
  faceController.setMicState(wsClientConnected, audioController.micMuted(), audioController.isMicStreaming());
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
  float maxDelta = NAN;
  if (!isnan(thermalStatus.chipTempC) && !isnan(thermalStatus.baselineChipTempC)) {
    maxDelta = thermalStatus.chipTempC - thermalStatus.baselineChipTempC;
  }
  if (!isnan(thermalStatus.pmicTempC) && !isnan(thermalStatus.baselinePmicTempC)) {
    const float pmicDelta = thermalStatus.pmicTempC - thermalStatus.baselinePmicTempC;
    maxDelta = isnan(maxDelta) ? pmicDelta : max(maxDelta, pmicDelta);
  }

  ThermalLevel nextLevel = ThermalLevel::Normal;
  if ((!isnan(hottest) && hottest >= THERMAL_HOT_ABSOLUTE_C) ||
      (!isnan(maxDelta) && maxDelta >= THERMAL_HOT_DELTA_C)) {
    nextLevel = ThermalLevel::Hot;
  } else if ((!isnan(hottest) && hottest >= THERMAL_WARM_ABSOLUTE_C) ||
             (!isnan(maxDelta) && maxDelta >= THERMAL_WARM_DELTA_C)) {
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
      faceController.setThermalFaceMode(nextLevel == ThermalLevel::Hot
                                          ? ThermalFaceMode::Hot
                                          : (nextLevel == ThermalLevel::Warm ? ThermalFaceMode::Warm : ThermalFaceMode::Normal));
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

void sendWifiPage(const String& message = "") {
  addCorsHeaders();
  httpServer.sendHeader("Cache-Control", "no-store");

  String body;
  body.reserve(9000);
  body += F("<!doctype html><html lang=\"ja\"><head><meta charset=\"utf-8\">");
  body += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">");
  body += F("<title>Stack-chan Wi-Fi Setup</title>");
  body += F("<style>");
  body += F("body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;margin:0;background:#f7f7f4;color:#1f2328}");
  body += F("main{max-width:720px;margin:0 auto;padding:20px}h1{font-size:24px;margin:0 0 16px}");
  body += F("section{background:#fff;border:1px solid #ddd;border-radius:8px;padding:16px;margin:14px 0}");
  body += F("label{display:block;font-weight:600;margin:12px 0 6px}input,select{box-sizing:border-box;width:100%;padding:10px;border:1px solid #bbb;border-radius:6px;font-size:16px}");
  body += F("button,a.btn{display:inline-block;margin:8px 6px 0 0;padding:9px 12px;border:1px solid #777;border-radius:6px;background:#222;color:#fff;text-decoration:none;font-size:14px}");
  body += F("button.secondary{background:#fff;color:#222}.row{border-top:1px solid #eee;padding:10px 0}.muted{color:#666;font-size:13px}.msg{background:#eaf6ef;border-color:#b9dfc8}");
  body += F("</style></head><body><main><h1>Stack-chan Wi-Fi Setup</h1>");
  body += F("<p class=\"muted\">SSIDを選択または入力して保存します。保存済みWi-Fiは上から順に接続を試します。</p>");

  if (message.length() > 0) {
    body += F("<section class=\"msg\">");
    body += htmlEscape(message);
    body += F("</section>");
  }

  body += F("<section><h2>見つかったSSID</h2>");
  body += F("<button class=\"secondary\" type=\"button\" onclick=\"scanWifi()\">再スキャン</button>");
  body += F("<div id=\"scan\" class=\"muted\">読み込み中...</div></section>");

  body += F("<section><h2>保存 / 変更</h2>");
  body += F("<form method=\"post\" action=\"/wifi/save\">");
  body += F("<label for=\"ssid\">SSID</label><input id=\"ssid\" name=\"ssid\" autocomplete=\"off\" required>");
  body += F("<label for=\"password\">Password</label><input id=\"password\" name=\"password\" type=\"password\" autocomplete=\"current-password\">");
  body += F("<p class=\"muted\">保存済みSSIDを選んでパスワード欄を空にすると、既存パスワードを維持します。</p>");
  body += F("<label for=\"priority\">優先度</label><select id=\"priority\" name=\"priority\">");
  for (size_t i = 0; i < kMaxWifiCredentials; ++i) {
    body += F("<option value=\"");
    body += String(i);
    body += F("\">");
    body += String(i + 1);
    body += F("</option>");
  }
  body += F("</select><button type=\"submit\">保存</button></form></section>");

  body += F("<section><h2>保存済みWi-Fi</h2>");
  if (wifiCredentialCount == 0) {
    body += F("<p class=\"muted\">保存済みWi-Fiはありません。</p>");
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
    body += F("\" onclick=\"editSaved(this.dataset.ssid,this.dataset.index)\">編集</button>");
    body += F("<form method=\"post\" action=\"/wifi/move\" style=\"display:inline\"><input type=\"hidden\" name=\"index\" value=\"");
    body += String(i);
    body += F("\"><input type=\"hidden\" name=\"dir\" value=\"up\"><button class=\"secondary\" type=\"submit\">上へ</button></form>");
    body += F("<form method=\"post\" action=\"/wifi/move\" style=\"display:inline\"><input type=\"hidden\" name=\"index\" value=\"");
    body += String(i);
    body += F("\"><input type=\"hidden\" name=\"dir\" value=\"down\"><button class=\"secondary\" type=\"submit\">下へ</button></form>");
    body += F("<form method=\"post\" action=\"/wifi/delete\" style=\"display:inline\"><input type=\"hidden\" name=\"index\" value=\"");
    body += String(i);
    body += F("\"><button class=\"secondary\" type=\"submit\">削除</button></form>");
    body += F("</div></div>");
  }
  body += F("</section>");

  body += F("<section><h2>接続</h2>");
  body += F("<p class=\"muted\">設定を保存したら、再起動してSTA接続を試します。</p>");
  body += F("<form method=\"post\" action=\"/wifi/restart\"><button type=\"submit\">保存済みWi-Fiで再起動</button></form>");
  body += F("</section>");

  body += F("<script>");
  body += F("function esc(s){return String(s).replace(/[&<>\"']/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;',\"'\":'&#39;'}[c]})}");
  body += F("function selectSsid(s){document.getElementById('ssid').value=s;document.getElementById('password').focus()}");
  body += F("function editSaved(s,i){document.getElementById('ssid').value=s;document.getElementById('priority').value=i;document.getElementById('password').focus()}");
  body += F("async function scanWifi(){let box=document.getElementById('scan');box.textContent='スキャン中...';try{let r=await fetch('/wifi/scan');let d=await r.json();if(!d.networks.length){box.textContent='SSIDが見つかりませんでした';return}box.innerHTML=d.networks.map(n=>'<div class=\"row\"><button class=\"secondary\" onclick=\"selectSsid(\\''+String(n.ssid).replace(/\\\\/g,'\\\\\\\\').replace(/'/g,\"\\\\'\")+'\\')\">選択</button> '+esc(n.ssid)+' <span class=\"muted\">'+n.rssi+' dBm '+esc(n.auth)+'</span></div>').join('')}catch(e){box.textContent='スキャンに失敗しました'}}");
  body += F("scanWifi();</script></main></body></html>");

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
    sendWifiPage("保存できませんでした。SSIDが空、または保存件数が上限です。");
    return;
  }
  saveWifiCredentials();
  sendWifiPage("Wi-Fi設定を保存しました。必要なら再起動して接続を試してください。");
}

void handleWifiDeleteRequest() {
  if (deleteWifiCredential(static_cast<size_t>(httpServer.arg("index").toInt()))) {
    saveWifiCredentials();
    sendWifiPage("Wi-Fi設定を削除しました。");
    return;
  }
  sendWifiPage("削除できませんでした。");
}

void handleWifiMoveRequest() {
  const size_t index = static_cast<size_t>(httpServer.arg("index").toInt());
  const int delta = httpServer.arg("dir") == "up" ? -1 : 1;
  if (moveWifiCredential(index, delta)) {
    saveWifiCredentials();
    sendWifiPage("優先度を変更しました。");
    return;
  }
  sendWifiPage("優先度を変更できませんでした。");
}

void handleWifiRestartRequest() {
  saveNetworkMode(NetworkMode::Sta);
  sendWifiPage("再起動します。保存済みWi-Fiへの接続を試します。");
  delay(500);
  ESP.restart();
}

void handleCaptureRequest() {
  faceController.setPhotoFaceMode(true);
  faceController.showFace("photo_0");
  const bool micPausedForCapture = audioController.pauseMicForCapture();

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
  doc["charging"] = M5.Power.isCharging() == m5::Power_Class::is_charging;
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
  drawButton(8, 206, 72, 26, "Net", settingsPage == SettingsPage::Network);
  drawButton(86, 206, 72, 26, "Disp", settingsPage == SettingsPage::Display);
  drawButton(164, 206, 72, 26, "Audio", settingsPage == SettingsPage::Audio);
  drawButton(242, 206, 70, 26, "Power", settingsPage == SettingsPage::Power);
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

  M5.Display.printf("Client: %s\n", wsClientConnected ? "connected" : "disconnected");
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
  drawButton(170, 150, 128, 32, "Low Power", deviceSettings.lowPowerMode);
}

void drawAudioSettingsPage() {
  drawSlider(24, 72, 210, "Volume", deviceSettings.volume, AUDIO_SPEAKER_VOLUME_MIN, AUDIO_SPEAKER_VOLUME_MAX);
  drawButton(246, 84, 54, 28, "-");
  drawButton(246, 122, 54, 28, "+");
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
  M5.Display.printf("Charging: %s\n", M5.Power.isCharging() == m5::Power_Class::is_charging ? "yes" : "no");
  M5.Display.println();
  M5.Display.printf("Suggest: %s\n", thermalStatus.suggestLowPower ? "Low Power" : "none");
  drawButton(164, 150, 136, 32, deviceSettings.lowPowerMode ? "Low Power On" : "Low Power Off", deviceSettings.lowPowerMode);
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

bool handleSettingsTouch(const m5::touch_detail_t& touch) {
  if (activeNetworkQr != NetworkQrType::None) {
    activeNetworkQr = NetworkQrType::None;
    drawInfoScreen();
    return true;
  }

  if (touchIn(touch, 8, 206, 72, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Network;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 86, 206, 72, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Display;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 164, 206, 72, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Audio;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 242, 206, 70, 26)) {
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
    if (touchIn(touch, 170, 150, 128, 32)) {
      applyLowPowerMode(!deviceSettings.lowPowerMode, true);
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
  } else if (settingsPage == SettingsPage::Power) {
    if (touchIn(touch, 164, 150, 136, 32)) {
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
  faceController.setState(state);
  audioController.setState(state);

  if (state == ChanState::Idle) {
    pendingFaceModeNormalAfterPlayback = false;
    deferredFaceModeReadyMs = 0;
    faceController.setPhotoFaceMode(false);
    currentAuthFaceMode = AuthFaceMode::Unknown;
    vadActive = false;
    cancelListeningNod(true);
    motionController.setMotion("center");
  } else if (state == ChanState::Listening) {
    if (previousState == ChanState::Speaking) {
      pendingFaceModeNormalAfterPlayback = false;
      deferredFaceModeReadyMs = 0;
      faceController.setPhotoFaceMode(false);
    }
    vadActive = false;
    applyListeningPresentation(millis());
  } else if (state == ChanState::Speaking) {
    vadActive = false;
    cancelListeningNod(currentAuthFaceMode != AuthFaceMode::NotMaster);
    faceController.setAuthFaceMode(currentAuthFaceMode);
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

  if (state == ChanState::Idle) {
    currentAuthFaceMode = AuthFaceMode::Unknown;
    vadActive = false;
    cancelListeningNod(true);
    motionController.setMotion("center");
  } else if (state == ChanState::Listening) {
    currentAuthFaceMode = AuthFaceMode::Unknown;
    vadActive = false;
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
  if (strcmp(value, "idle") == 0) {
    setState(ChanState::Idle);
  } else if (strcmp(value, "listening") == 0) {
    setState(ChanState::Listening);
  } else if (strcmp(value, "speaking") == 0) {
    setState(ChanState::Speaking);
  } else {
    Serial.printf("[json] unsupported state: %s\n", value);
  }
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
    setPettingActive(true, millis());
    Serial.println("[face] mode=nadenade");
  } else if (strcmp(value, "furifuri") == 0 || strcmp(value, "shake") == 0) {
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
    Serial.println("[auth] master");
  } else if (strcmp(result, "not_master") == 0) {
    currentAuthFaceMode = AuthFaceMode::NotMaster;
    applyListeningPresentation(millis());
    Serial.println("[auth] not_master");
  } else if (strcmp(result, "unknown") == 0 || strcmp(result, "none") == 0) {
    currentAuthFaceMode = AuthFaceMode::Unknown;
    applyListeningPresentation(millis());
    Serial.println("[auth] unknown");
  } else {
    Serial.printf("[auth] unsupported result: %s\n", result);
  }
}

void handleVadCommand(bool active) {
  vadActive = active;
  applyListeningPresentation(millis());
  Serial.printf("[vad] %s\n", vadActive ? "active" : "inactive");
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

void sendAffectionState(const char* requestId) {
  if (!wsServer.hasClient()) {
    return;
  }

  JsonDocument doc;
  writeAffectionState(doc, requestId, millis());

  String body;
  serializeJson(doc, body);
  wsServer.sendText(body.c_str());
}

void sendInteractionEvent(const char* event, const char* phase, unsigned long now) {
  if (!wsServer.hasClient() || event == nullptr || phase == nullptr) {
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
  wsServer.sendText(body.c_str());
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
  (void)source;
  const AffectionApplyResult result = affectionController.applyEvent(eventName, confidence, intensity, eventId, now);
  applyAffectionResult(result, now, true, doc["requestId"] | nullptr);
  if (result.duplicate) {
    sendAffectionState(doc["requestId"] | nullptr);
  }
}

void handleAffectionGetCommand(JsonDocument& doc) {
  sendAffectionState(doc["requestId"] | nullptr);
}

void handleAffectionResetCommand(JsonDocument& doc) {
  const int value = doc["value"] | 500;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.reset(value);
  applyAffectionResult(result, now, true, doc["requestId"] | nullptr);
}

void handleAffectionDebugAdjustCommand(JsonDocument& doc) {
  const int delta = doc["delta"] | 0;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.debugAdjust(delta);
  applyAffectionResult(result, now, true, doc["requestId"] | nullptr);
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
  applyAffectionResult(result, now, true, doc["requestId"] | nullptr);
  if (!result.applied) {
    sendAffectionState(doc["requestId"] | nullptr);
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
        applyListeningPresentation(millis());
      } else {
        cancelListeningNod(false);
        faceController.setAuthFaceMode(AuthFaceMode::NotMaster);
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

void onWsText(uint8_t clientId, const uint8_t* payload, size_t length) {
  (void)clientId;
  Serial.printf("[ws] text %u bytes\n", static_cast<unsigned>(length));
  handleJsonCommand(payload, length);
}

void onWsBinary(uint8_t clientId, uint8_t* payload, size_t length) {
  (void)clientId;
  audioController.onBinaryReceived(payload, length);
}

void onWsConnection(uint8_t clientId, bool connected) {
  (void)clientId;
  wsClientConnected = connected;
  updateMicStatusOverlay();
  if (connected) {
    const unsigned long now = millis();
    applyAffectionResult(affectionController.resetTransient(), now, false);
    sendAffectionState();
    sendInteractionEvent("session_start", "instant", now);
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
    if (!infoScreenVisible && wsClientConnected &&
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
  Serial.begin(115200);

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
  updateButtons(now);
  updateThermalStatus(now);
  updateMicStatusOverlay();
  updateTouch(now);
  updateBackTouch(now);
  updateShake(now);
  updateWiFi(now);

  if (wsStarted) {
    wsServer.loop();
  }
  if (httpStarted) {
    httpServer.handleClient();
  }

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
    if (settingsPage == SettingsPage::Power && now - lastInfoDrawMs >= 3000) {
      drawInfoScreen();
    }
    audioController.update(now);
    updateAffectionState(now);
    updateDeferredFaceState();
    updateDeferredFaceMode();
    updatePetting(now);
    updateListeningNod(now);
    motionController.update(now);
    return;
  }

  audioController.update(now);
  updateAffectionState(now);
  updateDeferredFaceState();
  updateDeferredFaceMode();
  if (displayOn) {
    const bool speaking = currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking;
    if (speaking || !deviceSettings.lowPowerMode || now - lastFaceUpdateMs >= LOW_POWER_FACE_UPDATE_INTERVAL_MS) {
      lastFaceUpdateMs = now;
      faceController.update(now);
      drawLowPowerPrompt();
    }
  }
  updatePetting(now);
  if (deviceSettings.lowPowerMode) {
    cancelListeningNod(false);
  } else {
    updateListeningNod(now);
  }
  motionController.update(now);
}
