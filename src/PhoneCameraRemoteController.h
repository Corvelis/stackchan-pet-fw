#pragma once

#include <Arduino.h>

enum class PhoneCameraState : uint8_t {
  Unavailable,
  Ready,
  Pending,
  Success,
  Failure,
};

enum class PhoneCameraTransport : uint8_t {
  None,
  WebSocket,
  UsbSerial,
};

enum class PhoneCameraLens : uint8_t {
  Unknown,
  Front,
  Back,
};

enum class PhoneCameraOperation : uint8_t {
  None,
  Shutter,
  LensChange,
};

class PhoneCameraRemoteController {
public:
  void begin(uint32_t sessionToken);
  bool setReady(PhoneCameraTransport transport, bool ready, unsigned long now);
  bool setLensInfo(PhoneCameraTransport transport,
                   PhoneCameraLens lens,
                   bool frontSupported,
                   bool backSupported);
  bool beginShutterRequest(unsigned long now);
  bool completeShutterRequest(PhoneCameraTransport transport,
                              const char* requestId,
                              bool success,
                              unsigned long now);
  bool beginLensRequest(PhoneCameraLens lens, unsigned long now);
  bool completeLensRequest(PhoneCameraTransport transport,
                           const char* requestId,
                           bool applied,
                           PhoneCameraLens lens,
                           unsigned long now);
  bool disconnectTransport(PhoneCameraTransport transport);
  bool reset();
  bool update(unsigned long now);

  PhoneCameraState state() const;
  PhoneCameraTransport readyTransport() const;
  PhoneCameraTransport pendingTransport() const;
  PhoneCameraLens lens() const;
  PhoneCameraLens pendingLens() const;
  PhoneCameraOperation pendingOperation() const;
  const char* pendingRequestId() const;
  bool canRequest() const;
  bool canSwitchLens() const;

private:
  bool beginOperation(PhoneCameraOperation operation,
                      PhoneCameraLens pendingLens,
                      const char* requestIdPrefix,
                      unsigned long now);
  void clearPending();
  void clearLensInfo();
  void startFeedback(PhoneCameraState state, unsigned long now);

  PhoneCameraState state_ = PhoneCameraState::Unavailable;
  PhoneCameraTransport readyTransport_ = PhoneCameraTransport::None;
  PhoneCameraTransport pendingTransport_ = PhoneCameraTransport::None;
  PhoneCameraLens lens_ = PhoneCameraLens::Unknown;
  PhoneCameraLens pendingLens_ = PhoneCameraLens::Unknown;
  PhoneCameraOperation pendingOperation_ = PhoneCameraOperation::None;
  bool frontSupported_ = false;
  bool backSupported_ = false;
  uint32_t sessionToken_ = 0;
  uint32_t requestSequence_ = 0;
  unsigned long requestStartedMs_ = 0;
  unsigned long feedbackStartedMs_ = 0;
  char pendingRequestId_[48] = {};
};
