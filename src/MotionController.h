#pragma once

#include <Arduino.h>

#include "AppState.h"

class MotionController {
public:
  void begin();
  void setMotion(const char* name);
  void setTargetPose(int pan, int tilt);
  void setImmediatePose(int pan, int tilt);
  void holdCurrentPose();
  void saveCurrentPoseAsHome();
  void moveToSavedHome();
  void update(unsigned long now);
  Pose currentPose() const;
  Pose targetPose() const;
  int savedYawOffset() const;
  int savedPitchOffset() const;

private:
  int clampPan(int pan) const;
  int clampTilt(int tilt) const;
  int approach(int current, int target) const;
  void loadCalibration();
  void saveCalibration();
  int toStackChanYaw(int pan) const;
  int toStackChanPitch(int tilt) const;
  Pose fromStackChanAngles(int yaw, int pitch) const;
  int nominalCenterYaw() const;
  int nominalCenterPitch() const;
  bool physicalAnglesLookValid(int yaw, int pitch) const;
  bool servoOutputReady(unsigned long now) const;
  void syncCurrentPoseFromServos();
  void writeServoPose(const Pose& pose);
  void logPose(const char* label, const Pose& pose) const;

  Pose currentPose_;
  Pose targetPose_;
  unsigned long lastUpdateMs_ = 0;
  int yawOffset_ = 0;
  int pitchOffset_ = 0;
  bool servoOutputEnabled_ = false;
  bool servoOutputStarted_ = false;
  bool disableAutoAngleSyncAfterFirstMove_ = false;
};
