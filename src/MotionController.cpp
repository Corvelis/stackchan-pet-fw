#include "MotionController.h"

#include "config.h"

#if STACKCHAN_HAS_SERVO
#include <M5StackChan.h>
#endif

#include <Preferences.h>

void MotionController::begin() {
  loadCalibration();
  currentPose_ = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
  targetPose_ = currentPose_;
  servoOutputEnabled_ = STACKCHAN_HAS_SERVO && SERVO_OUTPUT_ENABLED != 0;
  servoOutputStarted_ = false;
  disableAutoAngleSyncAfterFirstMove_ = false;
  logPose("initial", currentPose_);
#if STACKCHAN_HAS_SERVO
  if (servoOutputEnabled_) {
    M5StackChan.Motion.setAutoAngleSyncEnabled(true);
    // The BSP has already initialized the serial servos before this controller
    // starts. Read and adopt their physical pose now; this arms later user
    // interactions without sending a startup movement command.
    servoOutputStarted_ = true;
    syncCurrentPoseFromServos();
    Serial.println("[motion] StackChan serial servo ready at current pose");
  } else {
    Serial.println("[motion] servo output disabled");
  }
#else
  Serial.println("[motion] servo unavailable on this device");
#endif
}

void MotionController::setMotion(const char* name) {
  if (strcmp(name, "center") == 0) {
    setTargetPose(SERVO_PAN_CENTER, SERVO_TILT_CENTER);
  } else if (strcmp(name, "look_left") == 0) {
    setTargetPose(SERVO_PAN_CENTER - 25, SERVO_TILT_CENTER);
  } else if (strcmp(name, "look_right") == 0) {
    setTargetPose(SERVO_PAN_CENTER + 25, SERVO_TILT_CENTER);
  } else if (strcmp(name, "look_away") == 0 || strcmp(name, "not_master") == 0) {
    setTargetPose(SERVO_PAN_CENTER - 35, SERVO_TILT_CENTER + 5);
  } else if (strcmp(name, "nod") == 0) {
    setTargetPose(SERVO_PAN_CENTER, SERVO_TILT_CENTER + 18);
  } else if (strcmp(name, "small_nod") == 0) {
    setTargetPose(SERVO_PAN_CENTER, SERVO_TILT_CENTER + 10);
  } else if (strcmp(name, "small_bounce") == 0) {
    setTargetPose(SERVO_PAN_CENTER + random(-4, 5), SERVO_TILT_CENTER - 8);
  } else if (strcmp(name, "lean_forward") == 0) {
    setTargetPose(SERVO_PAN_CENTER, SERVO_TILT_CENTER - 12);
  } else if (strcmp(name, "wobble") == 0) {
    setTargetPose(SERVO_PAN_CENTER + random(-18, 19), SERVO_TILT_CENTER + random(-8, 9));
  } else if (strcmp(name, "shy_nod") == 0) {
    setTargetPose(SERVO_PAN_CENTER - 8, SERVO_TILT_CENTER + 12);
  } else if (strcmp(name, "thinking") == 0) {
    setTargetPose(SERVO_PAN_CENTER - 12, SERVO_TILT_CENTER + 10);
  } else {
    Serial.printf("[motion] unknown motion: %s\n", name);
  }
}

void MotionController::setTargetPose(int pan, int tilt) {
  if (movementPaused_) {
    return;
  }
  targetPose_ = {clampPan(pan), clampTilt(tilt)};
  logPose("target", targetPose_);
}

void MotionController::setImmediatePose(int pan, int tilt) {
  if (movementPaused_) {
    return;
  }
  targetPose_ = {clampPan(pan), clampTilt(tilt)};
  if (!servoOutputStarted_) {
    currentPose_ = targetPose_;
  }
  logPose("immediate target", targetPose_);
}

void MotionController::deferOutputUntil(unsigned long readyMs) {
  if (static_cast<int32_t>(readyMs - outputDeferredUntilMs_) > 0) {
    outputDeferredUntilMs_ = readyMs;
  }
}

