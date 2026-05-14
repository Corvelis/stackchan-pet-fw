#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <M5StackChan.h>
#include <Preferences.h>
#include <math.h>

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

ChanState currentState = ChanState::Idle;
AuthFaceMode currentAuthFaceMode = AuthFaceMode::Unknown;
NetworkMode networkMode = NetworkMode::Sta;
bool vadActive = false;
bool infoScreenVisible = false;
bool wsStarted = false;
bool httpStarted = false;
bool wsClientConnected = false;
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
bool shakeActive = false;
unsigned long shakeEndMs = 0;
unsigned long nextShakeCheckMs = 0;
unsigned long lastShakeTriggerMs = 0;
unsigned long nextShakeMotionMs = 0;
uint8_t shakeStrongSamples = 0;
struct WifiCredential {
  const char* ssid;
  const char* password;
};

WifiCredential wifiCredentials[] = {
  {WIFI_SSID, WIFI_PASSWORD},
  {WIFI_SSID_2, WIFI_PASSWORD_2},
};
constexpr size_t wifiCredentialCount = sizeof(wifiCredentials) / sizeof(wifiCredentials[0]);
size_t currentWifiIndex = 0;
uint8_t wifiConnectAttempts = 0;

void sendAffectionState(const char* requestId = nullptr);

bool isWifiCredentialConfigured(size_t index) {
  return index < wifiCredentialCount &&
         wifiCredentials[index].ssid != nullptr &&
         wifiCredentials[index].ssid[0] != '\0';
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

void applyAffectionResult(const AffectionApplyResult& result, unsigned long now, bool sendState) {
  if (!result.applied) {
    return;
  }
  faceController.setAffectionState(result.state);
  if (result.delta != 0) {
    faceController.showAffectionDelta(result.delta, now);
  }
  if (sendState) {
    sendAffectionState();
  }
}

const char* networkModeName() {
  return networkMode == NetworkMode::SoftAp ? "SoftAP" : "STA";
}

NetworkMode loadNetworkMode() {
  preferences.begin("stackchan", true);
  const uint8_t value = preferences.getUChar("net_mode", static_cast<uint8_t>(NetworkMode::Sta));
  preferences.end();
  return value == static_cast<uint8_t>(NetworkMode::SoftAp) ? NetworkMode::SoftAp : NetworkMode::Sta;
}

void saveNetworkMode(NetworkMode mode) {
  preferences.begin("stackchan", false);
  preferences.putUChar("net_mode", static_cast<uint8_t>(mode));
  preferences.end();
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

void setPettingActive(bool active, unsigned long now) {
  if (active) {
    pettingEndMs = now + PET_TOUCH_RELEASE_GRACE_MS;
    if (!pettingActive) {
      pettingActive = true;
      nextPetMoveMs = 0;
      cancelListeningNod(false);
      faceController.setPetFaceMode(true);
      motionController.setTargetPose(SERVO_PAN_CENTER + random(-6, 7), SERVO_TILT_CENTER + random(6, 13));
      applyAffectionResult(affectionController.applyEvent("petting", 1.0f, 1.0f, nullptr, now), now, true);
      Serial.println("[pet] start");
    }
    return;
  }

  if (!pettingActive) {
    return;
  }

  pettingActive = false;
  pettingEndMs = 0;
  nextPetMoveMs = 0;
  faceController.setPetFaceMode(false);
  motionController.setTargetPose(SERVO_PAN_CENTER + random(-3, 4), SERVO_TILT_CENTER + random(-2, 5));
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
  const bool bigMove = random(100) < 22;
  const int amplitude = bigMove
                          ? random(PET_BIG_PAN_MIN, PET_BIG_PAN_MAX + 1)
                          : random(PET_SMALL_PAN_MIN, PET_SMALL_PAN_MAX + 1);
  const int direction = random(100) < 50 ? -1 : 1;
  const int panJitter = random(-4, 5);
  const int tilt = bigMove
                     ? random(PET_BIG_TILT_MIN, PET_BIG_TILT_MAX + 1)
                     : random(PET_SMALL_TILT_MIN, PET_SMALL_TILT_MAX + 1);
  motionController.setTargetPose(SERVO_PAN_CENTER + direction * amplitude + panJitter, tilt);
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
      Serial.println("[shake] start");
    }
    return;
  }

  if (!shakeActive) {
    return;
  }

  shakeActive = false;
  shakeEndMs = 0;
  nextShakeMotionMs = 0;
  shakeStrongSamples = 0;
  faceController.setShakeFaceMode(false);
  if (!pettingActive && currentState != ChanState::Listening) {
    motionController.setMotion("center");
  }
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
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["freePsram"] = ESP.getFreePsram();
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

void drawInfoScreen() {
  lastInfoDrawMs = millis();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 16);
  M5.Display.println("Network");

  M5.Display.setTextSize(1);
  M5.Display.setCursor(12, 56);
  M5.Display.printf("Mode: %s\n", networkModeName());

  if (networkMode == NetworkMode::SoftAp) {
    const String ip = WiFi.softAPIP().toString();
    M5.Display.printf("SSID: %s\n", AP_SSID);
    M5.Display.printf("PASS: %s\n", AP_PASSWORD);
    M5.Display.printf("IP: %s\n", ip.c_str());
    M5.Display.printf("WS: ws://%s:%d%s\n", ip.c_str(), WS_PORT, WS_PATH);
    M5.Display.printf("Stations: %d\n", WiFi.softAPgetStationNum());
  } else if (WiFi.status() == WL_CONNECTED) {
    const String ip = WiFi.localIP().toString();
    M5.Display.printf("SSID: %s\n", wifiCredentials[currentWifiIndex].ssid);
    M5.Display.printf("IP: %s\n", ip.c_str());
    M5.Display.printf("WS: ws://%s:%d%s\n", ip.c_str(), WS_PORT, WS_PATH);
  } else {
    M5.Display.printf("SSID: %s\n", wifiCredentials[currentWifiIndex].ssid);
    M5.Display.println("IP: not connected");
    M5.Display.println("WS: not ready");
  }

  M5.Display.printf("Client: %s\n", wsClientConnected ? "connected" : "disconnected");
  M5.Display.println();
  M5.Display.println("Tap to return");
  M5.Display.printf("Hold: switch to %s\n", networkMode == NetworkMode::SoftAp ? "STA" : "SoftAP");
}

