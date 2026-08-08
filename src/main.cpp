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
#if STACKCHAN_DEVICE_STOPWATCH
#include <M5PM1.h>
#endif

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <cstring>
#include <esp_heap_caps.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <math.h>
#include <qrcodegen.h>
#include <sys/time.h>
#include <time.h>

#include "AffectionController.h"
#include "AppState.h"
#include "AudioController.h"
#include "CameraManager.h"
#include "FaceController.h"
#include "MotionController.h"
#include "PhoneCameraRemoteController.h"
#include "SpeechBubbleController.h"
#include "StepCounterController.h"
#include "StreetPassController.h"
#include "StreetPassProtocol.h"
#include "TimekeeperController.h"
#include "UsbSerialProtocol.h"
#include "Utf8Utils.h"
#include "WebSocketServerController.h"

namespace Usb = UsbSerialProtocol;

FaceController faceController;
MotionController motionController;
WebSocketServerController wsServer;
AudioController audioController;
SpeechBubbleController speechBubbleController;
CameraManager cameraManager;
AffectionController affectionController;
StreetPassController streetPassController;
StepCounterController stepCounterController;
PhoneCameraRemoteController phoneCameraRemoteController;
WebServer httpServer(HTTP_PORT);
Preferences preferences;
String deviceId;
String bootId;

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
  Steps = 6,
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

struct VoicePerfStats {
  bool active = false;
  uint32_t sessionSeq = 0;
  uint32_t loopCount = 0;
  uint32_t faceUpdateCount = 0;
  uint32_t audioUpdateCount = 0;
  uint32_t wsBinaryFrames = 0;
  uint32_t maxLoopGapMs = 0;
  uint32_t maxLoopDurationMs = 0;
  uint32_t maxFaceUpdateMs = 0;
  uint32_t maxAudioUpdateMs = 0;
  uint32_t maxWsLoopMs = 0;
  uint32_t maxHttpLoopMs = 0;
  uint32_t maxWsBinaryMs = 0;
  uint32_t maxWsBinaryBytes = 0;
  uint32_t maxFaceFrameGapMs = 0;
  uint64_t wsBinaryBytes = 0;
  unsigned long firstMs = 0;
  unsigned long lastMs = 0;
  unsigned long lastLoopMs = 0;
  unsigned long lastFaceUpdateMs = 0;
};

ChanState currentState = ChanState::Idle;
ExperienceMode experienceMode = ExperienceMode::Conversation;
uint32_t experienceModeRevision = 0;
bool pendingExperienceModeValid = false;
ExperienceMode pendingExperienceMode = ExperienceMode::Conversation;
AuthFaceMode currentAuthFaceMode = AuthFaceMode::Unknown;
NetworkMode networkMode = NetworkMode::Sta;
SettingsPage settingsPage = SettingsPage::Network;
DeviceSettings deviceSettings;
ThermalStatus thermalStatus;
VoicePerfStats voicePerfStats;
portMUX_TYPE voicePerfMux = portMUX_INITIALIZER_UNLOCKED;
bool vadActive = false;
bool infoScreenVisible = false;
bool streetPassProfileVisible = false;
uint8_t streetPassHistoryPage = 0;
uint8_t stepHistoryPage = 0;
bool displayOn = true;
bool wsStarted = false;
bool httpStarted = false;
bool httpRoutesRegistered = false;
bool wsClientConnected = false;
bool usbSerialClientConnected = false;
bool appCommsSuspendedForDisplayOff = false;
bool displayOffCpuFrequencyReduced = false;
NetworkQrType activeNetworkQr = NetworkQrType::None;
unsigned long lastWifiCheckMs = 0;
unsigned long wifiConnectStartedMs = 0;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;
volatile bool wifiReconnectEventPending = false;
volatile bool wifiDisconnectExpected = false;
volatile uint8_t wifiReconnectEventReason = 0;
volatile bool wifiGotIpReady = false;
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
bool voicePettingFaceActive = false;
bool voicePettingSessionActive = false;
unsigned long nextPetMoveMs = 0;
unsigned long lastPettingRepeatEventMs = 0;
Pose pettingBasePose = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
bool shakeActive = false;
bool shakeReturnMotionActive = false;
bool interactionMicPaused = false;
unsigned long servoMicQuietUntilMs = 0;
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
#if STACKCHAN_HAS_BACK_TOUCH && STACKCHAN_TIMEKEEPER_ENABLED
bool backTouchTimekeeperArmed = false;
bool backTouchTimekeeperPressed = false;
unsigned long backTouchTimekeeperPressSinceMs = 0;
unsigned long backTouchTimekeeperLastDetectedMs = 0;
unsigned long backTouchTimekeeperReleasedSinceMs = 0;
bool backTouchTravelArmed = false;
bool backTouchTravelPressed = false;
unsigned long backTouchTravelPressSinceMs = 0;
unsigned long backTouchTravelLastDetectedMs = 0;
unsigned long backTouchTravelReleasedSinceMs = 0;
#endif
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
#if STACKCHAN_TIMEKEEPER_ENABLED
TimekeeperController timekeeperController;
bool experienceModeMenuVisible = false;
bool timekeeperDurationMenuVisible = false;
TimekeeperActivity timekeeperTimerSubmode = TimekeeperActivity::Countdown;
ExperienceMode experienceModeMenuSelection = ExperienceMode::Conversation;
bool stopwatchYellowButtonPressed = false;
bool stopwatchYellowButtonLongHandled = false;
uint64_t stopwatchYellowButtonPressedAtMs = 0;
int8_t travelPhotoFaceIndex = -1;
bool travelFacePickerVisible = false;
uint8_t travelFacePickerPage = 0;
#if STACKCHAN_DEVICE_CORES3
unsigned long travelScreenFirstTapMs = 0;
int32_t travelScreenFirstTapX = 0;
int32_t travelScreenFirstTapY = 0;
#endif
uint64_t lastTimekeeperUiDrawMs = 0;
uint64_t lastTimekeeperUiValueMs = UINT64_MAX;
uint32_t timekeeperEventSequence = 0;
uint32_t communicationSuspendSequence = 0;

struct PendingTimekeeperAnnouncement {
  bool active = false;
  TimekeeperEvent event;
  String eventId;
  uint64_t expiresAtMs = 0;
  bool sentAfterLastDeviceInfo = false;
};

PendingTimekeeperAnnouncement pendingTimekeeperAnnouncement;

struct PendingTimekeeperSmileResult {
  bool active = false;
  bool animationStarted = false;
  bool prefetchSent = false;
  TimekeeperEvent event;
  String eventId;
  bool allowAnnouncement = false;
  uint64_t startDeadlineMs = 0;
};

PendingTimekeeperSmileResult pendingTimekeeperSmileResult;
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
enum class OverlayTouchTarget : uint8_t {
  None,
  PhoneCamera,
  DeviceCamera,
  Microphone,
};

struct OverlayTouchGesture {
  OverlayTouchTarget target = OverlayTouchTarget::None;
  unsigned long startedMs = 0;
  int32_t startX = 0;
  int32_t startY = 0;
  uint32_t maxTravelSquared = 0;
  bool longPressHandled = false;
};

OverlayTouchGesture overlayTouchGesture;
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
unsigned long streetPassResumeAfterCameraMs = 0;
bool streetPassAppWasConnected = false;
bool streetPassNtpConfigured = false;
bool streetPassNtpAwaitingResponse = false;
volatile bool streetPassNtpResyncRequested = false;
volatile bool streetPassNtpTimeAvailable = false;
volatile uint32_t streetPassNtpSyncedUnix = 0;
portMUX_TYPE streetPassNtpMux = portMUX_INITIALIZER_UNLOCKED;
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
constexpr unsigned long STREETPASS_DISPLAY_OFF_SCAN_DURATION_MS = 3000;
constexpr unsigned long STREETPASS_DISPLAY_OFF_SCAN_PERIOD_MS = 120000;
constexpr unsigned long STREETPASS_DISPLAY_OFF_SCAN_IDLE_INTERVAL_MS =
  STREETPASS_DISPLAY_OFF_SCAN_PERIOD_MS > STREETPASS_DISPLAY_OFF_SCAN_DURATION_MS
    ? STREETPASS_DISPLAY_OFF_SCAN_PERIOD_MS - STREETPASS_DISPLAY_OFF_SCAN_DURATION_MS
    : 0;
constexpr unsigned long STREETPASS_DISPLAY_OFF_INITIAL_SCAN_DELAY_MS = 10000;
constexpr unsigned long STREETPASS_DISPLAY_OFF_EXCHANGE_DELAY_MS = 5000;
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
uint32_t displayOnCpuFrequencyMhz = 0;
#if STEP_AFFECTION_REWARD_ENABLED
uint32_t stepAffectionRewardDay = 0;
uint32_t stepAffectionRewardMilestones = 0;
uint32_t pendingStepAffectionMilestones = 0;
#endif
#if STEP_COUNTER_ENABLED
uint32_t stepSyncSequence = 0;
uint32_t lastStepSyncActivityDay = 0;
uint32_t lastStepSyncSteps = 0;
unsigned long lastStepSyncUpdateMs = 0;
#endif
struct SharedImuSample {
  m5::imu_data_t data;
  bool valid = false;
  bool updated = false;
  unsigned long sampleMs = 0;
};

SharedImuSample sharedImuSample;
unsigned long nextSharedImuSampleMs = 0;

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
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
bool sendPhoneCameraShutterRequest(unsigned long now);
bool sendPhoneCameraLensRequest(unsigned long now);
void handlePhoneCameraStateCommand(JsonDocument& doc, SpeechBubbleTransport transport);
void handlePhoneCameraResultCommand(JsonDocument& doc, SpeechBubbleTransport transport);
void handlePhoneCameraLensResultCommand(JsonDocument& doc, SpeechBubbleTransport transport);
void disconnectPhoneCameraTransport(PhoneCameraTransport transport, const char* reason);
void updatePhoneCameraRemote(unsigned long now);
#endif
bool appClientConnected();
void updateUsbSerial(unsigned long now);
void updateWiFi(unsigned long now);
void stopServers(const char* reason);
void beginStreetPassBle();
bool suspendStreetPassBleForCamera();
void resumeStreetPassBleAfterCamera(bool wasSuspended);
void updateStreetPassBle(unsigned long now);
void updateStreetPassInboundEvents(unsigned long now, bool processWrites);
void restoreStreetPassTimeFromRtc(unsigned long now);
void writeStreetPassRtcTime(uint32_t unixTime);
void requestStreetPassNtpResync();
void updateClockOverlay(unsigned long now);
void updateSharedImuSample(unsigned long now);
void updateStepCounter(unsigned long now);
void loadStepAffectionRewardState();
void updateStepAffectionReward(unsigned long now);
void sendStepsSnapshot(const char* requestId = nullptr);
void updateStepSync(unsigned long now);
void handleStepsGetCommand(JsonDocument& doc);
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
uint64_t monotonicMs();
const char* experienceModeName(ExperienceMode mode);
void requestExperienceMode(ExperienceMode mode, unsigned long now);
void updatePendingExperienceMode(unsigned long now);
void sendExperienceModeChanged(ExperienceMode previousMode);
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
void cancelListeningNod(bool recenter);
void updateDeferredFaceState();
void updateSpeakingFaceStateAfterPlayback();
uint8_t voiceMouthLevelFromPlaybackPeak(unsigned long now);
#if STACKCHAN_CLASSIC_FACE_ENABLED
uint8_t classicMouthLevelFromPlaybackEnvelope(unsigned long now);
#endif
void updateDeferredFaceMode();
AuthFaceMode displayAuthFaceMode(AuthFaceMode mode);
bool audioBusyForUiEffects();
void updatePendingAffectionDelta();
void drawInfoScreen();
void drawNetworkQrScreen();
bool streetPassPageVisible();
void applyDisplayBrightness();
bool streetPassDisplayOffRadioWindowOpen(unsigned long now);
bool streetPassDisplayOffRadioBusy();
void sleepDisplayOffLowPower(unsigned long now);
void applyDisplayOffCpuFrequency();
void restoreDisplayOnCpuFrequency();
void suspendAppCommsForDisplayOff(unsigned long now);
void resumeAppCommsAfterDisplayOn(unsigned long now);
void applyLowPowerMode(bool enabled, bool persist);
void connectWiFi();
bool audioBusyForServoCalibration();
void drawStreetPassNotificationOverlay();
void beginDevice();
void updateDevice();
void logDeviceAudioConfig();
void applyStopwatchStatusLedSetting();
void pulseHaptic(uint8_t level, unsigned long durationMs, unsigned long now);
void updateHaptic(unsigned long now);
void resetOverlayTouchGesture();
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
#if STACKCHAN_TIMEKEEPER_ENABLED
void drawExperienceModeMenu();
void showExperienceModeMenu();
void hideExperienceModeMenu();
bool updateExperienceModeMenuTouch(const m5::touch_detail_t& touch, unsigned long now);
void resetTravelPhotoFace();
void advanceTravelPhotoFace();
void handleTravelYellowClickCount(uint8_t clickCount);
void drawTravelFacePicker();
void showTravelFacePicker();
void hideTravelFacePicker(bool redrawFace = true);
bool updateTravelFacePickerTouch(const m5::touch_detail_t& touch, unsigned long now);
#if STACKCHAN_DEVICE_CORES3
bool handleTravelScreenDoubleTap(const m5::touch_detail_t& touch,
                                 unsigned long now);
#endif
void drawTimekeeperDurationMenu();
void showTimekeeperDurationMenu();
void hideTimekeeperDurationMenu(bool redrawTimekeeper = true);
bool updateTimekeeperDurationMenuTouch(const m5::touch_detail_t& touch, uint64_t nowMs);
void drawTimekeeperFrameOverlay(M5Canvas& target);
void drawTimekeeperOverlay(uint64_t nowMs, bool force = false);
bool updateTimekeeperTouch(const m5::touch_detail_t& touch, uint64_t nowMs);
void handleTimekeeperEvent(const TimekeeperEvent& event, bool allowAnnouncement = true);
void sendTimekeeperAnnouncementPrefetch(const TimekeeperEvent& event,
                                        const String& eventId,
                                        bool allowAnnouncement);
void updatePendingTimekeeperSmileResult();
void flushPendingTimekeeperSmileResult(bool allowAnnouncement);
void updateTimekeeper(uint64_t nowMs);
void prepareTimekeeperForDisplayOff(uint64_t nowMs);
void sendCommunicationSuspending(const char* reason);
void handleTimekeeperAnnouncementResult(JsonDocument& doc);
void maybeResendPendingTimekeeperAnnouncement();
void loadTimekeeperSettings();
void saveTimekeeperCycles();
void saveTimekeeperTimerSubmode();
void handlePomodoroConfigGet(JsonDocument& doc);
void handlePomodoroConfigSet(JsonDocument& doc);
#endif
void redrawNetworkSettingsIfVisible();
#if STACKCHAN_SMALL_DISPLAY
void adjustSmallDisplayVolume(int delta);
#endif

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

uint64_t monotonicMs() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000ULL;
}

