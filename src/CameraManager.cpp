#include "CameraManager.h"

#include <esp_camera.h>
#include <img_converters.h>

#include "config.h"

bool CameraManager::init() {
  camera_config_t cameraConfig = {};
  cameraConfig.pin_pwdn = -1;
  cameraConfig.pin_reset = -1;
  cameraConfig.pin_xclk = -1;
  cameraConfig.pin_sccb_sda = -1;
  cameraConfig.pin_sccb_scl = -1;
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
  cameraConfig.pixel_format = PIXFORMAT_RGB565;
  cameraConfig.frame_size = FRAMESIZE_QQVGA;
  cameraConfig.jpeg_quality = 0;
  cameraConfig.fb_count = 1;
  cameraConfig.fb_location = CAMERA_FB_IN_DRAM;
  cameraConfig.grab_mode = CAMERA_GRAB_LATEST;
  cameraConfig.sccb_i2c_port = 1;

  const esp_err_t err = esp_camera_init(&cameraConfig);
  if (err != ESP_OK) {
    Serial.printf("[camera] init failed: 0x%x\n", static_cast<unsigned>(err));
    ready_ = false;
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    sensor->set_framesize(sensor, FRAMESIZE_QQVGA);
  }

  ready_ = true;
  Serial.println("[camera] ready");
  return true;
}

bool CameraManager::isReady() const {
  return ready_;
}

void CameraManager::deinit() {
  if (!ready_) {
    return;
  }

  const esp_err_t err = esp_camera_deinit();
  if (err != ESP_OK) {
    Serial.printf("[camera] deinit failed: 0x%x\n", static_cast<unsigned>(err));
  } else {
    Serial.println("[camera] deinit");
  }
  ready_ = false;
}

bool CameraManager::captureJpeg(uint8_t** buffer, size_t* length) {
  if (!ready_ || buffer == nullptr || length == nullptr) {
    return false;
  }

  *buffer = nullptr;
  *length = 0;

  camera_fb_t* frame = esp_camera_fb_get();
  if (frame == nullptr) {
    Serial.println("[camera] capture failed");
    return false;
  }

  bool ok = false;
  if (frame->format == PIXFORMAT_JPEG) {
    *buffer = static_cast<uint8_t*>(malloc(frame->len));
    if (*buffer != nullptr) {
      memcpy(*buffer, frame->buf, frame->len);
      *length = frame->len;
      ok = true;
    }
  } else {
    ok = frame2jpg(frame, CAMERA_JPEG_QUALITY, buffer, length);
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

  Serial.printf("[camera] captured jpeg: %u bytes\n", static_cast<unsigned>(*length));
  return true;
}

void CameraManager::releaseBuffer(uint8_t* buffer) {
  if (buffer != nullptr) {
    free(buffer);
  }
}