void setInfoScreenVisible(bool visible) {
  if (infoScreenVisible == visible) {
    return;
  }
  infoScreenVisible = visible;
  faceController.setEnabled(!visible);
  if (visible) {
    drawInfoScreen();
  }
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
    if (previousState == ChanState::Speaking) {
      currentAuthFaceMode = AuthFaceMode::Unknown;
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

void writeAffectionState(JsonDocument& doc, const char* requestId) {
  const AffectionState& affection = affectionController.state();
  doc["type"] = "affection.state";
  if (requestId != nullptr && requestId[0] != '\0') {
    doc["requestId"] = requestId;
  }
  doc["affection"] = affection.affection;
  doc["mood"] = affection.mood;
  doc["confusion"] = affection.confusion;
  doc["seq"] = affection.seq;
  doc["level"] = affectionController.level();
}

void sendAffectionState(const char* requestId) {
  if (!wsServer.hasClient()) {
    return;
  }

  JsonDocument doc;
  writeAffectionState(doc, requestId);

  String body;
  serializeJson(doc, body);
  wsServer.sendText(body.c_str());
}

void handleAffectionEventCommand(JsonDocument& doc) {
  const char* eventName = doc["event"] | "";
  const char* eventId = doc["id"] | "";
  const float confidence = doc["confidence"] | 1.0f;
  const float intensity = doc["intensity"] | 1.0f;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.applyEvent(eventName, confidence, intensity, eventId, now);
  applyAffectionResult(result, now, true);
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
  applyAffectionResult(result, now, true);
}

void handleAffectionDebugAdjustCommand(JsonDocument& doc) {
  const int delta = doc["delta"] | 0;
  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.debugAdjust(delta);
  applyAffectionResult(result, now, true);
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
    drawBootScreen("WiFi not configured");
    return;
  }

  WiFi.begin(wifiCredentials[currentWifiIndex].ssid, wifiCredentials[currentWifiIndex].password);
  Serial.printf("[wifi] connecting to %s\n", wifiCredentials[currentWifiIndex].ssid);
}

void updateWiFi(unsigned long now) {
  if (now - lastWifiCheckMs < 1000) {
    return;
  }
  lastWifiCheckMs = now;

  if (networkMode == NetworkMode::SoftAp) {
    if (infoScreenVisible) {
      drawInfoScreen();
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectAttempts = 0;
    if (!wsStarted) {
      Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
      startServers();
      if (infoScreenVisible) {
        drawInfoScreen();
      }
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
    Serial.printf("[wifi] switching candidate to %s\n", wifiCredentials[currentWifiIndex].ssid);
  }

  Serial.println("[wifi] disconnected, reconnecting");
  WiFi.disconnect();
  WiFi.begin(wifiCredentials[currentWifiIndex].ssid, wifiCredentials[currentWifiIndex].password);
  if (infoScreenVisible) {
    drawInfoScreen();
  }
}

void updateTouch() {
  auto touch = M5.Touch.getDetail();
  if (infoScreenVisible && touch.wasHold()) {
    switchNetworkModeAndRestart();
    return;
  }

  if (touch.wasClicked() || touch.wasFlicked()) {
    setInfoScreenVisible(!infoScreenVisible);
    return;
  }
}

void updateBackTouch(unsigned long now) {
  if (infoScreenVisible) {
    return;
  }

  // This is the physical touch sensor on Stack-chan's back, not the CoreS3 screen.
  auto& touchSensor = M5StackChan.TouchSensor;
  const auto& intensities = touchSensor.getIntensities();
  const bool touchingBack = intensities[0] > 0 || intensities[1] > 0 || intensities[2] > 0;

  if (touchSensor.wasPressed() || touchSensor.wasClicked() ||
      touchSensor.wasSwipedForward() || touchSensor.wasSwipedBackward() || touchingBack) {
    setPettingActive(true, now);
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
  drawBootScreen("Starting...");
  Serial.printf("[network] mode=%s\n", networkModeName());

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] LittleFS mount failed");
    drawBootScreen("LittleFS failed");
  } else {
    Serial.println("[fs] LittleFS mounted");
  }

  faceController.begin();
  affectionController.begin(&preferences);
  faceController.setAffectionState(affectionController.state());
  motionController.begin();
  audioController.begin(&wsServer);

  wsServer.onText(onWsText);
  wsServer.onBinary(onWsBinary);
  wsServer.onConnection(onWsConnection);

  connectWiFi();
}

void loop() {
  M5StackChan.update();

  unsigned long now = millis();
  updateTouch();
  updateBackTouch(now);
  updateShake(now);
  updateWiFi(now);

  if (wsStarted) {
    wsServer.loop();
  }
  if (httpStarted) {
    httpServer.handleClient();
  }

  if (infoScreenVisible) {
    if (now - lastInfoDrawMs >= 1000) {
      drawInfoScreen();
    }
    audioController.update(now);
    affectionController.update(now);
    updateDeferredFaceState();
    updateDeferredFaceMode();
    updatePetting(now);
    updateListeningNod(now);
    motionController.update(now);
    return;
  }

  audioController.update(now);
  affectionController.update(now);
  updateDeferredFaceState();
  updateDeferredFaceMode();
  faceController.update(now);
  updatePetting(now);
  updateListeningNod(now);
  motionController.update(now);
}
