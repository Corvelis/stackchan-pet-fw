#include "WebSocketServerController.h"

WebSocketServerController* WebSocketServerController::instance_ = nullptr;

void WebSocketServerController::begin(uint16_t port) {
  instance_ = this;
  server_ = new WebSocketsServer(port);
  server_->begin();
  server_->onEvent(WebSocketServerController::handleEvent);
  Serial.printf("[ws] server started on port %u\n", port);
}

void WebSocketServerController::loop() {
  if (server_ != nullptr) {
    server_->loop();
  }
}

void WebSocketServerController::onText(WebSocketTextHandler handler) {
  textHandler_ = handler;
}

void WebSocketServerController::onBinary(WebSocketBinaryHandler handler) {
  binaryHandler_ = handler;
}

void WebSocketServerController::onConnection(WebSocketConnectionHandler handler) {
  connectionHandler_ = handler;
}

bool WebSocketServerController::hasClient() const {
  return hasClient_;
}

uint8_t WebSocketServerController::activeClientId() const {
  return activeClientId_;
}

void WebSocketServerController::sendText(const char* payload) {
  if (!hasClient_ || payload == nullptr) {
    return;
  }
  if (server_ != nullptr) {
    server_->sendTXT(activeClientId_, payload);
  }
}

void WebSocketServerController::sendBinary(const uint8_t* payload, size_t length) {
  if (!hasClient_) {
    return;
  }
  if (server_ != nullptr) {
    server_->sendBIN(activeClientId_, payload, length);
  }
}

void WebSocketServerController::handleEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
  if (instance_ != nullptr) {
    instance_->handleEventInstance(clientId, type, payload, length);
  }
}

void WebSocketServerController::handleEventInstance(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = server_ != nullptr ? server_->remoteIP(clientId) : IPAddress();
      hasClient_ = true;
      activeClientId_ = clientId;
      Serial.printf("[ws] client %u connected from %s\n", clientId, ip.toString().c_str());
      if (connectionHandler_ != nullptr) {
        connectionHandler_(clientId, true);
      }
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[ws] client %u disconnected\n", clientId);
      if (clientId == activeClientId_) {
        hasClient_ = false;
      }
      if (connectionHandler_ != nullptr) {
        connectionHandler_(clientId, false);
      }
      break;
    case WStype_TEXT:
      if (textHandler_ != nullptr) {
        textHandler_(clientId, payload, length);
      }
      break;
    case WStype_BIN:
      if (binaryHandler_ != nullptr) {
        binaryHandler_(clientId, payload, length);
      }
      break;
    default:
      break;
  }
}
