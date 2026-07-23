#include "CameraManager.h"

#include "config.h"

#if STACKCHAN_HAS_CAMERA
#include <M5Unified.h>
#include <esp_camera.h>
#include <img_converters.h>
#endif

#if STACKCHAN_HAS_CAMERA && STACKCHAN_DEVICE_CORES3
namespace {
constexpr uint8_t kCameraI2cAddress = 0x21;
constexpr uint8_t kCameraIdRegister = 0x00;
constexpr uint8_t kExpectedCameraId = 0x9B;
constexpr uint32_t kCameraI2cFrequency = 400000;
constexpr uint32_t kCameraPowerOffMs = 250;
constexpr uint32_t kCameraPowerOnSettleMs = 300;
constexpr int kCameraPowerMillivolts = 3300;
constexpr uint8_t kCameraPowerCycleAttempts = 2;
constexpr int kCameraSdaPin = 12;
constexpr int kCameraSclPin = 11;
constexpr int kCameraInputPins[] = {47, 48, 16, 15, 42, 41, 40, 39, 46, 38, 45};
}  // namespace
#endif

bool CameraManager::init() {
#if STACKCHAN_HAS_CAMERA
  if (ready_) {
    return true;
  }

  if (!prepareCameraPower()) {
    ready_ = false;
    return false;
  }

  camera_config_t cameraConfig = {};
  cameraConfig.pin_pwdn = -1;
  cameraConfig.pin_reset = -1;
  cameraConfig.pin_xclk = -1;
  cameraConfig.pin_sccb_sda = 12;
  cameraConfig.pin_sccb_scl = 11;
  cameraConfig.pin_d7 = 47;
  cameraConfig.pin_d6 = 48;
  cameraConfig.pin_d5 = 16;
  cameraConfig.pin_d4 = 15;
  cameraConfig.pin_d3 = 42;
  cameraConfig.pin_d2 = 41;
  cameraConfig.pin_d1 = 40;
  cameraConfig.pin_d0 = 39;
  cameraConfig.pin_vsync = 46;
  cameraConfig.pin_href = 38;
  cameraConfig.pin_pclk = 45;
  cameraConfig.xclk_freq_hz = 20000000;
  cameraConfig.ledc_timer = LEDC_TIMER_0;
  cameraConfig.ledc_channel = LEDC_CHANNEL_0;
  cameraConfig.pixel_format = CAMERA_PIXEL_FORMAT;
  cameraConfig.frame_size = CAMERA_FRAME_SIZE;
  cameraConfig.jpeg_quality = CAMERA_JPEG_QUALITY;
  cameraConfig.fb_count = 1;
  cameraConfig.fb_location = CAMERA_FB_LOCATION;
  cameraConfig.grab_mode = CAMERA_GRAB_LATEST;
  cameraConfig.sccb_i2c_port = -1;

#if STACKCHAN_DEVICE_CORES3
  // CoreS3's GC0308 shares GPIO 12/11 with M5Unified's internal I2C bus.
  // esp_camera must own that bus while the sensor is active. Restore it in
  // every exit path so the RTC, IMU, power controller, and audio codec keep
  // working after capture.
  internalI2cSuspended_ = M5.In_I2C.release();
  if (!internalI2cSuspended_) {
    Serial.println("[camera] failed to release internal I2C");
    ready_ = false;
    return false;
  }
#endif

  const esp_err_t err = esp_camera_init(&cameraConfig);
  if (err != ESP_OK) {
    Serial.printf("[camera] init failed: 0x%x\n", static_cast<unsigned>(err));
    ready_ = false;
    restoreInternalI2c();
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_framesize(sensor, CAMERA_FRAME_SIZE);
  }

  ready_ = true;
  Serial.println("[camera] ready");
  return true;
#else
  ready_ = false;
  Serial.println("[camera] unavailable on this device");
  return false;
#endif
}

bool CameraManager::isReady() const {
  return ready_;
}

void CameraManager::deinit() {
#if STACKCHAN_HAS_CAMERA
  if (!ready_) {
    restoreInternalI2c();
    return;
  }

  const esp_err_t err = esp_camera_deinit();
  if (err != ESP_OK) {
    Serial.printf("[camera] deinit failed: 0x%x\n", static_cast<unsigned>(err));
  } else {
    Serial.println("[camera] deinit");
  }
  ready_ = false;
  restoreInternalI2c();
#else
  ready_ = false;
#endif
}

