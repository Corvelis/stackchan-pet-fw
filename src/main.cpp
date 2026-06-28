#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>

#include "config.h"

#if STACKCHAN_HAS_STACKCHAN_BSP
#include <M5StackChan.h>
#endif

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <cstring>
#include <math.h>
#include <qrcodegen.h>
#include <time.h>

#include "AffectionController.h"
#include "AppState.h"
#include "AudioController.h"
#include "CameraManager.h"
#include "FaceController.h"
#include "MotionController.h"
#include "StreetPassController.h"
#include "StreetPassProtocol.h"
#include "WebSocketServerController.h"

FaceController faceController;
MotionController motionController;
WebSocketServerController wsServer;
AudioController audioController;
CameraManager cameraManager;
AffectionController affectionController;
StreetPassController streetPassController;
WebServer httpServer(HTTP_PORT);
Preferences preferences;
String deviceId;

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
  StreetPass = 5,
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
bool streetPassProfileVisible = false;
uint8_t streetPassHistoryPage = 0;
bool displayOn = true;
bool wsStarted = false;
bool httpStarted = false;
bool wsClientConnected = false;
bool usbSerialClientConnected = false;
NetworkQrType activeNetworkQr = NetworkQrType::None;
unsigned long lastWifiCheckMs = 0;
unsigned long wifiConnectStartedMs = 0;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
unsigned long lastInfoDrawMs = 0;
bool pendingStateAfterPlayback = false;
ChanState deferredStateAfterPlayback = ChanState::Idle;
unsigned long deferredStateReadyMs = 0;
bool pendingSpeakingFaceState = false;
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
unsigned long pettingStartedMs = 0;
bool pettingFaceAnimated = false;
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
#if STACKCHAN_SMALL_DISPLAY
bool smallDisplayFacePettingHold = false;
uint8_t smallStreetPassView = 0;
bool smallVolumeAdjustMode = false;
unsigned long smallVolumeHoldRepeatMs = 0;
#endif
uint32_t cameraButtonEventSeq = 0;
unsigned long lastCameraButtonEventMs = 0;
bool cameraButtonPending = false;
bool backTouchReady = false;
unsigned long backTouchReleasedSinceMs = 0;
unsigned long backTouchCandidateSinceMs = 0;
unsigned long backTouchClearSinceMs = 0;
#if STACKCHAN_HAS_BACK_TOUCH && STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
bool backTouchGuruguruPressed = false;
bool backTouchGuruguruHoldFired = false;
unsigned long backTouchGuruguruPressSinceMs = 0;
unsigned long backTouchGuruguruFirstTapMs = 0;
#endif
bool screenPettingCandidate = false;
bool screenPettingTouchActive = false;
unsigned long screenPettingCandidateSinceMs = 0;
unsigned long screenPettingReleaseSinceMs = 0;
int32_t screenPettingTravelPx = 0;
#if STACKCHAN_GURUGURU_FACE_ENABLED
bool guruguruFaceMode = false;
bool guruguruFaceEffective = false;
bool guruguruFaceAssetsChecked = false;
bool guruguruFaceAssetsReady = false;
#endif
#if STACKCHAN_GURUGURU_IMU_ENABLED && STACKCHAN_GURUGURU_FACE_ENABLED
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
bool guruguruFaceImuInput = true;
#else
bool guruguruFaceImuInput = false;
#endif
#endif
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_GURUGURU_FACE_ENABLED
bool guruguruRestoreListeningOnExit = false;
#endif
#if STACKCHAN_GURUGURU_IMU_ENABLED && STACKCHAN_GURUGURU_FACE_ENABLED
bool guruguruImuBaseReady = false;
bool guruguruImuFilterReady = false;
float guruguruImuBaseX = 0.0f;
float guruguruImuBaseY = 0.0f;
float guruguruImuBaseZ = 1.0f;
float guruguruImuFilterX = 0.0f;
float guruguruImuFilterY = 0.0f;
float guruguruImuFilterZ = 1.0f;
uint8_t guruguruImuCandidateDirection = STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
uint8_t guruguruImuCandidateSamples = 0;
unsigned long nextGuruguruImuUpdateMs = 0;
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_CORES3
unsigned long lastGuruguruImuDebugMs = 0;
#endif
#if STACKCHAN_GURUGURU_FACE_ENABLED
int8_t guruguruDizzyLastDirection = -1;
unsigned long guruguruDizzyWindowStartMs = 0;
unsigned long guruguruDizzyCooldownUntilMs = 0;
uint16_t guruguruDizzyTotalSteps = 0;
int16_t guruguruDizzySignedSteps = 0;
uint16_t guruguruAffectionStepAccum = 0;
unsigned long guruguruAffectionStepStartMs = 0;
uint8_t guruguruAffectionDangerCombo = 0;
uint8_t guruguruAffectionRedDangerStreak = 0;
unsigned long guruguruImuDizzyShakeStartMs = 0;
unsigned long guruguruImuDizzyShakeLastActiveMs = 0;
#endif
#endif
unsigned long hapticOffMs = 0;
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
#if STACKCHAN_DEVICE_STOPWATCH
bool usbSerialDeferredIdlePending = false;
unsigned long usbSerialDeferredIdleRequestedMs = 0;
#endif
bool streetPassBleReady = false;
bool streetPassScanActive = false;
bool streetPassExchangeInProgress = false;
bool streetPassLastEnabled = false;
bool streetPassAdvertising = false;
bool streetPassBlePaused = false;
bool streetPassGattServerConnected = false;
bool streetPassForceNextExchange = false;
volatile bool streetPassInboundConnectedEvent = false;
volatile bool streetPassInboundDisconnectedEvent = false;
volatile int streetPassInboundDisconnectReason = 0;
volatile bool streetPassInboundWritePending = false;
volatile size_t streetPassInboundWriteLength = 0;
char streetPassInboundWriteBuffer[384] = {};
unsigned long streetPassScanStartedMs = 0;
unsigned long nextStreetPassScanMs = 0;
unsigned long nextStreetPassExchangeMs = 0;
unsigned long nextStreetPassNtpSyncMs = 0;
unsigned long streetPassBleSettleUntilMs = 0;
bool streetPassNtpConfigured = false;
uint32_t streetPassAdvertisedCardSeq = 0;
uint32_t streetPassAdvertisedPeerToken = 0;
bool streetPassAdvertisedEnabled = false;
NimBLEServer* streetPassGattServer = nullptr;
NimBLEClient* streetPassGattClient = nullptr;
NimBLECharacteristic* streetPassInfoCharacteristic = nullptr;
NimBLECharacteristic* streetPassPublicCardCharacteristic = nullptr;
NimBLECharacteristic* streetPassEncounterWriteCharacteristic = nullptr;
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
constexpr uint8_t kStreetPassCandidateCount = 8;
constexpr unsigned long STREETPASS_SCAN_DURATION_MS = 5000;
constexpr unsigned long STREETPASS_SCAN_IDLE_INTERVAL_MS = 12500;
constexpr unsigned long STREETPASS_SCAN_BUSY_INTERVAL_MS = 60000;
constexpr unsigned long STREETPASS_OBSERVE_MIN_MS = 1500;
constexpr uint8_t STREETPASS_OBSERVE_MIN_COUNT = 2;
constexpr int8_t STREETPASS_RSSI_MIN_DBM = -75;
constexpr unsigned long STREETPASS_CONNECT_RETRY_MS = 3UL * 60UL * 60UL * 1000UL;
constexpr unsigned long STREETPASS_CONNECT_PREPARE_MS = 300;
constexpr unsigned long STREETPASS_GATT_SETTLE_MS = 1000;
constexpr unsigned long STREETPASS_FORCE_PASSIVE_GRACE_MS = 5000;
constexpr unsigned long STREETPASS_NTP_RETRY_MS = 10000;
constexpr unsigned long STREETPASS_NTP_REFRESH_MS = 6UL * 60UL * 60UL * 1000UL;
constexpr uint32_t STREETPASS_VALID_UNIX_MIN = 1700000000UL;
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
unsigned long lastClockOverlayUpdateMs = 0;

struct StreetPassBleCandidate {
  bool active = false;
  String address;
  String name;
  String advertisementKey;
  NimBLEAddress bleAddress;
  uint8_t addressType = 0;
  uint32_t peerToken = 0;
  bool hasPeerToken = false;
  uint32_t firstSeenMs = 0;
  uint32_t lastSeenMs = 0;
  uint32_t lastAttemptMs = 0;
  uint16_t seenCount = 0;
  int8_t rssiMax = -127;
  bool exchangeQueued = false;
};

StreetPassBleCandidate streetPassCandidates[kStreetPassCandidateCount];

void sendAffectionState(const char* requestId = nullptr);
void sendInteractionEvent(const char* event, const char* phase, unsigned long now);
bool sendCameraButtonEvent(unsigned long now);
bool appClientConnected();
void updateUsbSerial(unsigned long now);
void updateWiFi(unsigned long now);
void beginStreetPassBle();
void updateStreetPassBle(unsigned long now);
void updateStreetPassInboundEvents(unsigned long now, bool processWrites);
void restoreStreetPassTimeFromRtc(unsigned long now);
void writeStreetPassRtcTime(uint32_t unixTime);
void updateClockOverlay(unsigned long now);
void clearStreetPassBleCandidates();
void resetStreetPassBleAttemptCooldowns();
bool updateDisplayOffStreetPassMode(unsigned long now);
String makeStreetPassInfoJson();
String makeStreetPassPublicCardJson();
String makeStreetPassAdvertisementName();
uint32_t streetPassFnv1a(const String& value);
uint32_t streetPassPeerToken();
bool streetPassManufacturerData(const std::string& manufacturer);
bool streetPassPeerTokenFromAdvertisement(const NimBLEAdvertisedDevice* device, uint32_t& token);
bool streetPassShouldInitiate(uint32_t peerToken, const String& peerAddress);
void updateStreetPassGattCharacteristics();
void updateStreetPassAdvertising();
void restartStreetPassAdvertising();
void stopStreetPassAdvertising(const char* reason);
void rememberStreetPassAdvertisedDevice(const NimBLEAdvertisedDevice* device, unsigned long now);
bool streetPassBusyForExchange();
const char* streetPassBusyReason();
bool exchangeStreetPassCandidate(StreetPassBleCandidate& candidate, unsigned long now);
bool sendUsbSerialJson(const char* payload);
bool sendUsbSerialFrame(uint8_t type, const uint8_t* payload, size_t length, uint8_t flags = 0);
bool sendUsbSerialMicPacket(const uint8_t* payload, size_t length, void* context);
void setState(ChanState state);
void writePongResponse(JsonDocument& response, JsonDocument& request);
void sendPongResponse(JsonDocument& request);
void sendJsonDocument(JsonDocument& doc);
void handleDeviceInfoGetCommand(JsonDocument& doc);
void handleUsbSerialLine(const uint8_t* payload, size_t length);
void handleUsbSerialFrame(uint8_t type, uint8_t flags, uint32_t seq, const uint8_t* payload, size_t length);
void handleUsbSerialJsonPayload(const uint8_t* payload, size_t length);
void handleUsbSerialCaptureRequest(JsonDocument& doc);
void handleAffectionSyncStateCommand(JsonDocument& doc);
void handleAffectionSyncApplyCommand(JsonDocument& doc);
void updateUsbSerialDeferredIdle(unsigned long now);
void updateCameraButtonPending(unsigned long now);
void clearCameraButtonPending(const char* reason);
bool interactionsReady(unsigned long now);
void applyListeningPresentation(unsigned long now);
void updateDeferredFaceState();
void updateSpeakingFaceStateAfterPlayback();
void updateDeferredFaceMode();
AuthFaceMode displayAuthFaceMode(AuthFaceMode mode);
bool audioBusyForUiEffects();
void updatePendingAffectionDelta();
void drawInfoScreen();
void drawNetworkQrScreen();
bool streetPassPageVisible();
void applyDisplayBrightness();
void applyLowPowerMode(bool enabled, bool persist);
void connectWiFi();
bool audioBusyForServoCalibration();
void drawStreetPassNotificationOverlay();
void beginDevice();
void updateDevice();
void logDeviceAudioConfig();
void pulseHaptic(uint8_t level, unsigned long durationMs, unsigned long now);
void updateHaptic(unsigned long now);
void updateScreenPetting(unsigned long now, const m5::touch_detail_t& touch);
#if STACKCHAN_GURUGURU_FACE_ENABLED
void setGuruguruFaceMode(bool enabled, unsigned long now);
bool guruguruFaceAssetsAvailable();
bool guruguruFaceCanRun();
bool updateGuruguruFaceTouch(unsigned long now, const m5::touch_detail_t& touch);
void updateGuruguruFaceAvailability(unsigned long now);
#if STACKCHAN_GURUGURU_IMU_ENABLED
bool guruguruFaceUsesImu();
void setGuruguruFaceImuInput(bool enabled);
void toggleGuruguruFaceInput();
void resetGuruguruImuBase();
bool resetGuruguruImuBase(const m5::imu_data_t& data);
#if STACKCHAN_GURUGURU_FACE_ENABLED
void resetGuruguruAffectionTracking();
void resetGuruguruImuDizzyShakeDetection();
void resetGuruguruDizzySpinDetection(bool resetLastDirection = true);
bool updateGuruguruDizzySpinDetection(unsigned long now, uint8_t direction, bool useGameRules = true);
void updateGuruguruImuStepTracking(unsigned long now, uint8_t direction);
bool updateGuruguruImuDizzyShakeDetection(unsigned long now, float sampleDelta);
#endif
bool updateGuruguruFaceImu(unsigned long now, const m5::imu_data_t& data, bool imuUpdated);
#endif
#endif
void redrawNetworkSettingsIfVisible();
#if STACKCHAN_SMALL_DISPLAY
void adjustSmallDisplayVolume(int delta);
#endif

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

void beginDevice() {
#if STACKCHAN_HAS_STACKCHAN_BSP
  M5StackChan.begin();
#else
  auto cfg = M5.config();
  cfg.internal_imu = true;
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  cfg.internal_rtc = true;
  cfg.clear_display = true;
#if STACKCHAN_HAS_ATOMIC_ECHO_BASE
  cfg.external_speaker.atomic_echo = true;
#endif
  M5.begin(cfg);
  logDeviceAudioConfig();
#endif
}

void updateDevice() {
#if STACKCHAN_HAS_STACKCHAN_BSP
  M5StackChan.update();
#else
  M5.update();
#endif
}

void pulseHaptic(uint8_t level, unsigned long durationMs, unsigned long now) {
#if STACKCHAN_HAS_HAPTIC
  M5.Power.setVibration(level);
  hapticOffMs = now + durationMs;
#else
  (void)level;
  (void)durationMs;
  (void)now;
#endif
}

void updateHaptic(unsigned long now) {
#if STACKCHAN_HAS_HAPTIC
  if (hapticOffMs != 0 && static_cast<long>(now - hapticOffMs) >= 0) {
    M5.Power.setVibration(0);
    hapticOffMs = 0;
  }
#else
  (void)now;
#endif
}

void logDeviceAudioConfig() {
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  const auto speakerCfg = M5.Speaker.config();
  const auto micCfg = M5.Mic.config();
  Serial.printf("[device] board=%d display=%dx%d speaker_enabled=%d spk dout=%d bck=%d ws=%d mck=%d port=%d stereo=%d mag=%u sample=%lu mic din=%d bck=%d ws=%d mck=%d port=%d\n",
                static_cast<int>(M5.getBoard()),
                M5.Display.width(),
                M5.Display.height(),
                M5.Speaker.isEnabled() ? 1 : 0,
                speakerCfg.pin_data_out,
                speakerCfg.pin_bck,
                speakerCfg.pin_ws,
                speakerCfg.pin_mck,
                static_cast<int>(speakerCfg.i2s_port),
                speakerCfg.stereo ? 1 : 0,
                speakerCfg.magnification,
                static_cast<unsigned long>(speakerCfg.sample_rate),
                micCfg.pin_data_in,
                micCfg.pin_bck,
                micCfg.pin_ws,
                micCfg.pin_mck,
                static_cast<int>(micCfg.i2s_port));
#endif
}

class StreetPassScanCallbacks : public NimBLEScanCallbacks {
public:
  void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
    rememberStreetPassAdvertisedDevice(advertisedDevice, millis());
  }

  void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
    (void)advertisedDevice;
  }
};

StreetPassScanCallbacks streetPassScanCallbacks;

class StreetPassServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    (void)server;
    (void)connInfo;
    streetPassInboundConnectedEvent = true;
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    (void)server;
    (void)connInfo;
    streetPassInboundDisconnectReason = reason;
    streetPassInboundDisconnectedEvent = true;
  }
};

class StreetPassEncounterWriteCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
    (void)connInfo;
    if (characteristic == nullptr) {
      return;
    }
    if (streetPassInboundWritePending) {
      return;
    }
    std::string value = characteristic->getValue();
    const size_t length = min(value.size(), sizeof(streetPassInboundWriteBuffer) - 1);
    memcpy(streetPassInboundWriteBuffer, value.data(), length);
    streetPassInboundWriteBuffer[length] = '\0';
    streetPassInboundWriteLength = length;
    streetPassInboundWritePending = true;
  }
};

StreetPassServerCallbacks streetPassServerCallbacks;
StreetPassEncounterWriteCallbacks streetPassEncounterWriteCallbacks;

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

String efuseDeviceId() {
  const uint64_t mac = ESP.getEfuseMac();
  const uint32_t high = static_cast<uint32_t>((mac >> 32) & 0xffff);
  const uint32_t low = static_cast<uint32_t>(mac & 0xffffffffUL);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "stackchan_%04lx%08lx",
           static_cast<unsigned long>(high),
           static_cast<unsigned long>(low));
  return String(buffer);
}

String randomDeviceId() {
  const uint64_t mac = ESP.getEfuseMac();
  const uint32_t first = esp_random() ^ static_cast<uint32_t>(mac & 0xffffffffUL);
  const uint32_t second = esp_random() ^ static_cast<uint32_t>((mac >> 16) & 0xffffffffUL);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "stackchan_%08lx%08lx",
           static_cast<unsigned long>(first),
           static_cast<unsigned long>(second));
  return String(buffer);
}

void ensureDeviceId() {
  if (deviceId.length() > 0) {
    return;
  }

  preferences.begin("device", false);
  deviceId = preferences.getString("id", "");
  if (deviceId.length() == 0) {
    const String generated = randomDeviceId();
    const size_t written = preferences.putString("id", generated);
    deviceId = written > 0 ? generated : efuseDeviceId();
  }
  preferences.end();

  Serial.printf("[device] id=%s\n", deviceId.c_str());
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

bool streetPassPageVisible() {
  return infoScreenVisible && settingsPage == SettingsPage::StreetPass && displayOn;
}

bool streetPassBusyForExchange() {
  return streetPassBusyReason()[0] != '\0';
}

const char* streetPassBusyReason() {
  const bool activelyListening = (currentState == ChanState::Listening ||
                                  audioController.state() == ChanState::Listening) &&
                                 audioController.isMicStreaming();
  if (activelyListening) {
    return "listening";
  }
  if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
    return "speaking";
  }
  if (audioController.isPlaybackDraining()) {
    return "playback";
  }
  return "";
}

uint32_t unixFromUtcParts(int year, int month, int day, int hour, int minute, int second) {
  tm value = {};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  value.tm_sec = second;
  value.tm_isdst = 0;
  setenv("TZ", "UTC", 1);
  tzset();
  return static_cast<uint32_t>(mktime(&value));
}

bool validStreetPassUnix(uint32_t unixTime) {
  return unixTime >= STREETPASS_VALID_UNIX_MIN;
}

String makeStreetPassInfoJson() {
  const StreetPassProfile& profile = streetPassController.profile();
  JsonDocument doc;
  doc["v"] = STREETPASS_PROTOCOL_VERSION;
  doc["role"] = "stackchan";
  doc["name"] = profile.name;
  doc["cardSeq"] = profile.cardSeq;
  doc["caps"] = "public_card,encounter_write";
  String body;
  serializeJson(doc, body);
  return body;
}

String makeStreetPassPublicCardJson() {
  const StreetPassProfile& profile = streetPassController.profile();
  JsonDocument doc;
  doc["v"] = STREETPASS_PROTOCOL_VERSION;
  doc["profileId"] = profile.shareProfile ? profile.profileId : "";
  doc["cardSeq"] = profile.cardSeq;
  doc["name"] = profile.shareProfile ? profile.name : "";
  doc["message"] = profile.shareProfile ? profile.message : "";
  doc["source"] = "stackchan";
  String body;
  serializeJson(doc, body);
  return body;
}

String makeStreetPassAdvertisementName() {
  return String("STC-") + String(streetPassController.profile().cardSeq);
}

uint32_t streetPassFnv1a(const String& value) {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < value.length(); ++i) {
    hash ^= static_cast<uint8_t>(value[i]);
    hash *= 16777619UL;
  }
  return hash;
}

uint16_t streetPassNameHash(const String& value) {
  const uint32_t hash = streetPassFnv1a(value);
  return static_cast<uint16_t>((hash >> 16) ^ (hash & 0xffff));
}

uint32_t streetPassPeerToken() {
  const StreetPassProfile& profile = streetPassController.profile();
  uint32_t token = streetPassFnv1a(profile.profileId);
  if (token == 0) {
    token = 1;
  }
  return token;
}

bool streetPassManufacturerData(const std::string& manufacturer) {
  return manufacturer.length() >= 10 &&
         static_cast<uint8_t>(manufacturer[2]) == 'S' &&
         static_cast<uint8_t>(manufacturer[3]) == 'P';
}

bool streetPassPeerTokenFromAdvertisement(const NimBLEAdvertisedDevice* device, uint32_t& token) {
  token = 0;
  if (device == nullptr || !device->haveManufacturerData()) {
    return false;
  }
  const std::string manufacturer = device->getManufacturerData();
  if (!streetPassManufacturerData(manufacturer)) {
    return false;
  }
  token = static_cast<uint32_t>(static_cast<uint8_t>(manufacturer[4])) |
          (static_cast<uint32_t>(static_cast<uint8_t>(manufacturer[5])) << 8) |
          (static_cast<uint32_t>(static_cast<uint8_t>(manufacturer[6])) << 16) |
          (static_cast<uint32_t>(static_cast<uint8_t>(manufacturer[7])) << 24);
  return token != 0;
}

bool streetPassShouldInitiate(uint32_t peerToken, const String& peerAddress) {
  const uint32_t ownToken = streetPassPeerToken();
  if (peerToken == 0 || ownToken == 0) {
    return true;
  }
  if (ownToken != peerToken) {
    return ownToken < peerToken;
  }

  const String ownAddress = String(NimBLEDevice::getAddress().toString().c_str());
  if (ownAddress.length() == 0 || peerAddress.length() == 0) {
    return true;
  }
  return ownAddress < peerAddress;
}

