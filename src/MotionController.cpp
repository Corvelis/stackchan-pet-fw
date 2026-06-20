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
    Serial.printf("[motion] StackChan serial servo output enabled after %u ms\n", SERVO_STARTUP_OUTPUT_DELAY_MS);
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
  targetPose_ = {clampPan(pan), clampTilt(tilt)};
  logPose("target", targetPose_);
}

void MotionController::setImmediatePose(int pan, int tilt) {
  currentPose_ = {clampPan(pan), clampTilt(tilt)};
  targetPose_ = currentPose_;
  writeServoPose(currentPose_);
  logPose("immediate", currentPose_);
}

void MotionController::holdCurrentPose() {
  targetPose_ = currentPose_;
  logPose("hold", currentPose_);
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
  disableAutoAngleSyncAfterFirstMove_ = true;
  writeServoPose(currentPose_);
  Serial.printf("[motion] saved servo home offset yaw=%d pitch=%d\n", yawOffset_, pitchOffset_);
#else
  Serial.println("[motion] servo home save ignored; servo unavailable");
#endif
}

void MotionController::moveToSavedHome() {
  currentPose_ = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
  targetPose_ = currentPose_;
  disableAutoAngleSyncAfterFirstMove_ = true;
  writeServoPose(currentPose_);
  logPose("saved home", currentPose_);
}

void MotionController::update(unsigned long now) {
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

  Pose next = {
    approach(currentPose_.pan, targetPose_.pan),
    approach(currentPose_.tilt, targetPose_.tilt),
  };

  if (next.pan == currentPose_.pan && next.tilt == currentPose_.tilt) {
    return;
  }

  currentPose_ = next;
  writeServoPose(currentPose_);
  logPose("current", currentPose_);
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

int MotionController::approach(int current, int target) const {
  if (current < target) {
    return min(current + SERVO_STEP_DEGREES, target);
  }
  if (current > target) {
    return max(current - SERVO_STEP_DEGREES, target);
  }
  return current;
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
    Serial.printf("[motion] startup pose sync yaw=%d pitch=%d\n", angles.x, angles.y);
    logPose("startup physical", currentPose_);
  } else {
    Serial.printf("[motion] startup pose sync ignored yaw=%d pitch=%d\n", angles.x, angles.y);
    logPose("startup fallback", currentPose_);
  }
  disableAutoAngleSyncAfterFirstMove_ = true;
#endif
}

void MotionController::writeServoPose(const Pose& pose) {
#if STACKCHAN_HAS_SERVO
  if (!servoOutputReady(millis())) {
    return;
  }

  const int yaw = toStackChanYaw(pose.pan);
  const int pitch = toStackChanPitch(pose.tilt);
  M5StackChan.Motion.move(yaw, pitch, SERVO_OUTPUT_SPEED);
  if (disableAutoAngleSyncAfterFirstMove_) {
    M5StackChan.Motion.setAutoAngleSyncEnabled(false);
    disableAutoAngleSyncAfterFirstMove_ = false;
  }
#else
  (void)pose;
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
