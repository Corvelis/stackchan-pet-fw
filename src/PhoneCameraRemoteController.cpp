#include "PhoneCameraRemoteController.h"

#include <cstring>

#include "config.h"

void PhoneCameraRemoteController::begin(uint32_t sessionToken) {
  sessionToken_ = sessionToken != 0 ? sessionToken : 1;
  requestSequence_ = 0;
  reset();
}

bool PhoneCameraRemoteController::setReady(PhoneCameraTransport transport,
                                           bool ready,
                                           unsigned long now) {
  (void)now;
  if (transport == PhoneCameraTransport::None) {
    return false;
  }

  if (ready) {
    if (state_ == PhoneCameraState::Pending && transport != pendingTransport_) {
      return false;
    }
    const bool changed = readyTransport_ != transport ||
                         state_ == PhoneCameraState::Unavailable;
    if (readyTransport_ != transport) {
      clearLensInfo();
    }
    readyTransport_ = transport;
    if (state_ == PhoneCameraState::Unavailable ||
        state_ == PhoneCameraState::Ready) {
      state_ = PhoneCameraState::Ready;
      feedbackStartedMs_ = 0;
    }
    return changed;
  }

  if (transport != readyTransport_ && transport != pendingTransport_) {
    return false;
  }

  if (transport == readyTransport_) {
    readyTransport_ = PhoneCameraTransport::None;
  }
  clearPending();
  clearLensInfo();
  feedbackStartedMs_ = 0;
  const bool changed = state_ != PhoneCameraState::Unavailable;
  state_ = PhoneCameraState::Unavailable;
  return changed;
}

bool PhoneCameraRemoteController::setLensInfo(PhoneCameraTransport transport,
                                              PhoneCameraLens lens,
                                              bool frontSupported,
                                              bool backSupported) {
  if (transport == PhoneCameraTransport::None ||
      transport != readyTransport_ ||
      lens == PhoneCameraLens::Unknown) {
    return false;
  }

  frontSupported = frontSupported || lens == PhoneCameraLens::Front;
  backSupported = backSupported || lens == PhoneCameraLens::Back;
  const bool changed = lens_ != lens ||
                       frontSupported_ != frontSupported ||
                       backSupported_ != backSupported;
  lens_ = lens;
  frontSupported_ = frontSupported;
  backSupported_ = backSupported;
  return changed;
}

bool PhoneCameraRemoteController::beginShutterRequest(unsigned long now) {
  if (!canRequest()) {
    return false;
  }
  return beginOperation(PhoneCameraOperation::Shutter,
                        PhoneCameraLens::Unknown,
                        "pcam",
                        now);
}

bool PhoneCameraRemoteController::completeShutterRequest(PhoneCameraTransport transport,
                                                         const char* requestId,
                                                         bool success,
                                                         unsigned long now) {
  if (state_ != PhoneCameraState::Pending ||
      pendingOperation_ != PhoneCameraOperation::Shutter ||
      transport == PhoneCameraTransport::None ||
      transport != pendingTransport_ ||
      requestId == nullptr ||
      requestId[0] == '\0' ||
      strcmp(requestId, pendingRequestId_) != 0) {
    return false;
  }

  clearPending();
  startFeedback(success ? PhoneCameraState::Success : PhoneCameraState::Failure, now);
  return true;
}

bool PhoneCameraRemoteController::beginLensRequest(PhoneCameraLens lens, unsigned long now) {
  if (!canSwitchLens() ||
      lens == PhoneCameraLens::Unknown ||
      lens == lens_ ||
      (lens == PhoneCameraLens::Front && !frontSupported_) ||
      (lens == PhoneCameraLens::Back && !backSupported_)) {
    return false;
  }
  return beginOperation(PhoneCameraOperation::LensChange, lens, "lens", now);
}

bool PhoneCameraRemoteController::completeLensRequest(PhoneCameraTransport transport,
                                                      const char* requestId,
                                                      bool applied,
                                                      PhoneCameraLens lens,
                                                      unsigned long now) {
  if (state_ != PhoneCameraState::Pending ||
      pendingOperation_ != PhoneCameraOperation::LensChange ||
      transport == PhoneCameraTransport::None ||
      transport != pendingTransport_ ||
      requestId == nullptr ||
      requestId[0] == '\0' ||
      strcmp(requestId, pendingRequestId_) != 0) {
    return false;
  }

  const PhoneCameraLens requestedLens = pendingLens_;
  if (lens != PhoneCameraLens::Unknown) {
    lens_ = lens;
    if (lens == PhoneCameraLens::Front) {
      frontSupported_ = true;
    } else if (lens == PhoneCameraLens::Back) {
      backSupported_ = true;
    }
  }
  const bool success = applied && lens == requestedLens;
  clearPending();
  startFeedback(success ? PhoneCameraState::Success : PhoneCameraState::Failure, now);
  return true;
}

