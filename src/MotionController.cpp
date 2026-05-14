#include "MotionController.h"

#include <M5StackChan.h>

#include "config.h"

void MotionController::begin() {
  currentPose_ = {SERVO_PAN_CENTER, SERVO_TILT_CENTER};
  targetPose_ = currentPose_;
  servoOutputEnabled_ = SERVO_OUTPUT_ENABLED != 0;
  logPose("initial", currentPose_);
  if (servoOutputEnabled_) {
    M5StackChan.Motion.setAutoAngleSyncEnabled(false);
    writeServoPose(currentPose_);
    Serial.println("[motion] StackChan serial servo output enabled");
  } else {
    Serial.println("[motion] servo output disabled");
  }
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

void MotionController::update(unsigned long now) {
  if (now - lastUpdateMs_ < SERVO_UPDATE_INTERVAL_MS) {
    return;
  }
  lastUpdateMs_ = now;

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

int MotionController::toStackChanYaw(int pan) const {
  const int clampedPan = clampPan(pan);
  return constrain((SERVO_PAN_CENTER - clampedPan) * 10, -1280, 1280);
}

int MotionController::toStackChanPitch(int tilt) const {
  const int clampedTilt = clampTilt(tilt);
  const long pitch = map(clampedTilt, SERVO_TILT_MIN, SERVO_TILT_MAX, 0, 900);
  return constrain(static_cast<int>(pitch), 0, 900);
}

void MotionController::writeServoPose(const Pose& pose) {
  if (!servoOutputEnabled_) {
    return;
  }

  const int yaw = toStackChanYaw(pose.pan);
  const int pitch = toStackChanPitch(pose.tilt);
  M5StackChan.Motion.move(yaw, pitch, SERVO_OUTPUT_SPEED);
}

void MotionController::logPose(const char* label, const Pose& pose) const {
  Serial.printf("[motion] %s pan=%d tilt=%d\n", label, pose.pan, pose.tilt);
}