String bytesToHexString(const std::string& data) {
  static const char* hex = "0123456789abcdef";
  String out;
  out.reserve(data.length() * 2);
  for (size_t i = 0; i < data.length(); ++i) {
    const uint8_t value = static_cast<uint8_t>(data[i]);
    out += hex[value >> 4];
    out += hex[value & 0x0f];
  }
  return out;
}

String streetPassAdvertisementKey(const NimBLEAdvertisedDevice* device) {
  if (device == nullptr) {
    return "";
  }

  if (device->haveManufacturerData()) {
    const std::string manufacturer = device->getManufacturerData();
    if (streetPassManufacturerData(manufacturer)) {
      char buffer[16];
      snprintf(buffer,
               sizeof(buffer),
               "sp-name:%02x%02x",
               static_cast<unsigned>(static_cast<uint8_t>(manufacturer[8])),
               static_cast<unsigned>(static_cast<uint8_t>(manufacturer[9])));
      return String(buffer);
    }
  }

  String key = device->haveName() ? String(device->getName().c_str()) : String();
  if (device->haveManufacturerData()) {
    key += "|m:";
    key += bytesToHexString(device->getManufacturerData());
  }
  return key;
}

void updateStreetPassGattCharacteristics() {
  if (streetPassInfoCharacteristic != nullptr) {
    const String info = makeStreetPassInfoJson();
    streetPassInfoCharacteristic->setValue(reinterpret_cast<const uint8_t*>(info.c_str()), info.length());
  }
  if (streetPassPublicCardCharacteristic != nullptr) {
    const String card = makeStreetPassPublicCardJson();
    streetPassPublicCardCharacteristic->setValue(reinterpret_cast<const uint8_t*>(card.c_str()), card.length());
  }
}

void updateStreetPassAdvertising() {
  if (!streetPassBleReady) {
    return;
  }

  const StreetPassProfile& profile = streetPassController.profile();
  const bool shouldAdvertise = profile.enabled && profile.shareProfile;
  const uint32_t peerToken = streetPassPeerToken();
  const bool needsRefresh = shouldAdvertise &&
                            (!streetPassAdvertising ||
                             streetPassAdvertisedCardSeq != profile.cardSeq ||
                             streetPassAdvertisedPeerToken != peerToken ||
                             streetPassAdvertisedEnabled != shouldAdvertise);
  if (!shouldAdvertise) {
    if (streetPassAdvertising) {
      stopStreetPassAdvertising("profile_off");
    }
    return;
  }
  if (!needsRefresh) {
    return;
  }

  updateStreetPassGattCharacteristics();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->stop();
  adv->clearData();
  const String name = makeStreetPassAdvertisementName();
  const uint16_t nameHash = streetPassNameHash(profile.name);
  uint8_t manufacturerData[10] = {
    0xff, 0xff, 'S', 'P',
    static_cast<uint8_t>(peerToken & 0xff),
    static_cast<uint8_t>((peerToken >> 8) & 0xff),
    static_cast<uint8_t>((peerToken >> 16) & 0xff),
    static_cast<uint8_t>((peerToken >> 24) & 0xff),
    static_cast<uint8_t>(nameHash & 0xff),
    static_cast<uint8_t>((nameHash >> 8) & 0xff)
  };
  adv->setName(name.c_str());
  adv->setManufacturerData(manufacturerData, sizeof(manufacturerData));
  adv->setMinInterval(160);
  adv->setMaxInterval(320);
  adv->enableScanResponse(true);
  adv->start();
  streetPassAdvertising = true;
  streetPassAdvertisedEnabled = true;
  streetPassAdvertisedCardSeq = profile.cardSeq;
  streetPassAdvertisedPeerToken = peerToken;
  Serial.printf("[streetpass] advertising started name=%s token=%08lx\n",
                name.c_str(),
                static_cast<unsigned long>(peerToken));
}

void restartStreetPassAdvertising() {
  if (!streetPassBleReady) {
    return;
  }
  streetPassAdvertising = false;
  streetPassAdvertisedEnabled = false;
  streetPassAdvertisedPeerToken = 0;
  updateStreetPassAdvertising();
}

void stopStreetPassAdvertising(const char* reason) {
  if (!streetPassBleReady || !streetPassAdvertising) {
    return;
  }
  NimBLEDevice::getAdvertising()->stop();
  streetPassAdvertising = false;
  streetPassAdvertisedEnabled = false;
  streetPassAdvertisedPeerToken = 0;
  Serial.printf("[streetpass] advertising stopped reason=%s\n", reason);
}

bool readStreetPassRtcUnix(uint32_t& unixTime) {
  if (!M5.Rtc.isEnabled()) {
    return false;
  }
  if (M5.Rtc.getVoltLow()) {
    Serial.println("[streetpass] rtc ignored: voltage low");
    return false;
  }

  m5::rtc_date_t date;
  m5::rtc_time_t time;
  if (!M5.Rtc.getDateTime(&date, &time)) {
    Serial.println("[streetpass] rtc read failed");
    return false;
  }
  if (date.year < 2023 || date.year > 2099 || date.month < 1 || date.month > 12 ||
      date.date < 1 || date.date > 31 || time.hours > 23 || time.minutes > 59 || time.seconds > 59) {
    Serial.printf("[streetpass] rtc invalid %04d-%02d-%02d %02d:%02d:%02d\n",
                  date.year,
                  date.month,
                  date.date,
                  time.hours,
                  time.minutes,
                  time.seconds);
    return false;
  }

  unixTime = unixFromUtcParts(date.year, date.month, date.date, time.hours, time.minutes, time.seconds);
  return validStreetPassUnix(unixTime);
}

void restoreStreetPassTimeFromRtc(unsigned long now) {
  uint32_t unixTime = 0;
  if (!readStreetPassRtcUnix(unixTime)) {
    return;
  }
  if (streetPassController.syncTime(unixTime, "UTC", now)) {
    Serial.printf("[streetpass] rtc time restored unix=%lu\n", static_cast<unsigned long>(unixTime));
  }
}

void writeStreetPassRtcTime(uint32_t unixTime) {
  if (!validStreetPassUnix(unixTime) || !M5.Rtc.isEnabled()) {
    return;
  }
  time_t raw = static_cast<time_t>(unixTime);
  tm utc = {};
  gmtime_r(&raw, &utc);
  M5.Rtc.setDateTime(&utc);
  Serial.printf("[streetpass] rtc time written unix=%lu\n", static_cast<unsigned long>(unixTime));
}

void updateStreetPassNetworkTime(unsigned long now) {
  if (networkMode != NetworkMode::Sta || WiFi.status() != WL_CONNECTED || now < nextStreetPassNtpSyncMs) {
    return;
  }

  if (!streetPassNtpConfigured) {
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    streetPassNtpConfigured = true;
  }

  const time_t unixTime = time(nullptr);
  if (unixTime >= static_cast<time_t>(STREETPASS_VALID_UNIX_MIN)) {
    const uint32_t syncedUnix = static_cast<uint32_t>(unixTime);
    if (streetPassController.syncTime(syncedUnix, "UTC", now)) {
      Serial.printf("[streetpass] ntp time synced unix=%lu\n", static_cast<unsigned long>(unixTime));
      writeStreetPassRtcTime(syncedUnix);
    }
    nextStreetPassNtpSyncMs = now + STREETPASS_NTP_REFRESH_MS;
    return;
  }

  nextStreetPassNtpSyncMs = now + STREETPASS_NTP_RETRY_MS;
}

void updateClockOverlay(unsigned long now) {
#if CLOCK_DISPLAY_ENABLED
  if (lastClockOverlayUpdateMs != 0 && now - lastClockOverlayUpdateMs < CLOCK_DISPLAY_UPDATE_MS) {
    return;
  }
  lastClockOverlayUpdateMs = now;

  const uint32_t unixTime = streetPassController.estimatedUnix(now);
  if (!validStreetPassUnix(unixTime)) {
    faceController.setClockText("--:--", false);
    return;
  }

  const int64_t localUnix = static_cast<int64_t>(unixTime) +
                            static_cast<int64_t>(CLOCK_DISPLAY_UTC_OFFSET_MINUTES) * 60LL;
  time_t raw = static_cast<time_t>(localUnix);
  tm local = {};
  gmtime_r(&raw, &local);
  char text[6] = {};
  snprintf(text, sizeof(text), "%02d:%02d", local.tm_hour, local.tm_min);
  faceController.setClockText(text, true);
#else
  (void)now;
#endif
}

void beginStreetPassBle() {
  if (streetPassBleReady) {
    return;
  }

  NimBLEDevice::init(STREETPASS_BLE_DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  streetPassGattServer = NimBLEDevice::createServer();
  streetPassGattServer->setCallbacks(&streetPassServerCallbacks);
  NimBLEService* service = streetPassGattServer->createService(STREETPASS_SERVICE_UUID);
  streetPassInfoCharacteristic = service->createCharacteristic(STREETPASS_INFO_UUID, NIMBLE_PROPERTY::READ);
  streetPassPublicCardCharacteristic = service->createCharacteristic(STREETPASS_PUBLIC_CARD_UUID, NIMBLE_PROPERTY::READ);
  streetPassEncounterWriteCharacteristic = service->createCharacteristic(STREETPASS_ENCOUNTER_WRITE_UUID, NIMBLE_PROPERTY::WRITE);
  streetPassEncounterWriteCharacteristic->setCallbacks(&streetPassEncounterWriteCallbacks);
  updateStreetPassGattCharacteristics();
  service->start();
  streetPassGattServer->start();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&streetPassScanCallbacks, true);
  scan->setActiveScan(false);
  scan->setInterval(80);
  scan->setWindow(80);
  streetPassBleReady = true;
  nextStreetPassScanMs = millis() + 2000;
  updateStreetPassAdvertising();
  Serial.println("[streetpass] BLE scan ready");
}

void clearStreetPassBleCandidates() {
  for (uint8_t i = 0; i < kStreetPassCandidateCount; ++i) {
    streetPassCandidates[i] = StreetPassBleCandidate();
  }
  streetPassForceNextExchange = true;
  nextStreetPassExchangeMs = millis();
  nextStreetPassScanMs = millis();
  Serial.println("[streetpass] BLE candidates cleared");
}

void resetStreetPassBleAttemptCooldowns() {
  for (uint8_t i = 0; i < kStreetPassCandidateCount; ++i) {
    streetPassCandidates[i].lastAttemptMs = 0;
    streetPassCandidates[i].exchangeQueued = false;
    if (streetPassCandidates[i].active) {
      streetPassCandidates[i].firstSeenMs = millis();
      streetPassCandidates[i].seenCount = 0;
    }
  }
  nextStreetPassExchangeMs = millis();
  nextStreetPassScanMs = millis();
  Serial.println("[streetpass] BLE attempt cooldowns reset");
}

void updateStreetPassInboundEvents(unsigned long now, bool processWrites) {
  if (streetPassInboundConnectedEvent) {
    streetPassInboundConnectedEvent = false;
    streetPassGattServerConnected = true;
    if (streetPassScanActive) {
      NimBLEDevice::getScan()->stop();
      streetPassScanActive = false;
    }
    Serial.println("[streetpass] inbound GATT connected");
  }

  if (processWrites && streetPassInboundWritePending) {
    char body[sizeof(streetPassInboundWriteBuffer)] = {};
    const size_t length = min(static_cast<size_t>(streetPassInboundWriteLength), sizeof(body) - 1);
    memcpy(body, streetPassInboundWriteBuffer, length);
    streetPassInboundWritePending = false;
    streetPassInboundWriteLength = 0;
    const bool ok = streetPassController.recordPublicCard(body, -127, now);
    Serial.printf("[streetpass] encounter write received bytes=%u ok=%d\n",
                  static_cast<unsigned>(length),
                  ok ? 1 : 0);
    if (ok && streetPassPageVisible()) {
      streetPassController.markAllRead();
      drawInfoScreen();
    }
    if (ok) {
      streetPassForceNextExchange = false;
    }
  }

  if (streetPassInboundDisconnectedEvent) {
    const int reason = streetPassInboundDisconnectReason;
    streetPassInboundDisconnectedEvent = false;
    streetPassGattServerConnected = false;
    streetPassBleSettleUntilMs = now + STREETPASS_GATT_SETTLE_MS;
    nextStreetPassScanMs = streetPassBleSettleUntilMs;
    nextStreetPassExchangeMs = streetPassBleSettleUntilMs;
    Serial.printf("[streetpass] inbound GATT disconnected reason=%d\n", reason);
  }
}

bool updateDisplayOffStreetPassMode(unsigned long now) {
  if (displayOn) {
    return false;
  }

  updateUsbSerial(now);
  updateUsbSerialDeferredIdle(now);
  updateWiFi(now);
  updateStreetPassNetworkTime(now);
  updateClockOverlay(now);
  streetPassController.update(now);
  updateStreetPassBle(now);

  if (wsStarted) {
    wsServer.loop();
  }
  if (httpStarted && (now % 200) < 20) {
    httpServer.handleClient();
  }
  updateCameraButtonPending(now);
  audioController.update(now);
  updateDeferredFaceState();
  updateDeferredFaceMode();
  delay(30);
  return true;
}

void rememberStreetPassAdvertisedDevice(const NimBLEAdvertisedDevice* device, unsigned long now) {
  if (!streetPassBleReady || device == nullptr || !streetPassController.enabled()) {
    return;
  }
  const bool hasStreetPassManufacturer = device->haveManufacturerData() &&
                                         streetPassManufacturerData(device->getManufacturerData());
  if (!hasStreetPassManufacturer) {
    return;
  }

  const String address = String(device->getAddress().toString().c_str());
  const String advertisedName = device->haveName() ? String(device->getName().c_str()) : String();
  const String advertisementKey = streetPassAdvertisementKey(device);
  uint32_t peerToken = 0;
  const bool hasPeerToken = streetPassPeerTokenFromAdvertisement(device, peerToken);
  const bool normalShouldInitiate = !hasPeerToken || streetPassShouldInitiate(peerToken, address);
  StreetPassBleCandidate* candidate = nullptr;
  StreetPassBleCandidate* empty = nullptr;
  uint8_t oldestIndex = 0;
  for (uint8_t i = 0; i < kStreetPassCandidateCount; ++i) {
    StreetPassBleCandidate& item = streetPassCandidates[i];
    if (!item.active && empty == nullptr) {
      empty = &item;
    }
    if (item.active && item.address == address) {
      candidate = &item;
      break;
    }
    if (item.lastSeenMs < streetPassCandidates[oldestIndex].lastSeenMs) {
      oldestIndex = i;
    }
  }
  if (candidate == nullptr) {
    candidate = empty != nullptr ? empty : &streetPassCandidates[oldestIndex];
    *candidate = StreetPassBleCandidate();
    candidate->active = true;
    candidate->address = address;
    candidate->name = advertisedName;
    candidate->advertisementKey = advertisementKey;
    candidate->bleAddress = device->getAddress();
    candidate->addressType = device->getAddressType();
    candidate->peerToken = peerToken;
    candidate->hasPeerToken = hasPeerToken;
    candidate->firstSeenMs = static_cast<uint32_t>(now);
    candidate->rssiMax = device->getRSSI();
    Serial.printf("[streetpass] seen addr=%s type=%u rssi=%d token=%08lx initiate=%d\n",
                  candidate->address.c_str(),
                  static_cast<unsigned>(candidate->addressType),
                  static_cast<int>(candidate->rssiMax),
                  static_cast<unsigned long>(peerToken),
                  (normalShouldInitiate || streetPassForceNextExchange) ? 1 : 0);
  }
  if (advertisedName.length() > 0 || advertisementKey.length() > 0) {
    const bool advertisementChanged = candidate->advertisementKey.length() > 0 && candidate->advertisementKey != advertisementKey;
    candidate->name = advertisedName;
    candidate->advertisementKey = advertisementKey;
    if (advertisementChanged) {
      candidate->firstSeenMs = static_cast<uint32_t>(now);
      candidate->seenCount = 0;
      candidate->lastAttemptMs = 0;
      candidate->exchangeQueued = false;
      Serial.printf("[streetpass] adv changed addr=%s key=%s\n",
                    candidate->address.c_str(),
                    candidate->advertisementKey.c_str());
    }
  }
  candidate->peerToken = peerToken;
  candidate->hasPeerToken = hasPeerToken;

  candidate->lastSeenMs = static_cast<uint32_t>(now);
  candidate->seenCount = min<uint16_t>(static_cast<uint16_t>(candidate->seenCount + 1), 65535);
  candidate->rssiMax = max<int8_t>(candidate->rssiMax, device->getRSSI());

  if (!normalShouldInitiate && streetPassForceNextExchange &&
      candidate->lastSeenMs - candidate->firstSeenMs < STREETPASS_FORCE_PASSIVE_GRACE_MS) {
    candidate->exchangeQueued = false;
    return;
  }

  if (!normalShouldInitiate && !streetPassForceNextExchange) {
    candidate->exchangeQueued = false;
    return;
  }

  const bool ready = candidate->seenCount >= STREETPASS_OBSERVE_MIN_COUNT &&
                     candidate->lastSeenMs - candidate->firstSeenMs >= STREETPASS_OBSERVE_MIN_MS &&
                     candidate->rssiMax >= STREETPASS_RSSI_MIN_DBM &&
                     (candidate->lastAttemptMs == 0 || now - candidate->lastAttemptMs >= STREETPASS_CONNECT_RETRY_MS);
  if (ready && !candidate->exchangeQueued) {
    candidate->exchangeQueued = true;
    nextStreetPassExchangeMs = now;
    Serial.printf("[streetpass] candidate ready addr=%s seen=%u rssi=%d\n",
                  candidate->address.c_str(),
                  static_cast<unsigned>(candidate->seenCount),
                  static_cast<int>(candidate->rssiMax));
  }
}

bool exchangeStreetPassCandidate(StreetPassBleCandidate& candidate, unsigned long now) {
  if (!candidate.active || candidate.address.length() == 0) {
    return false;
  }
  if (streetPassGattServerConnected) {
    candidate.exchangeQueued = false;
    nextStreetPassExchangeMs = now + 1000;
    Serial.printf("[streetpass] exchange deferred inbound addr=%s\n", candidate.address.c_str());
    return false;
  }

  candidate.lastAttemptMs = static_cast<uint32_t>(now);
  candidate.exchangeQueued = false;
  Serial.printf("[streetpass] exchange start addr=%s rssi=%d\n",
                candidate.address.c_str(),
                static_cast<int>(candidate.rssiMax));
  const bool resumeAdvertising = streetPassAdvertising;
  if (resumeAdvertising) {
    stopStreetPassAdvertising("exchange");
    delay(STREETPASS_CONNECT_PREPARE_MS);
  }
  auto finishExchange = [&](bool result) {
    streetPassBleSettleUntilMs = millis() + STREETPASS_GATT_SETTLE_MS;
    nextStreetPassScanMs = streetPassBleSettleUntilMs;
    nextStreetPassExchangeMs = streetPassBleSettleUntilMs;
    return result;
  };

  NimBLEClient* client = streetPassGattClient;
  if (client != nullptr && client->isConnected()) {
    client->disconnect();
  }
  if (client == nullptr) {
    client = NimBLEDevice::createClient();
    streetPassGattClient = client;
  }
  if (client == nullptr) {
    Serial.println("[streetpass] create client failed");
    return finishExchange(false);
  }
  client->setConnectTimeout(5000);
  client->setConnectionParams(24, 40, 0, 80);

  bool ok = false;
  String publicCard;
  bool connected = client->connect(candidate.bleAddress, true, false, false);
  if (!connected) {
    const int firstError = client->getLastError();
    Serial.printf("[streetpass] connect failed addr=%s type=%u err=%d\n",
                  candidate.address.c_str(),
                  static_cast<unsigned>(candidate.addressType),
                  firstError);
  }

  if (!connected) {
    Serial.printf("[streetpass] exchange skipped addr=%s err=%d\n",
                  candidate.address.c_str(),
                  client->getLastError());
    return finishExchange(false);
  } else {
    NimBLERemoteService* service = client->getService(STREETPASS_SERVICE_UUID);
    if (service == nullptr) {
      Serial.println("[streetpass] service not found");
    } else {
      NimBLERemoteCharacteristic* card = service->getCharacteristic(STREETPASS_PUBLIC_CARD_UUID);
      if (card == nullptr || !card->canRead()) {
        Serial.println("[streetpass] public card not readable");
      } else {
        NimBLEAttValue value = card->readValue();
        publicCard = String(value.c_str()).substring(0, value.length());
        ok = streetPassController.recordPublicCard(publicCard.c_str(), candidate.rssiMax, now);
        Serial.printf("[streetpass] public card read bytes=%u ok=%d\n",
                      static_cast<unsigned>(value.length()),
                      ok ? 1 : 0);
      }

      NimBLERemoteCharacteristic* write = service->getCharacteristic(STREETPASS_ENCOUNTER_WRITE_UUID);
      if (write != nullptr && write->canWrite()) {
        const StreetPassProfile& profile = streetPassController.profile();
        JsonDocument receipt;
        receipt["v"] = STREETPASS_PROTOCOL_VERSION;
        receipt["type"] = "encounter.card";
        receipt["receivedOk"] = ok;
        receipt["profileId"] = profile.profileId;
        receipt["name"] = profile.name;
        receipt["message"] = profile.message;
        receipt["cardSeq"] = profile.cardSeq;
        receipt["timestampMs"] = static_cast<uint32_t>(now);
        String body;
        serializeJson(receipt, body);
        write->writeValue(body.c_str(), body.length(), false);
      }
    }
    client->disconnect();
  }

  if (ok) {
    streetPassForceNextExchange = false;
    Serial.printf("[streetpass] exchanged addr=%s card=%s\n",
                  candidate.address.c_str(),
                  publicCard.c_str());
    candidate.firstSeenMs = static_cast<uint32_t>(now);
    candidate.lastSeenMs = static_cast<uint32_t>(now);
    candidate.seenCount = 0;
    candidate.exchangeQueued = false;
    if (streetPassPageVisible()) {
      streetPassController.markAllRead();
      drawInfoScreen();
    }
  }
  if (!ok) {
    streetPassForceNextExchange = false;
  }
  return finishExchange(ok);
}

