#pragma once

#include <Arduino.h>

class CameraManager {
public:
  bool init();
  void deinit();
  bool isReady() const;
  bool captureJpeg(uint8_t** buffer, size_t* length);
  void releaseBuffer(uint8_t* buffer);

private:
  bool ready_ = false;
};