void MotionController::setMovementPaused(bool paused) {
  if (movementPaused_ == paused) {
    return;
  }

  movementPaused_ = paused;
  if (!paused) {
    Serial.println("[motion] movement resumed");
    return;
  }

#if STACKCHAN_HAS_SERVO
  if (servoOutputEnabled_ && servoOutputStarted_) {
    // The logical pose is only updated after a complete axis movement. Stop at
    // the physical angle instead so an in-flight interaction cannot continue
    // into speech playback or snap back to the last completed target.
    if (M5StackChan.Motion.isMoving()) {
      M5StackChan.Motion.stop();
    }
    const auto angles = M5StackChan.Motion.getCurrentAngles();
    currentPose_ = fromStackChanAngles(angles.x, angles.y);
  }
#endif
  targetPose_ = currentPose_;
  activeServoAxis_ = ServoAxis::None;
  nextServoAxisStartMs_ = 0;
  outputDeferredUntilMs_ = 0;
  logPose("paused", currentPose_);
  Serial.println("[motion] movement paused and held");
}

void MotionController::saveCurrentPoseAsHome() {
#if STACKCHAN_HAS_SERVO
  const auto angles = M5StackChan.Motion.getCurrentAngles();
  if (!physicalAnglesLookValid(angles.x, angles.y)) {
    Serial.printf("[motion] servo home save ignored yaw=%d pitch=%d\n", angles.x, angles.y);
    return;
  }
  yawOffset_ = constrain(angles.x - nominalCenterYaw(), -500, 500);
  pitchOffset_ = constrain(angles.y - nominalCenterPitch(), -500, 500);
  saveCalibration();
  currentPose_ = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
  targetPose_ = currentPose_;
  activeServoAxis_ = ServoAxis::None;
  disableAutoAngleSyncAfterFirstMove_ = true;
  Serial.printf("[motion] saved servo home offset yaw=%d pitch=%d\n", yawOffset_, pitchOffset_);
#else
  Serial.println("[motion] servo home save ignored; servo unavailable");
#endif
}

void MotionController::moveToSavedHome() {
  if (movementPaused_) {
    return;
  }
  targetPose_ = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
  disableAutoAngleSyncAfterFirstMove_ = true;
  logPose("saved home target", targetPose_);
}