void updateStreetPassBle(unsigned long now) {
  const bool enabled = streetPassController.enabled();
  if (enabled != streetPassLastEnabled) {
    streetPassLastEnabled = enabled;
    Serial.printf("[streetpass] %s\n", enabled ? "enabled" : "disabled");
  }

  if (!enabled) {
    if (streetPassBleReady && streetPassScanActive) {
      NimBLEDevice::getScan()->stop();
      streetPassScanActive = false;
    }
    stopStreetPassAdvertising("disabled");
    streetPassBlePaused = false;
    streetPassBleSettleUntilMs = 0;
    return;
  }

  if (!streetPassBleReady) {
    beginStreetPassBle();
    return;
  }

  NimBLEScan* scan = NimBLEDevice::getScan();
  const char* busyReason = streetPassBusyReason();
  updateStreetPassInboundEvents(now, busyReason[0] == '\0');

  if (busyReason[0] != '\0') {
    if (streetPassScanActive) {
      scan->stop();
      streetPassScanActive = false;
    }
    stopStreetPassAdvertising(busyReason);
    if (!streetPassBlePaused) {
      Serial.printf("[streetpass] paused reason=%s currentState=%s audioState=%s vad=%d mic=%d playback=%d\n",
                    busyReason,
                    chanStateName(currentState),
                    chanStateName(audioController.state()),
                    vadActive ? 1 : 0,
                    audioController.isMicStreaming() ? 1 : 0,
                    audioController.isPlaybackDraining() ? 1 : 0);
    }
    streetPassBlePaused = true;
    nextStreetPassScanMs = now + STREETPASS_SCAN_BUSY_INTERVAL_MS;
    return;
  }
  if (streetPassBlePaused) {
    streetPassBlePaused = false;
    nextStreetPassScanMs = now;
    streetPassBleSettleUntilMs = 0;
    Serial.printf("[streetpass] resumed currentState=%s audioState=%s vad=%d mic=%d playback=%d\n",
                  chanStateName(currentState),
                  chanStateName(audioController.state()),
                  vadActive ? 1 : 0,
                  audioController.isMicStreaming() ? 1 : 0,
                  audioController.isPlaybackDraining() ? 1 : 0);
  }

  if (streetPassGattServerConnected) {
    if (streetPassScanActive) {
      scan->stop();
      streetPassScanActive = false;
    }
    nextStreetPassScanMs = now + 1000;
    return;
  }

  if (streetPassBleSettleUntilMs != 0 && now < streetPassBleSettleUntilMs) {
    if (streetPassScanActive) {
      scan->stop();
      streetPassScanActive = false;
    }
    nextStreetPassScanMs = streetPassBleSettleUntilMs;
    return;
  }
  if (streetPassBleSettleUntilMs != 0 && now >= streetPassBleSettleUntilMs) {
    streetPassBleSettleUntilMs = 0;
  }

  updateStreetPassAdvertising();

  if (streetPassScanActive && now - streetPassScanStartedMs >= STREETPASS_SCAN_DURATION_MS) {
    scan->stop();
    streetPassScanActive = false;
    restartStreetPassAdvertising();
    const unsigned long interval = STREETPASS_SCAN_IDLE_INTERVAL_MS;
    nextStreetPassScanMs = now + interval;
    Serial.printf("[streetpass] scan stop next=%lums\n", interval);
  }

  if (!streetPassScanActive && now >= nextStreetPassScanMs) {
    const bool started = scan->start(0, false, false);
    if (started) {
      streetPassScanStartedMs = now;
      streetPassScanActive = true;
      Serial.println("[streetpass] scan start");
    } else {
      nextStreetPassScanMs = now + STREETPASS_SCAN_IDLE_INTERVAL_MS;
      Serial.println("[streetpass] scan start failed");
    }
  }

  if (streetPassExchangeInProgress || now < nextStreetPassExchangeMs) {
    return;
  }

  for (uint8_t i = 0; i < kStreetPassCandidateCount; ++i) {
    StreetPassBleCandidate& candidate = streetPassCandidates[i];
    if (!candidate.active || !candidate.exchangeQueued) {
      continue;
    }
    streetPassExchangeInProgress = true;
    if (streetPassScanActive) {
      scan->stop();
      streetPassScanActive = false;
      restartStreetPassAdvertising();
      nextStreetPassScanMs = now + STREETPASS_SCAN_IDLE_INTERVAL_MS;
      delay(STREETPASS_CONNECT_PREPARE_MS);
    }
    exchangeStreetPassCandidate(candidate, now);
    streetPassExchangeInProgress = false;
    nextStreetPassExchangeMs = now + 1000;
    return;
  }
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
#if THERMAL_WARM_FACE_ENABLED
  if (level == ThermalLevel::Warm && !appClientConnected()) {
    return ThermalFaceMode::Warm;
  }
#endif
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
#if STACKCHAN_GURUGURU_FACE_ENABLED
  updateGuruguruFaceAvailability(millis());
#endif
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

#if STACKCHAN_SMALL_DISPLAY
void switchNetworkModeWithoutRestart() {
  const NetworkMode nextMode = networkMode == NetworkMode::SoftAp ? NetworkMode::Sta : NetworkMode::SoftAp;
  Serial.printf("[network] switching mode to %s without restart\n",
                nextMode == NetworkMode::SoftAp ? "SoftAP" : "STA");
  saveNetworkMode(nextMode);
  networkMode = nextMode;
  wifiConnectAttempts = 0;
  connectWiFi();
  if (infoScreenVisible && displayOn) {
    drawInfoScreen();
  }
}
#endif

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

bool shouldAnimatePettingFace() {
#if STACKCHAN_PET_ANIMATION_ENABLED
  return displayOn && !appClientConnected() && currentState != ChanState::Speaking;
#else
  return false;
#endif
}

uint8_t petAnimationTouchFrameForX(int32_t x, int32_t centerX) {
#if STACKCHAN_DEVICE_STOPWATCH
  const int32_t dx = x - centerX;
  if (dx < -PET_ANIMATION_TOUCH_CENTER_HALF_WIDTH_PX) {
    return 4;
  }
  if (dx > PET_ANIMATION_TOUCH_CENTER_HALF_WIDTH_PX) {
    return 2;
  }
#else
  (void)x;
  (void)centerX;
#endif
  return 3;
}

void setPettingActive(bool active, unsigned long now, unsigned long releaseGraceMs = PET_TOUCH_RELEASE_GRACE_MS) {
  if (active && !displayOn) {
    return;
  }

  if (active) {
    pettingEndMs = now + releaseGraceMs;
    if (!pettingActive) {
      pettingActive = true;
      pettingStartedMs = now;
      pettingFaceAnimated = shouldAnimatePettingFace();
      nextPetMoveMs = 0;
      pettingBasePose = motionController.currentPose();
      cancelListeningNod(false);
#if STACKCHAN_PET_ANIMATION_ENABLED
      faceController.setPetFaceMode(true, now, pettingFaceAnimated);
#else
      faceController.setPetFaceMode(true);
#endif
      const Pose pose = makePettingPoseFromBase(pettingBasePose, false);
      motionController.setTargetPose(pose.pan, pose.tilt);
      sendInteractionEvent("petting", "start", now);
      pulseHaptic(HAPTIC_PETTING_START_LEVEL, HAPTIC_PETTING_START_MS, now);
      lastPettingRepeatEventMs = now;
      Serial.println("[pet] start");
    } else if (now - lastPettingRepeatEventMs >= 800) {
#if STACKCHAN_PET_ANIMATION_ENABLED
      if (pettingFaceAnimated && !shouldAnimatePettingFace()) {
        pettingFaceAnimated = false;
        faceController.setPetFaceMode(true);
      }
#endif
      sendInteractionEvent("petting", "repeat", now);
      lastPettingRepeatEventMs = now;
    }
    return;
  }

  if (!pettingActive) {
    return;
  }

#if STACKCHAN_DEVICE_STOPWATCH
  if (audioBusyForUiEffects() && displayOn) {
    pettingEndMs = now + SCREEN_PETTING_RELEASE_MS;
    return;
  }
#endif

  pettingActive = false;
  const bool longPetting = pettingStartedMs != 0 &&
                           now - pettingStartedMs >= PET_ANIMATION_LONG_THRESHOLD_MS;
  const bool animateEnd = pettingFaceAnimated && displayOn && !appClientConnected();
  if (longPetting) {
    applyAffectionResult(affectionController.applyEvent("petting", 1.0f, 1.0f, nullptr, now), now, true);
  } else {
    applyAffectionResult(affectionController.debugAdjust(PETTING_SHORT_AFFECTION_DELTA), now, true);
  }
  pettingStartedMs = 0;
  pettingFaceAnimated = false;
  pettingEndMs = 0;
  nextPetMoveMs = 0;
  lastPettingRepeatEventMs = 0;
#if STACKCHAN_PET_ANIMATION_ENABLED
  faceController.setPetFaceMode(false, now, animateEnd, longPetting);
#else
  faceController.setPetFaceMode(false);
#endif
  if (!shakeActive) {
    if (currentState == ChanState::Listening) {
#if STACKCHAN_PET_ANIMATION_ENABLED
      if (!animateEnd) {
        applyListeningPresentation(now);
      }
#else
      applyListeningPresentation(now);
#endif
    } else {
      motionController.setMotion("center");
    }
  }
  sendInteractionEvent("petting", "end", now);
  Serial.println("[pet] end");
}

void extendPettingReleaseLinger(unsigned long now, unsigned long lingerMs) {
  if (pettingActive) {
    pettingEndMs = now + lingerMs;
  }
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

bool isGestureMotionStrong(const m5::imu_data_t& data, float* shakeAccOut, float* gyroMagOut) {
  const float ax = data.accel.x;
  const float ay = data.accel.y;
  const float az = data.accel.z;
  const float gx = data.gyro.x;
  const float gy = data.gyro.y;
  const float gz = data.gyro.z;
  const float accMag = sqrtf(ax * ax + ay * ay + az * az);
  const float shakeAcc = fabsf(accMag - 1.0f);
  const float gyroMag = sqrtf(gx * gx + gy * gy + gz * gz);
  if (shakeAccOut != nullptr) {
    *shakeAccOut = shakeAcc;
  }
  if (gyroMagOut != nullptr) {
    *gyroMagOut = gyroMag;
  }

  const float accThreshold = STACKCHAN_IMU_PETTING ? IMU_PETTING_ACC_THRESHOLD_G : SHAKE_ACC_THRESHOLD_G;
  const float gyroThreshold = STACKCHAN_IMU_PETTING ? IMU_PETTING_GYRO_THRESHOLD_DPS : SHAKE_GYRO_THRESHOLD_DPS;
#if STACKCHAN_IMU_PETTING
  return shakeAcc > accThreshold && gyroMag > gyroThreshold;
#elif STACKCHAN_DEVICE_STOPWATCH
  const bool gyroLedShake = gyroMag > gyroThreshold && shakeAcc > SHAKE_GYRO_ACCEL_GATE_G;
  const bool accelLedShake = shakeAcc > accThreshold && gyroMag > SHAKE_ACCEL_GYRO_GATE_DPS;
  return gyroLedShake || accelLedShake;
#else
  return shakeAcc > accThreshold || gyroMag > gyroThreshold;
#endif
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
      pulseHaptic(HAPTIC_SHAKE_LEVEL, HAPTIC_SHAKE_MS, now);
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

#if STACKCHAN_GURUGURU_IMU_ENABLED && STACKCHAN_GURUGURU_FACE_ENABLED
  if (guruguruFaceMode) {
    if (updateGuruguruFaceImu(now, data, imuUpdated)) {
      setShakeActive(false, now);
      shakeStrongSamples = 0;
      return;
    }
    if (guruguruFaceCanRun()) {
      setShakeActive(false, now);
      shakeStrongSamples = 0;
      return;
    }
  }
#endif

  if (shakeActive) {
    if (imuUpdated) {
      updateShakeMotion(data, now);
      if (isGestureMotionStrong(data, nullptr, nullptr)) {
        shakeEndMs = now + SHAKE_FACE_HOLD_MS;
      }
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

  const unsigned long shakeCooldownMs = STACKCHAN_IMU_PETTING ? IMU_PETTING_COOLDOWN_MS : SHAKE_COOLDOWN_MS;
  if (infoScreenVisible || now - lastShakeTriggerMs < shakeCooldownMs) {
    return;
  }

  if (!imuUpdated) {
    return;
  }

  if (!interactionsReady(now)) {
    shakeStrongSamples = 0;
    return;
  }

  const uint8_t requiredSamples = STACKCHAN_IMU_PETTING ? IMU_PETTING_REQUIRED_SAMPLES : SHAKE_REQUIRED_SAMPLES;
  float shakeAcc = 0.0f;
  float gyroMag = 0.0f;
  const bool strongShake = isGestureMotionStrong(data, &shakeAcc, &gyroMag);

  if (strongShake) {
    ++shakeStrongSamples;
    if (shakeStrongSamples >= requiredSamples) {
#if STACKCHAN_IMU_PETTING
      Serial.printf("[pet] imu detected acc=%.2f gyro=%.2f\n", shakeAcc, gyroMag);
      lastShakeTriggerMs = now;
      shakeStrongSamples = 0;
      setPettingActive(true, now);
#else
      Serial.printf("[shake] detected acc=%.2f gyro=%.2f\n", shakeAcc, gyroMag);
      setShakeActive(true, now);
#endif
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
#if STACKCHAN_ROUND_DISPLAY
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString("Stack-chan", cx, cy - 34);
  M5.Display.setTextSize(2);
  M5.Display.drawString(message, cx, cy + 18);
  M5.Display.setTextDatum(top_left);
#else
  M5.Display.setTextSize(2);
  M5.Display.setCursor(16, 32);
  M5.Display.println("Stack-chan");
  M5.Display.setCursor(16, 72);
  M5.Display.println(message);
#endif
}

void drawModeLoadingScreen(const char* title, const char* detail) {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
#if STACKCHAN_ROUND_DISPLAY
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString(title, cx, cy - 42);
  M5.Display.setTextSize(2);
  M5.Display.drawString("Loading...", cx, cy + 2);
  M5.Display.setTextSize(1);
  M5.Display.drawString(detail, cx, cy + 36);
  M5.Display.drawCircle(cx, cy + 72, 10, TFT_DARKGREY);
  M5.Display.fillCircle(cx + 7, cy + 65, 3, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
#else
  M5.Display.setTextSize(2);
  M5.Display.setCursor(16, 32);
  M5.Display.println(title);
  M5.Display.setCursor(16, 72);
  M5.Display.println("Loading...");
  M5.Display.setTextSize(1);
  M5.Display.setCursor(16, 112);
  M5.Display.println(detail);
#endif
  delay(1);
}

void drawGuruguruLoadingScreen() {
  drawModeLoadingScreen("Guruguru", "Preparing face cache");
}

void drawVoiceLoadingScreen() {
  drawModeLoadingScreen("Voice", "Preparing talk cache");
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
#if STACKCHAN_ROUND_DISPLAY
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString("Stack-chan", cx, cy - 48);
  M5.Display.setTextSize(2);
  M5.Display.drawString(String(spinner[initializeSpinnerFrame % 4]) + " Initialize", cx, cy + 6);
  M5.Display.setTextSize(1);
  M5.Display.drawString(String("Ready in ") + String(remainingMs / 1000) + "." + String((remainingMs % 1000) / 100) + "s",
                        cx,
                        cy + 42);
  M5.Display.setTextDatum(top_left);
#else
  M5.Display.setTextSize(2);
  M5.Display.setCursor(16, 32);
  M5.Display.println("Stack-chan");
  M5.Display.setCursor(16, 76);
  M5.Display.printf("%c Initialize", spinner[initializeSpinnerFrame % 4]);

  M5.Display.setTextSize(1);
  M5.Display.setCursor(16, 116);
  M5.Display.printf("Ready in %lu.%lus", remainingMs / 1000, (remainingMs % 1000) / 100);
#endif
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
  ensureDeviceId();
  JsonDocument doc;
  const AffectionState& affection = affectionController.state();
  doc["deviceId"] = deviceId;
  doc["displayName"] = STACKCHAN_DEVICE_DISPLAY_NAME;
  doc["firmwareName"] = STACKCHAN_FIRMWARE_NAME;
  doc["firmwareVersion"] = STACKCHAN_FIRMWARE_VERSION;
  doc["protocolVersion"] = STACKCHAN_APP_PROTOCOL_VERSION;
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
  const MicRuntimeStats micStats = audioController.micRuntimeStats();
  JsonObject mic = doc["mic"].to<JsonObject>();
  mic["enabled"] = micStats.enabled;
  mic["captureActive"] = micStats.captureActive;
  mic["captureRecording"] = micStats.captureRecording;
  mic["hasClient"] = micStats.hasClient;
  mic["muted"] = micStats.muted;
  mic["queuedPackets"] = micStats.queuedPackets;
  mic["queueCapacity"] = micStats.queueCapacity;
  mic["capturedChunks"] = micStats.capturedChunks;
  mic["enqueuedChunks"] = micStats.enqueuedChunks;
  mic["sentChunks"] = micStats.sentChunks;
  mic["sentBytes"] = micStats.sentBytes;
  mic["wsSentChunks"] = micStats.wsSentChunks;
  mic["usbSentChunks"] = micStats.usbSentChunks;
  mic["droppedChunks"] = micStats.droppedChunks;
  mic["captureUnderruns"] = micStats.captureUnderruns;
  mic["captureOverruns"] = micStats.captureOverruns;
  mic["queueOverflows"] = micStats.queueOverflows;
  mic["sendFails"] = micStats.sendFails;
  mic["txSeq"] = micStats.txSeq;
  mic["lastPeak"] = micStats.lastPeak;
  mic["lastCaptureMs"] = micStats.lastCaptureMs;
  mic["lastProcessMs"] = micStats.lastProcessMs;
  mic["lastEnqueueMs"] = micStats.lastEnqueueMs;
  mic["lastSendMs"] = micStats.lastSendMs;
  mic["lastWsSendMs"] = micStats.lastWsSendMs;
  mic["lastUsbSendMs"] = micStats.lastUsbSendMs;
  mic["magnification"] = AUDIO_MIC_MAGNIFICATION;
  mic["softwareGainQ8"] = AUDIO_MIC_SOFTWARE_GAIN_Q8;
  mic["noiseFilterLevel"] = AUDIO_MIC_NOISE_FILTER_LEVEL;
  mic["overSampling"] = AUDIO_MIC_OVERSAMPLING;
  mic["gateEnabled"] = AUDIO_MIC_GATE_ENABLED;
#if STACKCHAN_DEVICE_STOPWATCH
  doc["board"] = static_cast<int>(M5.getBoard());
  JsonObject speaker = doc["speaker"].to<JsonObject>();
  const auto speakerCfg = M5.Speaker.config();
  speaker["enabled"] = M5.Speaker.isEnabled();
  speaker["running"] = M5.Speaker.isRunning();
  speaker["pinDataOut"] = speakerCfg.pin_data_out;
  speaker["pinBck"] = speakerCfg.pin_bck;
  speaker["pinWs"] = speakerCfg.pin_ws;
  speaker["pinMck"] = speakerCfg.pin_mck;
  speaker["i2sPort"] = static_cast<int>(speakerCfg.i2s_port);
  speaker["stereo"] = speakerCfg.stereo;
  speaker["magnification"] = speakerCfg.magnification;
  speaker["sampleRate"] = speakerCfg.sample_rate;
#endif
  doc["lowPowerMode"] = deviceSettings.lowPowerMode;
  doc["thermalLevel"] = thermalLevelName(thermalStatus.level);
  doc["chipTempC"] = thermalStatus.chipTempC;
  doc["pmicTempC"] = thermalStatus.pmicTempC;
  doc["batteryLevel"] = M5.Power.getBatteryLevel();
  doc["charging"] = externalPowerPresent();
  JsonObject streetpass = doc["streetpass"].to<JsonObject>();
  streetPassController.writeStatus(streetpass);
  streetpass["bleReady"] = streetPassBleReady;
  streetpass["scanActive"] = streetPassScanActive;
  streetpass["advertising"] = streetPassAdvertising;
  streetpass["exchangeInProgress"] = streetPassExchangeInProgress;
  streetpass["gattServerConnected"] = streetPassGattServerConnected;
  streetpass["peerToken"] = streetPassPeerToken();
  streetpass["paused"] = streetPassBusyForExchange();
  streetpass["pauseReason"] = streetPassBusyReason();
  doc["currentState"] = chanStateName(currentState);
  doc["audioState"] = chanStateName(audioController.state());
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

void handleSpeakerTestRequest() {
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  const bool ok = audioController.playDiagnosticTone(450);
  JsonDocument doc;
  doc["ok"] = ok;
  doc["board"] = static_cast<int>(M5.getBoard());
  doc["volume"] = deviceSettings.volume;
  doc["audioState"] = chanStateName(audioController.state());
  const auto speakerCfg = M5.Speaker.config();
  JsonObject speaker = doc["speaker"].to<JsonObject>();
  speaker["enabled"] = M5.Speaker.isEnabled();
  speaker["running"] = M5.Speaker.isRunning();
  speaker["pinDataOut"] = speakerCfg.pin_data_out;
  speaker["pinBck"] = speakerCfg.pin_bck;
  speaker["pinWs"] = speakerCfg.pin_ws;
  speaker["pinMck"] = speakerCfg.pin_mck;
  speaker["i2sPort"] = static_cast<int>(speakerCfg.i2s_port);
  speaker["stereo"] = speakerCfg.stereo;
  speaker["sampleRate"] = speakerCfg.sample_rate;

  String body;
  serializeJson(doc, body);
  sendJson(ok ? 200 : 409, body.c_str());
#else
  sendJson(404, "{\"error\":\"not_supported\"}");
#endif
}

void writeMicTestResponse(JsonDocument& doc, const MicDiagnosticResult& result, bool ok) {
  doc["ok"] = ok;
  doc["beginOk"] = result.beginOk;
  doc["durationMs"] = result.durationMs;
  doc["chunks"] = result.chunks;
  doc["underruns"] = result.underruns;
  doc["samples"] = result.sampleCount;
  doc["peak"] = result.peak;
  doc["rms"] = result.rms;
  doc["dc"] = result.dc;
  doc["clipCount"] = result.clipCount;
  doc["board"] = static_cast<int>(M5.getBoard());
  doc["audioState"] = chanStateName(audioController.state());
  doc["micMuted"] = audioController.micMuted();

  const auto micCfg = M5.Mic.config();
  JsonObject mic = doc["mic"].to<JsonObject>();
  mic["enabled"] = M5.Mic.isEnabled();
  mic["running"] = M5.Mic.isRunning();
  mic["pinDataIn"] = micCfg.pin_data_in;
  mic["pinBck"] = micCfg.pin_bck;
  mic["pinWs"] = micCfg.pin_ws;
  mic["pinMck"] = micCfg.pin_mck;
  mic["i2sPort"] = static_cast<int>(micCfg.i2s_port);
  mic["sampleRate"] = micCfg.sample_rate;
  mic["magnification"] = micCfg.magnification;
  mic["noiseFilterLevel"] = micCfg.noise_filter_level;
  mic["overSampling"] = micCfg.over_sampling;
  mic["stereo"] = micCfg.stereo;
  mic["leftChannel"] = micCfg.left_channel;
}

void writePlaybackDiagnosticResponse(JsonDocument& doc, const AudioPlaybackDiagnostic& diag) {
  doc["type"] = "audio.playback_diag";
  doc["state"] = chanStateName(diag.state);
  doc["draining"] = diag.draining;
  doc["playbackStarted"] = diag.playbackStarted;
  doc["speakerEnabled"] = diag.speakerEnabled;
  doc["speakerStartPending"] = diag.speakerStartPending;
  doc["rxAvailable"] = static_cast<uint32_t>(diag.rxAvailable);
  doc["rxCapacity"] = static_cast<uint32_t>(diag.rxCapacity);
  doc["pcmFramesReceived"] = diag.pcmFramesReceived;
  doc["pcmBytesReceived"] = diag.pcmBytesReceived;
  doc["pcmBytesAccepted"] = diag.pcmBytesAccepted;
  doc["pcmBytesDropped"] = diag.pcmBytesDropped;
  doc["rxOverflowEvents"] = diag.rxOverflowEvents;
  doc["dropNotSpeakingEvents"] = diag.dropNotSpeakingEvents;
  doc["dropOddSizeEvents"] = diag.dropOddSizeEvents;
  doc["idleRequests"] = diag.idleRequests;
  doc["playbackStarts"] = diag.playbackStarts;
  doc["playbackFinishes"] = diag.playbackFinishes;
  doc["underflowResets"] = diag.underflowResets;
  doc["speakerQueueFullEvents"] = diag.speakerQueueFullEvents;
  doc["playRawFailEvents"] = diag.playRawFailEvents;
  doc["speakerChunksQueued"] = diag.speakerChunksQueued;
  doc["speakerBytesQueued"] = diag.speakerBytesQueued;
  doc["maxBufferedBytes"] = static_cast<uint32_t>(diag.maxBufferedBytes);
  doc["lastIdleRequestBufferedBytes"] = static_cast<uint32_t>(diag.lastIdleRequestBufferedBytes);
  doc["lastUnderflowBufferedBytes"] = static_cast<uint32_t>(diag.lastUnderflowBufferedBytes);
  doc["finishBufferedBytes"] = static_cast<uint32_t>(diag.finishBufferedBytes);
  doc["finishQueuedChunks"] = diag.finishQueuedChunks;
  doc["lastPcmMs"] = static_cast<uint32_t>(diag.lastPcmMs);
  doc["lastFinishMs"] = static_cast<uint32_t>(diag.lastFinishMs);
}

MicDiagnosticChannelMode parseMicDiagnosticChannelMode(const String& value) {
  if (value.equalsIgnoreCase("right")) {
    return MicDiagnosticChannelMode::Right;
  }
  if (value.equalsIgnoreCase("left")) {
    return MicDiagnosticChannelMode::Left;
  }
  if (value.equalsIgnoreCase("stereo")) {
    return MicDiagnosticChannelMode::Stereo;
  }
  return MicDiagnosticChannelMode::Default;
}

int parseMicDiagnosticIntArg(const char* name, int fallback) {
  if (!httpServer.hasArg(name)) {
    return fallback;
  }
  const String raw = httpServer.arg(name);
  const char* text = raw.c_str();
  char* end = nullptr;
  const long value = strtol(text, &end, 0);
  if (end == text) {
    return fallback;
  }
  return static_cast<int>(value);
}

void handleMicTestRequest() {
  MicDiagnosticConfig config;
  if (httpServer.hasArg("rate")) {
    const int rate = httpServer.arg("rate").toInt();
    if (rate >= 8000 && rate <= 48000) {
      config.sampleRate = static_cast<uint32_t>(rate);
    }
  }
  if (httpServer.hasArg("din")) {
    config.pinDataIn = httpServer.arg("din").toInt();
  }
  if (httpServer.hasArg("bck")) {
    config.pinBck = httpServer.arg("bck").toInt();
  }
  if (httpServer.hasArg("ws")) {
    config.pinWs = httpServer.arg("ws").toInt();
  }
  if (httpServer.hasArg("mck")) {
    config.pinMck = httpServer.arg("mck").toInt();
  }
  if (httpServer.hasArg("port")) {
    config.i2sPort = httpServer.arg("port").toInt();
  }
  config.codecReg14 = parseMicDiagnosticIntArg("reg14", config.codecReg14);
  config.codecReg16 = parseMicDiagnosticIntArg("reg16", config.codecReg16);
  config.codecReg17 = parseMicDiagnosticIntArg("reg17", config.codecReg17);
  config.magnification = parseMicDiagnosticIntArg("mag", config.magnification);
  config.noiseFilterLevel = parseMicDiagnosticIntArg("nf", config.noiseFilterLevel);
  config.overSampling = parseMicDiagnosticIntArg("os", config.overSampling);
  if (httpServer.hasArg("channel")) {
    config.channelMode = parseMicDiagnosticChannelMode(httpServer.arg("channel"));
  }
  uint32_t durationMs = 600;
  if (httpServer.hasArg("duration")) {
    const int requestedDurationMs = httpServer.arg("duration").toInt();
    if (requestedDurationMs >= 100 && requestedDurationMs <= 3000) {
      durationMs = static_cast<uint32_t>(requestedDurationMs);
    }
  }

  MicDiagnosticResult result;
  const bool ok = audioController.measureMicDiagnostic(durationMs, result, config);

  JsonDocument doc;
  writeMicTestResponse(doc, result, ok);

  String body;
  serializeJson(doc, body);
  sendJson(ok ? 200 : 409, body.c_str());
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
  httpServer.on("/speaker-test", HTTP_OPTIONS, []() {
    addCorsHeaders();
    httpServer.send(204);
  });
  httpServer.on("/speaker-test", HTTP_GET, handleSpeakerTestRequest);
  httpServer.on("/speaker-test", HTTP_POST, handleSpeakerTestRequest);
  httpServer.on("/mic-test", HTTP_OPTIONS, []() {
    addCorsHeaders();
    httpServer.send(204);
  });
  httpServer.on("/mic-test", HTTP_GET, handleMicTestRequest);
  httpServer.on("/mic-test", HTTP_POST, handleMicTestRequest);
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
    case SettingsPage::StreetPass:
      return "StreetPass";
    case SettingsPage::Network:
    default:
      return "Network";
  }
}

bool settingsPageAvailable(SettingsPage page) {
#if STACKCHAN_SMALL_DISPLAY
  if (page == SettingsPage::Display || page == SettingsPage::Servo) {
    return false;
  }
#endif
#if !STACKCHAN_HAS_SERVO
  if (page == SettingsPage::Servo) {
    return false;
  }
#endif
  return true;
}

uint8_t settingsPageCount() {
#if STACKCHAN_SMALL_DISPLAY
  return 4;
#else
  return STACKCHAN_HAS_SERVO ? 6 : 5;
#endif
}

SettingsPage settingsPageAt(uint8_t index) {
#if STACKCHAN_SMALL_DISPLAY
  static const SettingsPage kPages[] = {
    SettingsPage::Network,
    SettingsPage::StreetPass,
    SettingsPage::Audio,
    SettingsPage::Power,
  };
#elif STACKCHAN_HAS_SERVO
  static const SettingsPage kPages[] = {
    SettingsPage::Network,
    SettingsPage::Display,
    SettingsPage::Audio,
    SettingsPage::Servo,
    SettingsPage::Power,
    SettingsPage::StreetPass,
  };
#else
  static const SettingsPage kPages[] = {
    SettingsPage::Network,
    SettingsPage::Display,
    SettingsPage::Audio,
    SettingsPage::Power,
    SettingsPage::StreetPass,
  };
#endif
  const uint8_t count = sizeof(kPages) / sizeof(kPages[0]);
  return kPages[index < count ? index : 0];
}

int settingsPageIndex(SettingsPage page) {
  const uint8_t count = settingsPageCount();
  for (uint8_t i = 0; i < count; ++i) {
    if (settingsPageAt(i) == page) {
      return i;
    }
  }
  return 0;
}

void selectSettingsPage(SettingsPage page) {
  activeNetworkQr = NetworkQrType::None;
  const bool wasStreetPass = settingsPage == SettingsPage::StreetPass;
  settingsPage = settingsPageAvailable(page) ? page : SettingsPage::Network;
#if STACKCHAN_SMALL_DISPLAY
  if (settingsPage != SettingsPage::Audio) {
    smallVolumeAdjustMode = false;
    smallVolumeHoldRepeatMs = 0;
  }
#endif
  if (settingsPage != SettingsPage::StreetPass) {
    streetPassProfileVisible = false;
#if STACKCHAN_SMALL_DISPLAY
    smallStreetPassView = 0;
#endif
  }
  if (settingsPage == SettingsPage::StreetPass) {
#if STACKCHAN_SMALL_DISPLAY
    if (!wasStreetPass) {
      smallStreetPassView = 0;
    }
#endif
    streetPassController.markAllRead();
  }
  drawInfoScreen();
}

void selectAdjacentSettingsPage(int direction) {
  const uint8_t count = settingsPageCount();
  int index = settingsPageIndex(settingsPage);
  index = (index + direction + count) % count;
  selectSettingsPage(settingsPageAt(static_cast<uint8_t>(index)));
}

void drawButton(int32_t x, int32_t y, int32_t w, int32_t h, const char* label, bool active = false) {
  const uint16_t border = active ? M5.Display.color565(90, 210, 150) : M5.Display.color565(110, 120, 128);
  const uint16_t fill = active ? M5.Display.color565(20, 52, 38) : TFT_BLACK;
  M5.Display.fillRoundRect(x, y, w, h, 5, fill);
  M5.Display.drawRoundRect(x, y, w, h, 5, border);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, fill);
  M5.Display.setTextSize(1);
  M5.Display.drawString(label, x + w / 2, y + h / 2);
  M5.Display.setTextDatum(top_left);
}

void appendUtf8Codepoint(String& out, uint32_t cp) {
  if (cp <= 0x7f) {
    out += static_cast<char>(cp);
  } else if (cp <= 0x7ff) {
    out += static_cast<char>(0xc0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else if (cp <= 0xffff) {
    out += static_cast<char>(0xe0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (cp & 0x3f));
  }
}

uint32_t readUtf8Codepoint(const String& text, size_t& index) {
  const uint8_t b0 = static_cast<uint8_t>(text[index++]);
  if ((b0 & 0x80) == 0) {
    return b0;
  }
  if ((b0 & 0xe0) == 0xc0 && index < text.length()) {
    const uint8_t b1 = static_cast<uint8_t>(text[index++]);
    return ((b0 & 0x1f) << 6) | (b1 & 0x3f);
  }
  if ((b0 & 0xf0) == 0xe0 && index + 1 < text.length()) {
    const uint8_t b1 = static_cast<uint8_t>(text[index++]);
    const uint8_t b2 = static_cast<uint8_t>(text[index++]);
    return ((b0 & 0x0f) << 12) | ((b1 & 0x3f) << 6) | (b2 & 0x3f);
  }
  if ((b0 & 0xf8) == 0xf0 && index + 2 < text.length()) {
    const uint8_t b1 = static_cast<uint8_t>(text[index++]);
    const uint8_t b2 = static_cast<uint8_t>(text[index++]);
    const uint8_t b3 = static_cast<uint8_t>(text[index++]);
    return ((b0 & 0x07) << 18) | ((b1 & 0x3f) << 12) | ((b2 & 0x3f) << 6) | (b3 & 0x3f);
  }
  return 0xfffd;
}

uint32_t halfwidthKanaToFullwidth(uint32_t cp) {
  static const uint16_t map[] = {
    0x3002, 0x300c, 0x300d, 0x3001, 0x30fb, 0x30f2, 0x30a1, 0x30a3,
    0x30a5, 0x30a7, 0x30a9, 0x30e3, 0x30e5, 0x30e7, 0x30c3, 0x30fc,
    0x30a2, 0x30a4, 0x30a6, 0x30a8, 0x30aa, 0x30ab, 0x30ad, 0x30af,
    0x30b1, 0x30b3, 0x30b5, 0x30b7, 0x30b9, 0x30bb, 0x30bd, 0x30bf,
    0x30c1, 0x30c4, 0x30c6, 0x30c8, 0x30ca, 0x30cb, 0x30cc, 0x30cd,
    0x30ce, 0x30cf, 0x30d2, 0x30d5, 0x30d8, 0x30db, 0x30de, 0x30df,
    0x30e0, 0x30e1, 0x30e2, 0x30e4, 0x30e6, 0x30e8, 0x30e9, 0x30ea,
    0x30eb, 0x30ec, 0x30ed, 0x30ef, 0x30f3, 0x3099, 0x309a
  };
  if (cp < 0xff61 || cp > 0xff9f) {
    return cp;
  }
  return map[cp - 0xff61];
}

uint32_t voicedKatakana(uint32_t base, uint32_t mark) {
  if (mark == 0xff9e) {
    switch (base) {
      case 0x30a6: return 0x30f4;
      case 0x30ab: return 0x30ac;
      case 0x30ad: return 0x30ae;
      case 0x30af: return 0x30b0;
      case 0x30b1: return 0x30b2;
      case 0x30b3: return 0x30b4;
      case 0x30b5: return 0x30b6;
      case 0x30b7: return 0x30b8;
      case 0x30b9: return 0x30ba;
      case 0x30bb: return 0x30bc;
      case 0x30bd: return 0x30be;
      case 0x30bf: return 0x30c0;
      case 0x30c1: return 0x30c2;
      case 0x30c4: return 0x30c5;
      case 0x30c6: return 0x30c7;
      case 0x30c8: return 0x30c9;
      case 0x30cf: return 0x30d0;
      case 0x30d2: return 0x30d3;
      case 0x30d5: return 0x30d6;
      case 0x30d8: return 0x30d9;
      case 0x30db: return 0x30dc;
      default: return base;
    }
  }
  if (mark == 0xff9f) {
    switch (base) {
      case 0x30cf: return 0x30d1;
      case 0x30d2: return 0x30d4;
      case 0x30d5: return 0x30d7;
      case 0x30d8: return 0x30da;
      case 0x30db: return 0x30dd;
      default: return base;
    }
  }
  return base;
}

String normalizeHalfwidthKanaForDisplay(const String& text) {
  String out;
  out.reserve(text.length());
  for (size_t i = 0; i < text.length();) {
    const uint32_t cp = readUtf8Codepoint(text, i);
    if (cp >= 0xff61 && cp <= 0xff9f) {
      const uint32_t base = halfwidthKanaToFullwidth(cp);
      if (i < text.length()) {
        size_t markIndex = i;
        const uint32_t mark = readUtf8Codepoint(text, markIndex);
        if (mark == 0xff9e || mark == 0xff9f) {
          appendUtf8Codepoint(out, voicedKatakana(base, mark));
          i = markIndex;
          continue;
        }
      }
      appendUtf8Codepoint(out, base);
    } else {
      appendUtf8Codepoint(out, cp);
    }
  }
  return out;
}

void drawUtf8Clipped(int32_t x, int32_t y, int32_t w, int32_t h, const String& text,
                     uint16_t color = TFT_WHITE, uint16_t background = TFT_BLACK) {
  String displayText = normalizeHalfwidthKanaForDisplay(text);
  displayText.replace("\r", " ");
  displayText.replace("\n", " ");
  displayText.replace("\t", " ");
  M5.Display.setFont(&fonts::efontJA_12);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(color, background);
  if (M5.Display.textWidth(displayText) > w) {
    const String suffix = "...";
    const int32_t suffixWidth = M5.Display.textWidth(suffix);
    String shortened;
    shortened.reserve(displayText.length());
    for (size_t i = 0; i < displayText.length();) {
      const size_t before = i;
      const uint32_t cp = readUtf8Codepoint(displayText, i);
      String candidate = shortened;
      appendUtf8Codepoint(candidate, cp);
      candidate += suffix;
      if (before > 0 && M5.Display.textWidth(candidate) > w) {
        break;
      }
      if (before == 0 && M5.Display.textWidth(candidate) > w) {
        shortened = suffixWidth <= w ? suffix : "";
        break;
      }
      appendUtf8Codepoint(shortened, cp);
    }
    if (shortened.length() > 0 && shortened != suffix) {
      shortened += suffix;
    }
    displayText = shortened;
  }
  M5.Display.setClipRect(x, y, w, h);
  M5.Display.drawString(displayText, x, y);
  M5.Display.clearClipRect();
  M5.Display.setFont(&fonts::Font0);
}

void drawSlider(int32_t x, int32_t y, int32_t w, const char* label, int value, int minValue, int maxValue) {
  M5.Display.setFont(&fonts::Font0);
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

struct RoundBounds {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

int32_t roundCenterX() {
  return M5.Display.width() / 2;
}

int32_t roundCenterY() {
  return M5.Display.height() / 2;
}

int32_t roundSafeRadius(int32_t inset = 24) {
  return max<int32_t>(24, min(M5.Display.width(), M5.Display.height()) / 2 - inset);
}

RoundBounds roundBoundsAt(int32_t y, int32_t h, int32_t inset = 28, int32_t maxWidth = 0) {
  const int32_t cx = roundCenterX();
  const int32_t cy = roundCenterY();
  const int32_t safeRadius = roundSafeRadius(inset);
  const int32_t midY = y + h / 2;
  const int32_t dy = abs(midY - cy);
  int32_t halfWidth = 0;
  if (dy < safeRadius) {
    const float chord = sqrtf(static_cast<float>(safeRadius * safeRadius - dy * dy));
    halfWidth = static_cast<int32_t>(chord);
  }
  int32_t w = halfWidth * 2;
  if (maxWidth > 0) {
    w = min(w, maxWidth);
  }
  return {cx - w / 2, y, w, h};
}

bool touchInRoundBounds(const m5::touch_detail_t& touch, const RoundBounds& bounds) {
  return touch.x >= bounds.x && touch.x < bounds.x + bounds.w &&
         touch.y >= bounds.y && touch.y < bounds.y + bounds.h;
}

bool touchInCircle(const m5::touch_detail_t& touch, int32_t cx, int32_t cy, int32_t radius) {
  const int32_t dx = touch.x - cx;
  const int32_t dy = touch.y - cy;
  return dx * dx + dy * dy <= radius * radius;
}

int32_t roundMicButtonCenterX() {
  return roundCenterX() + min(M5.Display.width(), M5.Display.height()) * 36 / 100;
}

int32_t roundMicButtonCenterY() {
  return roundCenterY() + min(M5.Display.width(), M5.Display.height()) * 17 / 100;
}

int32_t roundMicButtonRadius() {
  return 34;
}

void drawRoundSmallButton(int32_t cx, int32_t cy, int32_t radius, const char* label, bool active = false) {
  const uint16_t border = active ? M5.Display.color565(90, 210, 150) : M5.Display.color565(96, 108, 116);
  const uint16_t fill = active ? M5.Display.color565(20, 52, 38) : M5.Display.color565(10, 12, 14);
  M5.Display.fillCircle(cx, cy, radius, fill);
  M5.Display.drawCircle(cx, cy, radius, border);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, fill);
  M5.Display.drawString(label, cx, cy);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_left);
}

void drawRoundButtonRow(int32_t y, int32_t h, const char* label, bool active = false, int32_t maxWidth = 260) {
  const RoundBounds row = roundBoundsAt(y, h, 34, maxWidth);
  drawButton(row.x, row.y, row.w, row.h, label, active);
}

void drawRoundSplitButtons(int32_t y, const char* leftLabel, const char* rightLabel) {
  const RoundBounds row = roundBoundsAt(y, 36, 36, 250);
  const int32_t gap = 12;
  const int32_t buttonW = (row.w - gap) / 2;
  drawButton(row.x, row.y, buttonW, row.h, leftLabel);
  drawButton(row.x + buttonW + gap, row.y, buttonW, row.h, rightLabel);
}

void drawRoundTextLine(int32_t y, const String& text, uint16_t color = TFT_WHITE) {
  const RoundBounds row = roundBoundsAt(y, 17, 42, 330);
  drawUtf8Clipped(row.x, row.y, row.w, row.h, text, color);
}

void drawRoundSlider(int32_t y, const char* label, int value, int minValue, int maxValue) {
  const RoundBounds row = roundBoundsAt(y, 48, 42, 292);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(row.x, y);
  M5.Display.printf("%s: %d", label, value);
  const int32_t trackY = y + 24;
  M5.Display.drawRoundRect(row.x, trackY, row.w, 12, 4, M5.Display.color565(88, 96, 104));
  const int32_t fillW = map(constrain(value, minValue, maxValue), minValue, maxValue, 0, row.w - 4);
  if (fillW > 0) {
    M5.Display.fillRoundRect(row.x + 2, trackY + 2, fillW, 8, 3, M5.Display.color565(90, 190, 245));
  }
}

void drawRoundSettingsHeader() {
  const int32_t cx = roundCenterX();
  const int32_t w = M5.Display.width();
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.drawString(settingsPageName(settingsPage), cx, 42);
  M5.Display.setTextSize(1);
  drawRoundSmallButton(54, 42, 24, "<");
  drawRoundSmallButton(w - 54, 42, 24, ">");

  const uint8_t count = settingsPageCount();
  const int32_t startX = cx - static_cast<int32_t>(count - 1) * 8;
  const int selectedIndex = settingsPageIndex(settingsPage);
  for (uint8_t i = 0; i < count; ++i) {
    const uint16_t color = static_cast<int>(i) == selectedIndex
                             ? M5.Display.color565(90, 210, 150)
                             : M5.Display.color565(78, 86, 92);
    M5.Display.fillCircle(startX + i * 16, 72, 4, color);
  }
  M5.Display.setTextDatum(top_left);
}

void drawRoundNetworkSettingsPage() {
  drawRoundTextLine(102, String("Mode: ") + networkModeName());
  if (networkMode == NetworkMode::SoftAp) {
    const String ip = WiFi.softAPIP().toString();
    drawRoundTextLine(126, String("SSID: ") + AP_SSID);
    drawRoundTextLine(150, String("IP: ") + ip);
    drawRoundTextLine(174, String("Setup: http://") + ip + "/wifi");
    drawRoundTextLine(198, String("Client: ") + (appClientConnected() ? "connected" : "waiting"));
    drawRoundButtonRow(260, 36, "Wi-Fi QR");
    drawRoundButtonRow(312, 36, "Setup QR");
  } else if (WiFi.status() == WL_CONNECTED) {
    const String ip = WiFi.localIP().toString();
    drawRoundTextLine(126, String("SSID: ") + wifiCredentials[currentWifiIndex].ssid);
    drawRoundTextLine(150, String("IP: ") + ip);
    drawRoundTextLine(174, String("WS: ws://") + ip + ":" + String(WS_PORT));
    drawRoundTextLine(198, String("USB: ") + (usbSerialClientConnected ? "connected" : "waiting"));
    drawRoundButtonRow(288, 36, "Setup QR");
  } else {
    drawRoundTextLine(126, String("SSID: ") + wifiCredentials[currentWifiIndex].ssid);
    drawRoundTextLine(150, "IP: not connected", M5.Display.color565(220, 170, 90));
    drawRoundTextLine(174, String("USB: ") + (usbSerialClientConnected ? "connected" : "waiting"));
  }
  drawRoundTextLine(370, String("Hold: ") + (networkMode == NetworkMode::SoftAp ? "STA" : "SoftAP"),
                    M5.Display.color565(178, 188, 196));
}

void drawRoundDisplaySettingsPage() {
  drawRoundSlider(132, "Brightness", deviceSettings.brightness, DISPLAY_BRIGHTNESS_MIN, DISPLAY_BRIGHTNESS_MAX);
  drawRoundSplitButtons(210, "-", "+");
  drawRoundButtonRow(286, 38, displayOn ? "Screen Off" : "Screen On", !displayOn);
}

void drawRoundAudioSettingsPage() {
  drawRoundSlider(148, "Volume", deviceSettings.volume, AUDIO_SPEAKER_VOLUME_MIN, AUDIO_SPEAKER_VOLUME_MAX);
  drawRoundSplitButtons(226, "-", "+");
}

void drawRoundPowerSettingsPage() {
  drawRoundTextLine(112, String("Thermal: ") + thermalLevelName(thermalStatus.level));
  drawRoundTextLine(138, String("Chip: ") + String(thermalStatus.chipTempC, 1) + " C");
  if (!isnan(thermalStatus.pmicTempC)) {
    drawRoundTextLine(164, String("PMIC: ") + String(thermalStatus.pmicTempC, 1) + " C");
  } else {
    drawRoundTextLine(164, "PMIC: n/a");
  }
  drawRoundTextLine(190, String("Battery: ") + String(M5.Power.getBatteryLevel()) + " %");
  drawRoundTextLine(216, String("Charging: ") + (externalPowerPresent() ? "yes" : "no"));
  drawRoundButtonRow(282, 38, deviceSettings.lowPowerMode ? "Low Power On" : "Low Power Off",
                     deviceSettings.lowPowerMode, 290);
}

void drawRoundStreetPassSettingsPage() {
  drawRoundTextLine(108, String("StreetPass: ") + (streetPassController.enabled() ? "On" : "Off"));
  drawRoundTextLine(132, String("Stored: ") + String(streetPassController.storedCount()) +
                         "/" + String(StreetPassController::kMaxRecords));
  drawRoundTextLine(156, String("Unsynced: ") + String(streetPassController.unsyncedCount()));
  drawRoundButtonRow(204, 36, streetPassController.enabled() ? "Turn Off" : "Turn On",
                     streetPassController.enabled());
  drawRoundButtonRow(252, 36, streetPassProfileVisible ? "History" : "Profile",
                     streetPassProfileVisible);

  if (streetPassProfileVisible) {
    const StreetPassProfile& profile = streetPassController.profile();
    drawRoundTextLine(310, String("Name: ") + profile.name, M5.Display.color565(255, 220, 90));
    drawRoundTextLine(334, String("Msg : ") + profile.message, M5.Display.color565(190, 198, 205));
    return;
  }

  const uint8_t storedCount = streetPassController.storedCount();
  if (storedCount == 0) {
    drawRoundTextLine(322, "No encounters yet", M5.Display.color565(190, 198, 205));
    return;
  }
  const StreetPassRecord* record = streetPassController.recordAt(storedCount - 1);
  if (record != nullptr) {
    drawRoundTextLine(316, String(record->synced ? "OK " : "WAIT ") + record->peerName,
                      record->synced ? M5.Display.color565(100, 220, 150) : M5.Display.color565(255, 205, 90));
    drawRoundTextLine(340, record->peerMessage, M5.Display.color565(190, 198, 205));
  }
}

void drawRoundInfoScreen() {
  lastInfoDrawMs = millis();
  M5.Display.fillScreen(TFT_BLACK);
  if (activeNetworkQr != NetworkQrType::None) {
    drawNetworkQrScreen();
    return;
  }

  drawRoundSettingsHeader();
  switch (settingsPage) {
    case SettingsPage::Display:
      drawRoundDisplaySettingsPage();
      break;
    case SettingsPage::Audio:
      drawRoundAudioSettingsPage();
      break;
    case SettingsPage::Power:
      drawRoundPowerSettingsPage();
      break;
    case SettingsPage::StreetPass:
      drawRoundStreetPassSettingsPage();
      break;
    case SettingsPage::Servo:
    case SettingsPage::Network:
    default:
      drawRoundNetworkSettingsPage();
      break;
  }
}

void drawSettingsTabs() {
  drawButton(4, 206, 50, 26, "Net", settingsPage == SettingsPage::Network);
  drawButton(57, 206, 50, 26, "Disp", settingsPage == SettingsPage::Display);
  drawButton(110, 206, 50, 26, "Aud", settingsPage == SettingsPage::Audio);
  drawButton(163, 206, 50, 26, "Srv", settingsPage == SettingsPage::Servo);
  drawButton(216, 206, 48, 26, "Pwr", settingsPage == SettingsPage::Power);
  drawButton(267, 206, 49, 26, "Pass", settingsPage == SettingsPage::StreetPass);
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
#if STACKCHAN_SMALL_DISPLAY
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 6);
#else
  M5.Display.setTextSize(2);
  M5.Display.setCursor(12, 12);
#endif
  M5.Display.println(isWifiQr ? "Wi-Fi QR" : "Setup QR");

  if (!encoded) {
    M5.Display.setTextSize(1);
#if STACKCHAN_SMALL_DISPLAY
    M5.Display.setCursor(6, 42);
    M5.Display.println("QR unavailable");
    M5.Display.setCursor(6, 58);
    M5.Display.println(networkMode == NetworkMode::SoftAp ? "SoftAP starting?" : "No IP address");
    M5.Display.setCursor(6, 100);
    M5.Display.println("Click: return");
#else
    M5.Display.setCursor(12, 72);
    M5.Display.println("QR unavailable");
    M5.Display.setCursor(12, 96);
    M5.Display.println("Tap to return");
#endif
    return;
  }

  const int size = qrcodegen_getSize(qrCodeBuffer);
  const int quietModules = 4;
  const int totalModules = size + quietModules * 2;
#if STACKCHAN_SMALL_DISPLAY
  const int maxQrPixels = min(M5.Display.width() - 12, M5.Display.height() - 34);
#else
  const int maxQrPixels = min(M5.Display.width() - 28, M5.Display.height() - 78);
#endif
  const int scale = max(1, maxQrPixels / totalModules);
  const int qrPixels = totalModules * scale;
  const int originX = (M5.Display.width() - qrPixels) / 2;
#if STACKCHAN_SMALL_DISPLAY
  const int originY = 20;
#else
  const int originY = 40;
#endif

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

  const int textY =
#if STACKCHAN_SMALL_DISPLAY
    min(originY + qrPixels + 4, M5.Display.height() - 18);
#else
    min(originY + qrPixels + 8, M5.Display.height() - 34);
#endif
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
#if STACKCHAN_SMALL_DISPLAY
  if (isWifiQr) {
    drawUtf8Clipped(6, textY, M5.Display.width() - 12, 12, String("SSID: ") + AP_SSID,
                    M5.Display.color565(190, 198, 205));
  } else {
    drawUtf8Clipped(6, textY, M5.Display.width() - 12, 12, payload,
                    M5.Display.color565(190, 198, 205));
  }
#else
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
#endif
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

void drawStreetPassSettingsPage() {
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(18, 54);
  M5.Display.printf("StreetPass: %s\n", streetPassController.enabled() ? "On" : "Off");
  M5.Display.printf("Stored: %u/%u\n",
                    static_cast<unsigned>(streetPassController.storedCount()),
                    static_cast<unsigned>(StreetPassController::kMaxRecords));
  M5.Display.printf("Unsynced: %u\n", static_cast<unsigned>(streetPassController.unsyncedCount()));
  M5.Display.printf("Dropped: %lu\n", static_cast<unsigned long>(streetPassController.droppedCount()));

  drawButton(182, 54, 116, 28, streetPassController.enabled() ? "Turn Off" : "Turn On",
             streetPassController.enabled());
  drawButton(182, 88, 116, 28, streetPassProfileVisible ? "History" : "Profile",
             streetPassProfileVisible);

  if (streetPassProfileVisible) {
    const StreetPassProfile& profile = streetPassController.profile();
    M5.Display.setTextColor(M5.Display.color565(255, 220, 90), TFT_BLACK);
    M5.Display.setCursor(18, 118);
    M5.Display.println("My profile");
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setCursor(18, 138);
    M5.Display.print("Name:");
    drawUtf8Clipped(58, 136, 244, 18, profile.name);
    M5.Display.setCursor(18, 158);
    M5.Display.print("Msg :");
    drawUtf8Clipped(58, 156, 244, 18, profile.message);
    return;
  }

  constexpr uint8_t kStreetPassRowsPerPage = 3;
  const uint8_t storedCount = streetPassController.storedCount();
  const uint8_t pageCount = max<uint8_t>(1, (storedCount + kStreetPassRowsPerPage - 1) / kStreetPassRowsPerPage);
  if (streetPassHistoryPage >= pageCount) {
    streetPassHistoryPage = pageCount - 1;
  }
  const uint8_t startIndex = streetPassHistoryPage * kStreetPassRowsPerPage;
  const uint8_t count = storedCount > startIndex
                          ? min<uint8_t>(storedCount - startIndex, kStreetPassRowsPerPage)
                          : 0;

  M5.Display.setTextColor(M5.Display.color565(255, 220, 90), TFT_BLACK);
  M5.Display.setCursor(18, 104);
  M5.Display.printf("History %u/%u", static_cast<unsigned>(streetPassHistoryPage + 1), static_cast<unsigned>(pageCount));
  drawButton(108, 98, 30, 22, "<", streetPassHistoryPage > 0);
  drawButton(144, 98, 30, 22, ">", streetPassHistoryPage + 1 < pageCount);

  int32_t y = 126;
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t recordIndex = storedCount - 1 - (startIndex + i);
    const StreetPassRecord* record = streetPassController.recordAt(recordIndex);
    if (record == nullptr) {
      continue;
    }
    M5.Display.setCursor(18, y);
    M5.Display.setTextColor(record->synced ? M5.Display.color565(100, 220, 150) : M5.Display.color565(255, 205, 90), TFT_BLACK);
    M5.Display.print(record->synced ? "OK" : "WAIT");
    drawUtf8Clipped(54, y - 1, 208, 15, record->peerName);
    drawUtf8Clipped(54, y + 12, 208, 15, record->peerMessage, M5.Display.color565(190, 198, 205));
    drawButton(270, y - 2, 42, 22, "DEL");
    y += 28;
  }
  if (count == 0) {
    M5.Display.setTextColor(M5.Display.color565(190, 198, 205), TFT_BLACK);
    M5.Display.setCursor(18, 126);
    M5.Display.println("No encounters yet");
  }
}

#if STACKCHAN_SMALL_DISPLAY
void drawSmallPageHeader(const char* title, uint16_t color = TFT_WHITE) {
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(color, TFT_BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.print(title);
  M5.Display.drawFastHLine(4, 24, M5.Display.width() - 8, M5.Display.color565(70, 78, 86));
  M5.Display.setTextSize(1);
}

void drawSmallPageIndicator() {
  const uint8_t count = settingsPageCount();
  const int selected = settingsPageIndex(settingsPage);
  const int32_t cx = M5.Display.width() / 2;
  const int32_t y = M5.Display.height() - 6;
  const int32_t startX = cx - static_cast<int32_t>(count - 1) * 6;
  for (uint8_t i = 0; i < count; ++i) {
    const uint16_t color = static_cast<int>(i) == selected
                             ? M5.Display.color565(90, 210, 150)
                             : M5.Display.color565(78, 86, 92);
    M5.Display.fillCircle(startX + i * 12, y, 3, color);
  }
}

void drawSmallTextLine(int32_t y, const String& text, uint16_t color = TFT_WHITE) {
  drawUtf8Clipped(6, y, M5.Display.width() - 12, 13, text, color);
}

#if STACKCHAN_SMALL_DISPLAY
constexpr uint8_t kSmallStreetPassViewCount = 3;

String shortIdTail(const String& value, size_t tailLen = 8) {
  if (value.length() <= tailLen) {
    return value;
  }
  return value.substring(value.length() - tailLen);
}

void drawSmallStreetPassSubIndicator() {
  const int32_t y = 18;
  const int32_t startX = M5.Display.width() - 34;
  for (uint8_t i = 0; i < kSmallStreetPassViewCount; ++i) {
    const uint16_t color = i == smallStreetPassView
                             ? M5.Display.color565(255, 220, 90)
                             : M5.Display.color565(78, 86, 92);
    M5.Display.fillCircle(startX + i * 10, y, 2, color);
  }
}
#endif

void drawSmallNetworkSettingsPage() {
  drawSmallPageHeader("Network", M5.Display.color565(120, 205, 255));
  drawSmallTextLine(31, String("Mode: ") + networkModeName());
  if (networkMode == NetworkMode::SoftAp) {
    const String ip = WiFi.softAPIP().toString();
    drawSmallTextLine(47, String("SSID: ") + AP_SSID);
    drawSmallTextLine(63, String("IP: ") + ip);
    drawSmallTextLine(79, String("Setup: ") + ip + "/wifi");
    drawSmallTextLine(95, String("Pass: ") + AP_PASSWORD);
  } else if (WiFi.status() == WL_CONNECTED) {
    const String ip = WiFi.localIP().toString();
    drawSmallTextLine(47, String("SSID: ") + wifiCredentials[currentWifiIndex].ssid);
    drawSmallTextLine(63, String("IP: ") + ip);
    drawSmallTextLine(79, String("WS: ") + (wsClientConnected ? "on" : "waiting"));
    drawSmallTextLine(95, String("USB: ") + (usbSerialClientConnected ? "on" : "waiting"));
  } else {
    drawSmallTextLine(47, String("SSID: ") + wifiCredentials[currentWifiIndex].ssid);
    drawSmallTextLine(63, "IP: not connected", M5.Display.color565(255, 205, 90));
    drawSmallTextLine(79, String("USB: ") + (usbSerialClientConnected ? "on" : "waiting"));
    drawSmallTextLine(95, "Hold: SoftAP", M5.Display.color565(178, 188, 196));
  }
  drawSmallPageIndicator();
}

void drawSmallStreetPassSettingsPage() {
  drawSmallPageHeader("Pass", M5.Display.color565(255, 220, 90));
#if STACKCHAN_SMALL_DISPLAY
  drawSmallStreetPassSubIndicator();
  const StreetPassProfile& profile = streetPassController.profile();
  if (smallStreetPassView == 0) {
    drawSmallTextLine(31, String("StreetPass: ") + (streetPassController.enabled() ? "On" : "Off"));
    drawSmallTextLine(47, String("Share: ") + (profile.shareProfile ? "On" : "Off"));
    drawSmallTextLine(63, String("BLE: ") + (streetPassBleReady ? "ready" : "init") +
                           " Adv:" + (streetPassAdvertising ? "on" : "off"));
    drawSmallTextLine(79, String("Scan: ") + (streetPassScanActive ? "on" : "idle") +
                           " Pause:" + (streetPassBusyForExchange() ? streetPassBusyReason() : "none"));
    drawSmallTextLine(95, String("Stored: ") + String(streetPassController.storedCount()) +
                           "/" + String(StreetPassController::kMaxRecords) +
                           " U:" + String(streetPassController.unreadCount()));
  } else if (smallStreetPassView == 1) {
    drawSmallTextLine(31, String("Name: ") + profile.name);
    drawSmallTextLine(47, String("Msg: ") + profile.message);
    drawSmallTextLine(63, String("CardSeq: ") + String(profile.cardSeq));
    drawSmallTextLine(79, String("Profile: ...") + shortIdTail(profile.profileId));
    drawSmallTextLine(95, String("Token: ") + String(streetPassPeerToken(), HEX),
                      M5.Display.color565(190, 198, 205));
  } else {
    const StreetPassRecord* latest = streetPassController.storedCount() > 0
                                       ? streetPassController.recordAt(streetPassController.storedCount() - 1)
                                       : nullptr;
    if (latest == nullptr) {
      drawSmallTextLine(31, "Latest: none");
      drawSmallTextLine(47, "No encounters yet", M5.Display.color565(190, 198, 205));
      drawSmallTextLine(63, String("Unsynced: ") + String(streetPassController.unsyncedCount()));
      drawSmallTextLine(79, String("Dropped: ") + String(streetPassController.droppedCount()));
      drawSmallTextLine(95, "Hold: toggle pass", M5.Display.color565(178, 188, 196));
    } else {
      drawSmallTextLine(31, String("Latest: ") + latest->peerName);
      drawSmallTextLine(47, String("Msg: ") + latest->peerMessage);
      drawSmallTextLine(63, String("Seen: ") + String(latest->seenCount) +
                             " RSSI:" + String(latest->rssiMax));
      drawSmallTextLine(79, String("Unread: ") + (latest->unread ? "yes" : "no") +
                             " Sync:" + (latest->synced ? "yes" : "no"));
      drawSmallTextLine(95, String("Unsynced: ") + String(streetPassController.unsyncedCount()),
                        M5.Display.color565(190, 198, 205));
    }
  }
#endif
  drawSmallPageIndicator();
}

void drawSmallPowerSettingsPage() {
  drawSmallPageHeader("Power", M5.Display.color565(180, 230, 150));
  drawSmallTextLine(31, String("Thermal: ") + thermalLevelName(thermalStatus.level));
  drawSmallTextLine(47, String("Chip: ") + String(thermalStatus.chipTempC, 1) + " C");
  if (!isnan(thermalStatus.pmicTempC)) {
    drawSmallTextLine(63, String("PMIC: ") + String(thermalStatus.pmicTempC, 1) + " C");
  } else {
    drawSmallTextLine(63, "PMIC: n/a");
  }
  drawSmallTextLine(79, String("LowPower: ") + (deviceSettings.lowPowerMode ? "On" : "Off"));
  drawSmallTextLine(95, String("Suggest: ") + (thermalStatus.suggestLowPower ? "On" : "None"));
  drawSmallPageIndicator();
}

void drawSmallAudioSettingsPage() {
  const uint16_t accent = smallVolumeAdjustMode
                            ? M5.Display.color565(255, 205, 90)
                            : M5.Display.color565(185, 170, 255);
  drawSmallPageHeader("Volume", accent);
  drawSmallTextLine(31, String("Mode: ") + (smallVolumeAdjustMode ? "Adjust" : "View"));
  drawSmallTextLine(47, String("Volume: ") + String(deviceSettings.volume) +
                         "/" + String(AUDIO_SPEAKER_VOLUME_MAX));

  const int32_t x = 8;
  const int32_t y = 66;
  const int32_t w = M5.Display.width() - 16;
  const int32_t h = 10;
  const int32_t fillW = map(constrain(deviceSettings.volume,
                                      AUDIO_SPEAKER_VOLUME_MIN,
                                      AUDIO_SPEAKER_VOLUME_MAX),
                            AUDIO_SPEAKER_VOLUME_MIN,
                            AUDIO_SPEAKER_VOLUME_MAX,
                            0,
                            w - 4);
  M5.Display.drawRoundRect(x, y, w, h, 3, M5.Display.color565(86, 94, 104));
  if (fillW > 0) {
    M5.Display.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, accent);
  }

  if (smallVolumeAdjustMode) {
    drawSmallTextLine(83, String("Click:+") + SETTINGS_STEP_VALUE +
                            " Hold:-" + SETTINGS_STEP_VALUE,
                      M5.Display.color565(220, 226, 232));
    drawSmallTextLine(99, "Double: done", M5.Display.color565(178, 188, 196));
  } else {
    drawSmallTextLine(83, "Double: adjust", M5.Display.color565(178, 188, 196));
    drawSmallTextLine(99, "Click: next page", M5.Display.color565(178, 188, 196));
  }
  drawSmallPageIndicator();
}

void drawSmallInfoScreen() {
  lastInfoDrawMs = millis();
  M5.Display.fillScreen(TFT_BLACK);
  if (activeNetworkQr != NetworkQrType::None) {
    drawNetworkQrScreen();
    return;
  }
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(top_left);
  switch (settingsPage) {
    case SettingsPage::StreetPass:
      drawSmallStreetPassSettingsPage();
      break;
    case SettingsPage::Power:
      drawSmallPowerSettingsPage();
      break;
    case SettingsPage::Audio:
      drawSmallAudioSettingsPage();
      break;
    case SettingsPage::Network:
    default:
      drawSmallNetworkSettingsPage();
      break;
  }
}
#endif

void drawInfoScreen() {
#if STACKCHAN_SMALL_DISPLAY
  drawSmallInfoScreen();
  return;
#endif
#if STACKCHAN_ROUND_DISPLAY
  drawRoundInfoScreen();
  return;
#endif
  lastInfoDrawMs = millis();
  M5.Display.fillScreen(TFT_BLACK);
  if (activeNetworkQr != NetworkQrType::None) {
    drawNetworkQrScreen();
    return;
  }
  M5.Display.setFont(&fonts::Font0);
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
    case SettingsPage::StreetPass:
      drawStreetPassSettingsPage();
      break;
    case SettingsPage::Network:
    default:
      drawNetworkSettingsPage();
      break;
  }

  drawSettingsTabs();
}

bool settingsPageNeedsPeriodicRefresh() {
#if STACKCHAN_SMALL_DISPLAY
  return settingsPage == SettingsPage::Network ||
         settingsPage == SettingsPage::StreetPass ||
         settingsPage == SettingsPage::Power;
#else
  return settingsPage == SettingsPage::Network || settingsPage == SettingsPage::Power;
#endif
}

void setInfoScreenVisible(bool visible) {
  if (infoScreenVisible == visible) {
    return;
  }
  if (!visible) {
    activeNetworkQr = NetworkQrType::None;
  }
  infoScreenVisible = visible;
#if STACKCHAN_SMALL_DISPLAY
  if (visible && !settingsPageAvailable(settingsPage)) {
    settingsPage = SettingsPage::Network;
  }
#endif
  faceController.setEnabled(displayOn && !visible);
#if STACKCHAN_GURUGURU_FACE_ENABLED
  updateGuruguruFaceAvailability(millis());
#endif
  applyDisplayBrightness();
  if (visible) {
    if (settingsPage == SettingsPage::StreetPass) {
      streetPassController.markAllRead();
    }
    drawInfoScreen();
  }
}

#if STACKCHAN_SMALL_DISPLAY
void toggleStreetPassEnabledFromDevice() {
  JsonDocument request;
  JsonDocument response;
  request["type"] = "streetpass.profile.set";
  request["enabled"] = !streetPassController.enabled();
  streetPassController.handleJsonCommand(request, response, millis());
  if (!streetPassController.enabled()) {
    clearStreetPassBleCandidates();
  }
  if (infoScreenVisible && displayOn) {
    drawInfoScreen();
  }
}

bool advanceSmallDisplayPage() {
  if (!displayOn) {
    setDisplayOn(true);
    return true;
  }
  if (infoScreenVisible && activeNetworkQr != NetworkQrType::None) {
    activeNetworkQr = NetworkQrType::None;
    drawInfoScreen();
    return true;
  }
  if (!infoScreenVisible) {
    settingsPage = SettingsPage::Network;
    setInfoScreenVisible(true);
    return true;
  }
  if (settingsPage == SettingsPage::Network) {
    selectSettingsPage(SettingsPage::StreetPass);
    return true;
  }
  if (settingsPage == SettingsPage::StreetPass) {
    smallStreetPassView = 0;
    selectSettingsPage(SettingsPage::Audio);
    return true;
  }
  if (settingsPage == SettingsPage::Audio) {
    selectSettingsPage(SettingsPage::Power);
    return true;
  }
  setInfoScreenVisible(false);
  return true;
}

bool handleSmallDisplayPageHold() {
  if (!displayOn) {
    smallDisplayFacePettingHold = false;
    setDisplayOn(true);
    return true;
  }
  if (!infoScreenVisible) {
    smallDisplayFacePettingHold = true;
    setPettingActive(true, millis(), PET_BUTTON_RELEASE_LINGER_MS);
    return true;
  }
  smallDisplayFacePettingHold = false;
  if (settingsPage == SettingsPage::Audio) {
    if (smallVolumeAdjustMode) {
      smallVolumeHoldRepeatMs = millis();
      adjustSmallDisplayVolume(-SETTINGS_STEP_VALUE);
    }
    return true;
  }
  if (settingsPage == SettingsPage::Network) {
    switchNetworkModeWithoutRestart();
    return true;
  }
  if (settingsPage == SettingsPage::StreetPass) {
    toggleStreetPassEnabledFromDevice();
    return true;
  }
  if (settingsPage == SettingsPage::Power) {
    applyLowPowerMode(!deviceSettings.lowPowerMode, true);
    return true;
  }
  return true;
}
#endif

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

#if STACKCHAN_SMALL_DISPLAY
void adjustSmallDisplayVolume(int delta) {
  const int next = constrain(static_cast<int>(deviceSettings.volume) + delta,
                             static_cast<int>(AUDIO_SPEAKER_VOLUME_MIN),
                             static_cast<int>(AUDIO_SPEAKER_VOLUME_MAX));
  if (next == deviceSettings.volume) {
    return;
  }
  deviceSettings.volume = static_cast<uint8_t>(next);
  audioController.setVolume(deviceSettings.volume);
  saveDeviceSettings();
  drawInfoScreen();
}
#endif

bool audioBusyForServoCalibration() {
  return audioController.state() != ChanState::Idle || audioController.isPlaybackDraining();
}

int roundSplitButtonHit(const m5::touch_detail_t& touch, int32_t y) {
  const RoundBounds row = roundBoundsAt(y, 36, 36, 250);
  const int32_t gap = 12;
  const int32_t buttonW = (row.w - gap) / 2;
  const RoundBounds left = {row.x, row.y, buttonW, row.h};
  const RoundBounds right = {row.x + buttonW + gap, row.y, buttonW, row.h};
  if (touchInRoundBounds(touch, left)) {
    return -1;
  }
  if (touchInRoundBounds(touch, right)) {
    return 1;
  }
  return 0;
}

bool handleRoundSettingsTouch(const m5::touch_detail_t& touch) {
  if (activeNetworkQr != NetworkQrType::None) {
    activeNetworkQr = NetworkQrType::None;
    drawInfoScreen();
    return true;
  }

  if (touch.wasFlicked()) {
    if (abs(touch.distanceX()) > abs(touch.distanceY()) && abs(touch.distanceX()) > 24) {
      selectAdjacentSettingsPage(touch.distanceX() < 0 ? 1 : -1);
      return true;
    }
    return false;
  }

  if (!touch.wasClicked()) {
    return false;
  }

  if (touchInCircle(touch, 54, 42, 30)) {
    selectAdjacentSettingsPage(-1);
    return true;
  }
  if (touchInCircle(touch, M5.Display.width() - 54, 42, 30)) {
    selectAdjacentSettingsPage(1);
    return true;
  }

  if (settingsPage == SettingsPage::Network) {
    if (networkMode == NetworkMode::SoftAp && touchInRoundBounds(touch, roundBoundsAt(260, 36, 34, 260))) {
      activeNetworkQr = NetworkQrType::WifiConnect;
      drawInfoScreen();
      return true;
    }
    const int32_t setupButtonY = networkMode == NetworkMode::SoftAp ? 312 : 288;
    if (setupQrAvailable() && touchInRoundBounds(touch, roundBoundsAt(setupButtonY, 36, 34, 260))) {
      activeNetworkQr = NetworkQrType::Setup;
      drawInfoScreen();
      return true;
    }
  } else if (settingsPage == SettingsPage::Display) {
    const int hit = roundSplitButtonHit(touch, 210);
    if (hit < 0) {
      adjustBrightness(-20);
      return true;
    }
    if (hit > 0) {
      adjustBrightness(20);
      return true;
    }
    if (touchInRoundBounds(touch, roundBoundsAt(286, 38, 34, 260))) {
      setDisplayOn(!displayOn);
      return true;
    }
  } else if (settingsPage == SettingsPage::Audio) {
    const int hit = roundSplitButtonHit(touch, 226);
    if (hit < 0) {
      adjustVolume(-20);
      return true;
    }
    if (hit > 0) {
      adjustVolume(20);
      return true;
    }
  } else if (settingsPage == SettingsPage::Power) {
    if (touchInRoundBounds(touch, roundBoundsAt(282, 38, 34, 290))) {
      applyLowPowerMode(!deviceSettings.lowPowerMode, true);
      return true;
    }
  } else if (settingsPage == SettingsPage::StreetPass) {
    if (touchInRoundBounds(touch, roundBoundsAt(204, 36, 34, 260))) {
      JsonDocument request;
      JsonDocument response;
      request["type"] = "streetpass.profile.set";
      request["enabled"] = !streetPassController.enabled();
      streetPassController.handleJsonCommand(request, response, millis());
      drawInfoScreen();
      return true;
    }
    if (touchInRoundBounds(touch, roundBoundsAt(252, 36, 34, 260))) {
      streetPassProfileVisible = !streetPassProfileVisible;
      drawInfoScreen();
      return true;
    }
  }

  return false;
}

bool handleSettingsTouch(const m5::touch_detail_t& touch) {
#if STACKCHAN_ROUND_DISPLAY
  return handleRoundSettingsTouch(touch);
#endif

  if (activeNetworkQr != NetworkQrType::None) {
    activeNetworkQr = NetworkQrType::None;
    drawInfoScreen();
    return true;
  }

  if (touchIn(touch, 4, 206, 50, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Network;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 57, 206, 50, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Display;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 110, 206, 50, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Audio;
    drawInfoScreen();
    return true;
  }
#if STACKCHAN_HAS_SERVO
  if (touchIn(touch, 163, 206, 50, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Servo;
    drawInfoScreen();
    return true;
  }
#endif
  if (touchIn(touch, 216, 206, 48, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::Power;
    drawInfoScreen();
    return true;
  }
  if (touchIn(touch, 267, 206, 49, 26)) {
    activeNetworkQr = NetworkQrType::None;
    settingsPage = SettingsPage::StreetPass;
    streetPassProfileVisible = false;
    streetPassHistoryPage = 0;
    streetPassController.markAllRead();
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
  }
#if STACKCHAN_HAS_SERVO
  else if (settingsPage == SettingsPage::Servo) {
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
  }
#endif
  else if (settingsPage == SettingsPage::Power) {
    if (touchIn(touch, 150, 144, 150, 28)) {
      applyLowPowerMode(!deviceSettings.lowPowerMode, true);
      return true;
    }
  } else if (settingsPage == SettingsPage::StreetPass) {
    if (touchIn(touch, 182, 54, 116, 28)) {
      JsonDocument request;
      JsonDocument response;
      request["type"] = "streetpass.profile.set";
      request["enabled"] = !streetPassController.enabled();
      streetPassController.handleJsonCommand(request, response, millis());
      drawInfoScreen();
      return true;
    }
    if (touchIn(touch, 182, 88, 116, 28)) {
      streetPassProfileVisible = !streetPassProfileVisible;
      drawInfoScreen();
      return true;
    }
    if (!streetPassProfileVisible) {
      constexpr uint8_t kStreetPassRowsPerPage = 3;
      const uint8_t storedCount = streetPassController.storedCount();
      const uint8_t pageCount = max<uint8_t>(1, (storedCount + kStreetPassRowsPerPage - 1) / kStreetPassRowsPerPage);
      if (touchIn(touch, 108, 98, 30, 22)) {
        if (streetPassHistoryPage > 0) {
          --streetPassHistoryPage;
          drawInfoScreen();
        }
        return true;
      }
      if (touchIn(touch, 144, 98, 30, 22)) {
        if (streetPassHistoryPage + 1 < pageCount) {
          ++streetPassHistoryPage;
          drawInfoScreen();
        }
        return true;
      }
      const uint8_t startIndex = streetPassHistoryPage * kStreetPassRowsPerPage;
      const uint8_t count = storedCount > startIndex
                              ? min<uint8_t>(storedCount - startIndex, kStreetPassRowsPerPage)
                              : 0;
      for (uint8_t i = 0; i < count; ++i) {
        const int32_t y = 126 + static_cast<int32_t>(i) * 28;
        if (!touchIn(touch, 270, y - 2, 42, 22)) {
          continue;
        }
        const uint8_t recordIndex = storedCount - 1 - (startIndex + i);
        const StreetPassRecord* record = streetPassController.recordAt(recordIndex);
        if (record != nullptr) {
          JsonDocument request;
          JsonDocument response;
          request["type"] = "streetpass.encounters.delete";
          JsonArray ids = request["recordIds"].to<JsonArray>();
          ids.add(record->recordId);
          streetPassController.handleJsonCommand(request, response, millis());
          if ((response["deletedCount"] | 0) > 0) {
            clearStreetPassBleCandidates();
          }
        }
        drawInfoScreen();
        return true;
      }
    }
  }

  return false;
}

void drawLowPowerPrompt() {
#if STACKCHAN_SMALL_DISPLAY
  return;
#endif
  if (!displayOn || infoScreenVisible || deviceSettings.lowPowerMode || !thermalStatus.suggestLowPower) {
    return;
  }
  drawButton(M5.Display.width() - 132, M5.Display.height() - 44, 66, 30, "LOW", true);
}

void drawStreetPassNotificationOverlay() {
#if STACKCHAN_SMALL_DISPLAY
  return;
#endif
  const uint8_t unread = streetPassController.unreadCount();
  if (!displayOn || infoScreenVisible || unread == 0) {
    return;
  }

  const int32_t x = M5.Display.width() - 31;
  const int32_t y = 42;
  const uint16_t color = M5.Display.color565(255, 220, 90);
  M5.Display.drawRect(x, y, 22, 15, color);
  M5.Display.drawLine(x, y, x + 11, y + 8, color);
  M5.Display.drawLine(x + 22, y, x + 11, y + 8, color);
  if (unread > 1) {
    M5.Display.fillCircle(x + 22, y - 2, 6, TFT_RED);
    M5.Display.setTextColor(TFT_WHITE, TFT_RED);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(x + 19, y - 6);
    M5.Display.print(min<uint8_t>(unread, 9));
  }
}

void setState(ChanState state) {
  if (currentState == state) {
    if (state == ChanState::Speaking && audioController.state() != ChanState::Speaking) {
      pendingStateAfterPlayback = false;
      deferredStateReadyMs = 0;
      pendingSpeakingFaceState = true;
      wsAudioSettleUntilMs = 0;
#if STACKCHAN_ROUND_DISPLAY
      faceController.prepareSpeakingCache(displayAuthFaceMode(currentAuthFaceMode));
#endif
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
  if (state == ChanState::Speaking) {
    pendingSpeakingFaceState = true;
#if STACKCHAN_ROUND_DISPLAY
    faceController.prepareSpeakingCache(displayAuthFaceMode(currentAuthFaceMode));
#endif
  } else {
    pendingSpeakingFaceState = false;
    faceController.setState(state);
  }
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
  }

  Serial.printf("[state] changed to %d\n", static_cast<int>(state));
}

void updateSpeakingFaceStateAfterPlayback() {
  if (!pendingSpeakingFaceState) {
    return;
  }
  if (currentState != ChanState::Speaking) {
    pendingSpeakingFaceState = false;
    return;
  }
  if (!audioController.hasPlaybackStarted()) {
    return;
  }

  pendingSpeakingFaceState = false;
  faceController.startSpeaking(displayAuthFaceMode(currentAuthFaceMode));
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

void sendJsonDocument(JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

void writeDeviceInfo(JsonDocument& response, const char* requestId) {
  ensureDeviceId();
  response["type"] = "device.info";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["deviceId"] = deviceId;
  response["displayName"] = STACKCHAN_DEVICE_DISPLAY_NAME;
  response["firmwareName"] = STACKCHAN_FIRMWARE_NAME;
  response["firmwareVersion"] = STACKCHAN_FIRMWARE_VERSION;
  response["protocolVersion"] = STACKCHAN_APP_PROTOCOL_VERSION;
  JsonArray capabilities = response["capabilities"].to<JsonArray>();
  capabilities.add("device.info");
  capabilities.add("affection.get");
  capabilities.add("affection.sync");
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

void writePongResponse(JsonDocument& response, JsonDocument& request) {
  response["type"] = "pong";
  const char* requestId = request["requestId"] | "";
  if (requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  const char* id = request["id"] | "";
  if (id[0] != '\0') {
    response["id"] = id;
  }
  response["timestampMs"] = millis();
}

void sendPongResponse(JsonDocument& request) {
  JsonDocument pong;
  writePongResponse(pong, request);
  String body;
  serializeJson(pong, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

void handleDeviceInfoGetCommand(JsonDocument& doc) {
  JsonDocument response;
  writeDeviceInfo(response, doc["requestId"] | "");
  sendJsonDocument(response);
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
#if STACKCHAN_DEVICE_STOPWATCH
  if (audioBusyForUiEffects()) {
    return;
  }
#endif
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

bool validCharacterId(const char* characterId) {
  return characterId != nullptr &&
         characterId[0] != '\0' &&
         strlen(characterId) <= 96;
}

void sendAffectionSyncError(const char* requestId, const char* error) {
  JsonDocument response;
  response["type"] = "affection.sync.error";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["error"] = error != nullptr ? error : "invalid_request";
  sendJsonDocument(response);
}

void writeAffectionSyncState(JsonDocument& response, const char* requestId, const char* characterId) {
  ensureDeviceId();
  const AffectionSyncState sync = affectionController.syncStateForCharacter(characterId);
  response["type"] = "affection.sync.state";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["deviceId"] = deviceId;
  response["characterId"] = sync.characterId;
  response["affection"] = sync.state.affection;
  response["mood"] = sync.state.mood;
  response["confusion"] = sync.state.confusion;
  response["syncedBaseAffection"] = sync.syncedBaseAffection;
  response["unsyncedDelta"] = sync.unsyncedDelta;
}

void sendAffectionSyncState(const char* requestId, const char* characterId) {
  JsonDocument response;
  writeAffectionSyncState(response, requestId, characterId);
  sendJsonDocument(response);
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

void handleAffectionSyncStateCommand(JsonDocument& doc) {
  const char* requestId = doc["requestId"] | "";
  const char* characterId = doc["characterId"] | "";
  if (!validCharacterId(characterId)) {
    sendAffectionSyncError(requestId, "invalid_characterId");
    return;
  }
  sendAffectionSyncState(requestId, characterId);
}

void handleAffectionSyncApplyCommand(JsonDocument& doc) {
  const char* requestId = doc["requestId"] | "";
  const char* characterId = doc["characterId"] | "";
  if (!validCharacterId(characterId)) {
    sendAffectionSyncError(requestId, "invalid_characterId");
    return;
  }
  if (doc["affection"].isNull()) {
    sendAffectionSyncError(requestId, "missing_affection");
    return;
  }

  const unsigned long now = millis();
  const AffectionApplyResult result = affectionController.applySyncAffection(
    characterId,
    doc["affection"] | affectionController.state().affection
  );
  applyAffectionResult(result, now, true, requestId);
  sendAffectionSyncState(requestId, characterId);
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

bool handleStreetPassCommand(JsonDocument& doc) {
  const char* commandType = doc["type"] | "";
  const uint16_t beforeProfileNameHash = streetPassNameHash(streetPassController.profile().name);
  JsonDocument response;
  if (!streetPassController.handleJsonCommand(doc, response, millis())) {
    return false;
  }
  if (strcmp(commandType, "streetpass.time.set") == 0 && (response["ok"] | false)) {
    writeStreetPassRtcTime(doc["unixTime"] | 0);
  }
  if (strcmp(commandType, "streetpass.profile.set") == 0 &&
      beforeProfileNameHash != streetPassNameHash(streetPassController.profile().name)) {
    resetStreetPassBleAttemptCooldowns();
  }
  if (strcmp(commandType, "streetpass.encounters.delete") == 0 && (response["deletedCount"] | 0) > 0) {
    clearStreetPassBleCandidates();
  }

  String body;
  serializeJson(response, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
  if (streetPassPageVisible()) {
    streetPassController.markAllRead();
    drawInfoScreen();
  }
  return true;
}

void handleSpeakerTestCommand(JsonDocument& doc) {
  const uint32_t requestedDurationMs = doc["durationMs"] | 450;
  uint32_t durationMs = requestedDurationMs;
  if (durationMs < 80) {
    durationMs = 80;
  } else if (durationMs > 2000) {
    durationMs = 2000;
  }
  const bool ok = audioController.playDiagnosticTone(durationMs);

  JsonDocument response;
  response["type"] = "audio.speaker_test";
  response["ok"] = ok;
  response["durationMs"] = durationMs;
  response["board"] = static_cast<int>(M5.getBoard());
  response["volume"] = deviceSettings.volume;
  response["audioState"] = chanStateName(audioController.state());
  const char* requestId = doc["requestId"] | doc["id"] | "";
  if (requestId[0] != '\0') {
    response["requestId"] = requestId;
  }

  String body;
  serializeJson(response, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

void handleMicTestCommand(JsonDocument& doc) {
  const uint32_t requestedDurationMs = doc["durationMs"] | 600;
  uint32_t durationMs = requestedDurationMs;
  if (durationMs < 80) {
    durationMs = 80;
  } else if (durationMs > 3000) {
    durationMs = 3000;
  }

  MicDiagnosticResult result;
  const bool ok = audioController.measureMicDiagnostic(durationMs, result);

  JsonDocument response;
  response["type"] = "audio.mic_test";
  writeMicTestResponse(response, result, ok);
  const char* requestId = doc["requestId"] | doc["id"] | "";
  if (requestId[0] != '\0') {
    response["requestId"] = requestId;
  }

  String body;
  serializeJson(response, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

void handlePlaybackDiagCommand(JsonDocument& doc) {
  JsonDocument response;
  writePlaybackDiagnosticResponse(response, audioController.playbackDiagnostic());
  const char* requestId = doc["requestId"] | doc["id"] | "";
  if (requestId[0] != '\0') {
    response["requestId"] = requestId;
  }

  String body;
  serializeJson(response, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
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
  if (strcmp(type, "ping") == 0) {
    sendPongResponse(doc);
  } else if (strcmp(type, "device.info.get") == 0) {
    handleDeviceInfoGetCommand(doc);
  } else if (strcmp(type, "audio.speaker_test") == 0) {
    handleSpeakerTestCommand(doc);
  } else if (strcmp(type, "audio.mic_test") == 0) {
    handleMicTestCommand(doc);
  } else if (strcmp(type, "audio.playback_diag") == 0) {
    handlePlaybackDiagCommand(doc);
  } else if (strcmp(type, "state") == 0) {
    const char* value = doc["value"] | "";
    handleStateCommand(value);
  } else if (strcmp(type, "affection.event") == 0) {
    handleAffectionEventCommand(doc);
  } else if (strcmp(type, "affection.get") == 0) {
    handleAffectionGetCommand(doc);
  } else if (strcmp(type, "affection.sync.state") == 0) {
    handleAffectionSyncStateCommand(doc);
  } else if (strcmp(type, "affection.sync.apply") == 0) {
    handleAffectionSyncApplyCommand(doc);
  } else if (strcmp(type, "affection.reset") == 0) {
    handleAffectionResetCommand(doc);
  } else if (strcmp(type, "affection.debug_adjust") == 0) {
    handleAffectionDebugAdjustCommand(doc);
  } else if (strcmp(type, "affection.debug_set") == 0) {
    handleAffectionDebugSetCommand(doc);
  } else if (strncmp(type, "streetpass.", 11) == 0) {
    if (!handleStreetPassCommand(doc)) {
      Serial.printf("[streetpass] unsupported type: %s\n", type);
    }
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
  if (usbSerialFramedMode) {
    size_t first = 0;
    while (first < length && isspace(payload[first])) {
      ++first;
    }
    if (first >= length || payload[first] != '{') {
      return;
    }
  }
  usbSerialFramedMode = false;
  handleUsbSerialJsonPayload(payload, length);
#else
  (void)payload;
  (void)length;
#endif
}

void updateUsbSerialDeferredIdle(unsigned long now) {
#if USB_SERIAL_PROTOCOL_ENABLED && STACKCHAN_DEVICE_STOPWATCH
  now = millis();
  if (!usbSerialDeferredIdlePending) {
    return;
  }
  if (currentState != ChanState::Speaking &&
      audioController.state() != ChanState::Speaking &&
      !audioController.isPlaybackDraining()) {
    usbSerialDeferredIdlePending = false;
    usbSerialDeferredIdleRequestedMs = 0;
    return;
  }
  if (Serial.available() > 0) {
    return;
  }

  const unsigned long referenceMs = usbSerialLastPcmMs != 0
                                      ? usbSerialLastPcmMs
                                      : usbSerialDeferredIdleRequestedMs;
  if (referenceMs != 0 && now - referenceMs < USB_SERIAL_IDLE_DEFER_AFTER_PCM_MS) {
    return;
  }

  usbSerialDeferredIdlePending = false;
  usbSerialDeferredIdleRequestedMs = 0;
  Serial.printf("[audio] usb deferred idle applied last_pcm_age=%lu ms\n",
                referenceMs == 0 ? 0UL : static_cast<unsigned long>(now - referenceMs));
  setState(ChanState::Idle);
#else
  (void)now;
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
    writePongResponse(pong, doc);

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

#if STACKCHAN_DEVICE_STOPWATCH
  if (strcmp(type, "state") == 0) {
    const char* value = doc["value"] | "";
    if (strcmp(value, "speaking") == 0) {
      usbSerialDeferredIdlePending = false;
      usbSerialDeferredIdleRequestedMs = 0;
    } else if (strcmp(value, "idle") == 0 &&
               (currentState == ChanState::Speaking ||
                audioController.state() == ChanState::Speaking ||
                audioController.isPlaybackDraining())) {
      usbSerialDeferredIdlePending = true;
      usbSerialDeferredIdleRequestedMs = millis();
      Serial.printf("[audio] usb idle deferred current=%s audio=%s last_pcm_age=%lu ms\n",
                    chanStateName(currentState),
                    chanStateName(audioController.state()),
                    usbSerialLastPcmMs == 0
                      ? 0UL
                      : static_cast<unsigned long>(usbSerialDeferredIdleRequestedMs - usbSerialLastPcmMs));
      return;
    }
  }
#endif

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
#if STACKCHAN_DEVICE_STOPWATCH
      usbSerialLastPcmMs = millis();
#endif
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
      JsonDocument ping;
      if (length > 0) {
        deserializeJson(ping, payload, length);
      }
      writePongResponse(pong, ping);
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
#if STACKCHAN_DEVICE_STOPWATCH
    usbSerialDeferredIdlePending = false;
    usbSerialDeferredIdleRequestedMs = 0;
#endif
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

void redrawNetworkSettingsIfVisible() {
  if (displayOn && infoScreenVisible && settingsPage == SettingsPage::Network &&
      activeNetworkQr == NetworkQrType::None) {
    drawInfoScreen();
  }
}

void beginStaWifiConnection(const char* reason) {
  wifiConnectStartedMs = millis();
  lastWifiStatus = WiFi.status();
  WiFi.persistent(false);
  WiFi.disconnect(false, false);
  WiFi.begin(wifiCredentials[currentWifiIndex].ssid.c_str(), wifiCredentials[currentWifiIndex].password.c_str());
  Serial.printf("[wifi] connecting to %s reason=%s\n",
                wifiCredentials[currentWifiIndex].ssid.c_str(),
                reason != nullptr ? reason : "start");
  redrawNetworkSettingsIfVisible();
}

void connectWiFi() {
  WiFi.persistent(false);
  wifiConnectStartedMs = 0;
  lastWifiStatus = WiFi.status();

  if (networkMode == NetworkMode::SoftAp) {
    IPAddress localIP(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
    IPAddress gateway(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.disconnect(false, false);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(true);
    const bool configOk = WiFi.softAPConfig(localIP, gateway, subnet);
    const bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);

    if (!configOk || !apOk) {
      Serial.printf("[wifi] SoftAP failed config=%d ap=%d\n", configOk, apOk);
      drawBootScreen("SoftAP failed");
      return;
    }

    lastWifiStatus = WiFi.status();
    Serial.printf("[wifi] SoftAP started ssid=%s ip=%s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
    startServers();
    redrawNetworkSettingsIfVisible();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  if (!isWifiCredentialConfigured(currentWifiIndex) && !selectNextConfiguredWifi()) {
    Serial.println("[wifi] no STA credentials configured");
    drawBootScreen("WiFi setup AP");
    networkMode = NetworkMode::SoftAp;
    connectWiFi();
    return;
  }

  beginStaWifiConnection("initial");
}

void updateWiFi(unsigned long now) {
  if (now - lastWifiCheckMs < WIFI_STATUS_CHECK_INTERVAL_MS) {
    return;
  }
  lastWifiCheckMs = now;

  if (networkMode == NetworkMode::SoftAp) {
    return;
  }

  const wl_status_t status = WiFi.status();
  if (status != lastWifiStatus) {
    Serial.printf("[wifi] status=%d\n", static_cast<int>(status));
    lastWifiStatus = status;
    redrawNetworkSettingsIfVisible();
  }

  if (status == WL_CONNECTED) {
    wifiConnectAttempts = 0;
    wifiConnectStartedMs = 0;
    if (!wsStarted) {
      Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
      startServers();
      redrawNetworkSettingsIfVisible();
    }
    return;
  }

  if (wifiConnectStartedMs == 0) {
    beginStaWifiConnection("resume");
    return;
  }

  if (now - wifiConnectStartedMs < WIFI_CONNECT_RETRY_MS) {
    return;
  }

  ++wifiConnectAttempts;
  if (wifiConnectAttempts >= WIFI_CONNECT_ATTEMPTS_PER_CREDENTIAL) {
    wifiConnectAttempts = 0;
    if (!selectNextConfiguredWifi()) {
      Serial.println("[wifi] no STA credentials configured");
      return;
    }
    Serial.printf("[wifi] switching candidate to %s\n", wifiCredentials[currentWifiIndex].ssid.c_str());
    redrawNetworkSettingsIfVisible();
  }

  Serial.printf("[wifi] reconnecting after timeout status=%d\n", static_cast<int>(status));
  beginStaWifiConnection("timeout");
}

void updateScreenPetting(unsigned long now, const m5::touch_detail_t& touch) {
#if STACKCHAN_HAS_SCREEN_TOUCH_PETTING
  if (!displayOn || infoScreenVisible) {
    screenPettingCandidate = false;
    screenPettingTouchActive = false;
    screenPettingCandidateSinceMs = 0;
    screenPettingReleaseSinceMs = 0;
    screenPettingTravelPx = 0;
    setPettingActive(false, now);
    return;
  }

	  if (!touch.isPressed()) {
	    screenPettingCandidate = false;
	    screenPettingTravelPx = 0;
	    screenPettingCandidateSinceMs = 0;
	    if (screenPettingTouchActive) {
	      screenPettingReleaseSinceMs = 0;
	      screenPettingTouchActive = false;
	      setPettingActive(false, now);
	    }
    return;
  }

  screenPettingReleaseSinceMs = 0;
  const int32_t cx = roundCenterX();
  const int32_t cy = roundCenterY();
  const int32_t baseDx = touch.base_x - cx;
  const int32_t baseDy = touch.base_y - cy;
  const int32_t baseRadius = SCREEN_PETTING_RADIUS_PX;
  const bool startedInPettingZone = baseDx * baseDx + baseDy * baseDy <= baseRadius * baseRadius;
  if (!startedInPettingZone) {
    if (screenPettingTouchActive) {
      screenPettingTouchActive = false;
      setPettingActive(false, now);
    }
    screenPettingCandidate = false;
    screenPettingCandidateSinceMs = 0;
    screenPettingTravelPx = 0;
    return;
  }

  if (!screenPettingCandidate && !screenPettingTouchActive) {
    screenPettingCandidate = true;
    screenPettingCandidateSinceMs = now;
    screenPettingTravelPx = 0;
  }

  screenPettingTravelPx += abs(touch.deltaX()) + abs(touch.deltaY());
  const bool heldLongEnough = screenPettingCandidateSinceMs != 0 &&
                              now - screenPettingCandidateSinceMs >= SCREEN_PETTING_REQUIRED_MS;
  const bool draggedEnough = screenPettingTravelPx >= SCREEN_PETTING_DRAG_DISTANCE_PX;
  if (heldLongEnough || draggedEnough || screenPettingTouchActive) {
    screenPettingTouchActive = true;
    setPettingActive(true, now);
#if STACKCHAN_DEVICE_STOPWATCH
    faceController.setPetAnimationTouchFrame(petAnimationTouchFrameForX(touch.x, cx), now);
#endif
  }
#else
  (void)now;
  (void)touch;
#endif
}

#if STACKCHAN_GURUGURU_FACE_ENABLED
bool littleFsImageExists(const char* path) {
  if (LittleFS.exists(path)) {
    return true;
  }
  const char* ext = strrchr(path, '.');
  if (ext == nullptr) {
    return false;
  }
  String jpegPath(path);
  jpegPath.remove(static_cast<unsigned int>(ext - path));
  jpegPath += ".jpg";
  return LittleFS.exists(jpegPath);
}

bool guruguruFaceAssetsAvailable() {
  if (guruguruFaceAssetsChecked) {
    return guruguruFaceAssetsReady;
  }
  char path[16];
#if STACKCHAN_DEVICE_STOPWATCH
  snprintf(path, sizeof(path), "/dir16.png");
#else
  snprintf(path, sizeof(path), "/dir%u.png", static_cast<unsigned>(STACKCHAN_GURUGURU_FACE_CENTER_INDEX));
#endif
  guruguruFaceAssetsReady = littleFsImageExists(path);
  guruguruFaceAssetsChecked = true;
  return guruguruFaceAssetsReady;
}

bool guruguruFaceCanRun() {
  return guruguruFaceMode &&
         displayOn &&
         !infoScreenVisible &&
         currentState == ChanState::Idle &&
         audioController.state() == ChanState::Idle &&
         !audioController.isPlaybackDraining() &&
         !appClientConnected() &&
         guruguruFaceAssetsAvailable();
}

#if STACKCHAN_GURUGURU_IMU_ENABLED
bool guruguruFaceUsesImu() {
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  return true;
#else
  return guruguruFaceImuInput;
#endif
}

void setGuruguruFaceImuInput(bool enabled) {
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  (void)enabled;
#else
  guruguruFaceImuInput = enabled;
#if STACKCHAN_GURUGURU_FACE_ENABLED
  resetGuruguruAffectionTracking();
  resetGuruguruDizzySpinDetection(true);
#endif
  if (guruguruFaceImuInput) {
    resetGuruguruImuBase();
  } else {
    faceController.setGuruguruFaceDirection(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
  }
  Serial.printf("[guruguru] input=%s\n", guruguruFaceImuInput ? "imu" : "touch");
#endif
}

void toggleGuruguruFaceInput() {
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  resetGuruguruImuBase();
#else
  setGuruguruFaceImuInput(!guruguruFaceImuInput);
#endif
}
#endif

void updateGuruguruFaceAvailability(unsigned long now) {
  if (guruguruFaceMode && appClientConnected()) {
    Serial.println("[guruguru] disabled: client connected");
    setGuruguruFaceMode(false, now);
    return;
  }

  const bool effective = guruguruFaceCanRun();
  if (guruguruFaceEffective == effective) {
    return;
  }

  guruguruFaceEffective = effective;
  faceController.setGuruguruFaceMode(effective);
  if (!effective) {
    faceController.setGuruguruFaceDirection(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
  }
}

void setGuruguruFaceMode(bool enabled, unsigned long now) {
  if (enabled && appClientConnected()) {
    enabled = false;
  }
  if (enabled && !guruguruFaceAssetsAvailable()) {
    enabled = false;
    Serial.println("[guruguru] disabled: assets missing");
  }
  if (guruguruFaceMode == enabled) {
    updateGuruguruFaceAvailability(now);
    return;
  }

#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  if (enabled) {
    guruguruRestoreListeningOnExit = currentState == ChanState::Listening;
    if (guruguruRestoreListeningOnExit &&
        audioController.state() != ChanState::Speaking &&
        !audioController.isPlaybackDraining()) {
      setState(ChanState::Idle);
    }
  }
#endif

  const bool leavingGuruguruMode = guruguruFaceMode && !enabled && displayOn;
  guruguruFaceMode = enabled;
  if (enabled && pettingActive) {
    setPettingActive(false, now);
  }
#if (STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_CORES3) && STACKCHAN_GURUGURU_IMU_ENABLED
  if (enabled) {
    guruguruFaceImuInput = false;
  }
#endif
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (enabled) {
    resetGuruguruImuBase();
  } else {
    resetGuruguruAffectionTracking();
    resetGuruguruImuDizzyShakeDetection();
    resetGuruguruDizzySpinDetection(true);
  }
#endif
  if (enabled && displayOn && guruguruFaceCanRun()) {
    drawGuruguruLoadingScreen();
  } else if (leavingGuruguruMode) {
    drawVoiceLoadingScreen();
  }
  updateGuruguruFaceAvailability(now);
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  if (!enabled && guruguruRestoreListeningOnExit &&
      currentState == ChanState::Idle &&
      audioController.state() == ChanState::Idle &&
      !audioController.isPlaybackDraining()) {
    guruguruRestoreListeningOnExit = false;
    setState(ChanState::Listening);
  } else if (!enabled) {
    guruguruRestoreListeningOnExit = false;
  }
#endif
  Serial.printf("[guruguru] mode=%d\n", guruguruFaceMode ? 1 : 0);
}

uint8_t guruguruDirectionFromTouch(const m5::touch_detail_t& touch) {
  const int32_t cx = roundCenterX();
  const int32_t cy = roundCenterY();

  const float dx = static_cast<float>(touch.x - cx);
  const float dy = static_cast<float>(touch.y - cy);

  if (dx * dx + dy * dy < 50.0f * 50.0f) {
    return STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
  }

  const float angle = atan2f(dy, dx) * 180.0f / PI;
  const float normalizedAngle = angle < 0.0f ? angle + 360.0f : angle;
#if STACKCHAN_DEVICE_STOPWATCH
  return static_cast<uint8_t>(static_cast<int>((normalizedAngle + 22.5f) / 45.0f) & 0x07);
#else
  return static_cast<uint8_t>(static_cast<int>((normalizedAngle + 11.25f) / 22.5f) & 0x0F);
#endif
}

#if STACKCHAN_GURUGURU_IMU_ENABLED && STACKCHAN_GURUGURU_FACE_ENABLED
void resetGuruguruDizzySpinDetection(bool resetLastDirection);
bool updateGuruguruDizzySpinDetection(unsigned long now, uint8_t direction, bool useGameRules);
void updateGuruguruImuStepTracking(unsigned long now, uint8_t direction);
bool updateGuruguruImuDizzyShakeDetection(unsigned long now, float sampleDelta);
#endif

bool updateGuruguruFaceTouch(unsigned long now, const m5::touch_detail_t& touch) {
  updateGuruguruFaceAvailability(now);
  if (!guruguruFaceEffective) {
    return false;
  }
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (faceController.guruguruDizzyAnimationActive()) {
    return true;
  }
#endif
#if STACKCHAN_GURUGURU_IMU_ENABLED
  if (guruguruFaceUsesImu()) {
#if STACKCHAN_DEVICE_CORES3
    if (touch.wasHold()) {
      resetGuruguruImuBase();
      Serial.println("[guruguru] imu base reset requested by screen hold");
    }
#endif
    return true;
  }
#endif

  if (touch.isPressed()) {
    const uint8_t direction = guruguruDirectionFromTouch(touch);
#if STACKCHAN_GURUGURU_FACE_ENABLED
    if (updateGuruguruDizzySpinDetection(now, direction, true)) {
      return true;
    }
#endif
    faceController.setGuruguruFaceDirection(direction);
  } else {
#if STACKCHAN_GURUGURU_FACE_ENABLED
    resetGuruguruDizzySpinDetection(true);
#endif
    faceController.setGuruguruFaceDirection(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
  }
  return true;
}

#if STACKCHAN_GURUGURU_IMU_ENABLED
bool normalizeGuruguruImuAccel(const m5::imu_data_t& data, float& x, float& y, float& z) {
  const float mag = sqrtf(data.accel.x * data.accel.x +
                          data.accel.y * data.accel.y +
                          data.accel.z * data.accel.z);
  if (mag < GURUGURU_IMU_MIN_ACCEL_MAG_G || mag > GURUGURU_IMU_MAX_ACCEL_MAG_G) {
    return false;
  }
  x = data.accel.x / mag;
  y = data.accel.y / mag;
  z = data.accel.z / mag;
  return true;
}

void resetGuruguruImuBase() {
  guruguruImuBaseReady = false;
  guruguruImuFilterReady = false;
  guruguruImuCandidateDirection = STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
  guruguruImuCandidateSamples = 0;
  nextGuruguruImuUpdateMs = 0;
#if STACKCHAN_GURUGURU_FACE_ENABLED
  resetGuruguruDizzySpinDetection(true);
#endif
  faceController.setGuruguruFaceDirection(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
}

bool resetGuruguruImuBase(const m5::imu_data_t& data) {
  float x;
  float y;
  float z;
  if (!normalizeGuruguruImuAccel(data, x, y, z)) {
    resetGuruguruImuBase();
    return false;
  }

  guruguruImuBaseX = x;
  guruguruImuBaseY = y;
  guruguruImuBaseZ = z;
  guruguruImuFilterX = x;
  guruguruImuFilterY = y;
  guruguruImuFilterZ = z;
  guruguruImuBaseReady = true;
  guruguruImuFilterReady = true;
  guruguruImuCandidateDirection = STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
  guruguruImuCandidateSamples = 0;
  faceController.setGuruguruFaceDirection(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
  Serial.println("[guruguru] imu base reset");
  return true;
}

#if STACKCHAN_GURUGURU_FACE_ENABLED
void applyGuruguruAffectionDelta(int delta, unsigned long now, const char* reason) {
  if (delta == 0) {
    return;
  }
  applyAffectionResult(affectionController.debugAdjust(delta), now, true);
  Serial.printf("[guruguru] affection %+d reason=%s\n",
                delta,
                reason != nullptr ? reason : "unknown");
}

void resetGuruguruAffectionTracking() {
  guruguruAffectionStepAccum = 0;
  guruguruAffectionStepStartMs = 0;
  guruguruAffectionDangerCombo = 0;
  guruguruAffectionRedDangerStreak = 0;
}

uint8_t guruguruAffectionDangerStart() {
#if STACKCHAN_DEVICE_CORES3
  static constexpr uint8_t kDangerStarts[] = {20, 24, 28, 30};
#else
  static constexpr uint8_t kDangerStarts[] = {10, 12, 14, 15};
#endif
  const uint8_t index = min<uint8_t>(
    guruguruAffectionDangerCombo,
    static_cast<uint8_t>((sizeof(kDangerStarts) / sizeof(kDangerStarts[0])) - 1)
  );
  return kDangerStarts[index];
}

void recordGuruguruAffectionMovement(uint8_t steps, unsigned long now, bool useGameRules) {
  if (steps == 0 || faceController.guruguruDizzyAnimationActive()) {
    return;
  }

  if (guruguruAffectionStepAccum == 0) {
    guruguruAffectionStepStartMs = now;
  }
  guruguruAffectionStepAccum += steps;
  while (guruguruAffectionStepAccum >= GURUGURU_AFFECTION_STEP_COUNT) {
    guruguruAffectionStepAccum -= GURUGURU_AFFECTION_STEP_COUNT;
    const unsigned long elapsedMs = guruguruAffectionStepStartMs == 0
                                      ? GURUGURU_DIZZY_WINDOW_MS
                                      : max<unsigned long>(1, now - guruguruAffectionStepStartMs);
    const uint16_t projectedSteps = static_cast<uint16_t>(
      (static_cast<uint32_t>(GURUGURU_AFFECTION_STEP_COUNT) * GURUGURU_DIZZY_WINDOW_MS +
       elapsedMs / 2) / elapsedMs
    );

    const uint16_t dangerStart = guruguruAffectionDangerStart();
    const bool inDangerZone =
      projectedSteps >= dangerStart;
    const bool inRedDangerZone =
      inDangerZone && dangerStart >= GURUGURU_AFFECTION_RED_PROJECTED_STEPS;

    if (!useGameRules) {
      guruguruAffectionDangerCombo = 0;
      guruguruAffectionRedDangerStreak = 0;
      applyGuruguruAffectionDelta(GURUGURU_AFFECTION_REWARD_DELTA, now, "guruguru_imu");
      guruguruAffectionStepStartMs = guruguruAffectionStepAccum > 0 ? now : 0;
      continue;
    }

    if (projectedSteps <= GURUGURU_AFFECTION_MIN_PROJECTED_STEPS) {
      guruguruAffectionRedDangerStreak = 0;
      faceController.showGuruguruStep(static_cast<uint8_t>(min<uint16_t>(projectedSteps, 99)),
                                      static_cast<uint8_t>(min<uint16_t>(dangerStart, 99)),
                                      now);
      Serial.printf("[guruguru] affection skipped slow projected=%u elapsed=%lu slowLimit=%u\n",
                    static_cast<unsigned>(projectedSteps),
                    elapsedMs,
                    static_cast<unsigned>(GURUGURU_AFFECTION_MIN_PROJECTED_STEPS));
      guruguruAffectionStepStartMs = guruguruAffectionStepAccum > 0 ? now : 0;
      continue;
    }

    int reward = GURUGURU_AFFECTION_REWARD_DELTA;
    const char* reason = "guruguru";
    if (inDangerZone) {
      reward += GURUGURU_AFFECTION_DANGER_BONUS_DELTA;
      reason = "guruguru_danger";
      if (inRedDangerZone) {
        reward += guruguruAffectionRedDangerStreak;
        reason = guruguruAffectionRedDangerStreak > 0
                   ? "guruguru_red_streak"
                   : "guruguru_red_danger";
        if (guruguruAffectionRedDangerStreak < 50) {
          ++guruguruAffectionRedDangerStreak;
        }
      } else {
        guruguruAffectionRedDangerStreak = 0;
      }
      if (guruguruAffectionDangerCombo < 3) {
        ++guruguruAffectionDangerCombo;
      }
      Serial.printf("[guruguru] danger bonus combo=%u redStreak=%u projected=%u elapsed=%lu range=%u-%u\n",
                    static_cast<unsigned>(guruguruAffectionDangerCombo),
                    static_cast<unsigned>(guruguruAffectionRedDangerStreak),
                    static_cast<unsigned>(projectedSteps),
                    elapsedMs,
                    static_cast<unsigned>(dangerStart),
                    static_cast<unsigned>(GURUGURU_DIZZY_TOTAL_STEPS));
    } else {
      guruguruAffectionRedDangerStreak = 0;
    }
    applyGuruguruAffectionDelta(reward, now, reason);
    faceController.showGuruguruStep(static_cast<uint8_t>(min<uint16_t>(projectedSteps, 99)),
                                    static_cast<uint8_t>(min<uint16_t>(dangerStart, 99)),
                                    now);

    guruguruAffectionStepStartMs = guruguruAffectionStepAccum > 0 ? now : 0;
  }
}

void resetGuruguruImuDizzyShakeDetection() {
  guruguruImuDizzyShakeStartMs = 0;
  guruguruImuDizzyShakeLastActiveMs = 0;
}

void resetGuruguruDizzySpinDetection(bool resetLastDirection) {
  if (resetLastDirection) {
    guruguruDizzyLastDirection = -1;
    resetGuruguruAffectionTracking();
  }
  guruguruDizzyWindowStartMs = 0;
  guruguruDizzyTotalSteps = 0;
  guruguruDizzySignedSteps = 0;
}

int8_t guruguruCircularDirectionDelta(uint8_t previous, uint8_t current) {
  int8_t delta = static_cast<int8_t>(current) - static_cast<int8_t>(previous);
  const int8_t ringCount = static_cast<int8_t>(STACKCHAN_GURUGURU_FACE_CENTER_INDEX);
  const int8_t halfRing = ringCount / 2;
  if (delta > halfRing) {
    delta -= ringCount;
  } else if (delta < -halfRing) {
    delta += ringCount;
  }
  return delta;
}

bool updateGuruguruDizzySpinDetection(unsigned long now, uint8_t direction, bool useGameRules) {
  if (direction >= STACKCHAN_GURUGURU_FACE_CENTER_INDEX) {
    resetGuruguruDizzySpinDetection(true);
    return false;
  }
  if (now < guruguruDizzyCooldownUntilMs) {
    guruguruDizzyLastDirection = static_cast<int8_t>(direction);
    resetGuruguruDizzySpinDetection(false);
    return false;
  }
  if (faceController.guruguruDizzyAnimationActive()) {
    resetGuruguruDizzySpinDetection(true);
    return false;
  }
  if (guruguruDizzyLastDirection < 0) {
    guruguruDizzyLastDirection = static_cast<int8_t>(direction);
    guruguruDizzyWindowStartMs = now;
    return false;
  }

  const int8_t delta = guruguruCircularDirectionDelta(
    static_cast<uint8_t>(guruguruDizzyLastDirection),
    direction
  );
  guruguruDizzyLastDirection = static_cast<int8_t>(direction);
  if (delta == 0) {
    return false;
  }

  if (guruguruDizzyWindowStartMs == 0 || now - guruguruDizzyWindowStartMs > GURUGURU_DIZZY_WINDOW_MS) {
    guruguruDizzyWindowStartMs = now;
    guruguruDizzyTotalSteps = 0;
    guruguruDizzySignedSteps = 0;
  }

  guruguruDizzyTotalSteps += abs(delta);
  guruguruDizzySignedSteps += delta;

  if (guruguruDizzyTotalSteps >= GURUGURU_DIZZY_TOTAL_STEPS &&
      abs(guruguruDizzySignedSteps) >= GURUGURU_DIZZY_BIAS_STEPS) {
    const uint16_t triggeredTotalSteps = guruguruDizzyTotalSteps;
    const int16_t triggeredSignedSteps = guruguruDizzySignedSteps;
    const bool reverse = guruguruDizzySignedSteps < 0;
    if (faceController.startGuruguruDizzyAnimation(reverse, now)) {
      applyGuruguruAffectionDelta(GURUGURU_DIZZY_AFFECTION_DELTA, now, "dizzy");
      guruguruDizzyCooldownUntilMs = now + GURUGURU_DIZZY_COOLDOWN_MS;
      resetGuruguruAffectionTracking();
      resetGuruguruImuDizzyShakeDetection();
      resetGuruguruDizzySpinDetection(true);
      Serial.printf("[guruguru] dizzy triggered total=%u signed=%d reverse=%d\n",
                    static_cast<unsigned>(triggeredTotalSteps),
                    static_cast<int>(triggeredSignedSteps),
                    reverse ? 1 : 0);
      return true;
    }
    guruguruDizzyCooldownUntilMs = now + GURUGURU_DIZZY_COOLDOWN_MS;
    resetGuruguruDizzySpinDetection(true);
    resetGuruguruAffectionTracking();
  }

  recordGuruguruAffectionMovement(static_cast<uint8_t>(abs(delta)), now, useGameRules);
  return false;
}

void updateGuruguruImuStepTracking(unsigned long now, uint8_t direction) {
  if (direction >= STACKCHAN_GURUGURU_FACE_CENTER_INDEX) {
    resetGuruguruDizzySpinDetection(true);
    return;
  }
  if (faceController.guruguruDizzyAnimationActive()) {
    resetGuruguruDizzySpinDetection(true);
    return;
  }
  if (guruguruDizzyLastDirection < 0) {
    guruguruDizzyLastDirection = static_cast<int8_t>(direction);
    guruguruDizzyWindowStartMs = now;
    return;
  }

  const int8_t delta = guruguruCircularDirectionDelta(
    static_cast<uint8_t>(guruguruDizzyLastDirection),
    direction
  );
  guruguruDizzyLastDirection = static_cast<int8_t>(direction);
  if (delta == 0) {
    return;
  }

  if (guruguruDizzyWindowStartMs == 0 || now - guruguruDizzyWindowStartMs > GURUGURU_DIZZY_WINDOW_MS) {
    guruguruDizzyWindowStartMs = now;
    guruguruDizzyTotalSteps = 0;
    guruguruDizzySignedSteps = 0;
  }
  guruguruDizzyTotalSteps += abs(delta);
  guruguruDizzySignedSteps += delta;
  recordGuruguruAffectionMovement(static_cast<uint8_t>(abs(delta)), now, false);
}

bool updateGuruguruImuDizzyShakeDetection(unsigned long now, float sampleDelta) {
  if (now < guruguruDizzyCooldownUntilMs || faceController.guruguruDizzyAnimationActive()) {
    resetGuruguruImuDizzyShakeDetection();
    return false;
  }

  if (sampleDelta >= GURUGURU_IMU_DIZZY_SHAKE_THRESHOLD_G) {
    if (guruguruImuDizzyShakeStartMs == 0 ||
        now - guruguruImuDizzyShakeLastActiveMs > GURUGURU_IMU_DIZZY_SHAKE_GRACE_MS) {
      guruguruImuDizzyShakeStartMs = now;
    }
    guruguruImuDizzyShakeLastActiveMs = now;

    if (now - guruguruImuDizzyShakeStartMs >= GURUGURU_IMU_DIZZY_SHAKE_MS) {
      const bool reverse = guruguruDizzySignedSteps < 0;
      if (faceController.startGuruguruDizzyAnimation(reverse, now)) {
        applyGuruguruAffectionDelta(GURUGURU_DIZZY_AFFECTION_DELTA, now, "imu_shake_dizzy");
        guruguruDizzyCooldownUntilMs = now + GURUGURU_DIZZY_COOLDOWN_MS;
        resetGuruguruAffectionTracking();
        resetGuruguruDizzySpinDetection(true);
        resetGuruguruImuDizzyShakeDetection();
        Serial.printf("[guruguru] imu shake dizzy sample=%.3f reverse=%d\n",
                      sampleDelta,
                      reverse ? 1 : 0);
        return true;
      }
      guruguruDizzyCooldownUntilMs = now + GURUGURU_DIZZY_COOLDOWN_MS;
      resetGuruguruDizzySpinDetection(true);
      resetGuruguruImuDizzyShakeDetection();
    }
    return false;
  }

  if (guruguruImuDizzyShakeLastActiveMs != 0 &&
      now - guruguruImuDizzyShakeLastActiveMs > GURUGURU_IMU_DIZZY_SHAKE_GRACE_MS) {
    resetGuruguruImuDizzyShakeDetection();
  }
  return false;
}
#endif

uint8_t guruguruDirectionFromImuDelta(float dx, float dy) {
#if STACKCHAN_DEVICE_STOPWATCH
  const float deadzone = GURUGURU_IMU_STOPWATCH_DEADZONE_G;
#else
  const float deadzone = GURUGURU_IMU_DEADZONE_G;
#endif
  if (dx * dx + dy * dy < deadzone * deadzone) {
    return STACKCHAN_GURUGURU_FACE_CENTER_INDEX;
  }

  const float angle = atan2f(dy, dx) * 180.0f / PI;
  const float normalizedAngle = angle < 0.0f ? angle + 360.0f : angle;
#if STACKCHAN_DEVICE_STOPWATCH
  return static_cast<uint8_t>(static_cast<int>((normalizedAngle + 22.5f) / 45.0f) & 0x07);
#else
  return static_cast<uint8_t>(static_cast<int>((normalizedAngle + 11.25f) / 22.5f) & 0x0F);
#endif
}

bool updateGuruguruFaceImu(unsigned long now, const m5::imu_data_t& data, bool imuUpdated) {
  updateGuruguruFaceAvailability(now);
  if (!guruguruFaceEffective || !guruguruFaceUsesImu()) {
    return false;
  }
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (faceController.guruguruDizzyAnimationActive()) {
    return true;
  }
#endif

  if (!imuUpdated || now < nextGuruguruImuUpdateMs) {
    return true;
  }
  nextGuruguruImuUpdateMs = now + GURUGURU_IMU_UPDATE_INTERVAL_MS;

  float x;
  float y;
  float z;
  if (!normalizeGuruguruImuAccel(data, x, y, z)) {
    return true;
  }

  if (!guruguruImuBaseReady || !guruguruImuFilterReady) {
    resetGuruguruImuBase(data);
    return true;
  }

  const float sampleDelta = sqrtf((x - guruguruImuFilterX) * (x - guruguruImuFilterX) +
                                  (y - guruguruImuFilterY) * (y - guruguruImuFilterY) +
                                  (z - guruguruImuFilterZ) * (z - guruguruImuFilterZ));
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (updateGuruguruImuDizzyShakeDetection(now, sampleDelta)) {
    return true;
  }
#endif
  if (sampleDelta > GURUGURU_IMU_MAX_SAMPLE_DELTA_G) {
    return true;
  }

  guruguruImuFilterX += (x - guruguruImuFilterX) * GURUGURU_IMU_EMA_ALPHA;
  guruguruImuFilterY += (y - guruguruImuFilterY) * GURUGURU_IMU_EMA_ALPHA;
  guruguruImuFilterZ += (z - guruguruImuFilterZ) * GURUGURU_IMU_EMA_ALPHA;

#if STACKCHAN_DEVICE_STOPWATCH
  const float dX = guruguruImuFilterX - guruguruImuBaseX;
  const float dY = guruguruImuFilterY - guruguruImuBaseY;
  const float dZ = guruguruImuFilterZ - guruguruImuBaseZ;
  const float dx = 0.36f * dX - 1.50f * dY - 0.20f * dZ;
  const float dy = -0.96f * dX - 0.13f * dY - 0.64f * dZ;
#elif STACKCHAN_DEVICE_CORES3
  const float dx = -(guruguruImuFilterX - guruguruImuBaseX);
  const float dy = -(guruguruImuFilterZ - guruguruImuBaseZ) * GURUGURU_IMU_VERTICAL_GAIN;
#else
  const float dx = -(guruguruImuFilterX - guruguruImuBaseX);
  const float dy = -(guruguruImuFilterZ - guruguruImuBaseZ) * GURUGURU_IMU_VERTICAL_GAIN;
#endif
  const uint8_t direction = guruguruDirectionFromImuDelta(dx, dy);

#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (updateGuruguruDizzySpinDetection(now, direction, false)) {
    return true;
  }
#endif

#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_CORES3
  if (now - lastGuruguruImuDebugMs >= 500) {
    lastGuruguruImuDebugMs = now;
    Serial.printf("[guruguru] imu x=%.2f y=%.2f z=%.2f dx=%.2f dy=%.2f dir=%u\n",
                  x,
                  y,
                  z,
                  dx,
                  dy,
                  static_cast<unsigned>(direction));
  }
#endif

  if (direction != guruguruImuCandidateDirection) {
    guruguruImuCandidateDirection = direction;
    guruguruImuCandidateSamples = 1;
    return true;
  }

  if (guruguruImuCandidateSamples < GURUGURU_IMU_DIRECTION_STABLE_SAMPLES) {
    ++guruguruImuCandidateSamples;
  }
  if (guruguruImuCandidateSamples >= GURUGURU_IMU_DIRECTION_STABLE_SAMPLES) {
    faceController.setGuruguruFaceDirection(direction);
  }
  return true;
}
#endif
#endif

void updateTouch(unsigned long now) {
#if STACKCHAN_SMALL_DISPLAY
  (void)now;
  return;
#endif
  if (!interactionsReady(now)) {
    return;
  }

  auto touch = M5.Touch.getDetail();
  if (!displayOn) {
    return;
  }

#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (updateGuruguruFaceTouch(now, touch)) {
    return;
  }
#endif

  updateScreenPetting(now, touch);

  if (infoScreenVisible && settingsPage == SettingsPage::Network &&
      activeNetworkQr == NetworkQrType::None && touch.wasHold()) {
    switchNetworkModeAndRestart();
    return;
  }

  if (touch.wasClicked()) {
#if STACKCHAN_HAS_CAMERA
    if (!infoScreenVisible && appClientConnected() &&
        touchIn(touch, M5.Display.width() - 40, M5.Display.height() - 144, 40, 72)) {
      sendCameraButtonEvent(now);
      return;
    }
#endif
#if !STACKCHAN_ROUND_DISPLAY
    if (!infoScreenVisible && appClientConnected() &&
        touchIn(touch, M5.Display.width() - 40, M5.Display.height() - 80, 40, 80)) {
      audioController.setMicMuted(!audioController.micMuted());
      updateMicStatusOverlay();
      return;
    }
#endif
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

#if STACKCHAN_ROUND_DISPLAY
  if (infoScreenVisible && touch.wasFlicked() && handleSettingsTouch(touch)) {
    return;
  }
#endif

#if !STACKCHAN_DEVICE_STOPWATCH
  if (isSettingsSwipe(touch)) {
    setInfoScreenVisible(!infoScreenVisible);
    return;
  }
#endif
}

void updateButtons(unsigned long now) {
  (void)now;
#if STACKCHAN_SMALL_DISPLAY
  if (!interactionsReady(now)) {
    return;
  }
  if (infoScreenVisible && settingsPage == SettingsPage::Audio && smallVolumeAdjustMode) {
    if (M5.BtnA.isHolding()) {
      if (smallVolumeHoldRepeatMs == 0 || now - smallVolumeHoldRepeatMs >= 1000) {
        smallVolumeHoldRepeatMs = now;
        adjustSmallDisplayVolume(-SETTINGS_STEP_VALUE);
      }
      return;
    }
    if (M5.BtnA.isReleased()) {
      smallVolumeHoldRepeatMs = 0;
    }
  }
  if (M5.BtnA.wasHold()) {
    Serial.println("[button] hold");
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_GURUGURU_FACE_ENABLED
    if (guruguruFaceMode && !infoScreenVisible) {
      smallDisplayFacePettingHold = false;
      resetGuruguruImuBase();
      return;
    }
#endif
    handleSmallDisplayPageHold();
    return;
  }
  if (smallDisplayFacePettingHold) {
    if (M5.BtnA.isHolding()) {
      setPettingActive(true, now, PET_BUTTON_RELEASE_LINGER_MS);
      return;
	    }
	    if (M5.BtnA.wasReleasedAfterHold() || M5.BtnA.isReleased()) {
	      smallDisplayFacePettingHold = false;
	      setPettingActive(false, now);
	      return;
	    }
  }
  if (M5.BtnA.wasDecideClickCount()) {
    const uint8_t clickCount = M5.BtnA.getClickCount();
    Serial.printf("[button] click_count=%u\n", clickCount);

    if (clickCount == 1) {
      if (infoScreenVisible && settingsPage == SettingsPage::Audio && smallVolumeAdjustMode) {
        adjustSmallDisplayVolume(SETTINGS_STEP_VALUE);
        return;
      }
      advanceSmallDisplayPage();
      return;
    }

    if (clickCount == 2) {
      if (infoScreenVisible && settingsPage == SettingsPage::Network) {
        if (activeNetworkQr != NetworkQrType::None) {
          activeNetworkQr = NetworkQrType::None;
        } else {
          activeNetworkQr = NetworkQrType::Setup;
        }
        if (displayOn) {
          drawInfoScreen();
        }
        return;
      }
      if (infoScreenVisible && settingsPage == SettingsPage::Audio) {
        smallVolumeAdjustMode = !smallVolumeAdjustMode;
        smallVolumeHoldRepeatMs = 0;
        if (displayOn) {
          drawInfoScreen();
        }
        return;
      }
      if (infoScreenVisible && settingsPage == SettingsPage::StreetPass) {
        smallStreetPassView = (smallStreetPassView + 1) % kSmallStreetPassViewCount;
        if (displayOn) {
          drawInfoScreen();
        }
        return;
      }
      if (infoScreenVisible) {
        return;
      }
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_GURUGURU_FACE_ENABLED
      if (guruguruFaceMode) {
        return;
      }
#endif
      audioController.setMicMuted(!audioController.micMuted());
      updateMicStatusOverlay();
      return;
    }

    if (clickCount == 3) {
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT && STACKCHAN_GURUGURU_FACE_ENABLED
      if (!infoScreenVisible) {
        setGuruguruFaceMode(!guruguruFaceMode, now);
      }
#endif
      return;
    }
  }
#else
#if STACKCHAN_DEVICE_STOPWATCH
  if (M5.BtnA.wasDoubleClicked()) {
    setGuruguruFaceMode(!guruguruFaceMode, now);
    return;
  }
  if (M5.BtnA.wasSingleClicked()) {
    setInfoScreenVisible(!infoScreenVisible);
    return;
  }
  if (M5.BtnB.wasHold()) {
#if STACKCHAN_GURUGURU_IMU_ENABLED
    if (guruguruFaceMode && !infoScreenVisible) {
      resetGuruguruImuBase();
      return;
    }
#endif
  }
  if (M5.BtnB.wasDecideClickCount()) {
    const uint8_t clickCount = M5.BtnB.getClickCount();
    Serial.printf("[button] key_b_click_count=%u\n", clickCount);
    if (clickCount == 1) {
      setDisplayOn(!displayOn);
      return;
    }
    if (clickCount == 2) {
#if STACKCHAN_GURUGURU_IMU_ENABLED
      if (guruguruFaceMode && !infoScreenVisible) {
        toggleGuruguruFaceInput();
      }
#endif
      return;
    }
    return;
  }
#endif
#if STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
  if (M5.BtnPWR.wasDecideClickCount()) {
    const uint8_t clickCount = M5.BtnPWR.getClickCount();
    Serial.printf("[button] pwr_click_count=%u\n", clickCount);
    if (clickCount == 1) {
      setDisplayOn(!displayOn);
      return;
    }
    if (clickCount == 2) {
      setGuruguruFaceMode(!guruguruFaceMode, now);
      return;
    }
    if (clickCount >= 3) {
#if STACKCHAN_GURUGURU_IMU_ENABLED
      if (guruguruFaceMode && !infoScreenVisible) {
        toggleGuruguruFaceInput();
      }
#endif
      return;
    }
    return;
  }
#else
  if (M5.BtnPWR.wasClicked()) {
    setDisplayOn(!displayOn);
  }
#endif
#endif
}

#if STACKCHAN_HAS_BACK_TOUCH && STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
void resetBackTouchGuruguruGesture() {
  backTouchGuruguruPressed = false;
  backTouchGuruguruHoldFired = false;
  backTouchGuruguruPressSinceMs = 0;
  backTouchGuruguruFirstTapMs = 0;
}

bool handleGuruguruBackTouch(unsigned long now, bool backTouchDetected) {
#if STACKCHAN_GURUGURU_IMU_ENABLED
  if (!guruguruFaceMode || infoScreenVisible) {
    resetBackTouchGuruguruGesture();
    return false;
  }

  if (pettingActive) {
    setPettingActive(false, now);
  }

  if (backTouchDetected) {
    if (!backTouchGuruguruPressed) {
      backTouchGuruguruPressed = true;
      backTouchGuruguruHoldFired = false;
      backTouchGuruguruPressSinceMs = now;
    }
    return true;
  }

  if (backTouchGuruguruPressed) {
    const unsigned long pressDurationMs = now - backTouchGuruguruPressSinceMs;
    backTouchGuruguruPressed = false;

    if (!backTouchGuruguruHoldFired &&
        pressDurationMs >= BACK_TOUCH_GURUGURU_TAP_MIN_MS &&
        pressDurationMs <= BACK_TOUCH_GURUGURU_TAP_MAX_MS) {
      if (backTouchGuruguruFirstTapMs != 0 &&
          now - backTouchGuruguruFirstTapMs <= BACK_TOUCH_GURUGURU_DOUBLE_TAP_MS) {
        backTouchGuruguruFirstTapMs = 0;
      } else {
        backTouchGuruguruFirstTapMs = now;
      }
    }
    return true;
  }

  if (backTouchGuruguruFirstTapMs != 0 &&
      now - backTouchGuruguruFirstTapMs > BACK_TOUCH_GURUGURU_DOUBLE_TAP_MS) {
    backTouchGuruguruFirstTapMs = 0;
  }
  return true;
#else
  (void)now;
  (void)backTouchDetected;
  return false;
#endif
}
#endif

void updateBackTouch(unsigned long now) {
#if STACKCHAN_HAS_BACK_TOUCH
  if (!displayOn) {
    setPettingActive(false, now);
    backTouchReady = false;
    backTouchReleasedSinceMs = 0;
    backTouchCandidateSinceMs = 0;
    backTouchClearSinceMs = 0;
#if STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
    resetBackTouchGuruguruGesture();
#endif
    return;
  }

  if (infoScreenVisible) {
#if STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
    resetBackTouchGuruguruGesture();
#endif
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

#if STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
  if (handleGuruguruBackTouch(now, backTouchDetected)) {
    backTouchCandidateSinceMs = 0;
    backTouchClearSinceMs = 0;
    return;
  }
#endif

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
#else
  (void)now;
#endif
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
  beginDevice();
  Serial.setRxBufferSize(USB_SERIAL_RX_BUFFER_BYTES);
  Serial.begin(USB_SERIAL_BAUD);
  Serial.printf("[boot] reset_reason=%d usb_baud=%lu rx_buffer=%u at_ms=%lu\n",
                static_cast<int>(esp_reset_reason()),
                static_cast<unsigned long>(USB_SERIAL_BAUD),
                static_cast<unsigned>(USB_SERIAL_RX_BUFFER_BYTES),
                static_cast<unsigned long>(millis()));

  randomSeed(esp_random());
  ensureDeviceId();
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
  streetPassController.begin(&preferences);
  restoreStreetPassTimeFromRtc(millis());
  faceController.setAffectionState(affectionController.state());
  motionController.begin();
  audioController.begin(&wsServer);
  audioController.setMicPacketSender(sendUsbSerialMicPacket, nullptr);
  audioController.setVolume(deviceSettings.volume);

  wsServer.onText(onWsText);
  wsServer.onBinary(onWsBinary);
  wsServer.onConnection(onWsConnection);

  connectWiFi();
  beginStreetPassBle();
  interactionReadyAtMs = millis() + INTERACTION_STARTUP_IGNORE_MS;
  lastInitializeDrawMs = 0;
  drawInitializeScreen(millis());
}

void loop() {
  updateDevice();

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
  if (updateDisplayOffStreetPassMode(now)) {
    return;
  }
  updateThermalStatus(now);
  updateMicStatusOverlay();
  updateTouch(now);
#if STACKCHAN_GURUGURU_FACE_ENABLED
  updateGuruguruFaceAvailability(now);
#endif
  updateBackTouch(now);
  updateShake(now);
  updateHaptic(now);
  updateUsbSerial(now);
  updateUsbSerialDeferredIdle(now);
#if STACKCHAN_DEVICE_STOPWATCH
  if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
    audioController.update(millis());
  }
#endif
  updateWiFi(now);
  updateStreetPassNetworkTime(now);
  updateClockOverlay(now);
  streetPassController.update(now);

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
  updateStreetPassBle(now);

  if (!interactionsReady(now)) {
    if (displayOn) {
      drawInitializeScreen(now);
    }
    audioController.update(now);
    updateSpeakingFaceStateAfterPlayback();
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
    updateSpeakingFaceStateAfterPlayback();
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
  updateSpeakingFaceStateAfterPlayback();
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
    bool guruguruDizzyAnimating = false;
#if STACKCHAN_GURUGURU_FACE_ENABLED
    guruguruDizzyAnimating = faceController.guruguruDizzyAnimationActive();
#endif
    bool petFaceAnimating = false;
#if STACKCHAN_PET_ANIMATION_ENABLED
    petFaceAnimating = faceController.petAnimationActive();
#endif
    if (speaking || guruguruDizzyAnimating || petFaceAnimating || !deviceSettings.lowPowerMode || now - lastFaceUpdateMs >= LOW_POWER_FACE_UPDATE_INTERVAL_MS) {
      lastFaceUpdateMs = now;
      faceController.update(now);
      drawLowPowerPrompt();
      drawStreetPassNotificationOverlay();
#if STACKCHAN_DEVICE_STOPWATCH
      if (speaking) {
        audioController.update(millis());
      }
#endif
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
