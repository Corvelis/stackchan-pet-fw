#pragma once

#include <Arduino.h>

#include "AppState.h"

class MotionController {
public:
  void begin();
  void setMotion(const char* name);
  void setTargetPose(int pan, int tilt);
  void setImmediatePose(int pan, int tilt);
  void update(unsigned long now);
  Pose currentPose() const;
  Pose targetPose() const;

private:
  int clampPan(int pan) const;
  int clampTilt(int tilt) const;
  int approach(int current, int target) const;
  int toStackChanYaw(int pan) const;
  int toStackChanPitch(int tilt) const;
  Pose fromStackChanAngles(int yaw, int pitch) const;
  bool physicalAnglesLookValid(int yaw, int pitch) const;
  bool servoOutputReady(unsigned long now) const;
  void syncCurrentPoseFromServos();
  void writeServoPose(const Pose& pose);
  void logPose(const char* label, const Pose& pose) const;

  Pose currentPose_;
  Pose targetPose_;
  unsigned long lastUpdateMs_ = 0;
  bool servoOutputEnabled_ = false;
  bool servoOutputStarted_ = false;
  bool disableAutoAngleSyncAfterFirstMove_ = false;
};