bool CameraManager::prepareCameraPower() {
#if STACKCHAN_HAS_CAMERA && STACKCHAN_DEVICE_CORES3
  // CoreS3's GC0308 has no reset or power-down GPIO. Its dedicated 3.3 V
  // supply is AXP2101 ALDO3, so an ESP restart does not necessarily reset a
  // wedged sensor. GPIO 12/11 also carry the internal I2C bus, whose pull-ups
  // can back-power an unpowered sensor. Hold them low during the off interval
  // so this is an electrical reset rather than only an ALDO3 register toggle.
  for (uint8_t attempt = 1; attempt <= kCameraPowerCycleAttempts; ++attempt) {
    if (!restoreInternalI2c()) {
      Serial.println("[camera] cannot restore internal I2C for power cycle");
      return false;
    }

    M5.Power.Axp2101.setALDO3(0);
    internalI2cSuspended_ = M5.In_I2C.release();
    if (!internalI2cSuspended_) {
      Serial.println("[camera] cannot release internal I2C for power-off hold");
      return false;
    }

    for (int pin : kCameraInputPins) {
      pinMode(pin, INPUT);
    }
    digitalWrite(kCameraSdaPin, LOW);
    digitalWrite(kCameraSclPin, LOW);
    pinMode(kCameraSdaPin, OUTPUT_OPEN_DRAIN);
    pinMode(kCameraSclPin, OUTPUT_OPEN_DRAIN);
    delay(kCameraPowerOffMs);

    pinMode(kCameraSdaPin, INPUT);
    pinMode(kCameraSclPin, INPUT);
    if (!restoreInternalI2c()) {
      Serial.println("[camera] internal I2C restore failed after power-off hold");
      return false;
    }

    M5.Power.Axp2101.setALDO3(kCameraPowerMillivolts);
    delay(kCameraPowerOnSettleMs);

    const bool powerEnabled = M5.Power.Axp2101.getALDO3Enabled();
    uint8_t cameraId = 0;
    const bool sensorResponded = powerEnabled && M5.In_I2C.readRegister(
      kCameraI2cAddress,
      kCameraIdRegister,
      &cameraId,
      1,
      kCameraI2cFrequency
    );
    Serial.printf("[camera] power cycle attempt=%u power=%s sensor=%s id=0x%02x\n",
                  static_cast<unsigned>(attempt),
                  powerEnabled ? "ready" : "failed",
                  sensorResponded ? "ready" : "missing",
                  static_cast<unsigned>(cameraId));
    if (sensorResponded) {
      if (cameraId != kExpectedCameraId) {
        Serial.printf("[camera] unexpected sensor id: 0x%02x\n",
                      static_cast<unsigned>(cameraId));
      }
      return true;
    }
  }

  Serial.println("[camera] sensor did not recover after power cycle");
  return false;
#else
  return true;
#endif
}

bool CameraManager::restoreInternalI2c() {
#if STACKCHAN_HAS_CAMERA && STACKCHAN_DEVICE_CORES3
  if (!internalI2cSuspended_) {
    return M5.In_I2C.isEnabled();
  }
  const bool restored = M5.In_I2C.begin();
  internalI2cSuspended_ = !restored;
  Serial.printf("[camera] internal I2C restore: %s\n", restored ? "ready" : "failed");
  return restored;
#else
  return true;
#endif
}

bool CameraManager::captureJpeg(uint8_t** buffer, size_t* length) {
#if STACKCHAN_HAS_CAMERA
  if (!ready_ || buffer == nullptr || length == nullptr) {
    return false;
  }

  *buffer = nullptr;
  *length = 0;

  const unsigned long captureStartMs = millis();
  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    Serial.println("[camera] capture failed");
    return false;
  }
  const unsigned long frameMs = millis() - captureStartMs;
  const pixformat_t frameFormat = frame->format;

  bool ok = false;
  if (frameFormat == PIXFORMAT_JPEG) {
    *buffer = static_cast<uint8_t*>(malloc(frame->len));
    if (*buffer != nullptr) {
      memcpy(*buffer, frame->buf, frame->len);
      *length = frame->len;
      ok = true;
    }
  } else {
    const unsigned long convertStartMs = millis();
    ok = frame2jpg(frame, CAMERA_JPEG_QUALITY, buffer, length);
#if CAMERA_DIAG_LOG_ENABLED
    Serial.printf("[camera] jpeg conversion took %lu ms\n", millis() - convertStartMs);
#endif
  }

  esp_camera_fb_return(frame);

  if (!ok || *buffer == nullptr || *length == 0) {
    Serial.println("[camera] jpeg conversion failed");
    if (*buffer != nullptr) {
      free(*buffer);
      *buffer = nullptr;
    }
    *length = 0;
    return false;
  }

#if CAMERA_DIAG_LOG_ENABLED
  Serial.printf("[camera] captured jpeg: %u bytes frame=%lu ms total=%lu ms format=%u\n",
                static_cast<unsigned>(*length),
                frameMs,
                millis() - captureStartMs,
                static_cast<unsigned>(frameFormat));
#else
  Serial.printf("[camera] captured jpeg: %u bytes\n", static_cast<unsigned>(*length));
#endif
  return true;
#else
  if (buffer != nullptr) {
    *buffer = nullptr;
  }
  if (length != nullptr) {
    *length = 0;
  }
  return false;
#endif
}

void CameraManager::releaseBuffer(uint8_t* buffer) {
#if STACKCHAN_HAS_CAMERA
  if (buffer != nullptr) {
    free(buffer);
  }
#else
  (void)buffer;
#endif
}