bool PhoneCameraRemoteController::beginOperation(PhoneCameraOperation operation,
                                                 PhoneCameraLens pendingLens,
                                                 const char* requestIdPrefix,
                                                 unsigned long now) {
  if (operation == PhoneCameraOperation::None ||
      requestIdPrefix == nullptr ||
      requestIdPrefix[0] == '\0' ||
      state_ != PhoneCameraState::Ready ||
      readyTransport_ == PhoneCameraTransport::None) {
    return false;
  }

  ++requestSequence_;
  if (requestSequence_ == 0) {
    ++requestSequence_;
  }
  snprintf(pendingRequestId_,
           sizeof(pendingRequestId_),
           "%s-%08lx-%08lx",
           requestIdPrefix,
           static_cast<unsigned long>(sessionToken_),
           static_cast<unsigned long>(requestSequence_));
  pendingTransport_ = readyTransport_;
  pendingOperation_ = operation;
  pendingLens_ = pendingLens;
  requestStartedMs_ = now;
  feedbackStartedMs_ = 0;
  state_ = PhoneCameraState::Pending;
  return true;
}

bool PhoneCameraRemoteController::disconnectTransport(PhoneCameraTransport transport) {
  if (transport == PhoneCameraTransport::None ||
      (transport != readyTransport_ && transport != pendingTransport_)) {
    return false;
  }

  if (transport == readyTransport_) {
    readyTransport_ = PhoneCameraTransport::None;
  }
  clearPending();
  clearLensInfo();
  feedbackStartedMs_ = 0;
  const bool changed = state_ != PhoneCameraState::Unavailable;
  state_ = PhoneCameraState::Unavailable;
  return changed;
}

bool PhoneCameraRemoteController::reset() {
  const bool changed = state_ != PhoneCameraState::Unavailable ||
                       readyTransport_ != PhoneCameraTransport::None ||
                       pendingTransport_ != PhoneCameraTransport::None;
  state_ = PhoneCameraState::Unavailable;
  readyTransport_ = PhoneCameraTransport::None;
  clearPending();
  clearLensInfo();
  feedbackStartedMs_ = 0;
  return changed;
}

bool PhoneCameraRemoteController::update(unsigned long now) {
  const unsigned long requestTimeoutMs =
    pendingOperation_ == PhoneCameraOperation::LensChange
      ? PHONE_CAMERA_LENS_REQUEST_TIMEOUT_MS
      : PHONE_CAMERA_REQUEST_TIMEOUT_MS;
  if (state_ == PhoneCameraState::Pending &&
      now - requestStartedMs_ >= requestTimeoutMs) {
    clearPending();
    startFeedback(PhoneCameraState::Failure, now);
    return true;
  }

  if ((state_ == PhoneCameraState::Success || state_ == PhoneCameraState::Failure) &&
      now - feedbackStartedMs_ >= PHONE_CAMERA_FEEDBACK_MS) {
    feedbackStartedMs_ = 0;
    state_ = readyTransport_ != PhoneCameraTransport::None
               ? PhoneCameraState::Ready
               : PhoneCameraState::Unavailable;
    return true;
  }
  return false;
}

PhoneCameraState PhoneCameraRemoteController::state() const {
  return state_;
}

PhoneCameraTransport PhoneCameraRemoteController::readyTransport() const {
  return readyTransport_;
}

PhoneCameraTransport PhoneCameraRemoteController::pendingTransport() const {
  return pendingTransport_;
}

PhoneCameraLens PhoneCameraRemoteController::lens() const {
  return lens_;
}

PhoneCameraLens PhoneCameraRemoteController::pendingLens() const {
  return pendingLens_;
}

PhoneCameraOperation PhoneCameraRemoteController::pendingOperation() const {
  return pendingOperation_;
}

const char* PhoneCameraRemoteController::pendingRequestId() const {
  return pendingRequestId_;
}

bool PhoneCameraRemoteController::canRequest() const {
  return state_ == PhoneCameraState::Ready &&
         readyTransport_ != PhoneCameraTransport::None;
}

bool PhoneCameraRemoteController::canSwitchLens() const {
  return state_ == PhoneCameraState::Ready &&
         readyTransport_ != PhoneCameraTransport::None &&
         frontSupported_ &&
         backSupported_ &&
         (lens_ == PhoneCameraLens::Front || lens_ == PhoneCameraLens::Back);
}

void PhoneCameraRemoteController::clearPending() {
  pendingTransport_ = PhoneCameraTransport::None;
  pendingLens_ = PhoneCameraLens::Unknown;
  pendingOperation_ = PhoneCameraOperation::None;
  requestStartedMs_ = 0;
  pendingRequestId_[0] = '\0';
}

void PhoneCameraRemoteController::clearLensInfo() {
  lens_ = PhoneCameraLens::Unknown;
  frontSupported_ = false;
  backSupported_ = false;
}

void PhoneCameraRemoteController::startFeedback(PhoneCameraState state, unsigned long now) {
  state_ = state;
  feedbackStartedMs_ = now;
}