const char* experienceModeName(ExperienceMode mode) {
  switch (mode) {
    case ExperienceMode::Conversation:
      return "conversation";
    case ExperienceMode::Guruguru:
      return "guruguru";
    case ExperienceMode::Timekeeper:
      return "timekeeper";
    case ExperienceMode::Travel:
      return "travel";
  }
  return "conversation";
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
#if STACKCHAN_DEVICE_ATOMS3R_CHATBOT
  M5.Display.setRotation(1);
#endif
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

void applyStopwatchStatusLedSetting() {
#if STACKCHAN_DEVICE_STOPWATCH
  M5PM1 pm1;
  const m5pm1_err_t beginErr = pm1.begin(&M5.In_I2C);
  if (beginErr != M5PM1_OK) {
    Serial.printf("[power] stopwatch pm1 init failed for status led err=%d\n", static_cast<int>(beginErr));
    return;
  }

#if STOPWATCH_STATUS_LED_ENABLED
  const m5pm1_err_t levelErr = pm1.setLedEnLevel(true);
  Serial.printf("[power] stopwatch status led enabled led_en=%d\n", static_cast<int>(levelErr));
#else
  const m5pm1_err_t disableErr = pm1.disableLeds();
  const m5pm1_err_t levelErr = pm1.setLedEnLevel(false);
  Serial.printf("[power] stopwatch status led disabled disable=%d led_en=%d\n",
                static_cast<int>(disableErr),
                static_cast<int>(levelErr));
#endif
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

void ensureBootId() {
  if (!bootId.isEmpty()) {
    return;
  }
  char buffer[24];
  snprintf(buffer,
           sizeof(buffer),
           "b%08lx%08lx",
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  bootId = buffer;
  Serial.printf("[device] boot_id=%s\n", bootId.c_str());
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
#if !STACKCHAN_DEVICE_STOPWATCH
  if (appClientConnected()) {
    return "app_client";
  }
#endif
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
  const uint32_t unixTime = static_cast<uint32_t>(mktime(&value));
  setenv("TZ", STACKCHAN_TIMEZONE_POSIX, 1);
  tzset();
  return unixTime;
}

bool validStreetPassUnix(uint32_t unixTime) {
  return unixTime >= STREETPASS_VALID_UNIX_MIN;
}

bool streetPassTimeDeadlineReached(unsigned long now, unsigned long deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

unsigned long streetPassScanDurationMs() {
  return displayOn ? STREETPASS_SCAN_DURATION_MS : STREETPASS_DISPLAY_OFF_SCAN_DURATION_MS;
}

unsigned long streetPassScanIdleIntervalMs() {
  return displayOn ? STREETPASS_SCAN_IDLE_INTERVAL_MS : STREETPASS_DISPLAY_OFF_SCAN_IDLE_INTERVAL_MS;
}

unsigned long streetPassInitialScanDelayMs() {
  return displayOn ? 2000UL : STREETPASS_DISPLAY_OFF_INITIAL_SCAN_DELAY_MS;
}

unsigned long streetPassExchangeDelayMs() {
  return displayOn ? 1000UL : STREETPASS_DISPLAY_OFF_EXCHANGE_DELAY_MS;
}

bool streetPassDisplayOffRadioWindowOpen(unsigned long now) {
  if (displayOn) {
    return true;
  }
  if (!streetPassController.enabled()) {
    return false;
  }
  if (streetPassScanActive || streetPassGattServerConnected || streetPassExchangeInProgress) {
    return true;
  }
  if (streetPassBleSettleUntilMs != 0 && now < streetPassBleSettleUntilMs) {
    return false;
  }
  if (static_cast<long>(now - nextStreetPassScanMs) < 0) {
    return false;
  }
  return now - nextStreetPassScanMs < STREETPASS_DISPLAY_OFF_SCAN_DURATION_MS;
}

bool streetPassDisplayOffRadioBusy() {
  if (displayOn || !streetPassController.enabled()) {
    return false;
  }
  if (streetPassScanActive || streetPassAdvertising || streetPassGattServerConnected ||
      streetPassExchangeInProgress) {
    return true;
  }
  return streetPassGattClient != nullptr && streetPassGattClient->isConnected();
}

void slowStreetPassBleForDisplayOff(unsigned long now) {
  if (!streetPassController.enabled()) {
    return;
  }
  if (!streetPassScanActive && nextStreetPassScanMs < now + STREETPASS_DISPLAY_OFF_INITIAL_SCAN_DELAY_MS) {
    nextStreetPassScanMs = now + STREETPASS_DISPLAY_OFF_INITIAL_SCAN_DELAY_MS;
  }
  if (nextStreetPassExchangeMs < now + STREETPASS_DISPLAY_OFF_EXCHANGE_DELAY_MS) {
    nextStreetPassExchangeMs = now + STREETPASS_DISPLAY_OFF_EXCHANGE_DELAY_MS;
  }
}

void resumeStreetPassBleAfterDisplayOn(unsigned long now) {
  if (!streetPassController.enabled()) {
    return;
  }
  if (!streetPassScanActive && nextStreetPassScanMs > now + 2000UL) {
    nextStreetPassScanMs = now + 2000UL;
  }
  if (nextStreetPassExchangeMs > now + 1000UL) {
    nextStreetPassExchangeMs = now + 1000UL;
  }
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
  if (!streetPassDisplayOffRadioWindowOpen(millis())) {
    if (streetPassAdvertising) {
      stopStreetPassAdvertising("display_off_idle");
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

bool setSystemUnixTime(uint32_t unixTime) {
  if (!validStreetPassUnix(unixTime)) {
    return false;
  }
  timeval value = {};
  value.tv_sec = static_cast<time_t>(unixTime);
  if (settimeofday(&value, nullptr) != 0) {
    Serial.printf("[time] system clock write failed unix=%lu\n",
                  static_cast<unsigned long>(unixTime));
    return false;
  }
  return true;
}

void restoreStreetPassTimeFromRtc(unsigned long now) {
  uint32_t unixTime = 0;
  if (!readStreetPassRtcUnix(unixTime)) {
    return;
  }
  setSystemUnixTime(unixTime);
  if (streetPassController.syncTime(unixTime, STACKCHAN_TIMEZONE_NAME, now)) {
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

void onStreetPassNtpTimeSync(timeval* syncedTime) {
  if (syncedTime == nullptr ||
      syncedTime->tv_sec < static_cast<time_t>(STREETPASS_VALID_UNIX_MIN) ||
      static_cast<uint64_t>(syncedTime->tv_sec) > 0xFFFFFFFFULL) {
    return;
  }
  portENTER_CRITICAL(&streetPassNtpMux);
  streetPassNtpSyncedUnix = static_cast<uint32_t>(syncedTime->tv_sec);
  streetPassNtpTimeAvailable = true;
  portEXIT_CRITICAL(&streetPassNtpMux);
}

void requestStreetPassNtpResync() {
  portENTER_CRITICAL(&streetPassNtpMux);
  streetPassNtpResyncRequested = true;
  portEXIT_CRITICAL(&streetPassNtpMux);
}

void updateStreetPassNetworkTime(unsigned long now) {
  bool resyncRequested = false;
  bool timeAvailable = false;
  uint32_t syncedUnix = 0;
  portENTER_CRITICAL(&streetPassNtpMux);
  resyncRequested = streetPassNtpResyncRequested;
  streetPassNtpResyncRequested = false;
  timeAvailable = streetPassNtpTimeAvailable;
  if (timeAvailable) {
    syncedUnix = streetPassNtpSyncedUnix;
    streetPassNtpTimeAvailable = false;
  }
  portEXIT_CRITICAL(&streetPassNtpMux);

  if (resyncRequested) {
    if (!timeAvailable) {
      streetPassNtpConfigured = false;
      streetPassNtpAwaitingResponse = false;
      nextStreetPassNtpSyncMs = now;
    }
  }

  if (timeAvailable && validStreetPassUnix(syncedUnix)) {
    const uint32_t previousUnix = streetPassController.estimatedUnix(now);
    if (streetPassController.syncTime(syncedUnix, STACKCHAN_TIMEZONE_NAME, now)) {
      const long correctionSeconds =
        validStreetPassUnix(previousUnix)
          ? static_cast<long>(static_cast<int64_t>(syncedUnix) - static_cast<int64_t>(previousUnix))
          : 0;
      Serial.printf("[streetpass] ntp time synced unix=%lu correction=%ld sec timezone=%s\n",
                    static_cast<unsigned long>(syncedUnix),
                    correctionSeconds,
                    STACKCHAN_TIMEZONE_NAME);
      writeStreetPassRtcTime(syncedUnix);
    }
    streetPassNtpAwaitingResponse = false;
    nextStreetPassNtpSyncMs = now + STREETPASS_NTP_REFRESH_MS;
    return;
  }

  if (networkMode != NetworkMode::Sta || WiFi.status() != WL_CONNECTED ||
      !streetPassTimeDeadlineReached(now, nextStreetPassNtpSyncMs)) {
    return;
  }

  if (!streetPassNtpConfigured) {
    sntp_set_time_sync_notification_cb(onStreetPassNtpTimeSync);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_interval(STREETPASS_NTP_REFRESH_MS);
    configTzTime(STACKCHAN_TIMEZONE_POSIX,
                 "pool.ntp.org",
                 "time.google.com",
                 "time.cloudflare.com");
    streetPassNtpConfigured = true;
    streetPassNtpAwaitingResponse = true;
    nextStreetPassNtpSyncMs = now + STREETPASS_NTP_RETRY_MS;
    Serial.printf("[streetpass] ntp sync requested timezone=%s\n", STACKCHAN_TIMEZONE_NAME);
    return;
  }

  const bool retrying = streetPassNtpAwaitingResponse;
  if (sntp_restart()) {
    streetPassNtpAwaitingResponse = true;
    nextStreetPassNtpSyncMs = now + STREETPASS_NTP_RETRY_MS;
    Serial.printf("[streetpass] ntp %s requested\n",
                  retrying ? "retry" : "refresh");
    return;
  }

  streetPassNtpConfigured = false;
  streetPassNtpAwaitingResponse = false;
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

void updateSharedImuSample(unsigned long now) {
  sharedImuSample.updated = false;
  if (now < nextSharedImuSampleMs) {
    return;
  }

  nextSharedImuSampleMs = now + SHAKE_UPDATE_INTERVAL_MS;
  if (M5.Imu.update()) {
    M5.Imu.getImuData(&sharedImuSample.data);
    sharedImuSample.valid = true;
    sharedImuSample.updated = true;
    sharedImuSample.sampleMs = now;
  }
}

void updateStepCounter(unsigned long now) {
#if STEP_COUNTER_ENABLED
  uint32_t localUnix = 0;
  bool timeValid = false;
  const uint32_t unixTime = streetPassController.estimatedUnix(now);
  if (validStreetPassUnix(unixTime)) {
    const int64_t adjustedUnix = static_cast<int64_t>(unixTime) +
                                 static_cast<int64_t>(CLOCK_DISPLAY_UTC_OFFSET_MINUTES) * 60LL;
    if (adjustedUnix > 0 && adjustedUnix <= 0xFFFFFFFFLL) {
      localUnix = static_cast<uint32_t>(adjustedUnix);
      timeValid = true;
    }
  }

  stepCounterController.update(now,
                               sharedImuSample.data,
                               sharedImuSample.updated,
                               localUnix,
                               timeValid);
  if (displayOn) {
    faceController.setStepCount(stepCounterController.todaySteps(),
                                stepCounterController.todayValid());
  }
#else
  (void)now;
#endif
}

#if STEP_COUNTER_ENABLED
uint32_t nextStepSyncSequence() {
  ++stepSyncSequence;
  if (stepSyncSequence == 0) {
    stepSyncSequence = 1;
  }
  return stepSyncSequence;
}

uint32_t stepSyncGeneratedAtUnix(unsigned long now) {
  const uint32_t unixTime = streetPassController.estimatedUnix(now);
  return validStreetPassUnix(unixTime) ? unixTime : 0;
}

uint32_t stepDayStartUnix(uint32_t activityDay) {
  if (activityDay == 0) {
    return 0;
  }
  constexpr uint32_t kSecondsPerDay = 24UL * 60UL * 60UL;
  const int64_t localDayStart =
    static_cast<int64_t>(activityDay) * static_cast<int64_t>(kSecondsPerDay) +
    static_cast<int64_t>(STEP_COUNTER_DAY_START_HOUR) * 60LL * 60LL;
  const int64_t utcDayStart =
    localDayStart - static_cast<int64_t>(CLOCK_DISPLAY_UTC_OFFSET_MINUTES) * 60LL;
  if (utcDayStart <= 0 || utcDayStart > 0xFFFFFFFFLL) {
    return 0;
  }
  return static_cast<uint32_t>(utcDayStart);
}

String stepLocalDateString(uint32_t activityDay) {
  if (activityDay == 0) {
    return "";
  }
  constexpr uint32_t kSecondsPerDay = 24UL * 60UL * 60UL;
  time_t raw = static_cast<time_t>(activityDay * kSecondsPerDay);
  tm date = {};
  gmtime_r(&raw, &date);
  char text[11] = {};
  snprintf(text,
           sizeof(text),
           "%04d-%02d-%02d",
           date.tm_year + 1900,
           date.tm_mon + 1,
           date.tm_mday);
  return String(text);
}

const StepDayRecord* newestStepSyncRecordBefore(uint32_t beforeActivityDay) {
  const StepDayRecord* best = nullptr;
  const uint8_t count = stepCounterController.historyCount();
  for (uint8_t i = 0; i < count; ++i) {
    const StepDayRecord* record = stepCounterController.recordAt(i);
    if (record == nullptr || record->activityDay == 0 ||
        record->activityDay >= beforeActivityDay) {
      continue;
    }
    if (best == nullptr || record->activityDay > best->activityDay) {
      best = record;
    }
  }
  return best;
}

void writeStepRecordJson(JsonObject target, const StepDayRecord& record) {
  target["activityDay"] = record.activityDay;
  target["localDate"] = stepLocalDateString(record.activityDay);
  target["dayStartUnix"] = stepDayStartUnix(record.activityDay);
  target["steps"] = record.steps;
}

void writeStepsEnvelope(JsonDocument& doc, const char* type, const char* requestId, unsigned long now) {
  ensureDeviceId();
  doc["type"] = type;
  doc["schemaVersion"] = STEP_SYNC_SCHEMA_VERSION;
  if (requestId != nullptr && requestId[0] != '\0') {
    doc["requestId"] = requestId;
  }
  doc["deviceId"] = deviceId;
  doc["sequence"] = nextStepSyncSequence();
  doc["generatedAt"] = stepSyncGeneratedAtUnix(now);
}

void writeStepsSnapshot(JsonDocument& doc, const char* requestId, unsigned long now) {
  writeStepsEnvelope(doc, "steps.snapshot", requestId, now);
  doc["resetHour"] = STEP_COUNTER_DAY_START_HOUR;
  doc["timezoneOffsetMinutes"] = CLOCK_DISPLAY_UTC_OFFSET_MINUTES;
  doc["currentActivityDay"] = stepCounterController.todayValid()
                                ? stepCounterController.currentActivityDay()
                                : 0;
  doc["todaySteps"] = stepCounterController.todaySteps();

  JsonArray history = doc["history"].to<JsonArray>();
  uint32_t beforeActivityDay = UINT32_MAX;
  while (true) {
    const StepDayRecord* record = newestStepSyncRecordBefore(beforeActivityDay);
    if (record == nullptr) {
      break;
    }
    beforeActivityDay = record->activityDay;
    JsonObject item = history.add<JsonObject>();
    writeStepRecordJson(item, *record);
  }
}

void sendStepsSnapshot(const char* requestId) {
  if (!appClientConnected()) {
    return;
  }
  const unsigned long now = millis();
  JsonDocument doc;
  writeStepsSnapshot(doc, requestId, now);
  sendJsonDocument(doc);

  if (stepCounterController.todayValid()) {
    lastStepSyncActivityDay = stepCounterController.currentActivityDay();
    lastStepSyncSteps = stepCounterController.todaySteps();
    lastStepSyncUpdateMs = now;
  }
}

void sendStepsUpdate(unsigned long now) {
  if (!appClientConnected() || !stepCounterController.todayValid()) {
    return;
  }
  const uint32_t activityDay = stepCounterController.currentActivityDay();
  StepDayRecord record = {activityDay, stepCounterController.todaySteps()};

  JsonDocument doc;
  writeStepsEnvelope(doc, "steps.update", nullptr, now);
  JsonObject root = doc.as<JsonObject>();
  writeStepRecordJson(root, record);
  sendJsonDocument(doc);

  lastStepSyncActivityDay = activityDay;
  lastStepSyncSteps = record.steps;
  lastStepSyncUpdateMs = now;
}

void updateStepSync(unsigned long now) {
  if (!displayOn) {
    return;
  }
  if (!appClientConnected() || !stepCounterController.todayValid()) {
    return;
  }

  const uint32_t activityDay = stepCounterController.currentActivityDay();
  const uint32_t steps = stepCounterController.todaySteps();
  if (lastStepSyncActivityDay == 0 || lastStepSyncActivityDay != activityDay) {
    sendStepsSnapshot();
    return;
  }
  if (steps == lastStepSyncSteps) {
    return;
  }

  const uint32_t diff = steps > lastStepSyncSteps ? steps - lastStepSyncSteps : 0;
  if (diff >= STEP_SYNC_UPDATE_STEP_DELTA ||
      now - lastStepSyncUpdateMs >= STEP_SYNC_UPDATE_INTERVAL_MS ||
      steps < lastStepSyncSteps) {
    sendStepsUpdate(now);
  }
}

void handleStepsGetCommand(JsonDocument& doc) {
  sendStepsSnapshot(doc["requestId"] | "");
}
#else
void sendStepsSnapshot(const char* requestId) {
  (void)requestId;
}
void updateStepSync(unsigned long now) {
  (void)now;
}
void handleStepsGetCommand(JsonDocument& doc) {
  JsonDocument response;
  response["type"] = "steps.error";
  response["schemaVersion"] = STEP_SYNC_SCHEMA_VERSION;
  const char* requestId = doc["requestId"] | "";
  if (requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["error"] = "steps_not_supported";
  sendJsonDocument(response);
}
#endif

#if STEP_AFFECTION_REWARD_ENABLED
namespace {
constexpr const char* kStepAffectionPrefsNamespace = "step_aff";
constexpr const char* kStepAffectionDayKey = "day";
constexpr const char* kStepAffectionMilestonesKey = "mile";
constexpr const char* kStepAffectionPendingKey = "pend";
constexpr uint32_t kStepAffectionPendingMax = 100000;
}

void saveStepAffectionRewardState() {
  preferences.begin(kStepAffectionPrefsNamespace, false);
  preferences.putUInt(kStepAffectionDayKey, stepAffectionRewardDay);
  preferences.putUInt(kStepAffectionMilestonesKey, stepAffectionRewardMilestones);
  preferences.putUInt(kStepAffectionPendingKey, pendingStepAffectionMilestones);
  preferences.end();
}

void loadStepAffectionRewardState() {
  preferences.begin(kStepAffectionPrefsNamespace, true);
  stepAffectionRewardDay = preferences.getUInt(kStepAffectionDayKey, 0);
  stepAffectionRewardMilestones = preferences.getUInt(kStepAffectionMilestonesKey, 0);
  pendingStepAffectionMilestones = preferences.getUInt(kStepAffectionPendingKey, 0);
  preferences.end();
  if (pendingStepAffectionMilestones > kStepAffectionPendingMax) {
    pendingStepAffectionMilestones = kStepAffectionPendingMax;
  }
  Serial.printf("[steps] affection reward loaded day=%lu milestones=%lu pending=%lu\n",
                static_cast<unsigned long>(stepAffectionRewardDay),
                static_cast<unsigned long>(stepAffectionRewardMilestones),
                static_cast<unsigned long>(pendingStepAffectionMilestones));
}

void applyStepAffectionMilestones(uint32_t milestones, unsigned long now) {
  if (milestones == 0) {
    return;
  }
  const uint32_t cappedMilestones = min<uint32_t>(milestones, 333);
  const int delta = static_cast<int>(cappedMilestones) * STEP_AFFECTION_DELTA_PER_REWARD;
  applyAffectionResult(affectionController.debugAdjust(delta), now, true);
  Serial.printf("[steps] affection reward milestones=%lu delta=%d steps=%lu\n",
                static_cast<unsigned long>(milestones),
                delta,
                static_cast<unsigned long>(stepCounterController.todaySteps()));
}

void updateStepAffectionReward(unsigned long now) {
  if (!displayOn) {
    return;
  }

  bool dirty = false;
  uint32_t milestonesToApply = 0;

  if (displayOn && pendingStepAffectionMilestones > 0) {
    milestonesToApply += pendingStepAffectionMilestones;
    pendingStepAffectionMilestones = 0;
    dirty = true;
  }

  if (!stepCounterController.todayValid()) {
    if (milestonesToApply > 0) {
      applyStepAffectionMilestones(milestonesToApply, now);
    }
    if (dirty) {
      saveStepAffectionRewardState();
    }
    return;
  }

  const uint32_t activityDay = stepCounterController.currentActivityDay();
  if (activityDay == 0) {
    if (milestonesToApply > 0) {
      applyStepAffectionMilestones(milestonesToApply, now);
    }
    if (dirty) {
      saveStepAffectionRewardState();
    }
    return;
  }

  if (stepAffectionRewardDay != activityDay) {
    stepAffectionRewardDay = activityDay;
    stepAffectionRewardMilestones = 0;
    dirty = true;
  }

  const uint32_t eligibleMilestones =
    stepCounterController.todaySteps() / STEP_AFFECTION_STEPS_PER_REWARD;

  if (eligibleMilestones > stepAffectionRewardMilestones) {
    const uint32_t newMilestones = eligibleMilestones - stepAffectionRewardMilestones;
    stepAffectionRewardMilestones = eligibleMilestones;
    dirty = true;
    if (displayOn) {
      milestonesToApply += newMilestones;
    } else {
      pendingStepAffectionMilestones =
        min<uint32_t>(kStepAffectionPendingMax, pendingStepAffectionMilestones + newMilestones);
      Serial.printf("[steps] affection reward deferred new=%lu pending=%lu steps=%lu\n",
                    static_cast<unsigned long>(newMilestones),
                    static_cast<unsigned long>(pendingStepAffectionMilestones),
                    static_cast<unsigned long>(stepCounterController.todaySteps()));
    }
  }

  if (milestonesToApply > 0) {
    applyStepAffectionMilestones(milestonesToApply, now);
  }

  if (dirty) {
    saveStepAffectionRewardState();
  }
}
#else
void loadStepAffectionRewardState() {}
void updateStepAffectionReward(unsigned long now) {
  (void)now;
}
#endif

void beginStreetPassBle() {
  if (streetPassBleReady) {
    return;
  }

  Serial.println("[streetpass] BLE init begin");
  NimBLEDevice::init(STREETPASS_BLE_DEVICE_NAME);
  Serial.println("[streetpass] BLE device initialized");
  NimBLEDevice::setPower(ESP_PWR_LVL_P3);

  streetPassGattServer = NimBLEDevice::createServer();
  // The callback object has static storage. NimBLE defaults to taking ownership
  // and deleting server callbacks during deinit(true), which attempts to free
  // this global object when an app connection suspends StreetPass BLE.
  streetPassGattServer->setCallbacks(&streetPassServerCallbacks, false);
  NimBLEService* service = streetPassGattServer->createService(STREETPASS_SERVICE_UUID);
  streetPassInfoCharacteristic = service->createCharacteristic(STREETPASS_INFO_UUID, NIMBLE_PROPERTY::READ);
  streetPassPublicCardCharacteristic = service->createCharacteristic(STREETPASS_PUBLIC_CARD_UUID, NIMBLE_PROPERTY::READ);
  streetPassEncounterWriteCharacteristic = service->createCharacteristic(STREETPASS_ENCOUNTER_WRITE_UUID, NIMBLE_PROPERTY::WRITE);
  streetPassEncounterWriteCharacteristic->setCallbacks(&streetPassEncounterWriteCallbacks);
  updateStreetPassGattCharacteristics();
  streetPassGattServer->start();

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&streetPassScanCallbacks, true);
  scan->setActiveScan(false);
  scan->setInterval(80);
  scan->setWindow(80);
  streetPassBleReady = true;
  nextStreetPassScanMs = millis() + streetPassInitialScanDelayMs();
  if (streetPassBusyReason()[0] == '\0') {
    updateStreetPassAdvertising();
  } else {
    streetPassBlePaused = true;
  }
  Serial.println("[streetpass] BLE scan ready");
}

bool deinitStreetPassBle(const char* reason) {
  if (!streetPassBleReady) {
    return false;
  }

  Serial.printf("[streetpass] BLE deinit reason=%s largestDMA=%u\n",
                reason != nullptr ? reason : "unknown",
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
  if (streetPassScanActive) {
    NimBLEDevice::getScan()->stop();
  }
  streetPassScanActive = false;
  stopStreetPassAdvertising(reason != nullptr ? reason : "deinit");
  if (streetPassGattClient != nullptr && streetPassGattClient->isConnected()) {
    streetPassGattClient->disconnect();
  }
  streetPassGattServerConnected = false;
  streetPassExchangeInProgress = false;

  const bool stopped = NimBLEDevice::deinit(true);
  streetPassBleReady = false;
  streetPassAdvertising = false;
  streetPassBlePaused = true;
  streetPassGattServer = nullptr;
  streetPassGattClient = nullptr;
  streetPassInfoCharacteristic = nullptr;
  streetPassPublicCardCharacteristic = nullptr;
  streetPassEncounterWriteCharacteristic = nullptr;
  delay(80);
  Serial.printf("[streetpass] BLE deinit=%s largestDMA=%u\n",
                stopped ? "ready" : "failed",
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
  return true;
}

bool suspendStreetPassBleForCamera() {
#if STACKCHAN_HAS_CAMERA
  const size_t largestBefore = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  if (largestBefore >= CAMERA_BLE_SUSPEND_DMA_THRESHOLD_BYTES || !streetPassBleReady) {
    return false;
  }

  Serial.printf("[camera] DMA memory low largest=%u; suspending StreetPass BLE\n",
                static_cast<unsigned>(largestBefore));
  return deinitStreetPassBle("camera_memory");
#else
  return false;
#endif
}

void resumeStreetPassBleAfterCamera(bool wasSuspended) {
  if (!wasSuspended) {
    return;
  }
  constexpr unsigned long kStreetPassResumeDelayMs = 3000;
  streetPassResumeAfterCameraMs = millis() + kStreetPassResumeDelayMs;
  Serial.printf("[camera] StreetPass BLE resume deferred %lums largestDMA=%u\n",
                kStreetPassResumeDelayMs,
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
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

  streetPassController.update(now);
  updateStreetPassBle(now);
#if STEP_COUNTER_ENABLED
  sleepDisplayOffLowPower(now);
#else
  delay(30);
#endif
  return true;
}

void sleepDisplayOffLowPower(unsigned long now) {
#if STEP_COUNTER_ENABLED
#if DISPLAY_OFF_LIGHT_SLEEP_ENABLED
  (void)now;
  if (streetPassDisplayOffRadioBusy()) {
    delay(SHAKE_UPDATE_INTERVAL_MS > 10 ? 10 : SHAKE_UPDATE_INTERVAL_MS);
    return;
  }

  const unsigned long currentMs = millis();
  const unsigned long wakeMarginMs = DISPLAY_OFF_LIGHT_SLEEP_WAKE_MARGIN_MS;
  if (static_cast<long>(nextSharedImuSampleMs - currentMs) <= static_cast<long>(wakeMarginMs)) {
    delay(1);
    return;
  }

  const unsigned long availableMs = nextSharedImuSampleMs - currentMs - wakeMarginMs;
  if (availableMs < DISPLAY_OFF_LIGHT_SLEEP_MIN_MS) {
    delay(1);
    return;
  }

  M5.Power.lightSleep(static_cast<uint64_t>(availableMs) * 1000ULL, false);
#else
  (void)now;
  delay(SHAKE_UPDATE_INTERVAL_MS > 10 ? 10 : SHAKE_UPDATE_INTERVAL_MS);
#endif
#else
  (void)now;
  delay(30);
#endif
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
    nextStreetPassExchangeMs = now + streetPassExchangeDelayMs();
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
    nextStreetPassExchangeMs = now + streetPassExchangeDelayMs();
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

#if STACKCHAN_DEVICE_CORES3
  if (appClientConnected()) {
    streetPassAppWasConnected = true;
    if (streetPassBleReady) {
      deinitStreetPassBle("app_client");
    }
    return;
  }
  if (streetPassAppWasConnected) {
    constexpr unsigned long kStreetPassAppDisconnectGraceMs = 120000;
    streetPassAppWasConnected = false;
    streetPassResumeAfterCameraMs = now + kStreetPassAppDisconnectGraceMs;
    Serial.printf("[streetpass] app disconnected; BLE resume deferred %lums\n",
                  kStreetPassAppDisconnectGraceMs);
  }
#endif

  if (!streetPassBleReady) {
    if (streetPassResumeAfterCameraMs != 0 &&
        static_cast<int32_t>(now - streetPassResumeAfterCameraMs) < 0) {
      return;
    }
    streetPassResumeAfterCameraMs = 0;
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

  if (streetPassScanActive && now - streetPassScanStartedMs >= streetPassScanDurationMs()) {
    scan->stop();
    streetPassScanActive = false;
    const unsigned long interval = streetPassScanIdleIntervalMs();
    nextStreetPassScanMs = now + interval;
    if (displayOn) {
      restartStreetPassAdvertising();
    } else {
      stopStreetPassAdvertising("display_off_scan_done");
    }
    Serial.printf("[streetpass] scan stop next=%lums\n", interval);
  }

  if (!streetPassScanActive && now >= nextStreetPassScanMs) {
    const bool started = scan->start(0, false, false);
    if (started) {
      streetPassScanStartedMs = now;
      streetPassScanActive = true;
      Serial.println("[streetpass] scan start");
    } else {
      nextStreetPassScanMs = now + streetPassScanIdleIntervalMs();
      if (!displayOn) {
        stopStreetPassAdvertising("display_off_scan_start_failed");
      }
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
      nextStreetPassScanMs = now + streetPassScanIdleIntervalMs();
      if (displayOn) {
        restartStreetPassAdvertising();
      } else {
        stopStreetPassAdvertising("display_off_exchange");
      }
      delay(STREETPASS_CONNECT_PREPARE_MS);
    }
    exchangeStreetPassCandidate(candidate, now);
    streetPassExchangeInProgress = false;
    nextStreetPassExchangeMs = now + streetPassExchangeDelayMs();
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
  return deviceSettings.brightness;
}

#if THERMAL_FACE_EXPRESSION_ENABLED
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
#endif

void applyThermalFaceMode() {
#if THERMAL_FACE_EXPRESSION_ENABLED
  faceController.setThermalFaceMode(deviceSettings.lowPowerMode
                                      ? ThermalFaceMode::LowPower
                                      : thermalFaceModeForLevel(thermalStatus.level));
#else
  faceController.setThermalFaceMode(ThermalFaceMode::Normal);
#endif
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

void resetUsbSerialParserForDisplayOff() {
#if USB_SERIAL_PROTOCOL_ENABLED
  usbSerialRxState = UsbSerialRxState::Line;
  usbSerialLineLength = 0;
  usbSerialLineOverflow = false;
  usbSerialHeaderIndex = 0;
  usbSerialPayloadIndex = 0;
  usbSerialCrcIndex = 0;
  usbSerialMagicIndex = 0;
  usbSerialFrameLength = 0;
#endif
}

void disconnectUsbSerialForDisplayOff() {
#if USB_SERIAL_PROTOCOL_ENABLED
  if (usbSerialClientConnected || usbSerialFramedMode) {
    Serial.println("[usb] suspended for display off");
  }
  usbSerialClientConnected = false;
  usbSerialFramedMode = false;
  usbSerialLastRxMs = 0;
#if STACKCHAN_DEVICE_STOPWATCH
  usbSerialDeferredIdlePending = false;
  usbSerialDeferredIdleRequestedMs = 0;
#endif
  audioController.setUsbSerialClientConnected(false);
  resetUsbSerialParserForDisplayOff();
#endif
}

void resetInteractionStateForDisplayOff(unsigned long now) {
  const bool hadPetting = pettingActive || pettingFaceAnimated ||
                          voicePettingFaceActive || voicePettingSessionActive;
  const bool hadShake = shakeActive;

  pettingActive = false;
  pettingEndMs = 0;
  pettingStartedMs = 0;
  pettingFaceAnimated = false;
  voicePettingFaceActive = false;
  voicePettingSessionActive = false;
  nextPetMoveMs = 0;
  lastPettingRepeatEventMs = 0;

  shakeActive = false;
  shakeReturnMotionActive = false;
  shakeEndMs = 0;
  nextShakeMotionMs = 0;
  lastShakeRepeatEventMs = 0;
  shakeStrongSamples = 0;

  if (hadPetting) {
    faceController.setVoicePettingActive(false, now);
#if STACKCHAN_PET_ANIMATION_ENABLED
    faceController.setPetFaceMode(false, now, false, false);
#else
    faceController.setPetFaceMode(false);
#endif
  }
  if (hadShake) {
    faceController.setShakeFaceMode(false);
  }
  if (hadPetting || hadShake) {
    motionController.setMotion("center");
  }
}

void applyDisplayOffCpuFrequency() {
#if DISPLAY_OFF_LIGHT_SLEEP_ENABLED && DISPLAY_OFF_CPU_FREQ_MHZ > 0
  if (displayOffCpuFrequencyReduced) {
    return;
  }
  displayOnCpuFrequencyMhz = getCpuFrequencyMhz();
  if (displayOnCpuFrequencyMhz <= DISPLAY_OFF_CPU_FREQ_MHZ) {
    return;
  }
  displayOffCpuFrequencyReduced = setCpuFrequencyMhz(DISPLAY_OFF_CPU_FREQ_MHZ);
  Serial.printf("[power] display off cpu freq %lu -> %u MHz ok=%d\n",
                static_cast<unsigned long>(displayOnCpuFrequencyMhz),
                static_cast<unsigned>(DISPLAY_OFF_CPU_FREQ_MHZ),
                displayOffCpuFrequencyReduced ? 1 : 0);
#endif
}

void restoreDisplayOnCpuFrequency() {
#if DISPLAY_OFF_LIGHT_SLEEP_ENABLED && DISPLAY_OFF_CPU_FREQ_MHZ > 0
  if (!displayOffCpuFrequencyReduced) {
    return;
  }
  const uint32_t targetMhz = displayOnCpuFrequencyMhz != 0 ? displayOnCpuFrequencyMhz : 240;
  const bool ok = setCpuFrequencyMhz(targetMhz);
  Serial.printf("[power] display on cpu freq restore %lu MHz ok=%d\n",
                static_cast<unsigned long>(targetMhz),
                ok ? 1 : 0);
  displayOffCpuFrequencyReduced = false;
  displayOnCpuFrequencyMhz = 0;
#endif
}

void suspendAppCommsForDisplayOff(unsigned long now) {
  if (appCommsSuspendedForDisplayOff) {
    return;
  }
#if STACKCHAN_TIMEKEEPER_ENABLED
  prepareTimekeeperForDisplayOff(monotonicMs());
  sendCommunicationSuspending("display_off");
  // WebSocket transmission is asynchronous. Give the server a small bounded
  // flush window, but never wait for an app ACK before entering low power.
  if (appClientConnected()) {
    delay(150);
  }
  if (pendingTimekeeperAnnouncement.active) {
    pendingTimekeeperAnnouncement.sentAfterLastDeviceInfo = false;
  }
#endif
  appCommsSuspendedForDisplayOff = true;

  pendingStateAfterPlayback = false;
  deferredStateReadyMs = 0;
  pendingSpeakingFaceState = false;
  pendingFaceModeNormalAfterPlayback = false;
  deferredFaceModeReadyMs = 0;
  wsAudioSettleUntilMs = 0;
  vadActive = false;
  currentState = ChanState::Idle;
  audioController.setRemoteVadActive(false);
  audioController.stopForDisplayOff();
  speechBubbleController.reset("display_off");
  interactionMicPaused = false;
  servoMicQuietUntilMs = 0;
  motionController.setMovementPaused(false);
  faceController.setState(ChanState::Idle);
  resetInteractionStateForDisplayOff(now);
  cancelListeningNod(true);
  clearCameraButtonPending("display_off");
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  const PhoneCameraState previousPhoneCameraState = phoneCameraRemoteController.state();
  resetOverlayTouchGesture();
  if (phoneCameraRemoteController.reset()) {
    faceController.setPhoneCameraState(phoneCameraRemoteController.state());
    Serial.printf("[phone_camera] reset display_off previous=%u\n",
                  static_cast<unsigned>(previousPhoneCameraState));
  }
#endif
  slowStreetPassBleForDisplayOff(now);

  disconnectUsbSerialForDisplayOff();
  stopServers("display_off");

  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_OFF);
  wifiConnectStartedMs = 0;
  wifiConnectAttempts = 0;
  lastWifiStatus = WL_IDLE_STATUS;
  lastWifiCheckMs = now;
  streetPassNtpConfigured = false;
  applyDisplayOffCpuFrequency();
  Serial.println("[power] app comms suspended for display off");
}

void resumeAppCommsAfterDisplayOn(unsigned long now) {
  if (!appCommsSuspendedForDisplayOff) {
    return;
  }
  appCommsSuspendedForDisplayOff = false;
  wifiConnectStartedMs = 0;
  wifiConnectAttempts = 0;
  lastWifiCheckMs = 0;
  nextStreetPassNtpSyncMs = now;
  streetPassNtpConfigured = false;
  restoreDisplayOnCpuFrequency();
  resumeStreetPassBleAfterDisplayOn(now);
  connectWiFi();
  Serial.println("[power] app comms resume requested after display on");
}

void setDisplayOn(bool on) {
  if (displayOn == on) {
    return;
  }
  displayOn = on;
#if STACKCHAN_TIMEKEEPER_ENABLED
  faceController.setEnabled(on && !infoScreenVisible && !experienceModeMenuVisible &&
                            !timekeeperDurationMenuVisible && !travelFacePickerVisible);
#else
  faceController.setEnabled(on && !infoScreenVisible);
#endif
#if STACKCHAN_GURUGURU_FACE_ENABLED
  updateGuruguruFaceAvailability(millis());
#endif
  if (displayOn) {
    M5.Display.wakeup();
    applyDisplayBrightness();
    if (infoScreenVisible) {
      drawInfoScreen();
    }
    updateStepAffectionReward(millis());
    resumeAppCommsAfterDisplayOn(millis());
  } else {
    suspendAppCommsForDisplayOff(millis());
#if STACKCHAN_HAS_HAPTIC
    M5.Power.setVibration(0);
    hapticOffMs = 0;
#endif
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
  const int panJitter = random(-2, 3);
  const int tiltTarget = bigMove
                           ? random(PET_BIG_TILT_MIN, PET_BIG_TILT_MAX + 1)
                           : random(PET_SMALL_TILT_MIN, PET_SMALL_TILT_MAX + 1);
  const int tiltLift = tiltTarget - SERVO_TILT_CENTER;
  // StackChan pitch uses a 0..900 range. Limit petting to a small lift from
  // the calibrated home pose instead of allowing the current base pose to
  // accumulate into a large upward look.
  const int pettingTiltMax = constrain(
    SERVO_TILT_CENTER + static_cast<int>(map(PET_MAX_PITCH_LIFT,
                                             0,
                                             900,
                                             0,
                                             SERVO_TILT_MAX - SERVO_TILT_MIN)),
    SERVO_TILT_MIN,
    SERVO_TILT_MAX);
  return {
    constrain(base.pan + direction * amplitude + panJitter, SERVO_PAN_MIN, SERVO_PAN_MAX),
    constrain(base.tilt + tiltLift, SERVO_TILT_MIN, pettingTiltMax)
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
#if STACKCHAN_TIMEKEEPER_ENABLED
  if (active && (experienceMode == ExperienceMode::Timekeeper ||
                 experienceMode == ExperienceMode::Travel)) {
    return;
  }
#endif

  if (active) {
    pettingEndMs = now + releaseGraceMs;
    if (!pettingActive) {
      pettingActive = true;
      pettingStartedMs = now;
      pettingFaceAnimated = shouldAnimatePettingFace();
      voicePettingSessionActive = appClientConnected();
      voicePettingFaceActive = voicePettingSessionActive;
      nextPetMoveMs = 0;
      pettingBasePose = motionController.currentPose();
      cancelListeningNod(false);
#if STACKCHAN_PET_ANIMATION_ENABLED
      if (voicePettingFaceActive) {
        faceController.setVoicePettingActive(true, now);
      } else {
        faceController.setPetFaceMode(true, now, pettingFaceAnimated);
      }
#else
      faceController.setPetFaceMode(true);
#endif
      if (voicePettingSessionActive) {
        const AffectionApplyResult result = affectionController.debugAdjust(VOICE_PETTING_AFFECTION_DELTA);
        applyAffectionResult(result, now, true);
      }
      const Pose pose = makePettingPoseFromBase(pettingBasePose, false);
      motionController.setTargetPose(pose.pan, pose.tilt);
      sendInteractionEvent("petting", "start", now);
      pulseHaptic(HAPTIC_PETTING_START_LEVEL, HAPTIC_PETTING_START_MS, now);
      motionController.deferOutputUntil(now + INTERACTION_SERVO_START_DELAY_MS);
      lastPettingRepeatEventMs = now;
      Serial.println("[pet] start");
    } else if (now - lastPettingRepeatEventMs >= 800) {
#if STACKCHAN_PET_ANIMATION_ENABLED
      if (voicePettingFaceActive && !appClientConnected()) {
        voicePettingFaceActive = false;
        faceController.setVoicePettingActive(false, now);
      } else if (pettingFaceAnimated && !shouldAnimatePettingFace()) {
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

  const bool finishVoicePettingSession = voicePettingSessionActive;
#if STACKCHAN_DEVICE_STOPWATCH
  if (!finishVoicePettingSession && audioBusyForUiEffects() && displayOn) {
    pettingEndMs = now + SCREEN_PETTING_RELEASE_MS;
    return;
  }
#endif

  pettingActive = false;
  const bool longPetting = pettingStartedMs != 0 &&
                           now - pettingStartedMs >= PET_ANIMATION_LONG_THRESHOLD_MS;
  const bool animateEnd = pettingFaceAnimated && displayOn && !appClientConnected();
  const bool wasVoicePettingFace = voicePettingFaceActive;
  if (!finishVoicePettingSession) {
    if (longPetting) {
      applyAffectionResult(affectionController.applyEvent("petting", 1.0f, 1.0f, nullptr, now), now, true);
    } else {
      applyAffectionResult(affectionController.debugAdjust(PETTING_SHORT_AFFECTION_DELTA), now, true);
    }
  }
  pettingStartedMs = 0;
  pettingFaceAnimated = false;
  voicePettingFaceActive = false;
  voicePettingSessionActive = false;
  pettingEndMs = 0;
  nextPetMoveMs = 0;
  lastPettingRepeatEventMs = 0;
  // Ignore motion caused by the petting servo returning to center. Without
  // this cooldown, the device can interpret its own movement as a user shake.
  lastShakeTriggerMs = now;
  shakeStrongSamples = 0;
#if STACKCHAN_PET_ANIMATION_ENABLED
  if (wasVoicePettingFace) {
    faceController.setVoicePettingActive(false, now);
  } else {
    faceController.setPetFaceMode(false, now, animateEnd, longPetting);
  }
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

  if (!motionController.readyForInteractionTarget(now)) {
    return;
  }

  nextPetMoveMs = now + random(PET_MOVE_MIN_INTERVAL_MS, PET_MOVE_MAX_INTERVAL_MS + 1);
  const bool bigMove = random(100) < PET_BIG_MOVE_CHANCE_PERCENT;
  const Pose pose = makePettingPoseFromBase(pettingBasePose, bigMove);
  motionController.setTargetPose(pose.pan, pose.tilt);
}

bool interactionMicShouldPause(unsigned long now) {
  if (motionController.servoMotionActive(now)) {
    // Keep mechanical settling noise out of the microphone after the BSP has
    // reported that the final servo movement itself has completed.
    servoMicQuietUntilMs = now + SERVO_MIC_QUIET_AFTER_MS;
  }
  const bool servoNoiseExpected = servoMicQuietUntilMs != 0 &&
                                  static_cast<int32_t>(now - servoMicQuietUntilMs) < 0;
  if (!servoNoiseExpected) {
    servoMicQuietUntilMs = 0;
  }
  if (pettingActive || shakeActive) {
    return true;
  }
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (faceController.guruguruDizzyAnimationActive()) {
    return true;
  }
#endif
  return servoNoiseExpected;
}

void updateInteractionMicPause() {
  const bool shouldPause = interactionMicShouldPause(millis());
  if (shouldPause == interactionMicPaused) {
    return;
  }

  audioController.setInteractionMicBlocked(shouldPause, "servo/interaction noise");
  interactionMicPaused = shouldPause;
}

bool interactionMotionAllowedDuringSpeech() {
  return pettingActive || shakeActive || shakeReturnMotionActive;
}

void updateShakeReturnMotion(unsigned long now) {
  if (!shakeReturnMotionActive || motionController.servoMotionActive(now)) {
    return;
  }

  shakeReturnMotionActive = false;
  if (currentState == ChanState::Speaking) {
    motionController.setMovementPaused(true);
  }
  Serial.println("[shake] return motion complete");
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
#if STACKCHAN_TIMEKEEPER_ENABLED
  if (active && (experienceMode == ExperienceMode::Timekeeper ||
                 experienceMode == ExperienceMode::Travel)) {
    return;
  }
#endif

  if (active) {
    shakeEndMs = now + SHAKE_FACE_HOLD_MS;
    lastShakeTriggerMs = now;
    if (!shakeActive) {
      shakeActive = true;
      shakeReturnMotionActive = false;
      shakeStrongSamples = 0;
      nextShakeMotionMs = 0;
      cancelListeningNod(false);
      // A shake is an explicit user interaction, so it may move even if the
      // app starts its spoken reaction at the same time. Ordinary speech
      // remains paused by setState().
      motionController.setMovementPaused(false);
      faceController.setShakeFaceMode(true);
      motionController.setTargetPose(SERVO_PAN_CENTER + random(-10, 11), SERVO_TILT_CENTER - random(4, 11));
      applyAffectionResult(affectionController.applyEvent("shake", 1.0f, 1.0f, nullptr, now), now, true);
      sendInteractionEvent("shake", "start", now);
      pulseHaptic(HAPTIC_SHAKE_LEVEL, HAPTIC_SHAKE_MS, now);
      motionController.deferOutputUntil(now + SHAKE_SERVO_START_DELAY_MS);
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
  if (!pettingActive) {
    // Keep speech-driven motion suppression lifted only until the explicit
    // shake return has reached the calibrated center.
    motionController.setMovementPaused(false);
    shakeReturnMotionActive = true;
    motionController.setMotion("center");
  }
  sendInteractionEvent("shake", "end", now);
  Serial.println("[shake] end");
}

void updateShakeMotion(const m5::imu_data_t& data, unsigned long now) {
  if (now < nextShakeMotionMs) {
    return;
  }

  if (!motionController.readyForInteractionTarget(now)) {
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

#if STACKCHAN_TIMEKEEPER_ENABLED
  if (experienceMode == ExperienceMode::Timekeeper ||
      experienceMode == ExperienceMode::Travel) {
    if (shakeActive) {
      setShakeActive(false, now);
    }
    shakeStrongSamples = 0;
    nextShakeMotionMs = 0;
    return;
  }
#endif

  if (!shakeActive && pettingActive) {
    shakeStrongSamples = 0;
    return;
  }

  if (now < nextShakeCheckMs) {
    return;
  }
  nextShakeCheckMs = now + SHAKE_UPDATE_INTERVAL_MS;

  const bool imuUpdated = sharedImuSample.updated;
  const m5::imu_data_t& data = sharedImuSample.data;

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

#if CAMERA_DIAG_LOG_ENABLED
void logCameraRequestPhase(const char* phase, size_t bytes = 0) {
  Serial.printf("[camera.http] phase=%s bytes=%u heap=%u psram=%u largest_dma=%u wifi=%d http=%d ws=%d t=%lu\n",
                phase != nullptr ? phase : "unknown",
                static_cast<unsigned>(bytes),
                static_cast<unsigned>(ESP.getFreeHeap()),
                static_cast<unsigned>(ESP.getFreePsram()),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)),
                static_cast<int>(WiFi.status()),
                httpStarted ? 1 : 0,
                wsStarted ? 1 : 0,
                millis());
}
#endif

void handleCaptureRequest() {
#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("begin");
#endif
  faceController.setCameraCaptureActive(true);
  const bool micPausedForCapture = CAMERA_PAUSE_MIC_DURING_CAPTURE
                                     ? audioController.pauseMicForCapture()
                                     : false;
  const bool streetPassSuspendedForCapture = suspendStreetPassBleForCamera();
#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase(streetPassSuspendedForCapture ? "ble_suspended" : "ble_unchanged");
#endif
  if (VERBOSE_LOG_ENABLED && !CAMERA_PAUSE_MIC_DURING_CAPTURE && audioController.isMicStreaming()) {
    Serial.println("[camera] capture while mic streaming");
  }

  if (!cameraManager.isReady() && !cameraManager.init()) {
#if CAMERA_DIAG_LOG_ENABLED
    logCameraRequestPhase("init_failed");
#endif
    faceController.setCameraCaptureActive(false);
    audioController.resetSpeakerAfterCameraCapture();
    audioController.resumeMicAfterCapture(micPausedForCapture);
    resumeStreetPassBleAfterCamera(streetPassSuspendedForCapture);
    sendJson(503, "{\"error\":\"camera_not_ready\"}");
    return;
  }

#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("camera_ready");
#endif

  uint8_t* jpg = nullptr;
  size_t jpgLen = 0;
  if (!cameraManager.captureJpeg(&jpg, &jpgLen)) {
#if CAMERA_DIAG_LOG_ENABLED
    logCameraRequestPhase("capture_failed");
#endif
    faceController.setCameraCaptureActive(false);
    cameraManager.deinit();
    audioController.resetSpeakerAfterCameraCapture();
    audioController.resumeMicAfterCapture(micPausedForCapture);
    resumeStreetPassBleAfterCamera(streetPassSuspendedForCapture);
    sendJson(500, "{\"error\":\"capture_failed\"}");
    return;
  }

#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("captured", jpgLen);
#endif
  // The RGB565 VGA frame occupies both PSRAM and scarce internal DMA memory.
  // The JPEG buffer is independent, so release the camera before asking the
  // Wi-Fi stack to allocate TCP buffers for the response.
  cameraManager.deinit();
  audioController.resetSpeakerAfterCameraCapture();
#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("camera_released", jpgLen);
#endif
  addCorsHeaders();
  httpServer.sendHeader("Cache-Control", "no-store");
  httpServer.setContentLength(jpgLen);
  httpServer.send(200, "image/jpeg", "");
  WiFiClient client = httpServer.client();
#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("send_begin", jpgLen);
#endif
  size_t written = 0;
  while (written < jpgLen && client.connected()) {
    const size_t chunk = min(static_cast<size_t>(CAMERA_HTTP_WRITE_CHUNK_BYTES),
                             jpgLen - written);
    const size_t chunkWritten = client.write(jpg + written, chunk);
    if (chunkWritten == 0) {
      break;
    }
    written += chunkWritten;
  }
#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("send_end", written);
#endif
  if (written != jpgLen) {
    Serial.printf("[camera] HTTP JPEG send incomplete: %u/%u bytes\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(jpgLen));
  }
  cameraManager.releaseBuffer(jpg);
  faceController.setCameraCaptureActive(false);
  delay(80);
  audioController.resumeMicAfterCapture(micPausedForCapture);
  resumeStreetPassBleAfterCamera(streetPassSuspendedForCapture);
  audioController.deferNextSpeakerStartUntil(millis() + AUDIO_AFTER_CAPTURE_SPEAKER_DELAY_MS);
#if CAMERA_DIAG_LOG_ENABLED
  logCameraRequestPhase("complete", written);
#endif
}

void resetVoicePerfStatsLocked(unsigned long now) {
  ++voicePerfStats.sessionSeq;
  voicePerfStats.active = true;
  voicePerfStats.loopCount = 0;
  voicePerfStats.faceUpdateCount = 0;
  voicePerfStats.audioUpdateCount = 0;
  voicePerfStats.wsBinaryFrames = 0;
  voicePerfStats.maxLoopGapMs = 0;
  voicePerfStats.maxLoopDurationMs = 0;
  voicePerfStats.maxFaceUpdateMs = 0;
  voicePerfStats.maxAudioUpdateMs = 0;
  voicePerfStats.maxWsLoopMs = 0;
  voicePerfStats.maxHttpLoopMs = 0;
  voicePerfStats.maxWsBinaryMs = 0;
  voicePerfStats.maxWsBinaryBytes = 0;
  voicePerfStats.maxFaceFrameGapMs = 0;
  voicePerfStats.wsBinaryBytes = 0;
  voicePerfStats.firstMs = now;
  voicePerfStats.lastMs = now;
  voicePerfStats.lastLoopMs = 0;
  voicePerfStats.lastFaceUpdateMs = 0;
}

void noteVoicePerfLoop(bool speaking, unsigned long now) {
  portENTER_CRITICAL(&voicePerfMux);
  if (speaking) {
    if (!voicePerfStats.active) {
      resetVoicePerfStatsLocked(now);
    }
    if (voicePerfStats.lastLoopMs != 0) {
      const uint32_t gapMs = static_cast<uint32_t>(now - voicePerfStats.lastLoopMs);
      voicePerfStats.maxLoopGapMs = max(voicePerfStats.maxLoopGapMs, gapMs);
    }
    voicePerfStats.lastLoopMs = now;
    voicePerfStats.lastMs = now;
    ++voicePerfStats.loopCount;
  } else if (voicePerfStats.active) {
    voicePerfStats.active = false;
    voicePerfStats.lastMs = now;
  }
  portEXIT_CRITICAL(&voicePerfMux);
}

void noteVoicePerfLoopDuration(bool speaking, uint32_t elapsedMs) {
  if (!speaking) {
    return;
  }
  portENTER_CRITICAL(&voicePerfMux);
  voicePerfStats.maxLoopDurationMs = max(voicePerfStats.maxLoopDurationMs, elapsedMs);
  portEXIT_CRITICAL(&voicePerfMux);
}

void noteVoicePerfAudioUpdate(bool speaking, uint32_t elapsedMs) {
  if (!speaking) {
    return;
  }
  portENTER_CRITICAL(&voicePerfMux);
  ++voicePerfStats.audioUpdateCount;
  voicePerfStats.maxAudioUpdateMs = max(voicePerfStats.maxAudioUpdateMs, elapsedMs);
  portEXIT_CRITICAL(&voicePerfMux);
}

void noteVoicePerfFaceUpdate(bool speaking, uint32_t elapsedMs, unsigned long now) {
  if (!speaking) {
    return;
  }
  portENTER_CRITICAL(&voicePerfMux);
  ++voicePerfStats.faceUpdateCount;
  voicePerfStats.maxFaceUpdateMs = max(voicePerfStats.maxFaceUpdateMs, elapsedMs);
  if (voicePerfStats.lastFaceUpdateMs != 0) {
    const uint32_t gapMs = static_cast<uint32_t>(now - voicePerfStats.lastFaceUpdateMs);
    voicePerfStats.maxFaceFrameGapMs = max(voicePerfStats.maxFaceFrameGapMs, gapMs);
  }
  voicePerfStats.lastFaceUpdateMs = now;
  portEXIT_CRITICAL(&voicePerfMux);
}

void noteVoicePerfWsLoop(bool speaking, uint32_t elapsedMs) {
  if (!speaking) {
    return;
  }
  portENTER_CRITICAL(&voicePerfMux);
  voicePerfStats.maxWsLoopMs = max(voicePerfStats.maxWsLoopMs, elapsedMs);
  portEXIT_CRITICAL(&voicePerfMux);
}

void noteVoicePerfHttpLoop(bool speaking, uint32_t elapsedMs) {
  if (!speaking) {
    return;
  }
  portENTER_CRITICAL(&voicePerfMux);
  voicePerfStats.maxHttpLoopMs = max(voicePerfStats.maxHttpLoopMs, elapsedMs);
  portEXIT_CRITICAL(&voicePerfMux);
}

void noteVoicePerfWsBinary(size_t length, uint32_t elapsedMs) {
  portENTER_CRITICAL(&voicePerfMux);
  ++voicePerfStats.wsBinaryFrames;
  voicePerfStats.wsBinaryBytes += length;
  voicePerfStats.maxWsBinaryBytes =
    max(voicePerfStats.maxWsBinaryBytes,
        static_cast<uint32_t>(length > UINT32_MAX ? UINT32_MAX : length));
  voicePerfStats.maxWsBinaryMs = max(voicePerfStats.maxWsBinaryMs, elapsedMs);
  portEXIT_CRITICAL(&voicePerfMux);
}

VoicePerfStats snapshotVoicePerfStats() {
  portENTER_CRITICAL(&voicePerfMux);
  const VoicePerfStats snapshot = voicePerfStats;
  portEXIT_CRITICAL(&voicePerfMux);
  return snapshot;
}

void writeVoicePerfStatus(JsonObject target) {
  const VoicePerfStats stats = snapshotVoicePerfStats();
  target["active"] = stats.active;
  target["sessionSeq"] = stats.sessionSeq;
  target["durationMs"] = static_cast<uint32_t>(stats.lastMs - stats.firstMs);
  target["loopCount"] = stats.loopCount;
  target["faceUpdateCount"] = stats.faceUpdateCount;
  target["audioUpdateCount"] = stats.audioUpdateCount;
  target["wsBinaryFrames"] = stats.wsBinaryFrames;
  target["wsBinaryBytes"] =
    static_cast<uint32_t>(stats.wsBinaryBytes > UINT32_MAX ? UINT32_MAX : stats.wsBinaryBytes);
  target["maxLoopGapMs"] = stats.maxLoopGapMs;
  target["maxLoopDurationMs"] = stats.maxLoopDurationMs;
  target["maxFaceUpdateMs"] = stats.maxFaceUpdateMs;
  target["maxAudioUpdateMs"] = stats.maxAudioUpdateMs;
  target["maxWsLoopMs"] = stats.maxWsLoopMs;
  target["maxHttpLoopMs"] = stats.maxHttpLoopMs;
  target["maxWsBinaryMs"] = stats.maxWsBinaryMs;
  target["maxWsBinaryBytes"] = stats.maxWsBinaryBytes;
  target["maxFaceFrameGapMs"] = stats.maxFaceFrameGapMs;
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
  JsonObject voicePerf = doc["voicePerf"].to<JsonObject>();
  writeVoicePerfStatus(voicePerf);
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
  doc["playbackStreamId"] = audioController.playbackStreamId();
  doc["playbackPcmReceivedCursor"] = audioController.playbackPcmReceivedBytes();
  doc["playbackPcmAcceptedCursor"] = audioController.playbackPcmAcceptedBytes();
  doc["playbackPcmDequeuedCursor"] = audioController.playbackPcmDequeuedBytes();
  doc["speechBubbleActive"] = speechBubbleController.active();
  doc["speechBubbleVisible"] = faceController.speechBubbleVisible();
  JsonObject voicePerf = doc["voicePerf"].to<JsonObject>();
  writeVoicePerfStatus(voicePerf);
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

  if (!httpRoutesRegistered) {
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
    httpRoutesRegistered = true;
  }
  httpServer.begin();
  httpStarted = true;
  Serial.printf("[http] server started on port %u\n", HTTP_PORT);
}

void stopServers(const char* reason) {
  if (wsStarted) {
    wsServer.stop();
    wsStarted = false;
  }
  wsClientConnected = false;
  if (httpStarted) {
    httpServer.stop();
    httpStarted = false;
  }
  Serial.printf("[network] servers stopped reason=%s\n",
                reason != nullptr ? reason : "unknown");
}

void startServers() {
  if (!wsStarted) {
    wsServer.begin(WS_PORT);
    wsStarted = wsServer.isStarted();
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
    case SettingsPage::Steps:
      return "Steps";
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
#if !STEP_COUNTER_ENABLED
  if (page == SettingsPage::Steps) {
    return false;
  }
#endif
  return true;
}

uint8_t settingsPageCount() {
#if STACKCHAN_SMALL_DISPLAY
  uint8_t count = 4;
#else
  uint8_t count = STACKCHAN_HAS_SERVO ? 6 : 5;
#endif
#if STEP_COUNTER_ENABLED
  ++count;
#endif
  return count;
}

SettingsPage settingsPageAt(uint8_t index) {
#if STACKCHAN_SMALL_DISPLAY
  static const SettingsPage kPages[] = {
    SettingsPage::Network,
    SettingsPage::StreetPass,
    SettingsPage::Audio,
    SettingsPage::Power,
#if STEP_COUNTER_ENABLED
    SettingsPage::Steps,
#endif
  };
#elif STACKCHAN_HAS_SERVO
  static const SettingsPage kPages[] = {
    SettingsPage::Network,
    SettingsPage::Display,
    SettingsPage::Audio,
    SettingsPage::Servo,
    SettingsPage::Power,
#if STEP_COUNTER_ENABLED
    SettingsPage::Steps,
#endif
    SettingsPage::StreetPass,
  };
#else
  static const SettingsPage kPages[] = {
    SettingsPage::Network,
    SettingsPage::Display,
    SettingsPage::Audio,
    SettingsPage::Power,
#if STEP_COUNTER_ENABLED
    SettingsPage::Steps,
#endif
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
#if STEP_COUNTER_ENABLED
  if (settingsPage != SettingsPage::Steps) {
    stepHistoryPage = 0;
  }
#endif
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

void drawUtf8Clipped(int32_t x, int32_t y, int32_t w, int32_t h, const String& text,
                     uint16_t color = TFT_WHITE, uint16_t background = TFT_BLACK) {
  String displayText = Utf8Utils::normalizeHalfwidthKana(text);
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
      const uint32_t cp = Utf8Utils::readCodepoint(displayText, i);
      String candidate = shortened;
      Utf8Utils::appendCodepoint(candidate, cp);
      candidate += suffix;
      if (before > 0 && M5.Display.textWidth(candidate) > w) {
        break;
      }
      if (before == 0 && M5.Display.textWidth(candidate) > w) {
        shortened = suffixWidth <= w ? suffix : "";
        break;
      }
      Utf8Utils::appendCodepoint(shortened, cp);
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

bool touchStartedInCircle(const m5::touch_detail_t& touch, int32_t cx, int32_t cy, int32_t radius) {
  const int32_t dx = touch.base_x - cx;
  const int32_t dy = touch.base_y - cy;
  return dx * dx + dy * dy <= radius * radius;
}

int32_t roundMicButtonCenterX() {
  return roundCenterX() - min(M5.Display.width(), M5.Display.height()) * 36 / 100;
}

#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
int32_t roundPhoneCameraButtonCenterX() {
  return roundCenterX() + min(M5.Display.width(), M5.Display.height()) * 36 / 100;
}

int32_t roundPhoneCameraButtonCenterY() {
  return roundCenterY() + min(M5.Display.width(), M5.Display.height()) * 17 / 100;
}

int32_t roundPhoneCameraButtonRadius() {
  return STOPWATCH_OVERLAY_BUTTON_TOUCH_RADIUS_PX;
}
#endif

int32_t roundMicButtonCenterY() {
  return roundCenterY() + min(M5.Display.width(), M5.Display.height()) * 17 / 100;
}

int32_t roundMicButtonRadius() {
  return STOPWATCH_OVERLAY_BUTTON_TOUCH_RADIUS_PX;
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

#if STEP_COUNTER_ENABLED
String stepActivityDayLabel(uint32_t activityDay) {
  if (activityDay == 0) {
    return "--/--";
  }

  constexpr uint32_t kSecondsPerDay = 24UL * 60UL * 60UL;
  time_t raw = static_cast<time_t>(activityDay * kSecondsPerDay);
  tm date = {};
  gmtime_r(&raw, &date);

  char text[6] = {};
  snprintf(text, sizeof(text), "%02d/%02d", date.tm_mon + 1, date.tm_mday);
  return String(text);
}

constexpr uint8_t kStepHistoryRowsPerPage = 7;

bool stepHistoryRecordVisible(const StepDayRecord& record) {
  if (record.activityDay == 0) {
    return false;
  }
  return !stepCounterController.todayValid() ||
         record.activityDay != stepCounterController.currentActivityDay();
}

uint8_t stepHistoryVisibleCount() {
  uint8_t visible = 0;
  const uint8_t count = stepCounterController.historyCount();
  for (uint8_t i = 0; i < count; ++i) {
    const StepDayRecord* record = stepCounterController.recordAt(i);
    if (record != nullptr && stepHistoryRecordVisible(*record)) {
      ++visible;
    }
  }
  return visible;
}

uint8_t stepHistoryPageCount() {
  const uint8_t visible = stepHistoryVisibleCount();
  if (visible == 0) {
    return 1;
  }
  return (visible + kStepHistoryRowsPerPage - 1) / kStepHistoryRowsPerPage;
}

void clampStepHistoryPage() {
  const uint8_t pageCount = stepHistoryPageCount();
  if (stepHistoryPage >= pageCount) {
    stepHistoryPage = pageCount - 1;
  }
}

const StepDayRecord* newestStepRecordBefore(uint32_t beforeActivityDay) {
  const StepDayRecord* best = nullptr;
  const uint8_t count = stepCounterController.historyCount();
  for (uint8_t i = 0; i < count; ++i) {
    const StepDayRecord* record = stepCounterController.recordAt(i);
    if (record == nullptr || !stepHistoryRecordVisible(*record) ||
        record->activityDay >= beforeActivityDay) {
      continue;
    }
    if (best == nullptr || record->activityDay > best->activityDay) {
      best = record;
    }
  }
  return best;
}

const StepDayRecord* stepHistoryRecordAtVisibleOffset(uint8_t offset) {
  uint32_t beforeActivityDay = UINT32_MAX;
  const StepDayRecord* record = nullptr;
  for (uint8_t i = 0; i <= offset; ++i) {
    record = newestStepRecordBefore(beforeActivityDay);
    if (record == nullptr) {
      return nullptr;
    }
    beforeActivityDay = record->activityDay;
  }
  return record;
}

String stepHistoryLine(const StepDayRecord& record) {
  String line = stepActivityDayLabel(record.activityDay);
  line += "  ";
  line += String(record.steps);
  return line;
}

String stepResetSummary() {
  char text[32] = {};
  snprintf(text,
           sizeof(text),
           "Reset %02d:00  Saved %u/%u",
           STEP_COUNTER_DAY_START_HOUR,
           static_cast<unsigned>(stepCounterController.historyCount()),
           static_cast<unsigned>(StepCounterController::kHistoryDays));
  return String(text);
}

String stepHistoryHeading() {
  const uint8_t visible = stepHistoryVisibleCount();
  const uint8_t pageCount = stepHistoryPageCount();
  if (visible <= kStepHistoryRowsPerPage) {
    return "Past days";
  }
  return String("Past days ") + String(stepHistoryPage + 1) + "/" + String(pageCount);
}

bool advanceStepHistoryPage(int delta) {
  clampStepHistoryPage();
  const uint8_t pageCount = stepHistoryPageCount();
  if (pageCount <= 1) {
    return false;
  }

  int next = static_cast<int>(stepHistoryPage) + delta;
  if (next < 0) {
    next = 0;
  } else if (next >= pageCount) {
    next = pageCount - 1;
  }
  if (next == stepHistoryPage) {
    return true;
  }
  stepHistoryPage = static_cast<uint8_t>(next);
  drawInfoScreen();
  return true;
}

void drawRoundStepsSettingsPage() {
  const uint16_t accent = M5.Display.color565(126, 226, 248);
  const uint16_t muted = M5.Display.color565(178, 188, 196);
  const uint16_t warning = M5.Display.color565(255, 205, 90);

  clampStepHistoryPage();
  drawRoundTextLine(108, String("Today: ") + String(stepCounterController.todaySteps()), accent);
  drawRoundTextLine(132, stepResetSummary(), muted);
  const int32_t headingY = stepCounterController.todayValid() ? 166 : 156;
  if (!stepCounterController.todayValid()) {
    drawRoundTextLine(156, "Waiting for clock", warning);
  }
  drawRoundTextLine(headingY + 28, stepHistoryHeading(), M5.Display.color565(255, 220, 90));

  const uint8_t visible = stepHistoryVisibleCount();
  const uint8_t startOffset = stepHistoryPage * kStepHistoryRowsPerPage;
  for (uint8_t row = 0; row < kStepHistoryRowsPerPage; ++row) {
    if (startOffset + row >= visible) {
      break;
    }
    const StepDayRecord* record = stepHistoryRecordAtVisibleOffset(startOffset + row);
    if (record == nullptr) {
      break;
    }
    drawRoundTextLine(headingY + 54 + static_cast<int32_t>(row) * 24,
                      stepHistoryLine(*record),
                      record->steps > 0 ? TFT_WHITE : muted);
  }

  if (visible == 0) {
    drawRoundTextLine(headingY + 54, "No past history yet", muted);
  }
}
#endif

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
#if STEP_COUNTER_ENABLED
    case SettingsPage::Steps:
      drawRoundStepsSettingsPage();
      break;
#endif
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

#if STEP_COUNTER_ENABLED
void drawStepsSettingsPage() {
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(20, 56);
  M5.Display.printf("Today: %lu\n", static_cast<unsigned long>(stepCounterController.todaySteps()));
  M5.Display.printf("Reset: %02d:00\n", STEP_COUNTER_DAY_START_HOUR);
  M5.Display.printf("Saved: %u/%u\n\n",
                    static_cast<unsigned>(stepCounterController.historyCount()),
                    static_cast<unsigned>(StepCounterController::kHistoryDays));
  if (!stepCounterController.todayValid()) {
    M5.Display.println("Waiting for clock");
  }
  M5.Display.println("Past");

  uint32_t beforeActivityDay = UINT32_MAX;
  bool anyRecord = false;
  for (uint8_t row = 0; row < 5; ++row) {
    const StepDayRecord* record = newestStepRecordBefore(beforeActivityDay);
    if (record == nullptr) {
      break;
    }
    beforeActivityDay = record->activityDay;
    anyRecord = true;
    M5.Display.printf("%s  %lu\n",
                      stepActivityDayLabel(record->activityDay).c_str(),
                      static_cast<unsigned long>(record->steps));
  }
  if (!anyRecord) {
    M5.Display.println("No past history yet");
  }
}
#endif

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

#if STEP_COUNTER_ENABLED
void drawSmallStepsSettingsPage() {
  drawSmallPageHeader("Steps", M5.Display.color565(126, 226, 248));
  drawSmallTextLine(31, String("Today: ") + String(stepCounterController.todaySteps()));
  drawSmallTextLine(47, stepResetSummary(), M5.Display.color565(178, 188, 196));
  if (!stepCounterController.todayValid()) {
    drawSmallTextLine(63, "Waiting for clock", M5.Display.color565(255, 205, 90));
  }

  uint32_t beforeActivityDay = UINT32_MAX;
  bool anyRecord = false;
  for (uint8_t row = 0; row < 3; ++row) {
    const StepDayRecord* record = newestStepRecordBefore(beforeActivityDay);
    if (record == nullptr) {
      break;
    }
    beforeActivityDay = record->activityDay;
    anyRecord = true;
    drawSmallTextLine(83 + static_cast<int32_t>(row) * 16,
                      stepHistoryLine(*record),
                      record->steps > 0 ? TFT_WHITE : M5.Display.color565(178, 188, 196));
  }
  if (!anyRecord) {
    drawSmallTextLine(83, "No past history yet", M5.Display.color565(178, 188, 196));
  }
  drawSmallPageIndicator();
}
#endif

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
#if STEP_COUNTER_ENABLED
    case SettingsPage::Steps:
      drawSmallStepsSettingsPage();
      break;
#endif
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
#if STEP_COUNTER_ENABLED
    case SettingsPage::Steps:
      drawStepsSettingsPage();
      break;
#endif
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
#if STEP_COUNTER_ENABLED
  if (settingsPage == SettingsPage::Steps) {
    return true;
  }
#endif
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
#if STACKCHAN_TIMEKEEPER_ENABLED
  if (visible) {
    travelFacePickerVisible = false;
  }
  faceController.setEnabled(displayOn && !visible && !experienceModeMenuVisible &&
                            !timekeeperDurationMenuVisible && !travelFacePickerVisible);
#else
  faceController.setEnabled(displayOn && !visible);
#endif
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

struct OverlayTouchBounds {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

bool overlayPointInBounds(int32_t x, int32_t y, const OverlayTouchBounds& bounds, int32_t slop = 0) {
  return x >= bounds.x - slop && x < bounds.x + bounds.w + slop &&
         y >= bounds.y - slop && y < bounds.y + bounds.h + slop;
}

OverlayTouchBounds coreCameraButtonTouchBounds() {
  return {
    M5.Display.width() - CORES3_OVERLAY_BUTTON_TOUCH_WIDTH_PX,
    M5.Display.height() - 148,
    CORES3_OVERLAY_BUTTON_TOUCH_WIDTH_PX,
    74,
  };
}

OverlayTouchBounds coreMicButtonTouchBounds() {
  return {
    M5.Display.width() - CORES3_OVERLAY_BUTTON_TOUCH_WIDTH_PX,
    M5.Display.height() - 74,
    CORES3_OVERLAY_BUTTON_TOUCH_WIDTH_PX,
    74,
  };
}

void resetOverlayTouchGesture() {
  overlayTouchGesture = OverlayTouchGesture();
}

OverlayTouchTarget overlayTouchTargetAtStart(const m5::touch_detail_t& touch) {
  if (infoScreenVisible || experienceMode != ExperienceMode::Conversation) {
    return OverlayTouchTarget::None;
  }

#if STACKCHAN_DEVICE_STOPWATCH
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  if ((phoneCameraRemoteController.canRequest() || phoneCameraRemoteController.canSwitchLens()) &&
      touchStartedInCircle(touch,
                           roundPhoneCameraButtonCenterX(),
                           roundPhoneCameraButtonCenterY(),
                           roundPhoneCameraButtonRadius())) {
    return OverlayTouchTarget::PhoneCamera;
  }
#endif
  if (appClientConnected() &&
      touchStartedInCircle(touch,
                           roundMicButtonCenterX(),
                           roundMicButtonCenterY(),
                           roundMicButtonRadius())) {
    return OverlayTouchTarget::Microphone;
  }
#endif

#if STACKCHAN_DEVICE_CORES3
  if (appClientConnected() &&
      overlayPointInBounds(touch.base_x, touch.base_y, coreCameraButtonTouchBounds())) {
    return OverlayTouchTarget::DeviceCamera;
  }
  if (appClientConnected() &&
      overlayPointInBounds(touch.base_x, touch.base_y, coreMicButtonTouchBounds())) {
    return OverlayTouchTarget::Microphone;
  }
#endif

  return OverlayTouchTarget::None;
}

bool overlayTouchReleasedNearTarget(OverlayTouchTarget target,
                                    const m5::touch_detail_t& touch,
                                    int32_t slop) {
  switch (target) {
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
    case OverlayTouchTarget::PhoneCamera:
      return touchInCircle(touch,
                           roundPhoneCameraButtonCenterX(),
                           roundPhoneCameraButtonCenterY(),
                           roundPhoneCameraButtonRadius() + slop);
#endif
#if STACKCHAN_DEVICE_STOPWATCH
    case OverlayTouchTarget::Microphone:
      return touchInCircle(touch,
                           roundMicButtonCenterX(),
                           roundMicButtonCenterY(),
                           roundMicButtonRadius() + slop);
#elif STACKCHAN_DEVICE_CORES3
    case OverlayTouchTarget::Microphone:
      return overlayPointInBounds(touch.x, touch.y, coreMicButtonTouchBounds(), slop);
#endif
#if STACKCHAN_DEVICE_CORES3
    case OverlayTouchTarget::DeviceCamera:
      return overlayPointInBounds(touch.x, touch.y, coreCameraButtonTouchBounds(), slop);
#endif
    case OverlayTouchTarget::None:
    default:
      return false;
  }
}

void updateOverlayTouchTravel(const m5::touch_detail_t& touch) {
  const int32_t dx = touch.x - overlayTouchGesture.startX;
  const int32_t dy = touch.y - overlayTouchGesture.startY;
  const uint32_t travelSquared = static_cast<uint32_t>(dx * dx + dy * dy);
  if (travelSquared > overlayTouchGesture.maxTravelSquared) {
    overlayTouchGesture.maxTravelSquared = travelSquared;
  }
}

bool updateOverlayButtonTouch(unsigned long now, const m5::touch_detail_t& touch) {
  if (overlayTouchGesture.target == OverlayTouchTarget::None) {
    if (!touch.wasPressed()) {
      return false;
    }
    const OverlayTouchTarget target = overlayTouchTargetAtStart(touch);
    if (target == OverlayTouchTarget::None) {
      return false;
    }
    overlayTouchGesture.target = target;
    overlayTouchGesture.startedMs = now;
    overlayTouchGesture.startX = touch.base_x;
    overlayTouchGesture.startY = touch.base_y;
    overlayTouchGesture.maxTravelSquared = 0;
    overlayTouchGesture.longPressHandled = false;
    return true;
  }

  updateOverlayTouchTravel(touch);
  const unsigned long pressedMs = now - overlayTouchGesture.startedMs;

#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  if (overlayTouchGesture.target == OverlayTouchTarget::PhoneCamera &&
      !overlayTouchGesture.longPressHandled &&
      touch.isPressed() &&
      pressedMs >= PHONE_CAMERA_LONG_PRESS_MS) {
    const uint32_t longMoveLimitSquared =
      static_cast<uint32_t>(PHONE_CAMERA_LONG_PRESS_MOVE_PX * PHONE_CAMERA_LONG_PRESS_MOVE_PX);
    if (overlayTouchGesture.maxTravelSquared <= longMoveLimitSquared &&
        overlayTouchReleasedNearTarget(overlayTouchGesture.target,
                                       touch,
                                       OVERLAY_BUTTON_RELEASE_SLOP_PX) &&
        phoneCameraRemoteController.canSwitchLens()) {
      overlayTouchGesture.longPressHandled = true;
      sendPhoneCameraLensRequest(now);
    }
  }
#endif

  if (touch.isPressed()) {
    return true;
  }

  if (!touch.wasReleased()) {
    resetOverlayTouchGesture();
    return true;
  }

  const OverlayTouchTarget target = overlayTouchGesture.target;
  const bool longPressHandled = overlayTouchGesture.longPressHandled;
  const uint32_t maxTravelSquared = overlayTouchGesture.maxTravelSquared;
  const bool releasedNearTarget =
    overlayTouchReleasedNearTarget(target, touch, OVERLAY_BUTTON_RELEASE_SLOP_PX);
  resetOverlayTouchGesture();

  if (longPressHandled) {
    return true;
  }

#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  if (target == OverlayTouchTarget::PhoneCamera && pressedMs >= PHONE_CAMERA_LONG_PRESS_MS) {
    const uint32_t longMoveLimitSquared =
      static_cast<uint32_t>(PHONE_CAMERA_LONG_PRESS_MOVE_PX * PHONE_CAMERA_LONG_PRESS_MOVE_PX);
    if (maxTravelSquared <= longMoveLimitSquared && releasedNearTarget &&
        phoneCameraRemoteController.canSwitchLens()) {
      sendPhoneCameraLensRequest(now);
    }
    return true;
  }
#endif

  const uint32_t moveLimitSquared =
    static_cast<uint32_t>(OVERLAY_BUTTON_MOVE_LIMIT_PX * OVERLAY_BUTTON_MOVE_LIMIT_PX);
  if (pressedMs >= OVERLAY_BUTTON_SHORT_PRESS_MAX_MS ||
      maxTravelSquared > moveLimitSquared ||
      !releasedNearTarget) {
    return true;
  }

  switch (target) {
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
    case OverlayTouchTarget::PhoneCamera:
      if (phoneCameraRemoteController.canRequest()) {
        sendPhoneCameraShutterRequest(now);
      }
      break;
#endif
#if STACKCHAN_DEVICE_CORES3
    case OverlayTouchTarget::DeviceCamera:
      if (appClientConnected()) {
        sendCameraButtonEvent(now);
      }
      break;
#endif
    case OverlayTouchTarget::Microphone:
      if (appClientConnected()) {
        audioController.setMicMuted(!audioController.micMuted());
        updateMicStatusOverlay();
      }
      break;
    case OverlayTouchTarget::None:
    default:
      break;
  }
  return true;
}

bool isLeftEdgeModeSwipe(const m5::touch_detail_t& touch) {
  if (!touch.wasFlicked()) {
    return false;
  }
  constexpr int32_t edge = 44;
  constexpr int32_t minimumTravel = 56;
  return touch.base_x <= edge && touch.distanceX() >= minimumTravel &&
         abs(touch.distanceX()) > abs(touch.distanceY());
}

bool isRightEdgeSettingsSwipe(const m5::touch_detail_t& touch) {
  if (!touch.wasFlicked()) {
    return false;
  }
  constexpr int32_t edge = 44;
  constexpr int32_t minimumTravel = 56;
  return touch.base_x >= M5.Display.width() - edge &&
         touch.distanceX() <= -minimumTravel &&
         abs(touch.distanceX()) > abs(touch.distanceY());
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
#if STEP_COUNTER_ENABLED
    if (settingsPage == SettingsPage::Steps &&
        abs(touch.distanceY()) > abs(touch.distanceX()) &&
        abs(touch.distanceY()) > 24) {
      return advanceStepHistoryPage(touch.distanceY() < 0 ? 1 : -1);
    }
#endif
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
    if (state == ChanState::Speaking) {
      motionController.setMovementPaused(!interactionMotionAllowedDuringSpeech());
      const bool wasDraining = audioController.isPlaybackDraining();
      const bool wasOutOfSync = audioController.state() != ChanState::Speaking;
      if (wasDraining || wasOutOfSync) {
        pendingStateAfterPlayback = false;
        deferredStateReadyMs = 0;
        pendingSpeakingFaceState = true;
        wsAudioSettleUntilMs = 0;
#if STACKCHAN_ROUND_DISPLAY
        if (wasOutOfSync) {
          faceController.prepareSpeakingCache(displayAuthFaceMode(currentAuthFaceMode));
        }
#endif
        audioController.setState(ChanState::Speaking);
        applyDisplayBrightness();
        Serial.printf("[state] speaking resynced draining=%d audio=%d\n",
                      wasDraining ? 1 : 0,
                      static_cast<int>(audioController.state()));
      }
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
  motionController.setMovementPaused(
    state == ChanState::Speaking && !interactionMotionAllowedDuringSpeech());
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

uint8_t voiceMouthLevelFromPlaybackPeak(unsigned long now) {
  const unsigned long peakMs = audioController.playbackPeakMs();
  if (peakMs == 0) {
    return 0;
  }
  const unsigned long peakAgeMs = now >= peakMs ? now - peakMs : 0;
  if (peakAgeMs > VOICE_SPRITE_MOUTH_HOLD_MS) {
    return 0;
  }

  const int32_t peak = audioController.playbackPeak();
  if (peak <= VOICE_SPRITE_PEAK_SILENCE) {
    return 0;
  }
  if (peak >= VOICE_SPRITE_PEAK_FULL_OPEN) {
    return static_cast<uint8_t>(VOICE_SPRITE_MOUTH_FRAME_COUNT - 1);
  }

  const int32_t range = VOICE_SPRITE_PEAK_FULL_OPEN - VOICE_SPRITE_PEAK_SILENCE;
  if (range <= 0) {
    return peak > VOICE_SPRITE_PEAK_SILENCE ? 1 : 0;
  }

  const int32_t scaled =
    ((peak - VOICE_SPRITE_PEAK_SILENCE) * (VOICE_SPRITE_MOUTH_FRAME_COUNT - 2)) / range;
  const int32_t level = constrain(1 + scaled,
                                  1,
                                  VOICE_SPRITE_MOUTH_FRAME_COUNT - 1);
  return static_cast<uint8_t>(level);
}

#if STACKCHAN_CLASSIC_FACE_ENABLED
uint8_t classicMouthLevelFromPlaybackEnvelope(unsigned long now) {
  const unsigned long envelopeMs = audioController.playbackEnvelopeMs();
  const int32_t envelope = audioController.playbackEnvelope();
  const unsigned long envelopeAgeMs = envelopeMs == 0
                                        ? ULONG_MAX
                                        : (now >= envelopeMs ? now - envelopeMs : 0);
  uint8_t level = 0;
  if (envelopeAgeMs <= CLASSIC_FACE_MOUTH_HOLD_MS &&
      envelope > CLASSIC_FACE_MOUTH_ENVELOPE_SILENCE) {
    if (envelope >= CLASSIC_FACE_MOUTH_ENVELOPE_FULL_OPEN) {
      level = static_cast<uint8_t>(CLASSIC_FACE_MOUTH_LEVEL_COUNT - 1);
    } else {
      const int32_t range = CLASSIC_FACE_MOUTH_ENVELOPE_FULL_OPEN -
                            CLASSIC_FACE_MOUTH_ENVELOPE_SILENCE;
      const float normalized = static_cast<float>(envelope - CLASSIC_FACE_MOUTH_ENVELOPE_SILENCE) /
                               static_cast<float>(range);
      const int32_t scaled = static_cast<int32_t>(
        sqrtf(constrain(normalized, 0.0f, 1.0f)) *
          (CLASSIC_FACE_MOUTH_LEVEL_COUNT - 1 - CLASSIC_FACE_MOUTH_MIN_ACTIVE_LEVEL) +
        0.5f);
      level = static_cast<uint8_t>(constrain(CLASSIC_FACE_MOUTH_MIN_ACTIVE_LEVEL + scaled,
                                             CLASSIC_FACE_MOUTH_MIN_ACTIVE_LEVEL,
                                             CLASSIC_FACE_MOUTH_LEVEL_COUNT - 1));
    }
  }
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  static unsigned long lastLoggedEnvelopeMs = ULONG_MAX;
  static uint8_t lastLoggedLevel = 0xff;
  const bool stale = envelopeAgeMs > CLASSIC_FACE_MOUTH_HOLD_MS;
  static bool lastLoggedStale = false;
  if (envelopeMs != lastLoggedEnvelopeMs || level != lastLoggedLevel || stale != lastLoggedStale) {
    Serial.printf("[lip.map] t=%lu envelope_t=%lu age=%lu envelope=%ld level=%u stale=%d\n",
                  now,
                  envelopeMs,
                  envelopeAgeMs,
                  static_cast<long>(envelope),
                  static_cast<unsigned>(level),
                  stale ? 1 : 0);
    lastLoggedEnvelopeMs = envelopeMs;
    lastLoggedLevel = level;
    lastLoggedStale = stale;
  }
#endif
  return level;
}
#endif

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
  motionController.setMovementPaused(false);
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
  if (strcmp(value, "listening") == 0 && experienceMode != ExperienceMode::Conversation) {
    Serial.printf("[state] listening suppressed mode=%s\n", experienceModeName(experienceMode));
    return;
  }
  if (strcmp(value, "speaking") == 0 &&
      (experienceMode == ExperienceMode::Guruguru ||
       experienceMode == ExperienceMode::Travel)) {
    Serial.printf("[state] speaking suppressed mode=%s\n", experienceModeName(experienceMode));
    return;
  }
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
    const bool sent = sendUsbSerialFrame(Usb::kTypeJson,
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

#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
PhoneCameraTransport phoneCameraTransportFromCommand(SpeechBubbleTransport transport) {
  if (transport == SpeechBubbleTransport::WebSocket) {
    return PhoneCameraTransport::WebSocket;
  }
  if (transport == SpeechBubbleTransport::UsbSerial) {
    return PhoneCameraTransport::UsbSerial;
  }
  return PhoneCameraTransport::None;
}

const char* phoneCameraTransportName(PhoneCameraTransport transport) {
  switch (transport) {
    case PhoneCameraTransport::WebSocket:
      return "websocket";
    case PhoneCameraTransport::UsbSerial:
      return "usb_serial";
    case PhoneCameraTransport::None:
    default:
      return "none";
  }
}

const char* phoneCameraStateName(PhoneCameraState state) {
  switch (state) {
    case PhoneCameraState::Ready:
      return "ready";
    case PhoneCameraState::Pending:
      return "pending";
    case PhoneCameraState::Success:
      return "success";
    case PhoneCameraState::Failure:
      return "failure";
    case PhoneCameraState::Unavailable:
    default:
      return "unavailable";
  }
}

const char* phoneCameraLensName(PhoneCameraLens lens) {
  switch (lens) {
    case PhoneCameraLens::Front:
      return "front";
    case PhoneCameraLens::Back:
      return "back";
    case PhoneCameraLens::Unknown:
    default:
      return "unknown";
  }
}

PhoneCameraLens parsePhoneCameraLens(const char* lens) {
  if (lens != nullptr && strcmp(lens, "front") == 0) {
    return PhoneCameraLens::Front;
  }
  if (lens != nullptr && strcmp(lens, "back") == 0) {
    return PhoneCameraLens::Back;
  }
  return PhoneCameraLens::Unknown;
}

void applyPhoneCameraPresentation(PhoneCameraState previousState, unsigned long now) {
  const PhoneCameraState state = phoneCameraRemoteController.state();
  faceController.setPhoneCameraState(state,
                                     phoneCameraRemoteController.lens(),
                                     phoneCameraRemoteController.pendingOperation());
  if (state != previousState) {
    if (state == PhoneCameraState::Success) {
      pulseHaptic(PHONE_CAMERA_HAPTIC_SUCCESS_LEVEL,
                  PHONE_CAMERA_HAPTIC_SUCCESS_MS,
                  now);
    } else if (state == PhoneCameraState::Failure) {
      pulseHaptic(PHONE_CAMERA_HAPTIC_FAILURE_LEVEL,
                  PHONE_CAMERA_HAPTIC_FAILURE_MS,
                  now);
    }
    Serial.printf("[phone_camera] state %s -> %s ready=%s pending=%s\n",
                  phoneCameraStateName(previousState),
                  phoneCameraStateName(state),
                  phoneCameraTransportName(phoneCameraRemoteController.readyTransport()),
                  phoneCameraTransportName(phoneCameraRemoteController.pendingTransport()));
  }
}

bool sendPhoneCameraJson(PhoneCameraTransport transport, const char* payload) {
  if (payload == nullptr) {
    return false;
  }
  if (transport == PhoneCameraTransport::WebSocket) {
    return wsServer.hasClient() && wsServer.sendText(payload);
  }
  if (transport == PhoneCameraTransport::UsbSerial) {
    return sendUsbSerialJson(payload);
  }
  return false;
}

bool sendPhoneCameraShutterRequest(unsigned long now) {
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  if (!phoneCameraRemoteController.beginShutterRequest(now)) {
    return false;
  }

  const PhoneCameraTransport transport = phoneCameraRemoteController.pendingTransport();
  const String requestId(phoneCameraRemoteController.pendingRequestId());
  JsonDocument request;
  request["type"] = "phone_camera.shutter.request";
  request["version"] = PHONE_CAMERA_PROTOCOL_VERSION;
  request["requestId"] = requestId;
  request["mode"] = "photo";

  String body;
  serializeJson(request, body);
  if (!sendPhoneCameraJson(transport, body.c_str())) {
    phoneCameraRemoteController.disconnectTransport(transport);
    applyPhoneCameraPresentation(previousState, now);
    Serial.printf("[phone_camera] request send failed id=%s transport=%s\n",
                  requestId.c_str(),
                  phoneCameraTransportName(transport));
    return false;
  }

  applyPhoneCameraPresentation(previousState, now);
  Serial.printf("[phone_camera] request sent id=%s transport=%s\n",
                requestId.c_str(),
                phoneCameraTransportName(transport));
  return true;
}

bool sendPhoneCameraLensRequest(unsigned long now) {
  const PhoneCameraLens currentLens = phoneCameraRemoteController.lens();
  const PhoneCameraLens requestedLens =
    currentLens == PhoneCameraLens::Front
      ? PhoneCameraLens::Back
      : PhoneCameraLens::Front;
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  if (!phoneCameraRemoteController.beginLensRequest(requestedLens, now)) {
    return false;
  }

  const PhoneCameraTransport transport = phoneCameraRemoteController.pendingTransport();
  const String requestId(phoneCameraRemoteController.pendingRequestId());
  JsonDocument request;
  request["type"] = "phone_camera.lens.set.request";
  request["version"] = PHONE_CAMERA_PROTOCOL_VERSION;
  request["requestId"] = requestId;
  request["lens"] = phoneCameraLensName(requestedLens);

  String body;
  serializeJson(request, body);
  if (!sendPhoneCameraJson(transport, body.c_str())) {
    phoneCameraRemoteController.disconnectTransport(transport);
    applyPhoneCameraPresentation(previousState, now);
    Serial.printf("[phone_camera] lens request send failed id=%s lens=%s transport=%s\n",
                  requestId.c_str(),
                  phoneCameraLensName(requestedLens),
                  phoneCameraTransportName(transport));
    return false;
  }

  applyPhoneCameraPresentation(previousState, now);
  Serial.printf("[phone_camera] lens request sent id=%s lens=%s transport=%s\n",
                requestId.c_str(),
                phoneCameraLensName(requestedLens),
                phoneCameraTransportName(transport));
  return true;
}

void handlePhoneCameraStateCommand(JsonDocument& doc, SpeechBubbleTransport commandTransport) {
  const uint32_t version = doc["version"] | 0U;
  if (version != PHONE_CAMERA_PROTOCOL_VERSION) {
    Serial.printf("[phone_camera] state ignored: unsupported version=%lu\n",
                  static_cast<unsigned long>(version));
    return;
  }
  if (!doc["ready"].is<bool>()) {
    Serial.println("[phone_camera] state ignored: missing ready");
    return;
  }
  const PhoneCameraTransport transport = phoneCameraTransportFromCommand(commandTransport);
  if (transport == PhoneCameraTransport::None) {
    return;
  }

  const bool available = doc["available"] | true;
  const bool ready = available && doc["ready"].as<bool>();
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  const bool readyChanged = phoneCameraRemoteController.setReady(transport, ready, millis());

  bool lensChanged = false;
  bool frontSupported = false;
  bool backSupported = false;
  const PhoneCameraLens lens = parsePhoneCameraLens(doc["lens"] | "");
  if (ready && lens != PhoneCameraLens::Unknown &&
      doc["supportedLenses"].is<JsonArrayConst>()) {
    for (JsonVariantConst item : doc["supportedLenses"].as<JsonArrayConst>()) {
      const PhoneCameraLens supportedLens = parsePhoneCameraLens(item.as<const char*>());
      frontSupported = frontSupported || supportedLens == PhoneCameraLens::Front;
      backSupported = backSupported || supportedLens == PhoneCameraLens::Back;
    }
    lensChanged = phoneCameraRemoteController.setLensInfo(transport,
                                                           lens,
                                                           frontSupported,
                                                           backSupported);
  }

  if (readyChanged || lensChanged) {
    applyPhoneCameraPresentation(previousState, millis());
  }
  Serial.printf("[phone_camera] ready=%d lens=%s front=%d back=%d transport=%s changed=%d\n",
                ready ? 1 : 0,
                phoneCameraLensName(phoneCameraRemoteController.lens()),
                frontSupported ? 1 : 0,
                backSupported ? 1 : 0,
                phoneCameraTransportName(transport),
                (readyChanged || lensChanged) ? 1 : 0);
}

void handlePhoneCameraResultCommand(JsonDocument& doc, SpeechBubbleTransport commandTransport) {
  const uint32_t version = doc["version"] | 0U;
  if (version != PHONE_CAMERA_PROTOCOL_VERSION) {
    Serial.printf("[phone_camera] result ignored: unsupported version=%lu\n",
                  static_cast<unsigned long>(version));
    return;
  }
  const char* requestId = doc["requestId"] | "";
  const char* status = doc["status"] | "";
  const bool success = strcmp(status, "captured") == 0;
  if (requestId[0] == '\0' || status[0] == '\0') {
    Serial.printf("[phone_camera] result ignored id=%s status=%s\n", requestId, status);
    return;
  }

  const PhoneCameraTransport transport = phoneCameraTransportFromCommand(commandTransport);
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  const bool accepted = phoneCameraRemoteController.completeShutterRequest(transport,
                                                                            requestId,
                                                                            success,
                                                                            millis());
  if (accepted) {
    applyPhoneCameraPresentation(previousState, millis());
  }
  Serial.printf("[phone_camera] result id=%s status=%s transport=%s accepted=%d\n",
                requestId,
                status,
                phoneCameraTransportName(transport),
                accepted ? 1 : 0);
}

void handlePhoneCameraLensResultCommand(JsonDocument& doc,
                                        SpeechBubbleTransport commandTransport) {
  const uint32_t version = doc["version"] | 0U;
  if (version != PHONE_CAMERA_PROTOCOL_VERSION) {
    Serial.printf("[phone_camera] lens result ignored: unsupported version=%lu\n",
                  static_cast<unsigned long>(version));
    return;
  }
  const char* requestId = doc["requestId"] | "";
  const char* status = doc["status"] | "";
  const PhoneCameraLens lens = parsePhoneCameraLens(doc["lens"] | "");
  if (requestId[0] == '\0' || status[0] == '\0') {
    Serial.printf("[phone_camera] lens result ignored id=%s status=%s\n",
                  requestId,
                  status);
    return;
  }

  const bool applied = strcmp(status, "applied") == 0;
  const PhoneCameraTransport transport = phoneCameraTransportFromCommand(commandTransport);
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  const bool accepted = phoneCameraRemoteController.completeLensRequest(transport,
                                                                        requestId,
                                                                        applied,
                                                                        lens,
                                                                        millis());
  if (accepted) {
    applyPhoneCameraPresentation(previousState, millis());
  }
  Serial.printf("[phone_camera] lens result id=%s status=%s lens=%s transport=%s accepted=%d\n",
                requestId,
                status,
                phoneCameraLensName(lens),
                phoneCameraTransportName(transport),
                accepted ? 1 : 0);
}

void disconnectPhoneCameraTransport(PhoneCameraTransport transport, const char* reason) {
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  if (!phoneCameraRemoteController.disconnectTransport(transport)) {
    return;
  }
  applyPhoneCameraPresentation(previousState, millis());
  Serial.printf("[phone_camera] transport disconnected transport=%s reason=%s\n",
                phoneCameraTransportName(transport),
                reason != nullptr ? reason : "unknown");
}

void updatePhoneCameraRemote(unsigned long now) {
  const PhoneCameraState previousState = phoneCameraRemoteController.state();
  if (phoneCameraRemoteController.update(now)) {
    applyPhoneCameraPresentation(previousState, now);
  }
}
#endif

void sendJsonDocument(JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  if (wsServer.hasClient()) {
    wsServer.sendText(body.c_str());
  }
  sendUsbSerialJson(body.c_str());
}

#if STACKCHAN_TIMEKEEPER_ENABLED
namespace {
constexpr const char* kTimekeeperPrefsNamespace = "timekeeper";
constexpr const char* kPomodoroWorkKey = "p_work";
constexpr const char* kPomodoroBreakKey = "p_break";
constexpr const char* kPomodoroRevisionKey = "p_rev";
constexpr const char* kPomodoroCyclesKey = "p_cycles";
constexpr const char* kTimerSubmodeKey = "timer_mode";

bool pomodoroSessionActive() {
  return timekeeperController.activity() == TimekeeperActivity::Pomodoro &&
         (timekeeperController.state() == TimekeeperState::Running ||
          timekeeperController.state() == TimekeeperState::Paused);
}

bool persistPomodoroConfig(uint64_t workDurationMs,
                           uint64_t breakDurationMs,
                           uint32_t revision) {
  preferences.begin(kTimekeeperPrefsNamespace, false);
  const bool workSaved = preferences.putUInt(
    kPomodoroWorkKey, static_cast<uint32_t>(workDurationMs)) == sizeof(uint32_t);
  const bool breakSaved = preferences.putUInt(
    kPomodoroBreakKey, static_cast<uint32_t>(breakDurationMs)) == sizeof(uint32_t);
  const bool revisionSaved = preferences.putUInt(
    kPomodoroRevisionKey, revision) == sizeof(uint32_t);
  preferences.end();
  return workSaved && breakSaved && revisionSaved;
}
}  // namespace

void loadTimekeeperSettings() {
  preferences.begin(kTimekeeperPrefsNamespace, true);
  const uint64_t workDurationMs = preferences.getUInt(
    kPomodoroWorkKey,
    static_cast<uint32_t>(TimekeeperController::kDefaultPomodoroWorkDurationMs));
  const uint64_t breakDurationMs = preferences.getUInt(
    kPomodoroBreakKey,
    static_cast<uint32_t>(TimekeeperController::kDefaultPomodoroBreakDurationMs));
  uint32_t revision = preferences.getUInt(kPomodoroRevisionKey, 1);
  const uint8_t cycles = preferences.getUChar(
    kPomodoroCyclesKey, TimekeeperController::kDefaultPomodoroCycles);
  const uint8_t timerMode = preferences.getUChar(kTimerSubmodeKey, 0);
  preferences.end();

  if (revision == 0 ||
      !timekeeperController.configurePomodoro(workDurationMs,
                                              breakDurationMs,
                                              revision)) {
    revision = 1;
    timekeeperController.configurePomodoro(
      TimekeeperController::kDefaultPomodoroWorkDurationMs,
      TimekeeperController::kDefaultPomodoroBreakDurationMs,
      revision);
  }
  timekeeperController.setPomodoroCycles(
    constrain(cycles,
              TimekeeperController::kMinPomodoroCycles,
              TimekeeperController::kMaxPomodoroCycles),
    monotonicMs());
  timekeeperTimerSubmode = timerMode == 1
                             ? TimekeeperActivity::Pomodoro
                             : TimekeeperActivity::Countdown;
  Serial.printf("[timekeeper] settings work_ms=%llu break_ms=%llu cycles=%u revision=%lu timer_mode=%s\n",
                static_cast<unsigned long long>(timekeeperController.pomodoroWorkDurationMs()),
                static_cast<unsigned long long>(timekeeperController.pomodoroBreakDurationMs()),
                static_cast<unsigned>(timekeeperController.pomodoroCycles()),
                static_cast<unsigned long>(timekeeperController.pomodoroConfigRevision()),
                timekeeperTimerSubmode == TimekeeperActivity::Pomodoro
                  ? "pomodoro"
                  : "countdown");
}

void saveTimekeeperCycles() {
  preferences.begin(kTimekeeperPrefsNamespace, false);
  preferences.putUChar(kPomodoroCyclesKey, timekeeperController.pomodoroCycles());
  preferences.end();
}

void saveTimekeeperTimerSubmode() {
  preferences.begin(kTimekeeperPrefsNamespace, false);
  preferences.putUChar(kTimerSubmodeKey,
                       timekeeperTimerSubmode == TimekeeperActivity::Pomodoro ? 1 : 0);
  preferences.end();
}

void sendPomodoroConfigResult(const char* requestId,
                               const char* result,
                               const char* appliesTo,
                               const char* reason = nullptr) {
  ensureDeviceId();
  ensureBootId();
  JsonDocument response;
  response["type"] = "timekeeper.pomodoro.config.result";
  response["version"] = 1;
  response["deviceId"] = deviceId;
  response["bootId"] = bootId;
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["result"] = result;
  if (appliesTo != nullptr && appliesTo[0] != '\0') {
    response["appliesTo"] = appliesTo;
  }
  if (reason != nullptr && reason[0] != '\0') {
    response["reason"] = reason;
  }
  response["workDurationMs"] = timekeeperController.pomodoroWorkDurationMs();
  response["breakDurationMs"] = timekeeperController.pomodoroBreakDurationMs();
  response["configRevision"] = timekeeperController.pomodoroConfigRevision();
  sendJsonDocument(response);
}

void handlePomodoroConfigGet(JsonDocument& doc) {
  if ((doc["version"] | 0U) != 1U) {
    sendPomodoroConfigResult(doc["requestId"] | "",
                             "rejected",
                             "",
                             "unsupported_version");
    return;
  }
  sendPomodoroConfigResult(doc["requestId"] | "",
                           "current",
                           pomodoroSessionActive() ? "next_session" : "next_start");
}

void handlePomodoroConfigSet(JsonDocument& doc) {
  const char* requestId = doc["requestId"] | "";
  if ((doc["version"] | 0U) != 1U) {
    sendPomodoroConfigResult(requestId,
                             "rejected",
                             "",
                             "unsupported_version");
    return;
  }
  if (requestId[0] == '\0' || !doc["workDurationMs"].is<uint64_t>() ||
      !doc["breakDurationMs"].is<uint64_t>()) {
    sendPomodoroConfigResult(requestId,
                             "rejected",
                             "",
                             "invalid_request");
    return;
  }
  const uint64_t workDurationMs = doc["workDurationMs"].as<uint64_t>();
  const uint64_t breakDurationMs = doc["breakDurationMs"].as<uint64_t>();
  const bool valid =
    workDurationMs >= TimekeeperController::kMinPomodoroWorkDurationMs &&
    workDurationMs <= TimekeeperController::kMaxPomodoroWorkDurationMs &&
    breakDurationMs >= TimekeeperController::kMinPomodoroBreakDurationMs &&
    breakDurationMs <= TimekeeperController::kMaxPomodoroBreakDurationMs &&
    workDurationMs % (60ULL * 1000ULL) == 0 &&
    breakDurationMs % (60ULL * 1000ULL) == 0;
  if (!valid) {
    sendPomodoroConfigResult(requestId,
                             "rejected",
                             "",
                             "duration_out_of_range");
    return;
  }
  uint32_t revision = timekeeperController.pomodoroConfigRevision() + 1U;
  if (revision == 0) {
    revision = 1;
  }
  if (!persistPomodoroConfig(workDurationMs, breakDurationMs, revision) ||
      !timekeeperController.configurePomodoro(workDurationMs,
                                              breakDurationMs,
                                              revision)) {
    sendPomodoroConfigResult(requestId,
                             "rejected",
                             "",
                             "storage_failed");
    return;
  }
  sendPomodoroConfigResult(requestId,
                           "saved",
                           pomodoroSessionActive() ? "next_session" : "next_start");
  lastTimekeeperUiValueMs = UINT64_MAX;
  if (experienceMode == ExperienceMode::Timekeeper &&
      timekeeperController.activity() == TimekeeperActivity::Pomodoro) {
    drawTimekeeperOverlay(monotonicMs(), true);
  }
  Serial.printf("[timekeeper] pomodoro config saved work_ms=%llu break_ms=%llu revision=%lu applies=%s\n",
                static_cast<unsigned long long>(workDurationMs),
                static_cast<unsigned long long>(breakDurationMs),
                static_cast<unsigned long>(revision),
                pomodoroSessionActive() ? "next_session" : "next_start");
}

const char* timekeeperActivityName(TimekeeperActivity activity) {
  switch (activity) {
    case TimekeeperActivity::Stopwatch:
      return "stopwatch";
    case TimekeeperActivity::Countdown:
      return "countdown";
    case TimekeeperActivity::TenSecondChallenge:
      return "ten_second_challenge";
    case TimekeeperActivity::Pomodoro:
      return "pomodoro";
  }
  return "stopwatch";
}

const char* timekeeperChallengeDifficultyName(TimekeeperChallengeDifficulty difficulty) {
  switch (difficulty) {
    case TimekeeperChallengeDifficulty::Low:
      return "low";
    case TimekeeperChallengeDifficulty::Medium:
      return "medium";
    case TimekeeperChallengeDifficulty::High:
      return "high";
  }
  return "medium";
}

const char* timekeeperStateName(TimekeeperState state) {
  switch (state) {
    case TimekeeperState::Ready:
      return "ready";
    case TimekeeperState::Running:
      return "running";
    case TimekeeperState::Paused:
      return "paused";
    case TimekeeperState::Finished:
      return "finished";
    case TimekeeperState::Aborted:
      return "aborted";
    case TimekeeperState::Completed:
      return "completed";
  }
  return "ready";
}

const char* timekeeperEventName(TimekeeperEventType type) {
  switch (type) {
    case TimekeeperEventType::Started:
      return "started";
    case TimekeeperEventType::Paused:
      return "paused";
    case TimekeeperEventType::Resumed:
      return "resumed";
    case TimekeeperEventType::Reset:
      return "reset";
    case TimekeeperEventType::Lap:
      return "lap";
    case TimekeeperEventType::Milestone:
      return "milestone";
    case TimekeeperEventType::Finished:
      return "finished";
    case TimekeeperEventType::Result:
      return "result";
    case TimekeeperEventType::Aborted:
      return "aborted";
    case TimekeeperEventType::Transition:
      return "transition";
    case TimekeeperEventType::Completed:
      return "completed";
    case TimekeeperEventType::None:
      break;
  }
  return "unknown";
}

const char* timekeeperMilestoneName(TimekeeperMilestone milestone) {
  switch (milestone) {
    case TimekeeperMilestone::Remaining30Seconds:
      return "remaining_30_seconds";
    case TimekeeperMilestone::Remaining10Seconds:
      return "remaining_10_seconds";
    case TimekeeperMilestone::PomodoroWorkHalf:
      return "work_half";
    case TimekeeperMilestone::PomodoroWorkRemaining5Minutes:
      return "work_remaining_5_minutes";
    case TimekeeperMilestone::PomodoroWorkFinishingSoon:
      return "work_finishing_soon";
    case TimekeeperMilestone::PomodoroBreakRemaining1Minute:
      return "break_remaining_1_minute";
    case TimekeeperMilestone::PomodoroBreakFinishingSoon:
      return "break_finishing_soon";
    case TimekeeperMilestone::None:
    case TimekeeperMilestone::Halfway:
    case TimekeeperMilestone::Remaining5Minutes:
    case TimekeeperMilestone::Remaining1Minute:
      return nullptr;
  }
  return nullptr;
}

const char* timekeeperPomodoroPhaseName(TimekeeperPomodoroPhase phase) {
  switch (phase) {
    case TimekeeperPomodoroPhase::Work:
      return "work";
    case TimekeeperPomodoroPhase::Break:
      return "break";
    case TimekeeperPomodoroPhase::None:
      break;
  }
  return nullptr;
}

const char* timekeeperTransitionName(TimekeeperTransition transition) {
  switch (transition) {
    case TimekeeperTransition::WorkToBreak:
      return "work_to_break";
    case TimekeeperTransition::BreakToWork:
      return "break_to_work";
    case TimekeeperTransition::None:
      break;
  }
  return nullptr;
}

String makeTimekeeperSessionId(const TimekeeperEvent& event) {
  const char* prefix = "sw";
  switch (event.activity) {
    case TimekeeperActivity::Stopwatch:
      prefix = "sw";
      break;
    case TimekeeperActivity::Countdown:
      prefix = "cd";
      break;
    case TimekeeperActivity::TenSecondChallenge:
      prefix = "challenge";
      break;
    case TimekeeperActivity::Pomodoro:
      prefix = "pomodoro";
      break;
  }
  return String(prefix) + "-" + String(event.sessionSequence);
}

String timekeeperAnnouncementKey(const TimekeeperEvent& event) {
  if (event.activity != TimekeeperActivity::Pomodoro) {
    return String(timekeeperActivityName(event.activity)) + "." +
           timekeeperEventName(event.type);
  }
  if (event.type == TimekeeperEventType::Started) {
    return "pomodoro.session.started";
  }
  if (event.type == TimekeeperEventType::Completed) {
    return "pomodoro.session.completed";
  }
  if (event.type == TimekeeperEventType::Transition) {
    return event.transition == TimekeeperTransition::WorkToBreak
             ? "pomodoro.transition.work_to_break"
             : "pomodoro.transition.break_to_work";
  }
  if (event.type == TimekeeperEventType::Milestone) {
    switch (event.milestone) {
      case TimekeeperMilestone::PomodoroWorkHalf:
        return "pomodoro.work.half";
      case TimekeeperMilestone::PomodoroWorkRemaining5Minutes:
        return "pomodoro.work.remaining_5_minutes";
      case TimekeeperMilestone::PomodoroWorkFinishingSoon:
        return "pomodoro.work.finishing_soon";
      case TimekeeperMilestone::PomodoroBreakRemaining1Minute:
        return "pomodoro.break.remaining_1_minute";
      case TimekeeperMilestone::PomodoroBreakFinishingSoon:
        return "pomodoro.break.finishing_soon";
      default:
        break;
    }
  }
  return String("pomodoro.") + timekeeperEventName(event.type);
}

bool timekeeperEventRequiresResult(const TimekeeperEvent& event) {
  return (event.type == TimekeeperEventType::Finished &&
          event.activity == TimekeeperActivity::Countdown) ||
         (event.type == TimekeeperEventType::Completed &&
          event.activity == TimekeeperActivity::Pomodoro);
}

String makeTimekeeperEventId() {
  ensureBootId();
  ++timekeeperEventSequence;
  if (timekeeperEventSequence == 0) {
    ++timekeeperEventSequence;
  }
  return bootId + "-e" + String(timekeeperEventSequence);
}

bool timekeeperEventHasAnnouncement(const TimekeeperEvent& event, bool allowAnnouncement) {
  if (!allowAnnouncement || event.reason != nullptr) {
    return false;
  }
  if (event.activity == TimekeeperActivity::Pomodoro) {
    return event.type == TimekeeperEventType::Started ||
           event.type == TimekeeperEventType::Milestone ||
           event.type == TimekeeperEventType::Transition ||
           event.type == TimekeeperEventType::Completed;
  }
  switch (event.type) {
    case TimekeeperEventType::Started:
    case TimekeeperEventType::Paused:
    case TimekeeperEventType::Resumed:
    case TimekeeperEventType::Lap:
    case TimekeeperEventType::Milestone:
    case TimekeeperEventType::Finished:
    case TimekeeperEventType::Result:
      return true;
    case TimekeeperEventType::Reset:
    case TimekeeperEventType::Aborted:
    case TimekeeperEventType::Transition:
    case TimekeeperEventType::Completed:
    case TimekeeperEventType::None:
      return false;
  }
  return false;
}

uint32_t timekeeperAnnouncementMaxAgeMs(const TimekeeperEvent& event) {
  if (timekeeperEventRequiresResult(event)) {
    return 120000;
  }
  if (event.type == TimekeeperEventType::Paused || event.type == TimekeeperEventType::Result) {
    return 30000;
  }
  return 10000;
}

const char* timekeeperAnnouncementImportance(const TimekeeperEvent& event) {
  if (timekeeperEventRequiresResult(event)) {
    return "critical";
  }
  if (event.type == TimekeeperEventType::Result ||
      event.type == TimekeeperEventType::Paused ||
      event.type == TimekeeperEventType::Transition) {
    return "high";
  }
  return "normal";
}

int timekeeperChallengeAffectionDelta(const TimekeeperEvent& event) {
  if (event.activity != TimekeeperActivity::TenSecondChallenge ||
      event.type != TimekeeperEventType::Result) {
    return 0;
  }
  uint8_t accuracyIndex = 5;
  if (event.absoluteErrorMs == 0) {
    accuracyIndex = 0;
  } else if (event.absoluteErrorMs <= 50) {
    accuracyIndex = 1;
  } else if (event.absoluteErrorMs <= 200) {
    accuracyIndex = 2;
  } else if (event.absoluteErrorMs <= 500) {
    accuracyIndex = 3;
  } else if (event.absoluteErrorMs <= 1000) {
    accuracyIndex = 4;
  }

  uint8_t difficultyIndex = 1;
  switch (event.challengeDifficulty) {
    case TimekeeperChallengeDifficulty::Low:
      difficultyIndex = 0;
      break;
    case TimekeeperChallengeDifficulty::Medium:
      difficultyIndex = 1;
      break;
    case TimekeeperChallengeDifficulty::High:
      difficultyIndex = 2;
      break;
  }

  const uint8_t durationIndex = event.durationMs <= 10000ULL
                                  ? 0
                                  : (event.durationMs <= 30000ULL ? 1 : 2);
  // [duration: 10/30/60 s][difficulty: low/medium/high]
  // [accuracy: perfect/50 ms/200 ms/500 ms/1 s/over 1 s]
  static constexpr uint8_t kReward[3][3][6] = {
    {
      {10, 8, 6, 4, 2, 1},
      {14, 11, 8, 5, 3, 1},
      {18, 14, 10, 7, 4, 2},
    },
    {
      {15, 12, 9, 6, 3, 1},
      {20, 16, 12, 8, 5, 2},
      {25, 20, 15, 10, 6, 3},
    },
    {
      {20, 16, 12, 8, 5, 2},
      {25, 20, 15, 10, 6, 3},
      {30, 24, 18, 12, 8, 4},
    },
  };
  return kReward[durationIndex][difficultyIndex][accuracyIndex];
}

void sendTimekeeperAnnouncementPrefetch(const TimekeeperEvent& event,
                                        const String& eventId,
                                        bool allowAnnouncement) {
  if (!event.valid() || !appClientConnected() ||
      !timekeeperEventHasAnnouncement(event, allowAnnouncement)) {
    return;
  }
  ensureDeviceId();
  ensureBootId();
  const uint64_t nowMs = monotonicMs();
  const uint64_t ageMs = nowMs >= event.createdAtMs ? nowMs - event.createdAtMs : 0;

  JsonDocument doc;
  doc["type"] = "timekeeper.announcement.prefetch";
  doc["version"] = 1;
  doc["deviceId"] = deviceId;
  doc["bootId"] = bootId;
  doc["eventId"] = eventId;
  doc["sessionId"] = makeTimekeeperSessionId(event);
  doc["activity"] = timekeeperActivityName(event.activity);
  doc["event"] = timekeeperEventName(event.type);
  doc["ageMs"] = ageMs;
  doc["elapsedMs"] = event.elapsedMs;
  if (event.activity == TimekeeperActivity::TenSecondChallenge) {
    doc["targetMs"] = event.durationMs;
    doc["difficulty"] =
      timekeeperChallengeDifficultyName(event.challengeDifficulty);
  }
  if (event.type == TimekeeperEventType::Result) {
    doc["signedErrorMs"] = event.signedErrorMs;
    doc["absoluteErrorMs"] = event.absoluteErrorMs;
    doc["rank"] = event.rank != nullptr ? event.rank : "try_again";
    doc["affectionDelta"] = timekeeperChallengeAffectionDelta(event);
  }

  JsonObject announcement = doc["announcement"].to<JsonObject>();
  announcement["key"] = timekeeperAnnouncementKey(event);
  announcement["importance"] = timekeeperAnnouncementImportance(event);
  announcement["maxAgeMs"] = timekeeperAnnouncementMaxAgeMs(event);
  announcement["delivery"] = "best_effort";
  announcement["playbackGate"] = "matching_timekeeper_event";
  sendJsonDocument(doc);
}

void sendTimekeeperEventJson(const TimekeeperEvent& event,
                             const String& eventId,
                             bool allowAnnouncement) {
  if (!event.valid()) {
    return;
  }
  const bool connected = appClientConnected();
  const bool hasAnnouncement =
    timekeeperEventHasAnnouncement(event, allowAnnouncement);
  if (!connected) {
    Serial.printf("[timekeeper.tx] dropped event_id=%s activity=%s event=%s app_connected=0 announcement=%d\n",
                  eventId.c_str(),
                  timekeeperActivityName(event.activity),
                  timekeeperEventName(event.type),
                  hasAnnouncement ? 1 : 0);
    return;
  }
  ensureDeviceId();
  ensureBootId();
  const uint64_t nowMs = monotonicMs();
  const uint64_t ageMs = nowMs >= event.createdAtMs ? nowMs - event.createdAtMs : 0;

  JsonDocument doc;
  doc["type"] = "timekeeper.event";
  doc["version"] = 1;
  doc["deviceId"] = deviceId;
  doc["bootId"] = bootId;
  doc["eventId"] = eventId;
  doc["sessionId"] = makeTimekeeperSessionId(event);
  doc["activity"] = timekeeperActivityName(event.activity);
  doc["event"] = timekeeperEventName(event.type);
  doc["state"] = timekeeperStateName(event.state);
  doc["ageMs"] = ageMs;
  doc["elapsedMs"] = event.elapsedMs;
  if (event.activity == TimekeeperActivity::Countdown) {
    doc["remainingMs"] = event.remainingMs;
    doc["durationMs"] = event.durationMs;
  } else if (event.activity == TimekeeperActivity::TenSecondChallenge) {
    doc["targetMs"] = event.durationMs;
    doc["difficulty"] =
      timekeeperChallengeDifficultyName(event.challengeDifficulty);
  } else if (event.activity == TimekeeperActivity::Pomodoro) {
    const char* phase = timekeeperPomodoroPhaseName(event.pomodoroPhase);
    if (phase != nullptr) {
      doc["phase"] = phase;
    }
    const char* transition = timekeeperTransitionName(event.transition);
    if (transition != nullptr) {
      doc["transition"] = transition;
    }
    doc["cycleIndex"] = event.cycleIndex;
    doc["totalCycles"] = event.totalCycles;
    doc["remainingCycles"] = event.remainingCycles;
    doc["isFinalCycle"] = event.isFinalCycle;
    doc["workDurationMs"] = event.workDurationMs;
    doc["breakDurationMs"] = event.breakDurationMs;
    if (event.phaseDurationMs != 0) {
      doc["phaseDurationMs"] = event.phaseDurationMs;
    }
    doc["remainingMs"] = event.remainingMs;
    doc["configRevision"] = event.configRevision;
  }
  if (event.type == TimekeeperEventType::Milestone) {
    const char* milestone = timekeeperMilestoneName(event.milestone);
    if (milestone != nullptr) {
      doc["milestone"] = milestone;
    }
  }
  if (event.reason != nullptr && event.reason[0] != '\0') {
    doc["reason"] = event.reason;
  }
  if (event.type == TimekeeperEventType::Lap) {
    doc["lapIndex"] = event.lapIndex;
    doc["lapDurationMs"] = event.lapDurationMs;
    if (event.previousLapDurationMs != 0) {
      doc["previousLapDurationMs"] = event.previousLapDurationMs;
      doc["lapDeltaMs"] = event.lapDeltaMs;
    }
    doc["isBestLap"] = event.isBestLap;
  }
  if (event.type == TimekeeperEventType::Result) {
    doc["signedErrorMs"] = event.signedErrorMs;
    doc["absoluteErrorMs"] = event.absoluteErrorMs;
    doc["rank"] = event.rank != nullptr ? event.rank : "try_again";
    doc["affectionDelta"] = timekeeperChallengeAffectionDelta(event);
  }

  if (hasAnnouncement) {
    JsonObject announcement = doc["announcement"].to<JsonObject>();
    announcement["key"] = timekeeperAnnouncementKey(event);
    announcement["importance"] = timekeeperAnnouncementImportance(event);
    announcement["maxAgeMs"] = timekeeperAnnouncementMaxAgeMs(event);
    announcement["delivery"] = timekeeperEventRequiresResult(event)
                                   ? "until_result"
                                   : "best_effort";
  }
  sendJsonDocument(doc);
  Serial.printf("[timekeeper.tx] sent event_id=%s activity=%s event=%s announcement=%d remaining_ms=%llu\n",
                eventId.c_str(),
                timekeeperActivityName(event.activity),
                timekeeperEventName(event.type),
                hasAnnouncement ? 1 : 0,
                static_cast<unsigned long long>(event.remainingMs));
}

void handleTimekeeperEvent(const TimekeeperEvent& event, bool allowAnnouncement) {
  if (!event.valid()) {
    return;
  }
  const int affectionDelta = timekeeperChallengeAffectionDelta(event);
  if (affectionDelta > 0) {
    const unsigned long affectionNow = millis();
    const AffectionApplyResult affectionResult =
      affectionController.debugAdjust(affectionDelta);
    if (affectionResult.applied) {
      applyAffectionResult(affectionResult, affectionNow, true);
    } else {
      // At the affection cap the stored value cannot change, but the player
      // still earned the Timekeeper reward. Keep the reward presentation even
      // when the persisted value remains capped at 1000.
      faceController.showAffectionDelta(affectionDelta, affectionNow);
    }
    Serial.printf("[timekeeper] challenge affection reward=+%d applied=%d value=%d error_ms=%llu\n",
                  affectionDelta,
                  affectionResult.applied ? 1 : 0,
                  affectionResult.state.affection,
                  static_cast<unsigned long long>(event.absoluteErrorMs));
  }
  const String eventId = makeTimekeeperEventId();

#if STACKCHAN_PET_ANIMATION_ENABLED
  const bool shouldSmile =
    event.activity == TimekeeperActivity::TenSecondChallenge &&
    event.type == TimekeeperEventType::Result &&
    event.absoluteErrorMs <= 200;
  if (shouldSmile && !pendingTimekeeperSmileResult.active) {
    pendingTimekeeperSmileResult.active = true;
    pendingTimekeeperSmileResult.animationStarted = false;
    pendingTimekeeperSmileResult.prefetchSent = false;
    pendingTimekeeperSmileResult.event = event;
    pendingTimekeeperSmileResult.eventId = eventId;
    pendingTimekeeperSmileResult.allowAnnouncement = allowAnnouncement;
    pendingTimekeeperSmileResult.startDeadlineMs = monotonicMs() + 5000ULL;
    if (!audioBusyForUiEffects()) {
      pendingTimekeeperSmileResult.animationStarted =
        faceController.startTimekeeperSmileAnimation(millis());
      if (!pendingTimekeeperSmileResult.animationStarted) {
        pendingTimekeeperSmileResult.startDeadlineMs = monotonicMs();
      }
    }
    lastTimekeeperUiValueMs = UINT64_MAX;
    Serial.printf("[timekeeper] result smile queued event_id=%s error_ms=%llu started=%d\n",
                  eventId.c_str(),
                  static_cast<unsigned long long>(event.absoluteErrorMs),
                  pendingTimekeeperSmileResult.animationStarted ? 1 : 0);
    return;
  }
#endif

  if (timekeeperEventRequiresResult(event)) {
    pendingTimekeeperAnnouncement.active = true;
    pendingTimekeeperAnnouncement.event = event;
    pendingTimekeeperAnnouncement.eventId = eventId;
    pendingTimekeeperAnnouncement.expiresAtMs = event.createdAtMs + 120000ULL;
    pendingTimekeeperAnnouncement.sentAfterLastDeviceInfo = appClientConnected();
  }
  sendTimekeeperEventJson(event, eventId, allowAnnouncement);
  lastTimekeeperUiValueMs = UINT64_MAX;
}

void updatePendingTimekeeperSmileResult() {
  if (!pendingTimekeeperSmileResult.active) {
    return;
  }
#if STACKCHAN_PET_ANIMATION_ENABLED
  if (!pendingTimekeeperSmileResult.animationStarted) {
    if (!audioBusyForUiEffects()) {
      if (faceController.startTimekeeperSmileAnimation(millis())) {
        pendingTimekeeperSmileResult.animationStarted = true;
        Serial.printf("[timekeeper] deferred result smile started event_id=%s\n",
                      pendingTimekeeperSmileResult.eventId.c_str());
        return;
      }
      pendingTimekeeperSmileResult.startDeadlineMs = monotonicMs();
    }
    if (monotonicMs() < pendingTimekeeperSmileResult.startDeadlineMs) {
      return;
    }
    Serial.printf("[timekeeper] result smile start timeout event_id=%s\n",
                  pendingTimekeeperSmileResult.eventId.c_str());
  }
  if (pendingTimekeeperSmileResult.animationStarted &&
      !pendingTimekeeperSmileResult.prefetchSent && appClientConnected()) {
    sendTimekeeperAnnouncementPrefetch(pendingTimekeeperSmileResult.event,
                                       pendingTimekeeperSmileResult.eventId,
                                       pendingTimekeeperSmileResult.allowAnnouncement);
    pendingTimekeeperSmileResult.prefetchSent = true;
    Serial.printf("[timekeeper] result speech prefetch sent event_id=%s\n",
                  pendingTimekeeperSmileResult.eventId.c_str());
  }
  if (faceController.timekeeperSmileAnimationActive()) {
    return;
  }
#endif
  const PendingTimekeeperSmileResult completed = pendingTimekeeperSmileResult;
  pendingTimekeeperSmileResult = PendingTimekeeperSmileResult();
  sendTimekeeperEventJson(completed.event,
                          completed.eventId,
                          completed.allowAnnouncement);
  Serial.printf("[timekeeper] result smile finished event_id=%s announcement=%d\n",
                completed.eventId.c_str(),
                completed.allowAnnouncement ? 1 : 0);
}

void flushPendingTimekeeperSmileResult(bool allowAnnouncement) {
  if (!pendingTimekeeperSmileResult.active) {
    return;
  }
  const PendingTimekeeperSmileResult pending = pendingTimekeeperSmileResult;
  pendingTimekeeperSmileResult = PendingTimekeeperSmileResult();
  sendTimekeeperEventJson(pending.event,
                          pending.eventId,
                          allowAnnouncement && pending.allowAnnouncement);
}

void maybeResendPendingTimekeeperAnnouncement() {
  if (!pendingTimekeeperAnnouncement.active) {
    return;
  }
  const uint64_t nowMs = monotonicMs();
  if (nowMs >= pendingTimekeeperAnnouncement.expiresAtMs) {
    Serial.printf("[timekeeper] pending expired event_id=%s\n",
                  pendingTimekeeperAnnouncement.eventId.c_str());
    pendingTimekeeperAnnouncement = PendingTimekeeperAnnouncement();
    return;
  }
  if (!appClientConnected() || pendingTimekeeperAnnouncement.sentAfterLastDeviceInfo) {
    return;
  }
  sendTimekeeperEventJson(pendingTimekeeperAnnouncement.event,
                          pendingTimekeeperAnnouncement.eventId,
                          true);
  pendingTimekeeperAnnouncement.sentAfterLastDeviceInfo = true;
  Serial.printf("[timekeeper] pending resent event_id=%s\n",
                pendingTimekeeperAnnouncement.eventId.c_str());
}

void handleTimekeeperAnnouncementResult(JsonDocument& doc) {
  if (!pendingTimekeeperAnnouncement.active || (doc["version"] | 0U) != 1U) {
    return;
  }
  ensureDeviceId();
  ensureBootId();
  const char* resultDeviceId = doc["deviceId"] | "";
  const char* resultBootId = doc["bootId"] | "";
  const char* resultEventId = doc["eventId"] | "";
  const char* result = doc["result"] | "";
  if (deviceId != resultDeviceId || bootId != resultBootId ||
      pendingTimekeeperAnnouncement.eventId != resultEventId) {
    Serial.println("[timekeeper] announcement result ignored: identity mismatch");
    return;
  }
  if (strcmp(result, "queued") == 0) {
    Serial.printf("[timekeeper] announcement queued event_id=%s\n", resultEventId);
    return;
  }
  if (strcmp(result, "sent") == 0 || strcmp(result, "suppressed") == 0 ||
      strcmp(result, "expired") == 0 || strcmp(result, "failed") == 0) {
    Serial.printf("[timekeeper] announcement terminal event_id=%s result=%s\n",
                  resultEventId,
                  result);
    pendingTimekeeperAnnouncement = PendingTimekeeperAnnouncement();
  }
}

void prepareTimekeeperForDisplayOff(uint64_t nowMs) {
  experienceModeMenuVisible = false;
  timekeeperDurationMenuVisible = false;
  travelFacePickerVisible = false;
  stopwatchYellowButtonPressed = false;
  stopwatchYellowButtonLongHandled = false;
  stopwatchYellowButtonPressedAtMs = 0;
  if (experienceMode != ExperienceMode::Timekeeper) {
    return;
  }
  flushPendingTimekeeperSmileResult(false);
  const TimekeeperEvent event = timekeeperController.suspend(nowMs, "display_off");
  // The state event is useful to the app, but it must never enqueue speech
  // immediately before communication is suspended.
  handleTimekeeperEvent(event, false);
}

void sendCommunicationSuspending(const char* reason) {
  if (!appClientConnected()) {
    return;
  }
  ensureDeviceId();
  ensureBootId();
  ++communicationSuspendSequence;
  if (communicationSuspendSequence == 0) {
    ++communicationSuspendSequence;
  }
  JsonDocument doc;
  doc["type"] = "device.communication.suspending";
  doc["version"] = 1;
  doc["deviceId"] = deviceId;
  doc["bootId"] = bootId;
  doc["sequence"] = communicationSuspendSequence;
  doc["reason"] = reason != nullptr ? reason : "unknown";
  sendJsonDocument(doc);
}
#endif

void writeDeviceInfo(JsonDocument& response, const char* requestId) {
  ensureDeviceId();
  ensureBootId();
  response["type"] = "device.info";
  if (requestId != nullptr && requestId[0] != '\0') {
    response["requestId"] = requestId;
  }
  response["deviceId"] = deviceId;
  response["bootId"] = bootId;
#if STACKCHAN_TIMEKEEPER_ENABLED
  response["experienceMode"] = experienceModeName(experienceMode);
  response["experienceModeRevision"] = experienceModeRevision;
  JsonObject pomodoro = response["pomodoro"].to<JsonObject>();
  pomodoro["workDurationMs"] = timekeeperController.pomodoroWorkDurationMs();
  pomodoro["breakDurationMs"] = timekeeperController.pomodoroBreakDurationMs();
  pomodoro["totalCycles"] = timekeeperController.pomodoroCycles();
  pomodoro["configRevision"] = timekeeperController.pomodoroConfigRevision();
#endif
  response["displayName"] = STACKCHAN_DEVICE_DISPLAY_NAME;
  response["firmwareName"] = STACKCHAN_FIRMWARE_NAME;
  response["firmwareVersion"] = STACKCHAN_FIRMWARE_VERSION;
  response["protocolVersion"] = STACKCHAN_APP_PROTOCOL_VERSION;
#if STACKCHAN_CLASSIC_FACE_ENABLED
  response["faceRenderer"] = "classic";
#else
  response["faceRenderer"] = "image";
#endif
  const FaceAssetStatus& faceAssets = faceController.faceAssetStatus();
  response["faceRendererMode"] = faceAssetModeName(faceAssets.mode);
  response["faceAssetSchema"] = faceAssets.schemaVersion;
  response["faceAssetManifestPresent"] = faceAssets.manifestPresent;
  response["faceAssetManifestValid"] = faceAssets.manifestValid;
  response["legacyFaceFallbackActive"] = faceAssetModeUsesLegacyFallback(faceAssets.mode);
  if (!faceAssets.error.isEmpty()) {
    response["faceAssetError"] = faceAssets.error;
  }
  if (!faceAssets.missingAsset.isEmpty()) {
    response["faceAssetMissing"] = faceAssets.missingAsset;
  }
  JsonArray capabilities = response["capabilities"].to<JsonArray>();
  capabilities.add("device.info");
#if STACKCHAN_TIMEKEEPER_ENABLED
  capabilities.add("experience.mode.v1");
  capabilities.add("device.communication.suspending.v1");
  capabilities.add("timekeeper.v1");
  capabilities.add("timekeeper.pomodoro.v1");
#endif
  capabilities.add("affection.get");
  capabilities.add("affection.sync");
  capabilities.add(SPEECH_BUBBLE_CAPABILITY);
#if STEP_COUNTER_ENABLED
  capabilities.add("steps.sync");
#endif
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  capabilities.add("phone_camera.remote_shutter.v1");
  capabilities.add("phone_camera.remote_lens.v1");
#endif
  JsonObject display = response["display"].to<JsonObject>();
  display["width"] = M5.Display.width();
  display["height"] = M5.Display.height();
#if STACKCHAN_ROUND_DISPLAY
  display["shape"] = "round";
#else
  display["shape"] = "rect";
#endif
  JsonObject speechBubble = response["speechBubble"].to<JsonObject>();
  speechBubble["version"] = SPEECH_BUBBLE_PROTOCOL_VERSION;
  speechBubble["sampleRate"] = AUDIO_SAMPLE_RATE;
  speechBubble["maxSequenceIdUtf8Bytes"] = SPEECH_BUBBLE_MAX_SEQUENCE_ID_BYTES;
  speechBubble["maxTextUtf8Bytes"] = SPEECH_BUBBLE_MAX_TEXT_BYTES;
  speechBubble["maxQueuedCues"] = SPEECH_BUBBLE_MAX_QUEUED_CUES;
  speechBubble["maxPcmBytes"] = SPEECH_BUBBLE_MAX_PCM_BYTES;
  speechBubble["defaultHoldMs"] = SPEECH_BUBBLE_DEFAULT_HOLD_MS;
  speechBubble["maxHoldMs"] = SPEECH_BUBBLE_MAX_HOLD_MS;
  speechBubble["stallTimeoutMs"] = SPEECH_BUBBLE_STALL_TIMEOUT_MS;
  speechBubble["preSpeakingHoldMs"] = SPEECH_BUBBLE_PRE_SPEAKING_HOLD_MS;
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
    Usb::kVersion,
    type,
    flags,
    0,
  };
  Usb::writeLe32(header + 8, usbSerialTxSeq++);
  Usb::writeLe32(header + 12, static_cast<uint32_t>(length));

  uint32_t crc = 0xFFFFFFFFUL;
  crc = Usb::crc32Update(crc, header + 4, 12);
  if (length > 0) {
    crc = Usb::crc32Update(crc, payload, length);
  }
  crc = ~crc;

  uint8_t crcBytes[4];
  Usb::writeLe32(crcBytes, crc);
  const size_t headerWritten = Serial.write(header, sizeof(header));
  size_t payloadWritten = 0;
  if (length > 0) {
    payloadWritten = Serial.write(payload, length);
  }
  const size_t crcWritten = Serial.write(crcBytes, sizeof(crcBytes));
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
  if (type == Usb::kTypePong || type == Usb::kTypeJson || type == Usb::kTypeError) {
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
  return sendUsbSerialFrame(Usb::kTypeMicPcm, payload, length);
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
#if STACKCHAN_TIMEKEEPER_ENABLED
  maybeResendPendingTimekeeperAnnouncement();
#endif
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
  const bool isTimeSet = strcmp(commandType, "streetpass.time.set") == 0;
  const unsigned long now = millis();
  const uint32_t previousUnix =
    isTimeSet ? streetPassController.estimatedUnix(now) : 0;
  if (isTimeSet) {
    doc["timezone"] = STACKCHAN_TIMEZONE_NAME;
  }
  const uint16_t beforeProfileNameHash = streetPassNameHash(streetPassController.profile().name);
  JsonDocument response;
  if (!streetPassController.handleJsonCommand(doc, response, now)) {
    return false;
  }
  if (isTimeSet && (response["ok"] | false)) {
    const uint32_t syncedUnix = doc["unixTime"] | 0;
    setSystemUnixTime(syncedUnix);
    writeStreetPassRtcTime(syncedUnix);
    lastClockOverlayUpdateMs = 0;
    const long correctionSeconds =
      validStreetPassUnix(previousUnix)
        ? static_cast<long>(static_cast<int64_t>(syncedUnix) - static_cast<int64_t>(previousUnix))
        : 0;
    Serial.printf("[streetpass] app time synced unix=%lu correction=%ld sec timezone=%s\n",
                  static_cast<unsigned long>(syncedUnix),
                  correctionSeconds,
                  STACKCHAN_TIMEZONE_NAME);
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

void sendSpeechBubbleProtocolError(SpeechBubbleTransport transport,
                                   JsonDocument& request,
                                   const char* error) {
  JsonDocument response;
  response["type"] = "display.speech_bubble.error";
  response["version"] = SPEECH_BUBBLE_PROTOCOL_VERSION;
  const char* sequenceId = request["sequenceId"] | "";
  if (sequenceId[0] != '\0') {
    response["sequenceId"] = sequenceId;
  }
  if (!request["segmentIndex"].isNull()) {
    response["segmentIndex"] = request["segmentIndex"];
  }
  response["error"] = error != nullptr ? error : "unknown";
  String body;
  serializeJson(response, body);
  if (transport == SpeechBubbleTransport::WebSocket) {
    wsServer.sendText(body.c_str());
  } else if (transport == SpeechBubbleTransport::UsbSerial) {
    sendUsbSerialJson(body.c_str());
  }
}

void handleSpeechBubbleCommand(JsonDocument& doc,
                               const char* type,
                               SpeechBubbleTransport transport) {
  const uint32_t version = doc["version"] | 0U;
  if (version != SPEECH_BUBBLE_PROTOCOL_VERSION) {
    sendSpeechBubbleProtocolError(transport, doc, "unsupported_version");
    return;
  }

#if STACKCHAN_TIMEKEEPER_ENABLED
  if (experienceMode == ExperienceMode::Timekeeper ||
      experienceMode == ExperienceMode::Travel) {
    // Timekeeper owns the available screen space. Treat bubble commands as
    // intentionally hidden so common app sequences cannot cover its clock.
    if (speechBubbleController.active() || faceController.speechBubbleVisible()) {
      speechBubbleController.reset(
        experienceMode == ExperienceMode::Travel ? "travel_mode" :
        "timekeeper_mode");
    }
    return;
  }
#endif

  const char* sequenceId = doc["sequenceId"] | "";
  const char* commandError = nullptr;
  bool ok = false;
  if (strcmp(type, "display.speech_bubble.cue") == 0) {
    const uint32_t segmentIndex = doc["segmentIndex"] | 0U;
    const char* text = doc["text"] | "";
    const uint32_t pcmBytes = doc["pcmBytes"] | 0U;
    const uint32_t sampleRate = doc["sampleRate"] | AUDIO_SAMPLE_RATE;
    ok = speechBubbleController.cue(sequenceId,
                                    segmentIndex,
                                    text,
                                    pcmBytes,
                                    sampleRate,
                                    transport,
                                    &commandError);
  } else if (strcmp(type, "display.speech_bubble.end") == 0) {
    const int64_t requestedHoldMs = doc["holdMs"] | static_cast<int64_t>(SPEECH_BUBBLE_DEFAULT_HOLD_MS);
    const uint32_t holdMs = requestedHoldMs <= 0
                              ? 0U
                              : static_cast<uint32_t>(min<int64_t>(requestedHoldMs,
                                                                    SPEECH_BUBBLE_MAX_HOLD_MS));
    ok = speechBubbleController.end(sequenceId, holdMs, transport, &commandError);
  } else if (strcmp(type, "display.speech_bubble.cancel") == 0) {
    ok = speechBubbleController.cancel(sequenceId, transport, &commandError);
  }

  if (!ok) {
    Serial.printf("[speech_bubble] command rejected type=%s sequence=%s error=%s\n",
                  type,
                  sequenceId,
                  commandError != nullptr ? commandError : "unknown");
    sendSpeechBubbleProtocolError(transport, doc, commandError);
  }
}

void handleJsonCommand(const uint8_t* payload,
                       size_t length,
                       SpeechBubbleTransport transport) {
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
#if STACKCHAN_TIMEKEEPER_ENABLED
  } else if (strcmp(type, "timekeeper.announcement.result") == 0) {
    handleTimekeeperAnnouncementResult(doc);
  } else if (strcmp(type, "timekeeper.pomodoro.config.get") == 0) {
    handlePomodoroConfigGet(doc);
  } else if (strcmp(type, "timekeeper.pomodoro.config.set") == 0) {
    handlePomodoroConfigSet(doc);
#endif
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  } else if (strcmp(type, "phone_camera.state") == 0) {
    handlePhoneCameraStateCommand(doc, transport);
  } else if (strcmp(type, "phone_camera.shutter.result") == 0) {
    handlePhoneCameraResultCommand(doc, transport);
  } else if (strcmp(type, "phone_camera.lens.set.result") == 0) {
    handlePhoneCameraLensResultCommand(doc, transport);
#endif
  } else if (strcmp(type, "steps.get") == 0) {
    handleStepsGetCommand(doc);
  } else if (strcmp(type, "audio.speaker_test") == 0) {
    handleSpeakerTestCommand(doc);
  } else if (strcmp(type, "audio.mic_test") == 0) {
    handleMicTestCommand(doc);
  } else if (strcmp(type, "audio.playback_diag") == 0) {
    handlePlaybackDiagCommand(doc);
  } else if (strcmp(type, "display.speech_bubble.cue") == 0 ||
             strcmp(type, "display.speech_bubble.end") == 0 ||
             strcmp(type, "display.speech_bubble.cancel") == 0) {
    handleSpeechBubbleCommand(doc, type, transport);
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

  const bool wasUsbSerialClientConnected = usbSerialClientConnected;
  usbSerialClientConnected = true;
  usbSerialLastRxMs = millis();
  audioController.setUsbSerialClientConnected(true);
  updateMicStatusOverlay();
  if (!wasUsbSerialClientConnected) {
    sendStepsSnapshot();
  }
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
  handleJsonCommand(payload, length, SpeechBubbleTransport::UsbSerial);
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

  faceController.setCameraCaptureActive(true);
  const bool micPausedForCapture = CAMERA_PAUSE_MIC_DURING_CAPTURE
                                     ? audioController.pauseMicForCapture()
                                     : false;
  const bool streetPassSuspendedForCapture = suspendStreetPassBleForCamera();

  if (!cameraManager.isReady() && !cameraManager.init()) {
    faceController.setCameraCaptureActive(false);
    audioController.resetSpeakerAfterCameraCapture();
    audioController.resumeMicAfterCapture(micPausedForCapture);
    resumeStreetPassBleAfterCamera(streetPassSuspendedForCapture);
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
    faceController.setCameraCaptureActive(false);
    cameraManager.deinit();
    audioController.resetSpeakerAfterCameraCapture();
    audioController.resumeMicAfterCapture(micPausedForCapture);
    resumeStreetPassBleAfterCamera(streetPassSuspendedForCapture);
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

  // Keep the encoded JPEG, but release the large camera framebuffer and DMA
  // allocations before serial framing and the rest of the device resume.
  cameraManager.deinit();
  audioController.resetSpeakerAfterCameraCapture();

  start["contentType"] = "image/jpeg";
  start["length"] = static_cast<uint32_t>(jpgLen);
  String body;
  serializeJson(start, body);
  sendUsbSerialJson(body.c_str());

  size_t offset = 0;
  while (offset < jpgLen) {
    const size_t chunk = min(static_cast<size_t>(USB_SERIAL_CAPTURE_CHUNK_BYTES), jpgLen - offset);
    sendUsbSerialFrame(Usb::kTypeCaptureImageChunk, jpg + offset, chunk);
    offset += chunk;
    delay(1);
  }

  cameraManager.releaseBuffer(jpg);
  faceController.setCameraCaptureActive(false);
  delay(80);
  audioController.resumeMicAfterCapture(micPausedForCapture);
  resumeStreetPassBleAfterCamera(streetPassSuspendedForCapture);
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
  const bool wasUsbSerialClientConnected = usbSerialClientConnected;
  usbSerialClientConnected = true;
  usbSerialFramedMode = true;
  usbSerialLastRxMs = millis();
  audioController.setUsbSerialClientConnected(true);
  updateMicStatusOverlay();
  if (!wasUsbSerialClientConnected) {
    sendStepsSnapshot();
  }

#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
  if (type == Usb::kTypeJson || type == Usb::kTypeTtsPcm || type == Usb::kTypePing) {
    Serial.printf("SCU1 rx type=0x%02x seq=%lu length=%u crc_ok=1\n",
                  type,
                  static_cast<unsigned long>(seq),
                  static_cast<unsigned>(length));
  }
#endif

  switch (type) {
    case Usb::kTypeJson:
      handleUsbSerialJsonPayload(payload, length);
      break;
    case Usb::kTypeTtsPcm: {
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
      speechBubbleController.onPcmReceived(SpeechBubbleTransport::UsbSerial);
      audioController.onBinaryReceived(const_cast<uint8_t*>(payload), length);
      break;
    }
    case Usb::kTypeCaptureRequest:
      if (length > 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload, length);
        if (!error) {
          handleUsbSerialCaptureRequest(doc);
        }
      }
      break;
    case Usb::kTypePing: {
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
      const bool sent = sendUsbSerialFrame(Usb::kTypePong, reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
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
#if STACKCHAN_TIMEKEEPER_ENABLED
    if (pendingTimekeeperAnnouncement.active) {
      pendingTimekeeperAnnouncement.sentAfterLastDeviceInfo = false;
    }
#endif
    audioController.setUsbSerialClientConnected(false);
    if (speechBubbleController.activeForTransport(SpeechBubbleTransport::UsbSerial)) {
      speechBubbleController.reset("usb_timeout");
    }
    updateMicStatusOverlay();
    clearCameraButtonPending("usb_timeout");
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
    disconnectPhoneCameraTransport(PhoneCameraTransport::UsbSerial, "timeout");
#endif
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
        if (static_cast<uint8_t>(value) == Usb::kMagic[usbSerialMagicIndex]) {
          if (usbSerialMagicIndex == 0) {
            usbSerialLineLength = 0;
            usbSerialLineOverflow = false;
          }
          usbSerialFrameHeader[usbSerialMagicIndex++] = static_cast<uint8_t>(value);
          if (usbSerialMagicIndex == sizeof(Usb::kMagic)) {
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
            Serial.printf("USB RX SCU1 magic detected at_ms=%lu\n",
                          static_cast<unsigned long>(millis()));
#endif
            usbSerialRxState = UsbSerialRxState::Header;
            usbSerialHeaderIndex = sizeof(Usb::kMagic);
            usbSerialMagicIndex = 0;
          }
          continue;
        }
        if (usbSerialMagicIndex != 0) {
          for (uint8_t i = 0; i < usbSerialMagicIndex; ++i) {
            if (usbSerialLineLength + 1 < USB_SERIAL_LINE_BUFFER_BYTES && !usbSerialLineOverflow) {
              usbSerialLineBuffer[usbSerialLineLength++] = static_cast<char>(Usb::kMagic[i]);
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
          usbSerialFrameLength = Usb::readLe32(usbSerialFrameHeader + 12);
          usbSerialPayloadIndex = 0;
          usbSerialCrcIndex = 0;
#if USB_SERIAL_RX_DIAG_LOG_ENABLED
          Serial.printf("SCU1 header version=0x%02x type=0x%02x seq=%lu length=%lu at_ms=%lu\n",
                        usbSerialFrameHeader[4],
                        usbSerialFrameHeader[5],
                        static_cast<unsigned long>(Usb::readLe32(usbSerialFrameHeader + 8)),
                        static_cast<unsigned long>(usbSerialFrameLength),
                        static_cast<unsigned long>(millis()));
#endif
          if (usbSerialFrameHeader[4] != Usb::kVersion ||
              usbSerialFrameLength > USB_SERIAL_FRAME_MAX_PAYLOAD_BYTES) {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
            Serial.printf("SCU1 invalid header version=0x%02x seq=%lu length=%lu max=%u\n",
                          usbSerialFrameHeader[4],
                          static_cast<unsigned long>(Usb::readLe32(usbSerialFrameHeader + 8)),
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
          crc = Usb::crc32Update(crc, usbSerialFrameHeader + 4, 12);
          if (usbSerialFrameLength > 0) {
            crc = Usb::crc32Update(crc, usbSerialFramePayload, usbSerialFrameLength);
          }
          crc = ~crc;
          const uint32_t actualCrc = Usb::readLe32(usbSerialFrameCrcBytes);
          if (crc == actualCrc) {
            handleUsbSerialFrame(usbSerialFrameHeader[5],
                                 usbSerialFrameHeader[6],
                                 Usb::readLe32(usbSerialFrameHeader + 8),
                                 usbSerialFramePayload,
                                 usbSerialFrameLength);
          } else {
#if USB_SERIAL_TTS_DIAG_LOG_ENABLED
            Serial.printf("SCU1 crc mismatch seq=%lu expected=0x%08lx actual=0x%08lx length=%lu\n",
                          static_cast<unsigned long>(Usb::readLe32(usbSerialFrameHeader + 8)),
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
      (usbSerialRxDiagEventCount < 40 || rxDiagFirstLength >= sizeof(Usb::kMagic))) {
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
  handleJsonCommand(payload, length, SpeechBubbleTransport::WebSocket);
}

void onWsBinary(uint8_t clientId, uint8_t* payload, size_t length) {
  (void)clientId;
  clearCameraButtonPending("binary");
  const unsigned long startedAt = millis();
  speechBubbleController.onPcmReceived(SpeechBubbleTransport::WebSocket);
  audioController.onBinaryReceived(payload, length);
  noteVoicePerfWsBinary(length, static_cast<uint32_t>(millis() - startedAt));
}

void onWsConnection(uint8_t clientId, bool connected) {
  (void)clientId;
  wsClientConnected = connected;
  updateMicStatusOverlay();
  if (displayOn) {
    applyDisplayBrightness();
  }
  applyThermalFaceMode();
  if (connected) {
    const unsigned long now = millis();
    wsAudioSettleUntilMs = now + AUDIO_WS_CONNECT_SETTLE_MS;
    audioController.deferMicCaptureUntil(wsAudioSettleUntilMs);
    Serial.printf("[ws] audio settle %u ms\n", AUDIO_WS_CONNECT_SETTLE_MS);
    applyAffectionResult(affectionController.resetTransient(), now, false);
    sendAffectionState();
    sendStepsSnapshot();
    sendInteractionEvent("session_start", "instant", now);
  } else {
    wsAudioSettleUntilMs = 0;
#if STACKCHAN_TIMEKEEPER_ENABLED
    if (pendingTimekeeperAnnouncement.active) {
      pendingTimekeeperAnnouncement.sentAfterLastDeviceInfo = false;
    }
#endif
    if (speechBubbleController.activeForTransport(SpeechBubbleTransport::WebSocket)) {
      speechBubbleController.reset("websocket_disconnect");
    }
    clearCameraButtonPending("disconnect");
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
    disconnectPhoneCameraTransport(PhoneCameraTransport::WebSocket, "disconnect");
#endif
  }
  if (infoScreenVisible && displayOn) {
    drawInfoScreen();
  }
}

void redrawNetworkSettingsIfVisible() {
  if (displayOn && infoScreenVisible && settingsPage == SettingsPage::Network &&
      activeNetworkQr == NetworkQrType::None) {
    drawInfoScreen();
  }
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    wifiGotIpReady = true;
    wifiDisconnectExpected = false;
    requestStreetPassNtpResync();
    return;
  }
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiGotIpReady = false;
    if (!wifiDisconnectExpected) {
      wifiReconnectEventReason = info.wifi_sta_disconnected.reason;
      wifiReconnectEventPending = true;
    }
    return;
  }
  if (event == ARDUINO_EVENT_WIFI_STA_LOST_IP && !wifiDisconnectExpected) {
    wifiGotIpReady = false;
    wifiReconnectEventReason = 0xFD;
    wifiReconnectEventPending = true;
  }
}

void beginStaWifiConnection(const char* reason, bool resetRadio = false) {
  stopServers(reason != nullptr ? reason : "wifi_reconnect");
  wifiConnectStartedMs = millis();
  lastWifiStatus = WiFi.status();
  WiFi.persistent(false);
  wifiDisconnectExpected = true;
  wifiGotIpReady = false;
  WiFi.disconnect(false, false);
  if (resetRadio) {
    WiFi.mode(WIFI_OFF);
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(true);
    Serial.println("[wifi] radio reset");
  }
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

  if (wifiReconnectEventPending) {
    const uint8_t reason = wifiReconnectEventReason;
    wifiReconnectEventPending = false;
    Serial.printf("[wifi] recovery requested reason=%u status=%d\n",
                  static_cast<unsigned>(reason),
                  static_cast<int>(WiFi.status()));
    wifiConnectAttempts = 0;
    wifiConnectStartedMs = 0;
    beginStaWifiConnection("wifi_event", true);
    return;
  }

  const wl_status_t status = WiFi.status();
  if (status != lastWifiStatus) {
    Serial.printf("[wifi] status=%d\n", static_cast<int>(status));
    lastWifiStatus = status;
    redrawNetworkSettingsIfVisible();
  }

  if (status == WL_CONNECTED) {
    if (!wifiGotIpReady) {
      return;
    }
    wifiDisconnectExpected = false;
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
  beginStaWifiConnection("timeout", true);
}

void updateScreenPetting(unsigned long now, const m5::touch_detail_t& touch) {
#if STACKCHAN_HAS_SCREEN_TOUCH_PETTING
  if (!displayOn || infoScreenVisible
#if STACKCHAN_DEVICE_STOPWATCH
      || experienceMode == ExperienceMode::Timekeeper
      || experienceMode == ExperienceMode::Travel
#endif
  ) {
    screenPettingCandidate = false;
    screenPettingTouchActive = false;
    screenPettingCandidateSinceMs = 0;
    screenPettingReleaseSinceMs = 0;
    screenPettingTravelPx = 0;
    setPettingActive(false, now);
    return;
  }

#if STACKCHAN_DEVICE_STOPWATCH
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  if (touchStartedInCircle(touch,
                           roundPhoneCameraButtonCenterX(),
                           roundPhoneCameraButtonCenterY(),
                           roundPhoneCameraButtonRadius())) {
    screenPettingCandidate = false;
    screenPettingTouchActive = false;
    screenPettingCandidateSinceMs = 0;
    screenPettingReleaseSinceMs = 0;
    screenPettingTravelPx = 0;
    setPettingActive(false, now);
    return;
  }
#endif
  if (appClientConnected() &&
      touchStartedInCircle(touch,
                           roundMicButtonCenterX(),
                           roundMicButtonCenterY(),
                           roundMicButtonRadius())) {
    screenPettingCandidate = false;
    screenPettingTouchActive = false;
    screenPettingCandidateSinceMs = 0;
    screenPettingReleaseSinceMs = 0;
    screenPettingTravelPx = 0;
    setPettingActive(false, now);
    return;
  }
#endif

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
#if STACKCHAN_DEVICE_STOPWATCH && STACKCHAN_PET_ANIMATION_ENABLED
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
  const FaceAssetMode assetMode = faceController.faceAssetStatus().mode;
  if (!faceAssetModeUsesAnimation(assetMode)) {
    guruguruFaceAssetsReady = false;
    guruguruFaceAssetsChecked = true;
    return false;
  }
  char path[16];
#if STACKCHAN_DEVICE_STOPWATCH
  const uint8_t centerIndex = faceAssetModeUsesV2(assetMode)
                                ? STACKCHAN_GURUGURU_FACE_CENTER_INDEX
                                : 16;
  snprintf(path, sizeof(path), "/dir%u.png", static_cast<unsigned>(centerIndex));
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
#if !STACKCHAN_DEVICE_STOPWATCH
         !appClientConnected() &&
#endif
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
#if !STACKCHAN_DEVICE_STOPWATCH
  if (guruguruFaceMode && appClientConnected()) {
    Serial.println("[guruguru] disabled: client connected");
    setGuruguruFaceMode(false, now);
    return;
  }
#endif

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
#if !STACKCHAN_DEVICE_STOPWATCH
  if (enabled && appClientConnected()) {
    enabled = false;
  }
#endif
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

void sendExperienceModeChanged(ExperienceMode previousMode) {
  if (!appClientConnected()) {
    return;
  }
  ensureDeviceId();
  ensureBootId();
  JsonDocument doc;
  doc["type"] = "experience.mode.changed";
  doc["version"] = 1;
  doc["deviceId"] = deviceId;
  doc["bootId"] = bootId;
  doc["mode"] = experienceModeName(experienceMode);
  doc["previousMode"] = experienceModeName(previousMode);
  doc["revision"] = experienceModeRevision;
  sendJsonDocument(doc);
}

void requestExperienceMode(ExperienceMode mode, unsigned long now) {
  if (mode == experienceMode) {
    pendingExperienceModeValid = false;
    return;
  }
#if STACKCHAN_GURUGURU_FACE_ENABLED
  if (mode == ExperienceMode::Guruguru && !guruguruFaceAssetsAvailable()) {
    Serial.println("[mode] guruguru rejected: assets missing");
    return;
  }
#else
  if (mode == ExperienceMode::Guruguru) {
    return;
  }
#endif

  if (audioBusyForUiEffects()) {
    pendingExperienceMode = mode;
    pendingExperienceModeValid = true;
    Serial.printf("[mode] deferred target=%s until playback drains\n", experienceModeName(mode));
    return;
  }

  pendingExperienceModeValid = false;
  const ExperienceMode previousMode = experienceMode;
#if STACKCHAN_TIMEKEEPER_ENABLED
  travelFacePickerVisible = false;
  if (previousMode == ExperienceMode::Timekeeper) {
    flushPendingTimekeeperSmileResult(false);
    const TimekeeperEvent event = timekeeperController.suspend(monotonicMs(), "mode_changed");
    handleTimekeeperEvent(event, false);
  }
  lastTimekeeperUiValueMs = UINT64_MAX;
#endif
  if (infoScreenVisible) {
    setInfoScreenVisible(false);
  }
  if (currentState == ChanState::Listening ||
      (mode == ExperienceMode::Travel && currentState != ChanState::Idle)) {
    setState(ChanState::Idle);
  }

#if STACKCHAN_TIMEKEEPER_ENABLED
  if (previousMode == ExperienceMode::Travel || mode == ExperienceMode::Travel) {
    resetTravelPhotoFace();
  }
  if (mode == ExperienceMode::Travel) {
    setPettingActive(false, now);
    setShakeActive(false, now);
    speechBubbleController.reset("travel_mode_enter");
  }
#endif

#if STACKCHAN_GURUGURU_FACE_ENABLED
  setGuruguruFaceMode(mode == ExperienceMode::Guruguru, now);
#endif
  experienceMode = mode;
#if STACKCHAN_TIMEKEEPER_ENABLED
  if (experienceMode == ExperienceMode::Timekeeper) {
    // Do not carry a conversation-mode bubble into the Timekeeper screen.
    speechBubbleController.reset("timekeeper_mode_enter");
    setPettingActive(false, now);
    setShakeActive(false, now);
  }
#endif
  faceController.setTimekeeperPresentationMode(
    experienceMode == ExperienceMode::Timekeeper ||
    experienceMode == ExperienceMode::Travel);
  faceController.setAffectionDeltaYOffset(
    experienceMode == ExperienceMode::Timekeeper
#if STACKCHAN_DEVICE_STOPWATCH
      ? 64
#else
      ? 0
#endif
      : 0);
  ++experienceModeRevision;
  if (experienceModeRevision == 0) {
    ++experienceModeRevision;
  }
#if STACKCHAN_TIMEKEEPER_ENABLED
  faceController.setEnabled(displayOn && !infoScreenVisible && !experienceModeMenuVisible &&
                            !timekeeperDurationMenuVisible && !travelFacePickerVisible);
  if (previousMode == ExperienceMode::Travel ||
      experienceMode == ExperienceMode::Travel) {
    faceController.redrawNow();
  }
#else
  faceController.setEnabled(displayOn && !infoScreenVisible);
#endif
  sendExperienceModeChanged(previousMode);
  Serial.printf("[mode] changed previous=%s current=%s revision=%lu\n",
                experienceModeName(previousMode),
                experienceModeName(experienceMode),
                static_cast<unsigned long>(experienceModeRevision));
}

void updatePendingExperienceMode(unsigned long now) {
  if (!displayOn || !pendingExperienceModeValid || audioBusyForUiEffects()) {
    return;
  }
  const ExperienceMode target = pendingExperienceMode;
  pendingExperienceModeValid = false;
  requestExperienceMode(target, now);
}

#if STACKCHAN_TIMEKEEPER_ENABLED
struct TravelPhotoFaceOption {
  const char* path;
  const char* label;
};

constexpr TravelPhotoFaceOption kTravelPhotoFaceOptions[] = {
  // Page 1: expressions that work especially well for travel photos.
  {"/pet_anim_8.jpg", "ハート"},
  {"/pet_anim_10.jpg", "にっこり"},
  {"/travel_wink.jpg", "ウインク"},
  {"/travel_sparkle.jpg", "キラキラ"},
  {"/travel_surprised.jpg", "びっくり"},
  {"/travel_shy.jpg", "照れ顔"},
  {"/travel_delicious.jpg", "おいしい"},
  {"/travel_peace.jpg", "ピース"},
  // Page 2: mood and playful expressions.
  {"/dizzy_01.jpg", "クラクラ"},
  {"/dizzy_09.jpg", "ふらふら"},
  {"/pet_anim_13.jpg", "むすっ"},
  {"/pet_anim_14.jpg", "ぷんぷん"},
  {"/travel_mischief.jpg", "いたずら"},
  {"/travel_teary.jpg", "うるうる"},
  {"/travel_yawn.jpg", "あくび"},
};
constexpr uint8_t kTravelPhotoFaceCount =
  static_cast<uint8_t>(sizeof(kTravelPhotoFaceOptions) /
                       sizeof(kTravelPhotoFaceOptions[0]));
constexpr uint8_t kTravelFacePickerPageSize = 8;
constexpr uint8_t kTravelFacePickerPageCount =
  static_cast<uint8_t>((kTravelPhotoFaceCount + kTravelFacePickerPageSize - 1) /
                       kTravelFacePickerPageSize);

bool selectTravelPhotoFace(uint8_t index) {
  if (experienceMode != ExperienceMode::Travel ||
      index >= kTravelPhotoFaceCount) {
    return false;
  }
  const char* path = kTravelPhotoFaceOptions[index].path;
  if (!LittleFS.exists(path)) {
    Serial.printf("[travel] face missing index=%u path=%s\n",
                  static_cast<unsigned>(index),
                  path);
    return false;
  }
  travelPhotoFaceIndex = static_cast<int8_t>(index);
  faceController.setTravelPhotoFace(path);
  Serial.printf("[travel] face_index=%d path=%s label=%s\n",
                static_cast<int>(travelPhotoFaceIndex),
                path,
                kTravelPhotoFaceOptions[index].label);
  return true;
}

void resetTravelPhotoFace() {
  travelPhotoFaceIndex = -1;
  faceController.setTravelPhotoFace(nullptr);
  Serial.println("[travel] face=normal");
}

void advanceTravelPhotoFace() {
  if (experienceMode != ExperienceMode::Travel) {
    return;
  }
  int16_t candidate = travelPhotoFaceIndex;
  for (uint8_t attempts = 0; attempts < kTravelPhotoFaceCount; ++attempts) {
    candidate = candidate < 0
                  ? 0
                  : (candidate + 1) % kTravelPhotoFaceCount;
    if (selectTravelPhotoFace(static_cast<uint8_t>(candidate))) {
      return;
    }
  }
  resetTravelPhotoFace();
}

void drawJapaneseCentered(const String& text,
                          int32_t x,
                          int32_t y,
                          uint16_t color,
                          uint16_t background = TFT_BLACK) {
  M5.Display.setFont(&fonts::efontJA_12);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(color, background);
  M5.Display.drawString(text, x, y);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextDatum(top_left);
}

constexpr const char* kTravelFacePickerPagePaths[] = {
  "/travel_picker_page_0.jpg",
  "/travel_picker_page_1.jpg",
};

int32_t travelFacePickerDrawSize() {
  return min(M5.Display.width(), M5.Display.height());
}

int32_t travelFacePickerDrawX() {
  return (M5.Display.width() - travelFacePickerDrawSize()) / 2;
}

int32_t travelFacePickerDrawY() {
  return (M5.Display.height() - travelFacePickerDrawSize()) / 2;
}

int32_t travelFacePickerThumbSize() {
#if STACKCHAN_DEVICE_CORES3
  return min(M5.Display.width() * 66 / 320,
             M5.Display.height() * 66 / 240);
#else
  return travelFacePickerDrawSize() * 74 / 386;
#endif
}

void travelFacePickerSlotCenter(uint8_t slot, int32_t& x, int32_t& y) {
#if STACKCHAN_DEVICE_CORES3
  constexpr int16_t kSlotX[] = {44, 121, 198, 275, 44, 121, 198, 275};
  constexpr int16_t kSlotY[] = {62, 62, 62, 62, 140, 140, 140, 140};
  slot %= kTravelFacePickerPageSize;
  x = static_cast<int32_t>(kSlotX[slot]) * M5.Display.width() / 320;
  y = static_cast<int32_t>(kSlotY[slot]) * M5.Display.height() / 240;
#else
  constexpr int16_t kSlotX[] = {193, 292, 334, 292, 193, 94, 52, 94};
  constexpr int16_t kSlotY[] = {52, 92, 193, 294, 334, 294, 193, 92};
  slot %= kTravelFacePickerPageSize;
  const int32_t drawSize = travelFacePickerDrawSize();
  x = travelFacePickerDrawX() +
      static_cast<int32_t>(kSlotX[slot]) * drawSize / 386;
  y = travelFacePickerDrawY() +
      static_cast<int32_t>(kSlotY[slot]) * drawSize / 386;
#endif
}

void drawTravelFacePicker() {
  if (!displayOn || !travelFacePickerVisible) {
    return;
  }

  if (travelFacePickerPage >= kTravelFacePickerPageCount) {
    travelFacePickerPage = 0;
  }
  M5.Display.fillScreen(TFT_BLACK);
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_CORES3
  const char* pagePath = kTravelFacePickerPagePaths[travelFacePickerPage];
  File pageFile = LittleFS.open(pagePath, "r");
  if (pageFile) {
#if STACKCHAN_DEVICE_CORES3
    const float drawScaleX = static_cast<float>(M5.Display.width()) / 320.0f;
    const float drawScaleY = static_cast<float>(M5.Display.height()) / 240.0f;
    const bool pageDrawn = M5.Display.drawJpg(&pageFile,
                                               0,
                                               0,
                                               M5.Display.width(),
                                               M5.Display.height(),
                                               0,
                                               0,
                                               drawScaleX,
                                               drawScaleY,
                                               datum_t::top_left);
#else
    const int32_t drawSize = travelFacePickerDrawSize();
    const float drawScale = static_cast<float>(drawSize) / 386.0f;
    const bool pageDrawn = M5.Display.drawJpg(&pageFile,
                                               travelFacePickerDrawX(),
                                               travelFacePickerDrawY(),
                                               drawSize,
                                               drawSize,
                                               0,
                                               0,
                                               drawScale,
                                               drawScale,
                                               datum_t::top_left);
#endif
    pageFile.close();
    if (!pageDrawn) {
      Serial.printf("[travel.picker] page decode failed path=%s\n", pagePath);
    }
  } else {
    Serial.printf("[travel.picker] page open failed path=%s\n", pagePath);
  }
#endif

  const int32_t thumbSize = travelFacePickerThumbSize();
  const int32_t halfThumb = thumbSize / 2;
  const uint8_t firstIndex = travelFacePickerPage * kTravelFacePickerPageSize;
  for (uint8_t slot = 0; slot < kTravelFacePickerPageSize; ++slot) {
    const uint8_t faceIndex = firstIndex + slot;
    if (faceIndex >= kTravelPhotoFaceCount) {
      break;
    }
    int32_t x = 0;
    int32_t y = 0;
    travelFacePickerSlotCenter(slot, x, y);
    const bool selected = travelPhotoFaceIndex == static_cast<int8_t>(faceIndex);
    const uint16_t borderColor = selected
                                   ? M5.Display.color565(255, 210, 32)
                                   : M5.Display.color565(112, 116, 122);
    M5.Display.drawRoundRect(x - halfThumb - 3,
                             y - halfThumb - 3,
                             thumbSize + 6,
                             thumbSize + 6,
                             11,
                             borderColor);
    if (selected) {
      M5.Display.drawRoundRect(x - halfThumb - 5,
                               y - halfThumb - 5,
                               thumbSize + 10,
                               thumbSize + 10,
                               13,
                               borderColor);
    }
  }

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  const bool normalSelected = travelPhotoFaceIndex < 0;
  const uint16_t centerBorder = normalSelected
                                  ? M5.Display.color565(255, 210, 32)
                                  : M5.Display.color565(96, 100, 106);
#if STACKCHAN_DEVICE_CORES3
  const int32_t normalW = M5.Display.width() * 78 / 320;
  const int32_t normalH = M5.Display.height() * 26 / 240;
  const int32_t normalX = cx - normalW / 2;
  const int32_t normalY = M5.Display.height() * 190 / 240;
  M5.Display.fillRoundRect(normalX,
                           normalY,
                           normalW,
                           normalH,
                           8,
                           M5.Display.color565(28, 30, 34));
  M5.Display.drawRoundRect(normalX,
                           normalY,
                           normalW,
                           normalH,
                           8,
                           centerBorder);
  if (normalSelected) {
    M5.Display.drawRoundRect(normalX - 2,
                             normalY - 2,
                             normalW + 4,
                             normalH + 4,
                             10,
                             centerBorder);
  }
  drawJapaneseCentered(travelFacePickerPage == 0 ? "写真向け" : "気分・ネタ",
                       cx,
                       M5.Display.height() * 12 / 240,
                       M5.Display.color565(156, 162, 170));
  drawJapaneseCentered("通常",
                       cx,
                       normalY + normalH / 2,
                       TFT_WHITE,
                       M5.Display.color565(28, 30, 34));
  const int32_t pageDotY = M5.Display.height() * 229 / 240;
#else
  M5.Display.fillCircle(cx, cy, 49, TFT_BLACK);
  M5.Display.drawCircle(cx, cy, 48, centerBorder);
  if (normalSelected) {
    M5.Display.drawCircle(cx, cy, 46, centerBorder);
  }
  drawJapaneseCentered(travelFacePickerPage == 0 ? "写真向け" : "気分・ネタ",
                       cx,
                       cy - 22,
                       M5.Display.color565(156, 162, 170));
  drawJapaneseCentered("通常", cx, cy + 1, TFT_WHITE);
  const int32_t pageDotY = cy + 27;
#endif
  for (uint8_t page = 0; page < kTravelFacePickerPageCount; ++page) {
    const int32_t dotX = cx +
      (static_cast<int32_t>(page) * 14) -
      (static_cast<int32_t>(kTravelFacePickerPageCount - 1) * 7);
    M5.Display.fillCircle(dotX,
                          pageDotY,
                          page == travelFacePickerPage ? 4 : 3,
                          page == travelFacePickerPage
                            ? TFT_WHITE
                            : M5.Display.color565(76, 80, 86));
  }
}

void showTravelFacePicker() {
#if STACKCHAN_DEVICE_STOPWATCH || STACKCHAN_DEVICE_CORES3
  if (!displayOn || experienceMode != ExperienceMode::Travel ||
      infoScreenVisible || experienceModeMenuVisible ||
      timekeeperDurationMenuVisible || travelFacePickerVisible) {
    return;
  }
  const uint8_t requestedPage = travelPhotoFaceIndex < 0
                                  ? 0
                                  : static_cast<uint8_t>(travelPhotoFaceIndex) /
                                      kTravelFacePickerPageSize;
  const char* pagePath = kTravelFacePickerPagePaths[requestedPage];
  if (!LittleFS.exists(pagePath)) {
    Serial.printf("[travel.picker] unavailable path=%s; falling back to next face\n",
                  pagePath);
    advanceTravelPhotoFace();
    return;
  }
  travelFacePickerPage = requestedPage;
  travelFacePickerVisible = true;
  faceController.setEnabled(false);
  drawTravelFacePicker();
  Serial.printf("[travel.picker] show page=%u\n",
                static_cast<unsigned>(travelFacePickerPage));
#else
  advanceTravelPhotoFace();
#endif
}

void hideTravelFacePicker(bool redrawFace) {
  if (!travelFacePickerVisible) {
    return;
  }
  travelFacePickerVisible = false;
  faceController.setEnabled(displayOn && !infoScreenVisible &&
                            !experienceModeMenuVisible &&
                            !timekeeperDurationMenuVisible);
  if (redrawFace && displayOn && !infoScreenVisible &&
      !experienceModeMenuVisible && !timekeeperDurationMenuVisible) {
    faceController.redrawNow();
  }
  Serial.println("[travel.picker] hide");
}

bool updateTravelFacePickerTouch(const m5::touch_detail_t& touch,
                                 unsigned long now) {
  (void)now;
  if (!travelFacePickerVisible) {
    return false;
  }
  if (touch.wasFlicked()) {
    const int32_t distanceX = touch.distanceX();
    const int32_t distanceY = touch.distanceY();
    if (abs(distanceX) > abs(distanceY) && abs(distanceX) >= 48 &&
        kTravelFacePickerPageCount > 1) {
      const int32_t direction = distanceX < 0 ? 1 : -1;
      travelFacePickerPage = static_cast<uint8_t>(
        (static_cast<int32_t>(travelFacePickerPage) + direction +
         kTravelFacePickerPageCount) % kTravelFacePickerPageCount);
      drawTravelFacePicker();
      Serial.printf("[travel.picker] swipe page=%u\n",
                    static_cast<unsigned>(travelFacePickerPage));
    }
    return true;
  }
  if (!touch.wasClicked()) {
    return true;
  }

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
#if STACKCHAN_DEVICE_CORES3
  const int32_t normalW = M5.Display.width() * 78 / 320;
  const int32_t normalH = M5.Display.height() * 26 / 240;
  const int32_t normalX = cx - normalW / 2;
  const int32_t normalY = M5.Display.height() * 190 / 240;
  const bool normalTapped = touchIn(touch,
                                    normalX - 6,
                                    normalY - 5,
                                    normalW + 12,
                                    normalH + 10);
#else
  const int32_t centerDx = touch.x - cx;
  const int32_t centerDy = touch.y - cy;
  const bool normalTapped = centerDx * centerDx + centerDy * centerDy <= 49 * 49;
#endif
  if (normalTapped) {
    resetTravelPhotoFace();
    hideTravelFacePicker(true);
    Serial.println("[travel.picker] selected normal");
    return true;
  }

  const int32_t hitRadius = travelFacePickerThumbSize() / 2 + 8;
  const int32_t hitRadiusSquared = hitRadius * hitRadius;
  for (uint8_t slot = 0; slot < kTravelFacePickerPageSize; ++slot) {
    const uint8_t faceIndex = travelFacePickerPage * kTravelFacePickerPageSize + slot;
    if (faceIndex >= kTravelPhotoFaceCount) {
      break;
    }
    int32_t x = 0;
    int32_t y = 0;
    travelFacePickerSlotCenter(slot, x, y);
    const int32_t dx = touch.x - x;
    const int32_t dy = touch.y - y;
    if (dx * dx + dy * dy > hitRadiusSquared) {
      continue;
    }
    if (selectTravelPhotoFace(faceIndex)) {
      hideTravelFacePicker(true);
    }
    return true;
  }
  return true;
}

void handleTravelYellowClickCount(uint8_t clickCount) {
  if (experienceMode != ExperienceMode::Travel || infoScreenVisible ||
      experienceModeMenuVisible || clickCount == 0) {
    return;
  }
  if (clickCount == 1) {
    if (travelFacePickerVisible) {
      hideTravelFacePicker(true);
      Serial.println("[button] yellow travel_picker_close");
    } else {
      showTravelFacePicker();
      Serial.println("[button] yellow travel_picker_open");
    }
    return;
  }
  resetTravelPhotoFace();
  if (travelFacePickerVisible) {
    hideTravelFacePicker(true);
  }
  Serial.printf("[button] yellow travel_reset clicks=%u\n",
                static_cast<unsigned>(clickCount));
}

#if STACKCHAN_DEVICE_CORES3
bool handleTravelScreenDoubleTap(const m5::touch_detail_t& touch,
                                 unsigned long now) {
  if (experienceMode != ExperienceMode::Travel || !displayOn ||
      infoScreenVisible || experienceModeMenuVisible ||
      timekeeperDurationMenuVisible || travelFacePickerVisible) {
    travelScreenFirstTapMs = 0;
    return false;
  }

  if (travelScreenFirstTapMs != 0 &&
      now - travelScreenFirstTapMs > TRAVEL_SCREEN_DOUBLE_TAP_MS) {
    travelScreenFirstTapMs = 0;
  }
  if (!touch.wasClicked()) {
    return false;
  }

  if (travelScreenFirstTapMs != 0) {
    const int32_t dx = touch.x - travelScreenFirstTapX;
    const int32_t dy = touch.y - travelScreenFirstTapY;
    const int32_t maxDistanceSquared =
      TRAVEL_SCREEN_DOUBLE_TAP_DISTANCE_PX *
      TRAVEL_SCREEN_DOUBLE_TAP_DISTANCE_PX;
    if (dx * dx + dy * dy <= maxDistanceSquared) {
      travelScreenFirstTapMs = 0;
      resetTravelPhotoFace();
      faceController.redrawNow();
      Serial.println("[travel.input] screen double tap expression_reset");
      return true;
    }
  }

  travelScreenFirstTapMs = now;
  travelScreenFirstTapX = touch.x;
  travelScreenFirstTapY = touch.y;
  return false;
}
#endif

uint16_t experienceModeSectorColor(ExperienceMode mode, bool selected) {
  (void)mode;
  return selected ? M5.Display.color565(48, 48, 48)
                  : M5.Display.color565(104, 104, 104);
}

void fillExperienceModeSector(float startAngle,
                              float endAngle,
                              ExperienceMode mode) {
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  const int32_t radius = min(M5.Display.width(), M5.Display.height()) / 2 - 2;
  const bool selected = experienceModeMenuSelection == mode;
  M5.Display.fillArc(cx,
                     cy,
                     radius,
                     0,
                     startAngle,
                     endAngle,
                     experienceModeSectorColor(mode, selected));
}

void drawExperienceModeLabel(const char* label,
                             int32_t x,
                             int32_t y,
                             ExperienceMode mode) {
  const bool selected = experienceModeMenuSelection == mode;
  M5.Display.setFont(&fonts::efontJA_12);
  M5.Display.setTextSize(selected ? 1.9f : 1.7f);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE);
  if (selected) {
    // The pressed face sits slightly lower. Repeating the transparent glyph
    // one pixel either side gives the selected label a clear bold weight.
    y += 4;
    M5.Display.drawString(label, x - 1, y);
    M5.Display.drawString(label, x + 1, y);
  }
  M5.Display.drawString(label, x, y);
  M5.Display.setFont(&fonts::Font0);
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_left);
}

void experienceModeSectorGeometry(ExperienceMode mode,
                                  float& boundaryAngleA,
                                  float& boundaryAngleB,
                                  float& arcStart,
                                  float& arcEnd) {
  boundaryAngleA = 315.0f;
  boundaryAngleB = 45.0f;
  arcStart = 225.0f;
  arcEnd = 315.0f;
  if (mode == ExperienceMode::Guruguru) {
    boundaryAngleA = 45.0f;
    boundaryAngleB = 135.0f;
    arcStart = 315.0f;
    arcEnd = 45.0f;
  } else if (mode == ExperienceMode::Timekeeper) {
    boundaryAngleA = 135.0f;
    boundaryAngleB = 225.0f;
    arcStart = 45.0f;
    arcEnd = 135.0f;
  } else if (mode == ExperienceMode::Travel) {
    boundaryAngleA = 225.0f;
    boundaryAngleB = 315.0f;
    arcStart = 135.0f;
    arcEnd = 225.0f;
  }
}

void drawExperienceModeArc(int32_t cx,
                           int32_t cy,
                           int32_t outerRadius,
                           int32_t innerRadius,
                           ExperienceMode mode,
                           uint16_t color) {
  float boundaryAngleA = 315.0f;
  float boundaryAngleB = 45.0f;
  float arcStart = 225.0f;
  float arcEnd = 315.0f;
  experienceModeSectorGeometry(mode,
                               boundaryAngleA,
                               boundaryAngleB,
                               arcStart,
                               arcEnd);
  if (arcStart > arcEnd) {
    M5.Display.fillArc(cx, cy, outerRadius, innerRadius, arcStart, 360.0f, color);
    M5.Display.fillArc(cx, cy, outerRadius, innerRadius, 0.0f, arcEnd, color);
  } else {
    M5.Display.fillArc(cx, cy, outerRadius, innerRadius, arcStart, arcEnd, color);
  }
}

void drawExperienceModeBevel(int32_t cx,
                             int32_t cy,
                             int32_t radius,
                             ExperienceMode mode) {
  const bool selected = experienceModeMenuSelection == mode;
  if (!selected) {
    // A bright outer lip makes an unpressed sector appear raised.
    drawExperienceModeArc(cx,
                          cy,
                          radius,
                          radius - 6,
                          mode,
                          M5.Display.color565(214, 214, 214));
    drawExperienceModeArc(cx,
                          cy,
                          radius - 7,
                          radius - 10,
                          mode,
                          M5.Display.color565(142, 142, 142));
    return;
  }

  // The selected sector loses the bright lip and gains an inset black rim,
  // like a physical button pushed below the surrounding surface.
  drawExperienceModeArc(cx, cy, radius, radius - 12, mode, TFT_BLACK);
  drawExperienceModeArc(cx,
                        cy,
                        radius - 12,
                        radius - 16,
                        mode,
                        M5.Display.color565(92, 92, 92));

  float boundaryAngleA = 0.0f;
  float boundaryAngleB = 0.0f;
  float arcStart = 0.0f;
  float arcEnd = 0.0f;
  experienceModeSectorGeometry(mode,
                               boundaryAngleA,
                               boundaryAngleB,
                               arcStart,
                               arcEnd);
  const float boundaries[] = {boundaryAngleA, boundaryAngleB};
  for (float angle : boundaries) {
    const float radians = angle * DEG_TO_RAD;
    const int32_t x = cx + static_cast<int32_t>(sinf(radians) * radius);
    const int32_t y = cy - static_cast<int32_t>(cosf(radians) * radius);
    M5.Display.drawWideLine(cx, cy, x, y, 3.5f, TFT_BLACK);
  }
}

void drawExperienceModeMenu() {
  if (!displayOn || !experienceModeMenuVisible) {
    return;
  }
#if STACKCHAN_DEVICE_CORES3
  {
  // Use the entire rectangular display as four pizza-like buttons. Lines from
  // the center to each corner form top/right/bottom/left triangular sectors.
  const int32_t width = M5.Display.width();
  const int32_t height = M5.Display.height();
  const int32_t cx = width / 2;
  const int32_t cy = height / 2;
  const auto sectorFill = [](ExperienceMode mode) {
    return experienceModeSectorColor(
      mode,
      experienceModeMenuSelection == mode);
  };
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillTriangle(0, 0, width - 1, 0, cx, cy,
                          sectorFill(ExperienceMode::Conversation));
  M5.Display.fillTriangle(width - 1, 0, width - 1, height - 1, cx, cy,
                          sectorFill(ExperienceMode::Guruguru));
  M5.Display.fillTriangle(width - 1, height - 1, 0, height - 1, cx, cy,
                          sectorFill(ExperienceMode::Timekeeper));
  M5.Display.fillTriangle(0, height - 1, 0, 0, cx, cy,
                          sectorFill(ExperienceMode::Travel));

  const uint16_t separator = M5.Display.color565(28, 28, 28);
  const uint16_t highlight = M5.Display.color565(200, 200, 200);
  M5.Display.drawWideLine(cx, cy, 0, 0, 3.0f, separator);
  M5.Display.drawWideLine(cx, cy, width - 1, 0, 3.0f, separator);
  M5.Display.drawWideLine(cx, cy, width - 1, height - 1, 3.0f, separator);
  M5.Display.drawWideLine(cx, cy, 0, height - 1, 3.0f, separator);
  // A bright top/left lip and dark bottom/right lip preserve the raised-button
  // appearance used by the round StopWatch selector.
  M5.Display.drawFastHLine(1, 1, width - 2, highlight);
  M5.Display.drawFastVLine(1, 1, height - 2, highlight);
  M5.Display.drawFastHLine(1, height - 2, width - 2, TFT_BLACK);
  M5.Display.drawFastVLine(width - 2, 1, height - 2, TFT_BLACK);
  M5.Display.fillCircle(cx, cy, 6, separator);

  drawExperienceModeLabel("音声対話", cx, height / 6,
                          ExperienceMode::Conversation);
  drawExperienceModeLabel("ぐるぐる", width * 5 / 6, cy,
                          ExperienceMode::Guruguru);
  drawExperienceModeLabel("ストップ", cx, height * 4 / 5 - 12,
                          ExperienceMode::Timekeeper);
  drawExperienceModeLabel("ウォッチ", cx, height * 4 / 5 + 14,
                          ExperienceMode::Timekeeper);
  drawExperienceModeLabel("旅モード", width / 6, cy,
                          ExperienceMode::Travel);
  return;
  }
#endif
  M5.Display.fillScreen(TFT_BLACK);
  // M5GFX arc angles use zero at 3 o'clock and advance clockwise. Match the
  // four touch quadrants: top, right, bottom, and left.
  fillExperienceModeSector(225.0f, 315.0f, ExperienceMode::Conversation);
  fillExperienceModeSector(315.0f, 360.0f, ExperienceMode::Guruguru);
  fillExperienceModeSector(0.0f, 45.0f, ExperienceMode::Guruguru);
  fillExperienceModeSector(45.0f, 135.0f, ExperienceMode::Timekeeper);
  fillExperienceModeSector(135.0f, 225.0f, ExperienceMode::Travel);

  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  const int32_t radius = min(M5.Display.width(), M5.Display.height()) / 2 - 2;
  const uint16_t separator = M5.Display.color565(28, 28, 28);
  const float boundaryAngles[] = {45.0f, 135.0f, 225.0f, 315.0f};
  for (float angle : boundaryAngles) {
    const float radians = angle * DEG_TO_RAD;
    const int32_t x = cx + static_cast<int32_t>(sinf(radians) * radius);
    const int32_t y = cy - static_cast<int32_t>(cosf(radians) * radius);
    M5.Display.drawWideLine(cx, cy, x, y, 2.0f, separator);
  }
  drawExperienceModeBevel(cx, cy, radius, ExperienceMode::Conversation);
  drawExperienceModeBevel(cx, cy, radius, ExperienceMode::Guruguru);
  drawExperienceModeBevel(cx, cy, radius, ExperienceMode::Timekeeper);
  drawExperienceModeBevel(cx, cy, radius, ExperienceMode::Travel);
  M5.Display.drawCircle(cx, cy, radius, TFT_BLACK);
  M5.Display.fillCircle(cx, cy, 6, separator);

  drawExperienceModeLabel("音声対話", cx, cy - 91, ExperienceMode::Conversation);
  drawExperienceModeLabel("ぐるぐる", cx + 88, cy, ExperienceMode::Guruguru);
  drawExperienceModeLabel("ストップ", cx, cy + 75, ExperienceMode::Timekeeper);
  drawExperienceModeLabel("ウォッチ", cx, cy + 99, ExperienceMode::Timekeeper);
  drawExperienceModeLabel("旅モード", cx - 88, cy, ExperienceMode::Travel);
}

void showExperienceModeMenu() {
  if (!displayOn || experienceModeMenuVisible) {
    return;
  }
  if (travelFacePickerVisible) {
    travelFacePickerVisible = false;
    Serial.println("[travel.picker] close for mode menu");
  }
  timekeeperDurationMenuVisible = false;
  experienceModeMenuVisible = true;
  experienceModeMenuSelection = pendingExperienceModeValid
                                  ? pendingExperienceMode
                                  : experienceMode;
  faceController.setEnabled(false);
  if (infoScreenVisible) {
    setInfoScreenVisible(false);
  }
  drawExperienceModeMenu();
}

void hideExperienceModeMenu() {
  if (!experienceModeMenuVisible) {
    return;
  }
  experienceModeMenuVisible = false;
  faceController.setEnabled(displayOn && !infoScreenVisible &&
                            !timekeeperDurationMenuVisible &&
                            !travelFacePickerVisible);
  if (displayOn && !infoScreenVisible) {
    faceController.redrawNow();
    if (experienceMode == ExperienceMode::Timekeeper) {
      drawTimekeeperOverlay(monotonicMs(), true);
    }
  }
}

bool updateExperienceModeMenuTouch(const m5::touch_detail_t& touch, unsigned long now) {
  if (!experienceModeMenuVisible) {
    return false;
  }
  if (!touch.wasClicked()) {
    return true;
  }
#if STACKCHAN_DEVICE_CORES3
  {
  const int32_t width = M5.Display.width();
  const int32_t height = M5.Display.height();
  const int32_t dx = touch.x - width / 2;
  const int32_t dy = touch.y - height / 2;
  // Compare aspect-ratio-normalized distances. This makes the boundaries the
  // exact center-to-corner diagonals of the rectangular screen.
  const int64_t horizontalWeight =
    static_cast<int64_t>(abs(dx)) * static_cast<int64_t>(height);
  const int64_t verticalWeight =
    static_cast<int64_t>(abs(dy)) * static_cast<int64_t>(width);
  ExperienceMode selected;
  if (horizontalWeight > verticalWeight) {
    selected = dx >= 0 ? ExperienceMode::Guruguru : ExperienceMode::Travel;
  } else {
    selected = dy >= 0 ? ExperienceMode::Timekeeper
                       : ExperienceMode::Conversation;
  }
  experienceModeMenuSelection = selected;
  requestExperienceMode(selected, now);
  hideExperienceModeMenu();
  return true;
  }
#endif
  const int32_t cx = M5.Display.width() / 2;
  const int32_t cy = M5.Display.height() / 2;
  const int32_t radius = min(M5.Display.width(), M5.Display.height()) / 2;
  const int32_t dx = touch.x - cx;
  const int32_t dy = touch.y - cy;
  if (dx * dx + dy * dy > radius * radius) {
#if STACKCHAN_DEVICE_CORES3
    hideExperienceModeMenu();
#endif
    return true;
  }

  float angle = atan2f(static_cast<float>(dx), static_cast<float>(-dy)) * 180.0f / PI;
  if (angle < 0.0f) {
    angle += 360.0f;
  }
  ExperienceMode selected;
  if (angle >= 315.0f || angle < 45.0f) {
    selected = ExperienceMode::Conversation;
  } else if (angle < 135.0f) {
    selected = ExperienceMode::Guruguru;
  } else if (angle < 225.0f) {
    selected = ExperienceMode::Timekeeper;
  } else {
    selected = ExperienceMode::Travel;
  }

  experienceModeMenuSelection = selected;
  requestExperienceMode(selected, now);
#if STACKCHAN_DEVICE_CORES3
  // The CoreS3 menu was opened by a completed edge swipe, so a sector tap
  // selects immediately and returns to the chosen experience.
  hideExperienceModeMenu();
#else
  // Keep the selector visible until the physical yellow button is released.
  // Repaint only when the selection changes; the face renderer remains paused.
  drawExperienceModeMenu();
#endif
  return true;
}

String formatTimekeeperMs(uint64_t valueMs) {
  const uint64_t totalMinutes = valueMs / 60000ULL;
  const uint32_t seconds = static_cast<uint32_t>((valueMs / 1000ULL) % 60ULL);
  const uint32_t milliseconds = static_cast<uint32_t>(valueMs % 1000ULL);
  char text[28];
  if (totalMinutes < 100ULL) {
    snprintf(text,
             sizeof(text),
             "%02lu:%02lu.%03lu",
             static_cast<unsigned long>(totalMinutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(milliseconds));
  } else {
    const uint64_t hours = totalMinutes / 60ULL;
    const uint32_t minutes = static_cast<uint32_t>(totalMinutes % 60ULL);
    snprintf(text,
             sizeof(text),
             "%02llu:%02lu:%02lu.%03lu",
             static_cast<unsigned long long>(hours),
             static_cast<unsigned long>(minutes),
             static_cast<unsigned long>(seconds),
             static_cast<unsigned long>(milliseconds));
  }
  return String(text);
}

struct TimekeeperLayout {
  int32_t tabY = 0;
  int32_t tabH = 0;
  int32_t tabW = 0;
  int32_t tabGap = 0;
  int32_t tabX = 0;
  int32_t panelX = 0;
  int32_t panelY = 0;
  int32_t panelW = 0;
  int32_t panelH = 0;
  int32_t buttonY = 0;
  int32_t buttonH = 0;
  int32_t adjustButtonY = 0;
  int32_t adjustButtonRadius = 0;
  int32_t adjustButtonLeftX = 0;
  int32_t adjustButtonRightX = 0;
  int32_t twoButtonX = 0;
  int32_t twoButtonW = 0;
  int32_t twoButtonGap = 0;
  int32_t threeButtonX = 0;
  int32_t threeButtonW = 0;
  int32_t threeButtonGap = 0;
};

TimekeeperLayout makeTimekeeperLayout(bool speechBubbleVisible) {
  TimekeeperLayout layout;
  const int32_t width = M5.Display.width();
  const int32_t height = M5.Display.height();
  const int32_t size = min(width, height);
  const int32_t cx = width / 2;

#if STACKCHAN_DEVICE_CORES3
  (void)speechBubbleVisible;
  layout.tabH = 32;
  layout.tabW = 82;
  layout.tabGap = 6;
  layout.tabY = 6;
  const int32_t coreTabsW = layout.tabW * 3 + layout.tabGap * 2;
  layout.tabX = cx - coreTabsW / 2;
  layout.panelW = min<int32_t>(232, width - 72);
  // Keep the clock and optional latest-lap line together below the mouth.
  // The CoreS3 face fills the 240 px display height, so a separate middle
  // lap banner would cover the character's mouth/neck area.
  layout.panelH = 48;
  layout.panelX = cx - layout.panelW / 2;
  layout.panelY = height - layout.panelH - 8;
  layout.buttonH = 36;
  layout.buttonY = layout.panelY - layout.buttonH - 5;
  layout.adjustButtonRadius = 24;
  layout.adjustButtonY = height * 51 / 100;
  layout.adjustButtonLeftX = layout.adjustButtonRadius + 4;
  layout.adjustButtonRightX = width - layout.adjustButtonRadius - 4;
  layout.twoButtonW = 92;
  layout.twoButtonGap = 16;
  layout.twoButtonX = cx - (layout.twoButtonW * 2 + layout.twoButtonGap) / 2;
  layout.threeButtonW = 72;
  layout.threeButtonGap = 8;
  layout.threeButtonX = cx - (layout.threeButtonW * 3 + layout.threeButtonGap * 2) / 2;
  return layout;
#endif

  layout.tabH = max<int32_t>(38, size * 9 / 100);
  layout.tabW = max<int32_t>(82, size * 19 / 100);
  layout.tabGap = max<int32_t>(8, size * 2 / 100);
  layout.tabY = max<int32_t>(24, size * 8 / 100);
  const int32_t tabsW = layout.tabW * 3 + layout.tabGap * 2;
  layout.tabX = cx - tabsW / 2;

  layout.panelW = min<int32_t>(360, width - 80);
  layout.panelH = 64;
  layout.panelX = cx - layout.panelW / 2;
  const int32_t faceTop = (height - FACE_IMAGE_HEIGHT) / 2;
  const int32_t belowMouthY = faceTop + FACE_IMAGE_HEIGHT * 3 / 4;
  layout.panelY = speechBubbleVisible
                    ? max<int32_t>(layout.tabY + layout.tabH + 14, height / 5)
                    : max<int32_t>(height - 135, belowMouthY);

  layout.buttonH = 42;
  layout.buttonY = height - 58;
  layout.adjustButtonRadius = max<int32_t>(28, size * 7 / 100);
  layout.adjustButtonY = height * 60 / 100;
  layout.adjustButtonLeftX = layout.adjustButtonRadius + 14;
  layout.adjustButtonRightX = width - layout.adjustButtonRadius - 14;
  layout.twoButtonW = min<int32_t>(112, size * 24 / 100);
  layout.twoButtonGap = max<int32_t>(24, size * 6 / 100);
  layout.twoButtonX = cx - (layout.twoButtonW * 2 + layout.twoButtonGap) / 2;
  layout.threeButtonW = min<int32_t>(76, size * 17 / 100);
  layout.threeButtonGap = max<int32_t>(10, size * 2 / 100);
  layout.threeButtonX = cx - (layout.threeButtonW * 3 + layout.threeButtonGap * 2) / 2;
  return layout;
}

bool ensureTimekeeperCanvas(M5Canvas& canvas,
                            int32_t& allocatedW,
                            int32_t& allocatedH,
                            int32_t width,
                            int32_t height) {
  if (allocatedW == width && allocatedH == height && canvas.getBuffer() != nullptr) {
    return true;
  }
  if (canvas.getBuffer() != nullptr) {
    canvas.deleteSprite();
  }
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  if (canvas.createSprite(width, height) == nullptr) {
    allocatedW = 0;
    allocatedH = 0;
    Serial.printf("[timekeeper.ui] canvas allocation failed size=%ldx%ld\n",
                  static_cast<long>(width),
                  static_cast<long>(height));
    return false;
  }
  allocatedW = width;
  allocatedH = height;
  return true;
}

enum class TimekeeperButtonLayer : uint8_t {
  None = 0,
  CountdownAdjust = 1,
  LatestLap = 2,
  ChallengeControls = 3,
  PomodoroCycles = 4,
};

struct TimekeeperUiCanvasCache {
  M5Canvas tabCanvas;
  M5Canvas panelCanvas;
  M5Canvas buttonCanvas;
  int32_t tabCanvasW = 0;
  int32_t tabCanvasH = 0;
  int32_t panelCanvasW = 0;
  int32_t panelCanvasH = 0;
  int32_t buttonCanvasW = 0;
  int32_t buttonCanvasH = 0;
  TimekeeperLayout layout;
  int32_t buttonX = 0;
  int32_t buttonY = 0;
  TimekeeperButtonLayer buttonLayer = TimekeeperButtonLayer::None;
  bool speechBubbleVisible = false;
  bool ready = false;

  TimekeeperUiCanvasCache()
    : tabCanvas(&M5.Display),
      panelCanvas(&M5.Display),
      buttonCanvas(&M5.Display) {}
};

TimekeeperUiCanvasCache& timekeeperUiCanvasCache() {
  static TimekeeperUiCanvasCache cache;
  return cache;
}

void drawTimekeeperJapaneseText(M5Canvas& target,
                                const String& text,
                                int32_t x,
                                int32_t y,
                                uint16_t foreground,
                                uint16_t background,
                                float scale) {
  target.setFont(&fonts::efontJA_12);
  target.setTextSize(scale);
  target.setTextDatum(middle_center);
  target.setTextColor(foreground, background);
  target.drawString(text, x, y);
  target.setFont(&fonts::Font0);
  target.setTextSize(1);
  target.setTextDatum(top_left);
}

constexpr uint16_t kTimekeeperCountdownPresetsMinutes[] = {1, 3, 5, 10, 30, 60, 120};
constexpr size_t kTimekeeperCountdownPresetCount =
  sizeof(kTimekeeperCountdownPresetsMinutes) /
  sizeof(kTimekeeperCountdownPresetsMinutes[0]);

void timekeeperDurationOptionCenter(size_t index, int32_t& x, int32_t& y) {
  const int32_t width = M5.Display.width();
  const int32_t height = M5.Display.height();
#if STACKCHAN_DEVICE_CORES3
  if (index < 4) {
    x = width * static_cast<int32_t>(index * 2 + 1) / 8;
    y = height * 54 / 100;
  } else {
    const size_t lowerIndex = index - 4;
    x = width * static_cast<int32_t>(lowerIndex * 2 + 1) / 6;
    y = height * 79 / 100;
  }
  return;
#endif
  if (index < 4) {
    x = width * static_cast<int32_t>(index * 2 + 1) / 8;
    y = height * 48 / 100;
  } else {
    const size_t lowerIndex = index - 4;
    x = width * static_cast<int32_t>(lowerIndex * 2 + 1) / 6;
    y = height * 72 / 100;
  }
}

void timekeeperTimerModeButtonBounds(TimekeeperActivity mode,
                                     int32_t& x,
                                     int32_t& y,
                                     int32_t& w,
                                     int32_t& h) {
  const int32_t width = M5.Display.width();
  const int32_t height = M5.Display.height();
#if STACKCHAN_DEVICE_CORES3
  w = min<int32_t>(124, width * 39 / 100);
  h = 42;
  const int32_t coreGap = 10;
  const int32_t coreTotalW = w * 2 + coreGap;
  x = (width - coreTotalW) / 2 +
      (mode == TimekeeperActivity::Pomodoro ? w + coreGap : 0);
  y = 36;
  return;
#endif
  w = min<int32_t>(170, width * 34 / 100);
  h = max<int32_t>(48, height * 10 / 100);
  const int32_t gap = max<int32_t>(12, width * 3 / 100);
  const int32_t totalW = w * 2 + gap;
  x = (width - totalW) / 2 +
      (mode == TimekeeperActivity::Pomodoro ? w + gap : 0);
  y = height * 17 / 100;
}

bool selectTimekeeperTimerSubmode(TimekeeperActivity mode, uint64_t nowMs) {
  if (mode != TimekeeperActivity::Countdown && mode != TimekeeperActivity::Pomodoro) {
    return false;
  }
  if (timekeeperController.activity() != mode) {
    if (timekeeperController.state() != TimekeeperState::Ready) {
      handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
    }
    if (!timekeeperController.selectActivity(mode, nowMs)) {
      return false;
    }
  }
  timekeeperTimerSubmode = mode;
  saveTimekeeperTimerSubmode();
  lastTimekeeperUiValueMs = UINT64_MAX;
  return true;
}

void drawTimekeeperDurationMenu() {
  if (!displayOn || !timekeeperDurationMenuVisible) {
    return;
  }
  const int32_t width = M5.Display.width();
  const int32_t height = M5.Display.height();
  const int32_t radius =
#if STACKCHAN_DEVICE_CORES3
    25;
#else
    max<int32_t>(34, min(width, height) * 8 / 100);
#endif
  const uint16_t background = TFT_BLACK;
  const uint16_t normalFill = M5.Display.color565(18, 22, 26);
  const uint16_t normalBorder = M5.Display.color565(104, 114, 124);
  const uint16_t selectedFill = M5.Display.color565(22, 72, 54);
  const uint16_t selectedBorder = M5.Display.color565(92, 230, 164);
  const uint16_t shadow = M5.Display.color565(2, 4, 6);
  const uint16_t selectedMinutes = static_cast<uint16_t>(
    timekeeperController.countdownDurationMs() / (60ULL * 1000ULL));

  M5.Display.fillScreen(background);
  M5.Display.setFont(&fonts::efontJA_12);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, background);
  M5.Display.setTextSize(1.7f);
  M5.Display.drawString("タイマー設定", width / 2,
#if STACKCHAN_DEVICE_CORES3
                        18);
#else
                        height * 9 / 100);
#endif

  for (const TimekeeperActivity mode : {
         TimekeeperActivity::Countdown,
         TimekeeperActivity::Pomodoro,
       }) {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
    timekeeperTimerModeButtonBounds(mode, x, y, w, h);
    const bool selected = timekeeperController.activity() == mode;
    M5.Display.fillRoundRect(x + 2, y + 4, w, h, 14, shadow);
    M5.Display.fillRoundRect(x, y, w, h, 14, selected ? selectedFill : normalFill);
    M5.Display.drawRoundRect(x, y, w, h, 14, selected ? selectedBorder : normalBorder);
    M5.Display.setFont(&fonts::efontJA_12);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, selected ? selectedFill : normalFill);
    M5.Display.setTextSize(1.45f);
    M5.Display.drawString(mode == TimekeeperActivity::Pomodoro
                            ? "ポモドーロ"
                            : "通常タイマー",
                          x + w / 2,
                          y + h / 2);
  }

  if (timekeeperController.activity() == TimekeeperActivity::Pomodoro) {
    const uint64_t workMinutes =
      timekeeperController.pomodoroWorkDurationMs() / (60ULL * 1000ULL);
    const uint64_t breakMinutes =
      timekeeperController.pomodoroBreakDurationMs() / (60ULL * 1000ULL);
    M5.Display.setFont(&fonts::efontJA_12);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, background);
    M5.Display.setTextSize(2.0f);
    M5.Display.drawString(String("作業 ") + String(workMinutes) + "分",
                          width / 2,
                          height * 47 / 100);
    M5.Display.drawString(String("休憩 ") + String(breakMinutes) + "分",
                          width / 2,
                          height * 61 / 100);
    M5.Display.setTextColor(M5.Display.color565(164, 174, 184), background);
    M5.Display.setTextSize(1.25f);
    M5.Display.drawString("時間はスマホアプリで設定",
                          width / 2,
                          height * 76 / 100);
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(top_left);
    return;
  }

  M5.Display.setFont(&fonts::efontJA_12);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_WHITE, background);
  M5.Display.setTextSize(1.2f);
  M5.Display.drawString("時間（分）", width / 2,
#if STACKCHAN_DEVICE_CORES3
                        94);
#else
                        height * 36 / 100);
#endif

  for (size_t index = 0; index < kTimekeeperCountdownPresetCount; ++index) {
    int32_t x = 0;
    int32_t y = 0;
    timekeeperDurationOptionCenter(index, x, y);
    const uint16_t minutes = kTimekeeperCountdownPresetsMinutes[index];
    const bool selected = minutes == selectedMinutes;
    M5.Display.fillCircle(x + 2, y + 4, radius, shadow);
    M5.Display.fillCircle(x, y, radius, selected ? selectedFill : normalFill);
    M5.Display.drawCircle(x, y, radius, selected ? selectedBorder : normalBorder);
    M5.Display.drawCircle(x, y, radius - 1, selected ? selectedBorder : normalBorder);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE, selected ? selectedFill : normalFill);
    M5.Display.setTextSize(minutes >= 100 ? 2 : 3);
    M5.Display.drawString(String(minutes), x, y);
  }
  M5.Display.setTextSize(1);
  M5.Display.setTextDatum(top_left);
}

void showTimekeeperDurationMenu() {
  if (!displayOn || experienceMode != ExperienceMode::Timekeeper ||
      (timekeeperController.activity() != TimekeeperActivity::Countdown &&
       timekeeperController.activity() != TimekeeperActivity::Pomodoro) ||
      timekeeperController.isRunning()) {
    return;
  }
  timekeeperDurationMenuVisible = true;
  faceController.setEnabled(false);
  drawTimekeeperDurationMenu();
}

void hideTimekeeperDurationMenu(bool redrawTimekeeper) {
  if (!timekeeperDurationMenuVisible) {
    return;
  }
  timekeeperDurationMenuVisible = false;
  faceController.setEnabled(displayOn && !infoScreenVisible &&
                            !experienceModeMenuVisible &&
                            !travelFacePickerVisible);
  if (redrawTimekeeper && displayOn && !infoScreenVisible &&
      experienceMode == ExperienceMode::Timekeeper) {
    drawTimekeeperOverlay(monotonicMs(), true);
  }
}

bool updateTimekeeperDurationMenuTouch(const m5::touch_detail_t& touch,
                                       uint64_t nowMs) {
  if (!timekeeperDurationMenuVisible) {
    return false;
  }
  if (!touch.wasClicked()) {
    return true;
  }
  for (const TimekeeperActivity mode : {
         TimekeeperActivity::Countdown,
         TimekeeperActivity::Pomodoro,
       }) {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;
    timekeeperTimerModeButtonBounds(mode, x, y, w, h);
    if (!touchIn(touch, x, y, w, h)) {
      continue;
    }
    selectTimekeeperTimerSubmode(mode, nowMs);
    drawTimekeeperDurationMenu();
    Serial.printf("[timekeeper.ui] timer submode=%s\n",
                  timekeeperActivityName(mode));
    return true;
  }
  if (timekeeperController.activity() == TimekeeperActivity::Pomodoro) {
    hideTimekeeperDurationMenu(true);
    return true;
  }
  const int32_t radius =
#if STACKCHAN_DEVICE_CORES3
    25;
#else
    max<int32_t>(34,
                 min(M5.Display.width(), M5.Display.height()) * 8 / 100);
#endif
  for (size_t index = 0; index < kTimekeeperCountdownPresetCount; ++index) {
    int32_t x = 0;
    int32_t y = 0;
    timekeeperDurationOptionCenter(index, x, y);
    const int32_t dx = touch.x - x;
    const int32_t dy = touch.y - y;
    if (dx * dx + dy * dy > radius * radius) {
      continue;
    }
    if (timekeeperController.state() != TimekeeperState::Ready) {
      handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
    }
    const uint16_t minutes = kTimekeeperCountdownPresetsMinutes[index];
    timekeeperController.setCountdownMinutes(minutes, nowMs);
    lastTimekeeperUiValueMs = UINT64_MAX;
    Serial.printf("[timekeeper.ui] countdown preset=%u minutes\n",
                  static_cast<unsigned>(minutes));
    hideTimekeeperDurationMenu(true);
    return true;
  }
  hideTimekeeperDurationMenu(true);
  return true;
}

void drawTimekeeperTab(M5Canvas& target,
                       int32_t x,
                       int32_t h,
                       int32_t w,
                       const char* label,
                       TimekeeperActivity activity) {
  const bool active = activity == TimekeeperActivity::Countdown
                        ? (timekeeperController.activity() == TimekeeperActivity::Countdown ||
                           timekeeperController.activity() == TimekeeperActivity::Pomodoro)
                        : timekeeperController.activity() == activity;
  const uint16_t fill = active
                          ? M5.Display.color565(20, 64, 48)
                          : M5.Display.color565(8, 12, 16);
  const uint16_t border = active
                            ? M5.Display.color565(86, 224, 158)
                            : M5.Display.color565(70, 82, 94);
  target.fillRoundRect(x, 0, w, h, 11, fill);
  target.drawRoundRect(x, 0, w, h, 11, border);
  target.drawRoundRect(x + 1, 1, w - 2, h - 2, 10, border);
  drawTimekeeperJapaneseText(target,
                             label,
                             x + w / 2,
                             h / 2,
                             TFT_WHITE,
                             fill,
                             1.25f);
}

void drawTimekeeperAdjustButton(M5Canvas& target,
                                int32_t x,
                                int32_t y,
                                int32_t radius,
                                const char* label) {
  const uint16_t fill = M5.Display.color565(8, 12, 16);
  const uint16_t border = M5.Display.color565(96, 180, 220);
  const uint16_t shadow = M5.Display.color565(2, 4, 6);
  target.fillCircle(x + 2, y + 4, radius, shadow);
  target.fillCircle(x, y, radius, fill);
  target.drawCircle(x, y, radius, border);
  target.drawCircle(x, y, radius - 1, border);
  target.setFont(&fonts::Font0);
  target.setTextDatum(middle_center);
  target.setTextColor(TFT_WHITE, fill);
  target.setTextSize(3);
  target.drawString(label, x, y);
  target.setTextSize(1);
  target.setTextDatum(top_left);
}

const char* timekeeperChallengeDifficultyLabel(
  TimekeeperChallengeDifficulty difficulty) {
  switch (difficulty) {
    case TimekeeperChallengeDifficulty::Low:
      return "低";
    case TimekeeperChallengeDifficulty::Medium:
      return "中";
    case TimekeeperChallengeDifficulty::High:
      return "高";
  }
  return "中";
}

void drawTimekeeperDifficultyButton(M5Canvas& target,
                                    int32_t x,
                                    int32_t y,
                                    int32_t radius) {
  const uint16_t fill = M5.Display.color565(30, 42, 50);
  const uint16_t border = M5.Display.color565(224, 190, 90);
  const uint16_t shadow = M5.Display.color565(2, 4, 6);
  target.fillCircle(x + 2, y + 4, radius, shadow);
  target.fillCircle(x, y, radius, fill);
  target.drawCircle(x, y, radius, border);
  target.drawCircle(x, y, radius - 1, border);
  drawTimekeeperJapaneseText(
    target,
    timekeeperChallengeDifficultyLabel(timekeeperController.challengeDifficulty()),
    x,
    y,
    TFT_WHITE,
    fill,
    1.8f);
}

void drawTimekeeperOverlay(uint64_t nowMs, bool force) {
  if (!displayOn || experienceMode != ExperienceMode::Timekeeper ||
      infoScreenVisible || experienceModeMenuVisible || timekeeperDurationMenuVisible) {
    return;
  }
  const uint64_t valueMs = timekeeperController.displayMs(nowMs);
  const bool running = timekeeperController.isRunning();
  const uint64_t minimumIntervalMs = running ? 33ULL : 250ULL;
  if (!force && lastTimekeeperUiDrawMs != 0 &&
      nowMs - lastTimekeeperUiDrawMs < minimumIntervalMs) {
    return;
  }
  lastTimekeeperUiValueMs = valueMs / minimumIntervalMs;
  lastTimekeeperUiDrawMs = nowMs;

  const bool speechBubbleVisible = faceController.speechBubbleVisible();
  const TimekeeperLayout layout = makeTimekeeperLayout(speechBubbleVisible);
  constexpr uint16_t transparent = 0xF81F;
  TimekeeperUiCanvasCache& cache = timekeeperUiCanvasCache();
  const bool previousCacheReady = cache.ready;
  const TimekeeperButtonLayer previousButtonLayer = cache.buttonLayer;
  cache.ready = false;

  const int32_t tabsW = layout.tabW * 3 + layout.tabGap * 2;
  if (!ensureTimekeeperCanvas(cache.tabCanvas,
                              cache.tabCanvasW,
                              cache.tabCanvasH,
                              tabsW,
                              layout.tabH)) {
    return;
  }
  cache.tabCanvas.fillSprite(transparent);
  drawTimekeeperTab(cache.tabCanvas,
                    0,
                    layout.tabH,
                    layout.tabW,
                    "計測",
                    TimekeeperActivity::Stopwatch);
  drawTimekeeperTab(cache.tabCanvas,
                    layout.tabW + layout.tabGap,
                    layout.tabH,
                    layout.tabW,
                    "タイマー",
                    TimekeeperActivity::Countdown);
  drawTimekeeperTab(cache.tabCanvas,
                    (layout.tabW + layout.tabGap) * 2,
                    layout.tabH,
                    layout.tabW,
                    "ピタリ",
                    TimekeeperActivity::TenSecondChallenge);
  cache.tabCanvas.pushSprite(&M5.Display, layout.tabX, layout.tabY, transparent);

  if (!ensureTimekeeperCanvas(cache.panelCanvas,
                              cache.panelCanvasW,
                              cache.panelCanvasH,
                              layout.panelW,
                              layout.panelH)) {
    return;
  }
  const uint16_t panel = M5.Display.color565(4, 8, 12);
  cache.panelCanvas.fillSprite(transparent);
  cache.panelCanvas.fillRoundRect(0, 0, layout.panelW, layout.panelH, 16, panel);
  cache.panelCanvas.drawRoundRect(0,
                            0,
                            layout.panelW,
                            layout.panelH,
                            16,
                            M5.Display.color565(66, 90, 106));
  const int32_t panelCenterX = layout.panelW / 2;
  const bool pomodoroActive =
    timekeeperController.activity() == TimekeeperActivity::Pomodoro;
#if STACKCHAN_DEVICE_CORES3
  const bool latestLapInClockPanel =
    timekeeperController.activity() == TimekeeperActivity::Stopwatch &&
    timekeeperController.lapCount() > 0;
#else
  constexpr bool latestLapInClockPanel = false;
#endif
  const int32_t timeY = (pomodoroActive || latestLapInClockPanel)
                          ? layout.panelH * 68 / 100
                          : layout.panelH / 2;
  bool hideChallengeTime = false;
  if (timekeeperController.activity() == TimekeeperActivity::TenSecondChallenge &&
      running) {
    switch (timekeeperController.challengeDifficulty()) {
      case TimekeeperChallengeDifficulty::Low:
        hideChallengeTime = false;
        break;
      case TimekeeperChallengeDifficulty::Medium: {
        const uint64_t targetMs = timekeeperController.challengeTargetMs();
        const uint64_t hideAtMs = targetMs / 2ULL;
        hideChallengeTime = valueMs >= hideAtMs;
        break;
      }
      case TimekeeperChallengeDifficulty::High:
        hideChallengeTime = true;
        break;
    }
  }
  if (pomodoroActive) {
    String phaseLabel;
    if (timekeeperController.state() == TimekeeperState::Completed) {
      phaseLabel = String(timekeeperController.pomodoroCycles()) + "セット完了";
    } else if (timekeeperController.state() == TimekeeperState::Ready) {
      phaseLabel = String("ポモドーロ ") +
                   String(timekeeperController.pomodoroCycles()) + "セット";
    } else {
      phaseLabel = timekeeperController.pomodoroPhase() ==
                       TimekeeperPomodoroPhase::Break
                     ? "休憩 "
                     : "作業 ";
      phaseLabel += String(timekeeperController.pomodoroCycleIndex()) + "/" +
                    String(timekeeperController.pomodoroCycles());
    }
    drawTimekeeperJapaneseText(cache.panelCanvas,
                               phaseLabel,
                               panelCenterX,
                               layout.panelH * 22 / 100,
                               M5.Display.color565(170, 226, 255),
                               panel,
                               1.0f);
    const String formattedTime = formatTimekeeperMs(valueMs);
    cache.panelCanvas.setFont(&fonts::Font0);
    cache.panelCanvas.setTextDatum(middle_center);
    cache.panelCanvas.setTextColor(TFT_WHITE, panel);
#if STACKCHAN_DEVICE_CORES3
    cache.panelCanvas.setTextSize(formattedTime.length() <= 9 ? 3 : 2);
#else
    cache.panelCanvas.setTextSize(formattedTime.length() <= 9 ? 4 : 3);
#endif
    cache.panelCanvas.drawString(formattedTime, panelCenterX, timeY);
    cache.panelCanvas.setTextSize(1);
    cache.panelCanvas.setTextDatum(top_left);
  } else if (hideChallengeTime) {
    drawTimekeeperJapaneseText(cache.panelCanvas,
                               "時間はひみつ",
                               panelCenterX,
                               timeY,
                               TFT_WHITE,
                               panel,
                               1.8f);
  } else {
    if (latestLapInClockPanel) {
      const String lapValue =
        formatTimekeeperMs(timekeeperController.lastLapDurationMs());
      const String lapTextValue = String("LAP ") +
                                  String(timekeeperController.lapCount()) +
                                  "  " + lapValue;
      cache.panelCanvas.setFont(&fonts::Font0);
      cache.panelCanvas.setTextDatum(middle_center);
      cache.panelCanvas.setTextColor(M5.Display.color565(136, 220, 255), panel);
#if STACKCHAN_DEVICE_CORES3
      cache.panelCanvas.setTextSize(1);
#else
      cache.panelCanvas.setTextSize(2);
#endif
      cache.panelCanvas.drawString(lapTextValue,
                                   panelCenterX,
                                   layout.panelH * 20 / 100);
      cache.panelCanvas.setTextSize(1);
      cache.panelCanvas.setTextDatum(top_left);
    }
    const String formattedTime = formatTimekeeperMs(valueMs);
    cache.panelCanvas.setFont(&fonts::Font0);
    cache.panelCanvas.setTextDatum(middle_center);
    cache.panelCanvas.setTextColor(TFT_WHITE, panel);
#if STACKCHAN_DEVICE_CORES3
    cache.panelCanvas.setTextSize(
      latestLapInClockPanel ? 3 : (formattedTime.length() <= 9 ? 4 : 3));
#else
    cache.panelCanvas.setTextSize(formattedTime.length() <= 9 ? 5 : 4);
#endif
    cache.panelCanvas.drawString(formattedTime, panelCenterX, timeY);
    cache.panelCanvas.setTextSize(1);
    cache.panelCanvas.setTextDatum(top_left);
  }

  cache.panelCanvas.pushSprite(&M5.Display,
                               layout.panelX,
                               layout.panelY,
                               transparent);

  const TimekeeperActivity activity = timekeeperController.activity();
  const bool countdownAdjustButtons = activity == TimekeeperActivity::Countdown && !running;
  const bool pomodoroCycleButtons =
    activity == TimekeeperActivity::Pomodoro &&
    timekeeperController.state() == TimekeeperState::Ready;
#if STACKCHAN_DEVICE_CORES3
  constexpr bool separateLatestLapPanel = false;
#else
  const bool separateLatestLapPanel =
    activity == TimekeeperActivity::Stopwatch &&
    timekeeperController.lapCount() > 0;
#endif
  const TimekeeperButtonLayer buttonLayer =
    countdownAdjustButtons
      ? TimekeeperButtonLayer::CountdownAdjust
      : (pomodoroCycleButtons
           ? TimekeeperButtonLayer::PomodoroCycles
           : (activity == TimekeeperActivity::TenSecondChallenge
                ? TimekeeperButtonLayer::ChallengeControls
                : (separateLatestLapPanel
                     ? TimekeeperButtonLayer::LatestLap
                     : TimekeeperButtonLayer::None)));
  const int32_t buttonGroupW = layout.twoButtonW * 2 + layout.twoButtonGap;
  const int32_t buttonCanvasW = M5.Display.width();
  const int32_t buttonCanvasH = max<int32_t>(layout.buttonH,
                                             layout.adjustButtonRadius * 2 + 8);
  if (!ensureTimekeeperCanvas(cache.buttonCanvas,
                              cache.buttonCanvasW,
                              cache.buttonCanvasH,
                              buttonCanvasW,
                              buttonCanvasH)) {
    return;
  }
  cache.buttonCanvas.fillSprite(transparent);
  if (countdownAdjustButtons || pomodoroCycleButtons) {
    cache.buttonY = layout.adjustButtonY - layout.adjustButtonRadius - 4;
    const int32_t localCenterY = layout.adjustButtonRadius + 4;
    drawTimekeeperAdjustButton(cache.buttonCanvas,
                               layout.adjustButtonLeftX,
                               localCenterY,
                               layout.adjustButtonRadius,
                               "-1");
    drawTimekeeperAdjustButton(cache.buttonCanvas,
                               layout.adjustButtonRightX,
                               localCenterY,
                               layout.adjustButtonRadius,
                               "+1");
  } else if (activity == TimekeeperActivity::TenSecondChallenge) {
    cache.buttonY = layout.adjustButtonY - layout.adjustButtonRadius - 4;
    char targetSeconds[4];
    snprintf(targetSeconds,
             sizeof(targetSeconds),
             "%u",
             static_cast<unsigned>(timekeeperController.challengeTargetMs() / 1000ULL));
    drawTimekeeperAdjustButton(cache.buttonCanvas,
                               layout.adjustButtonLeftX,
                               layout.adjustButtonRadius + 4,
                               layout.adjustButtonRadius,
                               targetSeconds);
    drawTimekeeperDifficultyButton(cache.buttonCanvas,
                                   layout.adjustButtonRightX,
                                   layout.adjustButtonRadius + 4,
                                   layout.adjustButtonRadius);
  } else if (separateLatestLapPanel) {
    cache.buttonY = layout.buttonY;
    const uint16_t lapPanel = M5.Display.color565(4, 8, 12);
    const uint16_t lapBorder = M5.Display.color565(70, 100, 116);
    const uint16_t lapText = M5.Display.color565(136, 220, 255);
    cache.buttonCanvas.fillRoundRect(layout.twoButtonX,
                                     3,
                                     buttonGroupW,
                                     layout.buttonH - 6,
                                     12,
                                     lapPanel);
    cache.buttonCanvas.drawRoundRect(layout.twoButtonX,
                                     3,
                                     buttonGroupW,
                                     layout.buttonH - 6,
                                     12,
                                     lapBorder);
    const String lapValue = formatTimekeeperMs(timekeeperController.lastLapDurationMs());
    const String lapTextValue = String("LAP ") +
                                String(timekeeperController.lapCount()) +
                                "  " + lapValue;
    cache.buttonCanvas.setFont(&fonts::Font0);
    cache.buttonCanvas.setTextDatum(middle_center);
    cache.buttonCanvas.setTextColor(lapText, lapPanel);
    cache.buttonCanvas.setTextSize(2);
    cache.buttonCanvas.drawString(lapTextValue,
                                  layout.twoButtonX + buttonGroupW / 2,
                                  layout.buttonH / 2);
    cache.buttonCanvas.setTextSize(1);
    cache.buttonCanvas.setTextDatum(top_left);
  } else {
    cache.buttonY = layout.buttonY;
  }
  cache.buttonX = 0;
  cache.buttonCanvas.pushSprite(&M5.Display,
                                cache.buttonX,
                                cache.buttonY,
                                transparent);
  cache.layout = layout;
  cache.buttonLayer = buttonLayer;
  cache.speechBubbleVisible = speechBubbleVisible;
  cache.ready = true;
  if (previousCacheReady && previousButtonLayer != buttonLayer) {
    // Transparent pixels cannot erase an older button layer already present
    // on the LCD. Recompose the face once whenever that layer changes so
    // removed countdown/lap controls disappear immediately rather than on
    // the next blink frame.
    faceController.redrawNow();
  }
}

void drawTimekeeperFrameOverlay(M5Canvas& target) {
  if (!displayOn || experienceMode != ExperienceMode::Timekeeper ||
      infoScreenVisible || experienceModeMenuVisible || timekeeperDurationMenuVisible) {
    return;
  }
  TimekeeperUiCanvasCache& cache = timekeeperUiCanvasCache();
  const bool speechBubbleVisible = faceController.speechBubbleVisible();
  if (!cache.ready || cache.speechBubbleVisible != speechBubbleVisible) {
    return;
  }
  constexpr uint16_t transparent = 0xF81F;
  cache.tabCanvas.pushSprite(&target,
                             cache.layout.tabX,
                             cache.layout.tabY,
                             transparent);
  cache.panelCanvas.pushSprite(&target,
                               cache.layout.panelX,
                               cache.layout.panelY,
                               transparent);
  cache.buttonCanvas.pushSprite(&target,
                                cache.buttonX,
                                cache.buttonY,
                                transparent);
}

bool selectTimekeeperActivity(TimekeeperActivity activity, uint64_t nowMs) {
  if (activity == timekeeperController.activity() || !timekeeperController.canChangeActivity()) {
    return false;
  }
  if (timekeeperController.state() != TimekeeperState::Ready) {
    handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
  }
  if (!timekeeperController.selectActivity(activity, nowMs)) {
    return false;
  }
  if (activity == TimekeeperActivity::Countdown ||
      activity == TimekeeperActivity::Pomodoro) {
    timekeeperTimerSubmode = activity;
    saveTimekeeperTimerSubmode();
  }
  lastTimekeeperUiValueMs = UINT64_MAX;
  drawTimekeeperOverlay(nowMs, true);
  return true;
}

bool updateTimekeeperTouch(const m5::touch_detail_t& touch, uint64_t nowMs) {
  if (experienceMode != ExperienceMode::Timekeeper || infoScreenVisible) {
    return false;
  }
  if (pendingTimekeeperSmileResult.active) {
    // Keep the result pose uninterrupted until its deferred announcement has
    // been released. The smile is short, so consuming touches here also avoids
    // a new challenge announcement overtaking the result announcement.
    return true;
  }
  if (updateTimekeeperDurationMenuTouch(touch, nowMs)) {
    return true;
  }

  if (touch.wasFlicked()) {
    const int32_t distanceX = touch.distanceX();
    const int32_t distanceY = touch.distanceY();
    if (abs(distanceX) > abs(distanceY) && abs(distanceX) >= 48) {
      // Keep the activities arranged in the same left-to-right order as the
      // tabs. A left swipe advances; a right swipe goes back.
      int activityIndex = timekeeperController.activity() == TimekeeperActivity::Stopwatch
                            ? 0
                            : ((timekeeperController.activity() == TimekeeperActivity::Countdown ||
                                timekeeperController.activity() == TimekeeperActivity::Pomodoro)
                                 ? 1
                                 : 2);
      const int direction = distanceX < 0 ? 1 : -1;
      activityIndex = constrain(activityIndex + direction, 0, 2);
      const TimekeeperActivity selected = activityIndex == 0
                                            ? TimekeeperActivity::Stopwatch
                                            : (activityIndex == 1
                                                 ? timekeeperTimerSubmode
                                                 : TimekeeperActivity::TenSecondChallenge);
      if (selected != timekeeperController.activity() &&
          timekeeperController.canChangeActivity()) {
        selectTimekeeperActivity(selected, nowMs);
        Serial.printf("[timekeeper.ui] swipe activity=%s\n",
                      timekeeperActivityName(selected));
      }
    }
    // Timekeeper owns all flick gestures, including vertical flicks and
    // attempts made while the timer is running.
    return true;
  }
  if (!touch.wasClicked()) {
    return false;
  }
  const TimekeeperLayout layout = makeTimekeeperLayout(faceController.speechBubbleVisible());
  if (touchIn(touch,
              layout.tabX,
              layout.tabY,
              layout.tabW,
              layout.tabH)) {
    selectTimekeeperActivity(TimekeeperActivity::Stopwatch, nowMs);
    return true;
  }
  if (touchIn(touch,
              layout.tabX + layout.tabW + layout.tabGap,
              layout.tabY,
              layout.tabW,
              layout.tabH)) {
    selectTimekeeperActivity(timekeeperTimerSubmode, nowMs);
    return true;
  }
  if (touchIn(touch,
              layout.tabX + (layout.tabW + layout.tabGap) * 2,
              layout.tabY,
              layout.tabW,
              layout.tabH)) {
    selectTimekeeperActivity(TimekeeperActivity::TenSecondChallenge, nowMs);
    return true;
  }

  const TimekeeperActivity activity = timekeeperController.activity();
  const bool running = timekeeperController.isRunning();
  if (activity == TimekeeperActivity::Countdown && !running) {
    const int32_t leftDx = touch.x - layout.adjustButtonLeftX;
    const int32_t leftDy = touch.y - layout.adjustButtonY;
    const int32_t rightDx = touch.x - layout.adjustButtonRightX;
    const int32_t rightDy = touch.y - layout.adjustButtonY;
    const int32_t radiusSquared = layout.adjustButtonRadius * layout.adjustButtonRadius;
    if (leftDx * leftDx + leftDy * leftDy <= radiusSquared) {
      if (timekeeperController.state() != TimekeeperState::Ready) {
        handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
      }
      timekeeperController.adjustCountdownMinutes(-1, nowMs);
      drawTimekeeperOverlay(nowMs, true);
      return true;
    }
    if (rightDx * rightDx + rightDy * rightDy <= radiusSquared) {
      if (timekeeperController.state() != TimekeeperState::Ready) {
        handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
      }
      timekeeperController.adjustCountdownMinutes(1, nowMs);
      drawTimekeeperOverlay(nowMs, true);
      return true;
    }
    if (touchIn(touch,
                layout.panelX,
                layout.panelY,
                layout.panelW,
                layout.panelH)) {
      showTimekeeperDurationMenu();
      return true;
    }
  }
  if (activity == TimekeeperActivity::Pomodoro) {
    if (timekeeperController.state() == TimekeeperState::Ready) {
      const int32_t leftDx = touch.x - layout.adjustButtonLeftX;
      const int32_t leftDy = touch.y - layout.adjustButtonY;
      const int32_t rightDx = touch.x - layout.adjustButtonRightX;
      const int32_t rightDy = touch.y - layout.adjustButtonY;
      const int32_t radiusSquared = layout.adjustButtonRadius * layout.adjustButtonRadius;
      if (leftDx * leftDx + leftDy * leftDy <= radiusSquared) {
        if (timekeeperController.adjustPomodoroCycles(-1, nowMs)) {
          saveTimekeeperCycles();
          drawTimekeeperOverlay(nowMs, true);
          Serial.printf("[timekeeper.ui] pomodoro cycles=%u\n",
                        static_cast<unsigned>(timekeeperController.pomodoroCycles()));
        }
        return true;
      }
      if (rightDx * rightDx + rightDy * rightDy <= radiusSquared) {
        if (timekeeperController.adjustPomodoroCycles(1, nowMs)) {
          saveTimekeeperCycles();
          drawTimekeeperOverlay(nowMs, true);
          Serial.printf("[timekeeper.ui] pomodoro cycles=%u\n",
                        static_cast<unsigned>(timekeeperController.pomodoroCycles()));
        }
        return true;
      }
    }
    if (!running && touchIn(touch,
                            layout.panelX,
                            layout.panelY,
                            layout.panelW,
                            layout.panelH)) {
      showTimekeeperDurationMenu();
      return true;
    }
  }
  if (activity == TimekeeperActivity::TenSecondChallenge && !running) {
    const int32_t targetDx = touch.x - layout.adjustButtonLeftX;
    const int32_t targetDy = touch.y - layout.adjustButtonY;
    const int32_t difficultyDx = touch.x - layout.adjustButtonRightX;
    const int32_t difficultyDy = touch.y - layout.adjustButtonY;
    const int32_t radiusSquared = layout.adjustButtonRadius * layout.adjustButtonRadius;
    if (targetDx * targetDx + targetDy * targetDy <= radiusSquared) {
      if (timekeeperController.state() != TimekeeperState::Ready) {
        handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
      }
      const uint64_t currentTargetMs = timekeeperController.challengeTargetMs();
      const uint16_t nextTargetSeconds = currentTargetMs == 10000ULL
                                           ? 30
                                           : (currentTargetMs == 30000ULL ? 60 : 10);
      timekeeperController.setChallengeTargetSeconds(nextTargetSeconds, nowMs);
      drawTimekeeperOverlay(nowMs, true);
      Serial.printf("[timekeeper.ui] challenge target=%u seconds\n",
                    static_cast<unsigned>(nextTargetSeconds));
      return true;
    }
    if (difficultyDx * difficultyDx + difficultyDy * difficultyDy <= radiusSquared) {
      if (timekeeperController.state() != TimekeeperState::Ready) {
        handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
      }
      timekeeperController.cycleChallengeDifficulty(nowMs);
      drawTimekeeperOverlay(nowMs, true);
      Serial.printf("[timekeeper.ui] challenge difficulty=%s\n",
                    timekeeperChallengeDifficultyName(
                      timekeeperController.challengeDifficulty()));
      return true;
    }
  }
#if STACKCHAN_DEVICE_CORES3
  // CoreS3 has no dedicated lap/reset button. A tap that did not hit a tab,
  // timer control, or setup panel acts like the StopWatch blue button.
  if (activity == TimekeeperActivity::Stopwatch && running) {
    handleTimekeeperEvent(timekeeperController.lap(nowMs), true);
    drawTimekeeperOverlay(nowMs, true);
    Serial.println("[timekeeper.ui] background tap lap");
  } else if (!running && timekeeperController.state() != TimekeeperState::Ready) {
    handleTimekeeperEvent(timekeeperController.reset(nowMs), false);
    drawTimekeeperOverlay(nowMs, true);
    Serial.println("[timekeeper.ui] background tap reset");
  } else {
    Serial.printf("[timekeeper.ui] background tap ignored activity=%s running=%d state=%d\n",
                  timekeeperActivityName(activity),
                  running ? 1 : 0,
                  static_cast<int>(timekeeperController.state()));
  }
  return true;
#endif
  return false;
}

bool handleTimekeeperBlueButton(uint64_t nowMs) {
  if (experienceMode != ExperienceMode::Timekeeper || infoScreenVisible ||
      experienceModeMenuVisible || timekeeperDurationMenuVisible) {
    return false;
  }
  if (pendingTimekeeperSmileResult.active) {
    return true;
  }
  const TimekeeperActivity activity = timekeeperController.activity();
  const bool running = timekeeperController.isRunning();
  TimekeeperEvent event;
  bool handled = false;
  bool allowAnnouncement = false;

  if (activity == TimekeeperActivity::Stopwatch && running) {
    event = timekeeperController.lap(nowMs);
    handled = true;
    allowAnnouncement = true;
  } else if (!running && timekeeperController.state() != TimekeeperState::Ready) {
    event = timekeeperController.reset(nowMs);
    handled = true;
  }

  if (handled) {
    handleTimekeeperEvent(event, allowAnnouncement);
    drawTimekeeperOverlay(nowMs, true);
    Serial.printf("[button] blue timekeeper activity=%s action=%s\n",
                  timekeeperActivityName(activity),
                  allowAnnouncement ? "lap" : "reset");
  } else {
    Serial.printf("[button] blue timekeeper ignored activity=%s running=%d state=%d\n",
                  timekeeperActivityName(activity),
                  running ? 1 : 0,
                  static_cast<int>(timekeeperController.state()));
  }
  // Always consume the blue button in Timekeeper mode so it never turns the
  // display off, even when the current state has no blue-button action.
  return true;
}

void updateTimekeeper(uint64_t nowMs) {
  if (pendingTimekeeperAnnouncement.active &&
      nowMs >= pendingTimekeeperAnnouncement.expiresAtMs) {
    pendingTimekeeperAnnouncement = PendingTimekeeperAnnouncement();
  }
  if (experienceMode != ExperienceMode::Timekeeper || !displayOn) {
    return;
  }
  updatePendingTimekeeperSmileResult();
  handleTimekeeperEvent(timekeeperController.update(nowMs), true);
}
#endif

void updateTouch(unsigned long now) {
#if STACKCHAN_SMALL_DISPLAY
  (void)now;
  return;
#endif
  if (!interactionsReady(now)) {
    resetOverlayTouchGesture();
    return;
  }

  auto touch = M5.Touch.getDetail();
  if (!displayOn) {
    resetOverlayTouchGesture();
    return;
  }

#if STACKCHAN_DEVICE_CORES3
  // Edge gestures have precedence over Timekeeper's ordinary horizontal
  // activity swipe. When an overlay is already open, the opposite edge
  // gesture closes it instead of navigating directly to the other overlay.
  if (isLeftEdgeModeSwipe(touch)) {
    if (infoScreenVisible) {
      setInfoScreenVisible(false);
      Serial.println("[gesture] settings close left_to_right");
      return;
    }
    showExperienceModeMenu();
    return;
  }
  if (isRightEdgeSettingsSwipe(touch)) {
    if (experienceModeMenuVisible) {
      hideExperienceModeMenu();
      Serial.println("[gesture] mode_menu close right_to_left");
      return;
    }
    setInfoScreenVisible(true);
    return;
  }
#endif

#if STACKCHAN_TIMEKEEPER_ENABLED
  if (updateExperienceModeMenuTouch(touch, now)) {
    return;
  }
#if STACKCHAN_DEVICE_CORES3
  if (handleTravelScreenDoubleTap(touch, now)) {
    return;
  }
#endif
  if (updateTravelFacePickerTouch(touch, now)) {
    return;
  }
  if (updateTimekeeperTouch(touch, monotonicMs())) {
    return;
  }
#endif

  if (updateOverlayButtonTouch(now, touch)) {
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

}

void updateButtons(unsigned long now) {
  (void)now;
#if STACKCHAN_SMALL_DISPLAY
  if (!interactionsReady(now)) {
    return;
  }
  if (!displayOn) {
    smallDisplayFacePettingHold = false;
    smallVolumeHoldRepeatMs = 0;
    if (M5.BtnA.wasHold()) {
      Serial.println("[button] hold display_wake");
      setDisplayOn(true);
      return;
    }
    if (M5.BtnA.wasDecideClickCount()) {
      const uint8_t clickCount = M5.BtnA.getClickCount();
      Serial.printf("[button] click_count=%u display_wake\n", clickCount);
      setDisplayOn(true);
      return;
    }
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
  if (!displayOn) {
#if STACKCHAN_DEVICE_STOPWATCH
    if (M5.BtnB.wasDecideClickCount()) {
      const uint8_t clickCount = M5.BtnB.getClickCount();
      Serial.printf("[button] key_b_click_count=%u display_wake\n", clickCount);
      setDisplayOn(true);
      return;
    }
    if (M5.BtnPWR.wasClicked()) {
      setDisplayOn(true);
      return;
    }
#elif STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
    if (M5.BtnPWR.wasDecideClickCount()) {
      const uint8_t clickCount = M5.BtnPWR.getClickCount();
      Serial.printf("[button] pwr_click_count=%u display_wake\n", clickCount);
      setDisplayOn(true);
      return;
    }
#else
    if (M5.BtnPWR.wasClicked()) {
      setDisplayOn(true);
      return;
    }
#endif
    return;
  }
#if STACKCHAN_DEVICE_STOPWATCH
  if (M5.BtnA.wasPressed()) {
    stopwatchYellowButtonPressed = true;
    stopwatchYellowButtonLongHandled = false;
    stopwatchYellowButtonPressedAtMs = monotonicMs();
  }
  if (stopwatchYellowButtonPressed && M5.BtnA.isPressed() &&
      !stopwatchYellowButtonLongHandled &&
      monotonicMs() - stopwatchYellowButtonPressedAtMs >= 700ULL) {
    stopwatchYellowButtonLongHandled = true;
    showExperienceModeMenu();
    Serial.println("[button] yellow hold mode_menu");
    return;
  }
  if (stopwatchYellowButtonPressed && M5.BtnA.wasReleased()) {
    const bool wasLongPress = stopwatchYellowButtonLongHandled;
    stopwatchYellowButtonPressed = false;
    stopwatchYellowButtonLongHandled = false;
    stopwatchYellowButtonPressedAtMs = 0;
    if (wasLongPress || experienceModeMenuVisible) {
      hideExperienceModeMenu();
      return;
    }
    if (timekeeperDurationMenuVisible) {
      hideTimekeeperDurationMenu(true);
      return;
    }
    if (experienceMode == ExperienceMode::Travel && !infoScreenVisible) {
      // Travel clicks use M5Button's finalized click count so a double click
      // can be distinguished from two single-expression advances.
      if (M5.BtnA.wasDecideClickCount()) {
        handleTravelYellowClickCount(M5.BtnA.getClickCount());
      }
      return;
    }
    if (experienceMode == ExperienceMode::Timekeeper && !infoScreenVisible) {
      if (pendingTimekeeperSmileResult.active) {
        Serial.println("[button] yellow ignored during result smile");
        return;
      }
      const uint64_t releasedAtMs = monotonicMs();
      handleTimekeeperEvent(timekeeperController.toggle(releasedAtMs), true);
      drawTimekeeperOverlay(releasedAtMs, true);
      Serial.println("[button] yellow timekeeper_toggle");
    } else {
      setInfoScreenVisible(!infoScreenVisible);
    }
    return;
  }
  if (experienceMode == ExperienceMode::Travel && !infoScreenVisible &&
      M5.BtnA.wasDecideClickCount()) {
    handleTravelYellowClickCount(M5.BtnA.getClickCount());
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
#if STACKCHAN_DEVICE_STOPWATCH
    if (experienceMode == ExperienceMode::Timekeeper && !infoScreenVisible) {
      if (clickCount == 1) {
        handleTimekeeperBlueButton(monotonicMs());
      }
      // Timekeeper owns the blue button. Never fall through to display-off or
      // Guruguru actions, including for double/multiple clicks.
      return;
    }
#endif
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
      requestExperienceMode(
        experienceMode == ExperienceMode::Guruguru
          ? ExperienceMode::Conversation
          : ExperienceMode::Guruguru,
        now);
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

#if STACKCHAN_HAS_BACK_TOUCH && STACKCHAN_TIMEKEEPER_ENABLED
void resetBackTouchTimekeeperGesture() {
  backTouchTimekeeperArmed = false;
  backTouchTimekeeperPressed = false;
  backTouchTimekeeperPressSinceMs = 0;
  backTouchTimekeeperLastDetectedMs = 0;
  backTouchTimekeeperReleasedSinceMs = 0;
}

bool handleTimekeeperBackTouch(unsigned long now, bool backTouchDetected) {
  if (experienceMode != ExperienceMode::Timekeeper || infoScreenVisible) {
    resetBackTouchTimekeeperGesture();
    return false;
  }

  if (!backTouchTimekeeperArmed) {
    backTouchTimekeeperPressed = false;
    if (backTouchDetected) {
      backTouchTimekeeperReleasedSinceMs = 0;
      return true;
    }
    if (backTouchTimekeeperReleasedSinceMs == 0) {
      backTouchTimekeeperReleasedSinceMs = now;
      return true;
    }
    if (now - backTouchTimekeeperReleasedSinceMs >=
        BACK_TOUCH_TIMEKEEPER_RELEASE_MS) {
      backTouchTimekeeperArmed = true;
      Serial.println("[timekeeper.input] back touch armed");
    }
    return true;
  }

  if (backTouchDetected) {
    backTouchTimekeeperReleasedSinceMs = 0;
    backTouchTimekeeperLastDetectedMs = now;
    if (!backTouchTimekeeperPressed) {
      backTouchTimekeeperPressed = true;
      backTouchTimekeeperPressSinceMs = now;
    }
    return true;
  }

  if (!backTouchTimekeeperPressed) {
    return true;
  }
  if (now - backTouchTimekeeperLastDetectedMs <
      BACK_TOUCH_TIMEKEEPER_RELEASE_MS) {
    return true;
  }

  const unsigned long pressDurationMs =
    backTouchTimekeeperLastDetectedMs - backTouchTimekeeperPressSinceMs;
  backTouchTimekeeperPressed = false;
  backTouchTimekeeperPressSinceMs = 0;
  backTouchTimekeeperLastDetectedMs = 0;
  backTouchTimekeeperReleasedSinceMs = now;
  if (pressDurationMs < BACK_TOUCH_TIMEKEEPER_TAP_MIN_MS ||
      pressDurationMs > BACK_TOUCH_TIMEKEEPER_TAP_MAX_MS) {
    Serial.printf("[timekeeper.input] back touch ignored duration_ms=%lu\n",
                  pressDurationMs);
    return true;
  }
  if (pendingTimekeeperSmileResult.active) {
    Serial.println("[timekeeper.input] back touch ignored during result smile");
    return true;
  }

  const uint64_t eventNowMs = monotonicMs();
  handleTimekeeperEvent(timekeeperController.toggle(eventNowMs), true);
  drawTimekeeperOverlay(eventNowMs, true);
  Serial.printf("[timekeeper.input] back touch toggle duration_ms=%lu state=%s\n",
                pressDurationMs,
                timekeeperStateName(timekeeperController.state()));
  return true;
}

void resetBackTouchTravelGesture() {
  backTouchTravelArmed = false;
  backTouchTravelPressed = false;
  backTouchTravelPressSinceMs = 0;
  backTouchTravelLastDetectedMs = 0;
  backTouchTravelReleasedSinceMs = 0;
}

bool handleTravelBackTouch(unsigned long now, bool backTouchDetected) {
  if (experienceMode != ExperienceMode::Travel || infoScreenVisible ||
      experienceModeMenuVisible) {
    resetBackTouchTravelGesture();
    return false;
  }

  if (!backTouchTravelArmed) {
    backTouchTravelPressed = false;
    if (backTouchDetected) {
      backTouchTravelReleasedSinceMs = 0;
      return true;
    }
    if (backTouchTravelReleasedSinceMs == 0) {
      backTouchTravelReleasedSinceMs = now;
      return true;
    }
    if (now - backTouchTravelReleasedSinceMs >= BACK_TOUCH_TRAVEL_RELEASE_MS) {
      backTouchTravelArmed = true;
      Serial.println("[travel.input] back touch armed");
    }
    return true;
  }

  if (backTouchDetected) {
    backTouchTravelReleasedSinceMs = 0;
    backTouchTravelLastDetectedMs = now;
    if (!backTouchTravelPressed) {
      backTouchTravelPressed = true;
      backTouchTravelPressSinceMs = now;
    }
    return true;
  }

  if (!backTouchTravelPressed) {
    return true;
  }
  if (now - backTouchTravelLastDetectedMs < BACK_TOUCH_TRAVEL_RELEASE_MS) {
    return true;
  }

  const unsigned long pressDurationMs =
    backTouchTravelLastDetectedMs - backTouchTravelPressSinceMs;
  backTouchTravelPressed = false;
  backTouchTravelPressSinceMs = 0;
  backTouchTravelLastDetectedMs = 0;
  backTouchTravelReleasedSinceMs = now;
  if (pressDurationMs < BACK_TOUCH_TRAVEL_TAP_MIN_MS ||
      pressDurationMs > BACK_TOUCH_TRAVEL_TAP_MAX_MS) {
    Serial.printf("[travel.input] back touch ignored duration_ms=%lu\n",
                  pressDurationMs);
    return true;
  }

  if (travelFacePickerVisible) {
    hideTravelFacePicker(true);
    Serial.println("[travel.input] back touch tap picker_close");
  } else {
    showTravelFacePicker();
    Serial.println("[travel.input] back touch tap picker_open");
  }
  return true;
}
#endif

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
#if STACKCHAN_TIMEKEEPER_ENABLED
    resetBackTouchTimekeeperGesture();
    resetBackTouchTravelGesture();
#endif
#if STACKCHAN_DEVICE_CORES3 && STACKCHAN_GURUGURU_FACE_ENABLED
    resetBackTouchGuruguruGesture();
#endif
    return;
  }

  if (infoScreenVisible || experienceModeMenuVisible) {
#if STACKCHAN_TIMEKEEPER_ENABLED
    resetBackTouchTimekeeperGesture();
    resetBackTouchTravelGesture();
#endif
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

#if STACKCHAN_TIMEKEEPER_ENABLED
  if (handleTimekeeperBackTouch(now, backTouchDetected)) {
    backTouchCandidateSinceMs = 0;
    backTouchClearSinceMs = 0;
    return;
  }
  if (handleTravelBackTouch(now, backTouchDetected)) {
    backTouchCandidateSinceMs = 0;
    backTouchClearSinceMs = 0;
    return;
  }
#endif

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
  applyStopwatchStatusLedSetting();

  randomSeed(esp_random());
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  phoneCameraRemoteController.begin(esp_random());
#endif
  ensureDeviceId();
  ensureBootId();
#if STACKCHAN_TIMEKEEPER_ENABLED
  timekeeperController.begin(esp_random());
  loadTimekeeperSettings();
#endif
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
#if STACKCHAN_TIMEKEEPER_ENABLED
  faceController.setFrameOverlayRenderer(drawTimekeeperFrameOverlay);
#endif
  applyThermalFaceMode();
  affectionController.begin(&preferences);
  streetPassController.begin(&preferences);
  stepCounterController.begin(&preferences);
  loadStepAffectionRewardState();
  restoreStreetPassTimeFromRtc(millis());
  faceController.setAffectionState(affectionController.state());
  motionController.begin();
  audioController.begin(&wsServer);
  audioController.setMicPacketSender(sendUsbSerialMicPacket, nullptr);
  audioController.setVolume(deviceSettings.volume);
  speechBubbleController.begin(&audioController, &faceController);

  wsServer.onText(onWsText);
  wsServer.onBinary(onWsBinary);
  wsServer.onConnection(onWsConnection);

  WiFi.onEvent(onWiFiEvent);
  connectWiFi();
  interactionReadyAtMs = millis() + INTERACTION_STARTUP_IGNORE_MS;
  lastInitializeDrawMs = 0;
  drawInitializeScreen(millis());
}

void loop() {
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  unsigned long lipDiagStepStartedAt = millis();
#endif
  updateDevice();
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  if (millis() - lipDiagStepStartedAt > 20) {
    Serial.printf("[lip.loop] step=updateDevice elapsed_ms=%lu current=%d audio=%d\n",
                  millis() - lipDiagStepStartedAt,
                  static_cast<int>(currentState),
                  static_cast<int>(audioController.state()));
  }
#endif

  unsigned long now = millis();
  const unsigned long loopStartedAt = now;
  const bool diagSpeakingAtLoopStart =
    currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking;
  noteVoicePerfLoop(diagSpeakingAtLoopStart, now);
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
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  lipDiagStepStartedAt = millis();
#endif
  updateSharedImuSample(now);
  updateStepCounter(now);
  // Display-off policy is intentionally strict: only raw step counting,
  // StreetPass and wake/power housekeeping continue. Rewards and app sync are
  // deferred until the display is back on.
  if (displayOn) {
    updateStepAffectionReward(now);
    updateStepSync(now);
  }
  updateButtons(now);
  updatePendingExperienceMode(now);
#if STACKCHAN_TIMEKEEPER_ENABLED
  if (displayOn) {
    updateTimekeeper(monotonicMs());
  }
#endif
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
  updateInteractionMicPause();
  updateHaptic(now);
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  if (millis() - lipDiagStepStartedAt > 20) {
    Serial.printf("[lip.loop] step=input elapsed_ms=%lu current=%d audio=%d\n",
                  millis() - lipDiagStepStartedAt,
                  static_cast<int>(currentState),
                  static_cast<int>(audioController.state()));
  }
  lipDiagStepStartedAt = millis();
#endif
  updateUsbSerial(now);
  updateUsbSerialDeferredIdle(now);
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  if (millis() - lipDiagStepStartedAt > 20) {
    Serial.printf("[lip.loop] step=usb elapsed_ms=%lu current=%d audio=%d\n",
                  millis() - lipDiagStepStartedAt,
                  static_cast<int>(currentState),
                  static_cast<int>(audioController.state()));
  }
  lipDiagStepStartedAt = millis();
#endif
#if STACKCHAN_DEVICE_STOPWATCH
  if (currentState == ChanState::Speaking || audioController.state() == ChanState::Speaking) {
    const unsigned long audioStartedAt = millis();
    audioController.update(millis());
    noteVoicePerfAudioUpdate(diagSpeakingAtLoopStart,
                             static_cast<uint32_t>(millis() - audioStartedAt));
  }
#endif
  updateWiFi(now);
  updateStreetPassNetworkTime(now);
  updateClockOverlay(now);
  streetPassController.update(now);
#if CLASSIC_FACE_LIP_SYNC_DIAG_LOG_ENABLED
  if (millis() - lipDiagStepStartedAt > 20) {
    Serial.printf("[lip.loop] step=network elapsed_ms=%lu current=%d audio=%d\n",
                  millis() - lipDiagStepStartedAt,
                  static_cast<int>(currentState),
                  static_cast<int>(audioController.state()));
  }
#endif

  if (wsStarted) {
    const unsigned long stepStartedAt = millis();
    wsServer.loop();
    noteVoicePerfWsLoop(diagSpeakingAtLoopStart,
                        static_cast<uint32_t>(millis() - stepStartedAt));
#if FACE_DIAG_LOG_ENABLED
    logFaceDiagStep("wsServer.loop", stepStartedAt);
#endif
  }
  if (httpStarted) {
    const unsigned long stepStartedAt = millis();
    httpServer.handleClient();
    noteVoicePerfHttpLoop(diagSpeakingAtLoopStart,
                          static_cast<uint32_t>(millis() - stepStartedAt));
#if FACE_DIAG_LOG_ENABLED
    logFaceDiagStep("httpServer.handleClient", stepStartedAt);
#endif
  }
  updateCameraButtonPending(now);
#if STACKCHAN_PHONE_CAMERA_REMOTE_ENABLED
  updatePhoneCameraRemote(now);
#endif
  updateStreetPassBle(now);
  // State/pose commands are processed by USB and WebSocket earlier in this
  // loop. Re-evaluate here so a freshly started listening mic is stopped and
  // its buffers are cleared before audioController.update() can transmit a
  // chunk recorded during servo movement.
  updateInteractionMicPause();

  if (!interactionsReady(now)) {
    const bool speaking = currentState == ChanState::Speaking ||
                          audioController.state() == ChanState::Speaking;
    if (displayOn && !speaking) {
      drawInitializeScreen(now);
    }
    audioController.update(now);
    updateSpeakingFaceStateAfterPlayback();
    speechBubbleController.update(now);
    updateAffectionState(now);
    if (displayOn && speaking) {
      const unsigned long faceNow = millis();
#if STACKCHAN_CLASSIC_FACE_ENABLED
      faceController.setVoiceMouthLevel(classicMouthLevelFromPlaybackEnvelope(faceNow), faceNow);
#else
      faceController.setVoiceMouthLevel(voiceMouthLevelFromPlaybackPeak(faceNow), faceNow);
#endif
      faceController.update(faceNow);
    }
    updateInteractionMicPause();
    motionController.update(now);
    updateShakeReturnMotion(now);
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
    unsigned long infoAudioStartedAt = millis();
    audioController.update(now);
    noteVoicePerfAudioUpdate(diagSpeakingAtLoopStart,
                             static_cast<uint32_t>(millis() - infoAudioStartedAt));
    updateSpeakingFaceStateAfterPlayback();
    speechBubbleController.update(now);
    updateAffectionState(now);
    updateDeferredFaceState();
    updateDeferredFaceMode();
    updatePendingAffectionDelta();
    updatePetting(now);
    updateInteractionMicPause();
    updateListeningNod(now);
    motionController.update(now);
    updateShakeReturnMotion(now);
    return;
  }

  unsigned long stepStartedAt = millis();
  audioController.update(now);
  noteVoicePerfAudioUpdate(diagSpeakingAtLoopStart,
                           static_cast<uint32_t>(millis() - stepStartedAt));
  updateSpeakingFaceStateAfterPlayback();
  speechBubbleController.update(now);
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
    const bool voicePettingAnimating = faceController.voicePettingAnimationActive();
    if (speaking || guruguruDizzyAnimating || petFaceAnimating || voicePettingAnimating || !deviceSettings.lowPowerMode || now - lastFaceUpdateMs >= LOW_POWER_FACE_UPDATE_INTERVAL_MS) {
      const unsigned long faceNow = millis();
      lastFaceUpdateMs = faceNow;
      const unsigned long faceStartedAt = millis();
      if (speaking) {
#if STACKCHAN_CLASSIC_FACE_ENABLED
        faceController.setVoiceMouthLevel(classicMouthLevelFromPlaybackEnvelope(faceNow), faceNow);
#else
        faceController.setVoiceMouthLevel(voiceMouthLevelFromPlaybackPeak(faceNow), faceNow);
#endif
      }
      faceController.update(faceNow);
#if STACKCHAN_TIMEKEEPER_ENABLED
      if (!experienceModeMenuVisible && !timekeeperDurationMenuVisible &&
          experienceMode == ExperienceMode::Timekeeper) {
        drawTimekeeperOverlay(monotonicMs(), false);
      }
#endif
      noteVoicePerfFaceUpdate(speaking,
                              static_cast<uint32_t>(millis() - faceStartedAt),
                              faceNow);
#if STACKCHAN_TIMEKEEPER_ENABLED
      if (!experienceModeMenuVisible && !timekeeperDurationMenuVisible &&
          !travelFacePickerVisible) {
        drawLowPowerPrompt();
        drawStreetPassNotificationOverlay();
      }
#else
      drawLowPowerPrompt();
      drawStreetPassNotificationOverlay();
#endif
      if (speaking) {
        const unsigned long audioStartedAt = millis();
        audioController.update(millis());
        noteVoicePerfAudioUpdate(diagSpeakingAtLoopStart,
                                 static_cast<uint32_t>(millis() - audioStartedAt));
      }
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
  updateInteractionMicPause();
  if (deviceSettings.lowPowerMode) {
    cancelListeningNod(false);
  } else {
    updateListeningNod(now);
  }
  motionController.update(now);
  updateShakeReturnMotion(now);
  noteVoicePerfLoopDuration(diagSpeakingAtLoopStart,
                            static_cast<uint32_t>(millis() - loopStartedAt));
}