void MotionController::update(unsigned long now) {
  if (movementPaused_) {
    return;
  }
  if (now - lastUpdateMs_ < SERVO_UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdateMs_ = now;

  if (!servoOutputReady(now)) {
    return;
  }

  if (!servoOutputStarted_) {
    servoOutputStarted_ = true;
    syncCurrentPoseFromServos();
    return;
  }

  if (activeServoAxis_ != ServoAxis::None) {
    if (activeServoAxisMoving()) {
      return;
    }
    finishActiveServoAxis();
    nextServoAxisStartMs_ = now + SERVO_AXIS_SETTLE_MS;
    return;
  }

  if (outputDeferredUntilMs_ != 0 &&
      static_cast<int32_t>(now - outputDeferredUntilMs_) < 0) {
    return;
  }
  outputDeferredUntilMs_ = 0;

  if (nextServoAxisStartMs_ != 0 &&
      static_cast<int32_t>(now - nextServoAxisStartMs_) < 0) {
    return;
  }
  nextServoAxisStartMs_ = 0;

  const bool panChanged = targetPose_.pan != currentPose_.pan;
  const bool tiltChanged = targetPose_.tilt != currentPose_.tilt;
  if (!panChanged && !tiltChanged) {
    return;
  }

  // Starting both servos in the same 20 ms BSP update produces a supply-current
  // spike on Stack-chan. On CoreS3 that spike can reset USB and leave the Wi-Fi
  // interface stale. Run one complete axis movement at a time; newer targets
  // remain queued in targetPose_ and are picked up when the active axis rests.
  ServoAxis nextAxis = ServoAxis::None;
  if (panChanged && tiltChanged) {
    nextAxis = preferPanNext_ ? ServoAxis::Pan : ServoAxis::Tilt;
    preferPanNext_ = !preferPanNext_;
  } else {
    nextAxis = panChanged ? ServoAxis::Pan : ServoAxis::Tilt;
  }
  startServoAxis(nextAxis,
                 nextAxis == ServoAxis::Pan ? targetPose_.pan : targetPose_.tilt);
}

bool MotionController::readyForInteractionTarget(unsigned long now) const {
#if STACKCHAN_HAS_SERVO
  if (movementPaused_ || !servoOutputEnabled_ || !servoOutputStarted_) {
    return false;
  }
  if (activeServoAxis_ != ServoAxis::None ||
      currentPose_.pan != targetPose_.pan ||
      currentPose_.tilt != targetPose_.tilt) {
    return false;
  }
  if (nextServoAxisStartMs_ != 0 &&
      static_cast<int32_t>(now - nextServoAxisStartMs_) < 0) {
    return false;
  }
  if (outputDeferredUntilMs_ != 0 &&
      static_cast<int32_t>(now - outputDeferredUntilMs_) < 0) {
    return false;
  }
#else
  (void)now;
#endif
  return true;
}

bool MotionController::servoMotionActive(unsigned long now) const {
#if STACKCHAN_HAS_SERVO
  if (movementPaused_ || !servoOutputEnabled_ || !servoOutputStarted_) {
    return false;
  }
  if (activeServoAxis_ != ServoAxis::None ||
      currentPose_.pan != targetPose_.pan ||
      currentPose_.tilt != targetPose_.tilt) {
    return true;
  }
  if (nextServoAxisStartMs_ != 0 &&
      static_cast<int32_t>(now - nextServoAxisStartMs_) < 0) {
    return true;
  }
  if (outputDeferredUntilMs_ != 0 &&
      static_cast<int32_t>(now - outputDeferredUntilMs_) < 0) {
    return true;
  }
#else
  (void)now;
#endif
  return false;
}

Pose MotionController::currentPose() const {
  return currentPose_;
}

Pose MotionController::targetPose() const {
  return targetPose_;
}

int MotionController::savedYawOffset() const {
  return yawOffset_;
}

int MotionController::savedPitchOffset() const {
  return pitchOffset_;
}

int MotionController::clampPan(int pan) const {
  return constrain(pan, SERVO_PAN_MIN, SERVO_PAN_MAX);
}

int MotionController::clampTilt(int tilt) const {
  return constrain(tilt, SERVO_TILT_MIN, SERVO_TILT_MAX);
}

void MotionController::loadCalibration() {
  Preferences prefs;
  prefs.begin("motion", true);
  yawOffset_ = prefs.getInt("yaw_offset", 0);
  pitchOffset_ = prefs.getInt("pitch_offset", 0);
  prefs.end();
  yawOffset_ = constrain(yawOffset_, -500, 500);
  pitchOffset_ = constrain(pitchOffset_, -500, 500);
  Serial.printf("[motion] calibration yaw_offset=%d pitch_offset=%d\n", yawOffset_, pitchOffset_);
}

void MotionController::saveCalibration() {
  Preferences prefs;
  prefs.begin("motion", false);
  prefs.putInt("yaw_offset", yawOffset_);
  prefs.putInt("pitch_offset", pitchOffset_);
  prefs.end();
}

int MotionController::toStackChanYaw(int pan) const {
  const int clampedPan = clampPan(pan);
  return constrain((SERVO_PAN_CENTER - clampedPan) * 10 + yawOffset_, -1280, 1280);
}

int MotionController::toStackChanPitch(int tilt) const {
  const int clampedTilt = clampTilt(tilt);
  const long pitch = map(clampedTilt, SERVO_TILT_MIN, SERVO_TILT_MAX, 0, 900);
  return constrain(static_cast<int>(pitch) + pitchOffset_, 0, 900);
}

Pose MotionController::fromStackChanAngles(int yaw, int pitch) const {
  const int adjustedYaw = yaw - yawOffset_;
  const int adjustedPitch = pitch - pitchOffset_;
  const int roundedYawDegrees = adjustedYaw >= 0 ? (adjustedYaw + 5) / 10 : (adjustedYaw - 5) / 10;
  const int pan = SERVO_PAN_CENTER - roundedYawDegrees;
  const long tilt = map(constrain(adjustedPitch, 0, 900), 0, 900, SERVO_TILT_MIN, SERVO_TILT_MAX);
  return {clampPan(pan), clampTilt(static_cast<int>(tilt))};
}

int MotionController::nominalCenterYaw() const {
  return (SERVO_PAN_CENTER - clampPan(SERVO_PAN_CENTER)) * 10;
}

int MotionController::nominalCenterPitch() const {
  const long pitch = map(clampTilt(SERVO_TILT_CENTER), SERVO_TILT_MIN, SERVO_TILT_MAX, 0, 900);
  return constrain(static_cast<int>(pitch), 0, 900);
}

bool MotionController::physicalAnglesLookValid(int yaw, int pitch) const {
  return yaw > -1200 && yaw < 1200 && pitch > 60 && pitch < 840;
}

bool MotionController::servoOutputReady(unsigned long now) const {
  return STACKCHAN_HAS_SERVO && servoOutputEnabled_ && now >= SERVO_STARTUP_OUTPUT_DELAY_MS;
}

void MotionController::syncCurrentPoseFromServos() {
#if STACKCHAN_HAS_SERVO
  const auto angles = M5StackChan.Motion.getCurrentAngles();
  if (physicalAnglesLookValid(angles.x, angles.y)) {
    currentPose_ = fromStackChanAngles(angles.x, angles.y);
    // Startup must never turn the default logical center into an unsolicited
    // physical movement. Adopt the position in which the device was powered on
    // as both the current and target pose; later app/interaction commands may
    // move from here normally.
    targetPose_ = currentPose_;
    Serial.printf("[motion] startup pose sync yaw=%d pitch=%d\n", angles.x, angles.y);
    Serial.println("[motion] startup pose adopted without movement");
    logPose("startup physical", currentPose_);
  } else {
    Serial.printf("[motion] startup pose sync ignored yaw=%d pitch=%d\n", angles.x, angles.y);
    logPose("startup fallback", currentPose_);
  }
  disableAutoAngleSyncAfterFirstMove_ = true;
#endif
}

bool MotionController::activeServoAxisMoving() const {
#if STACKCHAN_HAS_SERVO
  if (activeServoAxis_ == ServoAxis::Pan) {
    return M5StackChan.Motion.isYawMoving();
  }
  if (activeServoAxis_ == ServoAxis::Tilt) {
    return M5StackChan.Motion.isPitchMoving();
  }
#endif
  return false;
}

void MotionController::finishActiveServoAxis() {
  if (activeServoAxis_ == ServoAxis::Pan) {
    currentPose_.pan = activeServoAxisTarget_;
  } else if (activeServoAxis_ == ServoAxis::Tilt) {
    currentPose_.tilt = activeServoAxisTarget_;
  }
  activeServoAxis_ = ServoAxis::None;
  logPose("current", currentPose_);
}

void MotionController::startServoAxis(ServoAxis axis, int logicalAngle) {
#if STACKCHAN_HAS_SERVO
  if (!servoOutputReady(millis()) || axis == ServoAxis::None) {
    return;
  }

  activeServoAxis_ = axis;
  activeServoAxisTarget_ = logicalAngle;
  if (axis == ServoAxis::Pan) {
    M5StackChan.Motion.moveYaw(toStackChanYaw(logicalAngle), SERVO_OUTPUT_SPEED);
  } else {
    M5StackChan.Motion.movePitch(toStackChanPitch(logicalAngle), SERVO_OUTPUT_SPEED);
  }
  if (disableAutoAngleSyncAfterFirstMove_) {
    M5StackChan.Motion.setAutoAngleSyncEnabled(false);
    disableAutoAngleSyncAfterFirstMove_ = false;
  }
#else
  (void)axis;
  (void)logicalAngle;
#endif
}

void MotionController::logPose(const char* label, const Pose& pose) const {
#if VERBOSE_LOG_ENABLED
  Serial.printf("[motion] %s pan=%d tilt=%d\n", label, pose.pan, pose.tilt);
#else
  (void)label;
  (void)pose;
#endif
}
