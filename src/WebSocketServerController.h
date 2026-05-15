#pragma once

#include <Arduino.h>
#include <esp_http_server.h>

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
  static esp_err_t handleWsRequest(httpd_req_t* request);
  esp_err_t handleWsRequestInstance(httpd_req_t* request);
  void handleDisconnect();
  bool clientIsConnected() const;
  bool sendFrame(httpd_ws_type_t type, const uint8_t* payload, size_t length);

  httpd_handle_t server_ = nullptr;
  WebSocketTextHandler textHandler_ = nullptr;
  WebSocketBinaryHandler binaryHandler_ = nullptr;
  WebSocketConnectionHandler connectionHandler_ = nullptr;
  bool hasClient_ = false;
  int activeClientFd_ = -1;
};
