#pragma once

#include <Arduino.h>

#include "AppState.h"

class MotionController {
public:
  void begin();
  void setMotion(const char* name);
  void setTargetPose(int pan, int tilt);
  void setImmediatePose(int pan, int tilt);
  void deferOutputUntil(unsigned long readyMs);
  void setMovementPaused(bool paused);
  void saveCurrentPoseAsHome();
  void moveToSavedHome();
  void update(unsigned long now);
  bool readyForInteractionTarget(unsigned long now) const;
  bool servoMotionActive(unsigned long now) const;
  Pose currentPose() const;
  Pose targetPose() const;
  int savedYawOffset() const;
  int savedPitchOffset() const;

private:
  enum class ServoAxis : uint8_t {
    None,
    Pan,
    Tilt,
  };

  int clampPan(int pan) const;
  int clampTilt(int tilt) const;
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
  bool activeServoAxisMoving() const;
  void finishActiveServoAxis();
  void startServoAxis(ServoAxis axis, int logicalAngle);
  void logPose(const char* label, const Pose& pose) const;

  Pose currentPose_;
  Pose targetPose_;
  unsigned long lastUpdateMs_ = 0;
  int yawOffset_ = 0;
  int pitchOffset_ = 0;
  bool servoOutputEnabled_ = false;
  bool servoOutputStarted_ = false;
  bool movementPaused_ = false;
  bool disableAutoAngleSyncAfterFirstMove_ = false;
  ServoAxis activeServoAxis_ = ServoAxis::None;
  int activeServoAxisTarget_ = 0;
  bool preferPanNext_ = true;
  unsigned long nextServoAxisStartMs_ = 0;
  unsigned long outputDeferredUntilMs_ = 0;
};
