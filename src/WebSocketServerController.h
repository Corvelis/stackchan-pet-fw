#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>

using WebSocketTextHandler = void (*)(uint8_t clientId, const uint8_t* payload, size_t length);
using WebSocketBinaryHandler = void (*)(uint8_t clientId, uint8_t* payload, size_t length);
using WebSocketConnectionHandler = void (*)(uint8_t clientId, bool connected);

class WebSocketServerController {
public:
  void begin(uint16_t port);
  void loop();
  void onText(WebSocketTextHandler handler);
  void onBinary(WebSocketBinaryHandler handler);
  void onConnection(WebSocketConnectionHandler handler);
  bool hasClient() const;
  uint8_t activeClientId() const;
  void sendText(const char* payload);
  void sendBinary(const uint8_t* payload, size_t length);

private:
  static void handleEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length);
  void handleEventInstance(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length);

  WebSocketsServer* server_ = nullptr;
  WebSocketTextHandler textHandler_ = nullptr;
  WebSocketBinaryHandler binaryHandler_ = nullptr;
  WebSocketConnectionHandler connectionHandler_ = nullptr;
  bool hasClient_ = false;
  uint8_t activeClientId_ = 0;

  static WebSocketServerController* instance_;
};
