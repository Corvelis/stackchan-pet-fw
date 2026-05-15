#include "WebSocketServerController.h"

#include <cstring>
#include <cstdlib>

void WebSocketServerController::begin(uint16_t port) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  config.ctrl_port = port + 1;
  config.lru_purge_enable = true;

  esp_err_t error = httpd_start(&server_, &config);
  if (error != ESP_OK) {
    Serial.printf("[ws] server start failed: 0x%x\n", static_cast<unsigned>(error));
    server_ = nullptr;
    return;
  }

  httpd_uri_t wsUri;
  std::memset(&wsUri, 0, sizeof(wsUri));
  wsUri.uri = "/";
  wsUri.method = HTTP_GET;
  wsUri.handler = WebSocketServerController::handleWsRequest;
  wsUri.user_ctx = this;
  wsUri.is_websocket = true;
  wsUri.handle_ws_control_frames = false;

  error = httpd_register_uri_handler(server_, &wsUri);
  if (error != ESP_OK) {
    Serial.printf("[ws] uri register failed: 0x%x\n", static_cast<unsigned>(error));
    httpd_stop(server_);
    server_ = nullptr;
    return;
  }

  Serial.printf("[ws] server started on port %u\n", port);
}

void WebSocketServerController::loop() {
  if (hasClient_ && !clientIsConnected()) {
    handleDisconnect();
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
  return hasClient_ && clientIsConnected();
}

uint8_t WebSocketServerController::activeClientId() const {
  return activeClientFd_ >= 0 ? static_cast<uint8_t>(activeClientFd_) : 0;
}

void WebSocketServerController::sendText(const char* payload) {
  if (payload == nullptr) {
    return;
  }
  sendFrame(HTTPD_WS_TYPE_TEXT, reinterpret_cast<const uint8_t*>(payload), std::strlen(payload));
}

void WebSocketServerController::sendBinary(const uint8_t* payload, size_t length) {
  sendFrame(HTTPD_WS_TYPE_BINARY, payload, length);
}

esp_err_t WebSocketServerController::handleWsRequest(httpd_req_t* request) {
  if (request == nullptr || request->user_ctx == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  auto* controller = static_cast<WebSocketServerController*>(request->user_ctx);
  return controller->handleWsRequestInstance(request);
}

esp_err_t WebSocketServerController::handleWsRequestInstance(httpd_req_t* request) {
  const int clientFd = httpd_req_to_sockfd(request);

  if (request->method == HTTP_GET) {
    hasClient_ = true;
    activeClientFd_ = clientFd;
    Serial.printf("[ws] client %d connected\n", clientFd);
    if (connectionHandler_ != nullptr) {
      connectionHandler_(activeClientId(), true);
    }
    return ESP_OK;
  }

  httpd_ws_frame_t frame;
  std::memset(&frame, 0, sizeof(frame));
  esp_err_t error = httpd_ws_recv_frame(request, &frame, 0);
  if (error != ESP_OK) {
    Serial.printf("[ws] recv size failed: 0x%x\n", static_cast<unsigned>(error));
    if (clientFd == activeClientFd_) {
      handleDisconnect();
    }
    return error;
  }

  uint8_t* payload = nullptr;
  if (frame.len > 0) {
    payload = static_cast<uint8_t*>(std::malloc(frame.len + 1));
    if (payload == nullptr) {
      Serial.printf("[ws] payload alloc failed: %u bytes\n", static_cast<unsigned>(frame.len));
      return ESP_ERR_NO_MEM;
    }
    std::memset(payload, 0, frame.len + 1);
    frame.payload = payload;

    error = httpd_ws_recv_frame(request, &frame, frame.len);
    if (error != ESP_OK) {
      Serial.printf("[ws] recv payload failed: 0x%x\n", static_cast<unsigned>(error));
      std::free(payload);
      if (clientFd == activeClientFd_) {
        handleDisconnect();
      }
      return error;
    }
  }

  if (frame.type == HTTPD_WS_TYPE_TEXT) {
    if (textHandler_ != nullptr) {
      textHandler_(activeClientId(), payload, frame.len);
    }
  } else if (frame.type == HTTPD_WS_TYPE_BINARY) {
    if (binaryHandler_ != nullptr) {
      binaryHandler_(activeClientId(), payload, frame.len);
    }
  } else if (frame.type == HTTPD_WS_TYPE_CLOSE && clientFd == activeClientFd_) {
    handleDisconnect();
  }

  std::free(payload);
  return ESP_OK;
}

void WebSocketServerController::handleDisconnect() {
  if (!hasClient_) {
    return;
  }

  const uint8_t clientId = activeClientId();
  Serial.printf("[ws] client %u disconnected\n", clientId);
  hasClient_ = false;
  activeClientFd_ = -1;
  if (connectionHandler_ != nullptr) {
    connectionHandler_(clientId, false);
  }
}

bool WebSocketServerController::clientIsConnected() const {
  return server_ != nullptr &&
         activeClientFd_ >= 0 &&
         httpd_ws_get_fd_info(server_, activeClientFd_) == HTTPD_WS_CLIENT_WEBSOCKET;
}

bool WebSocketServerController::sendFrame(httpd_ws_type_t type, const uint8_t* payload, size_t length) {
  if (!hasClient_ || !clientIsConnected()) {
    if (hasClient_) {
      const_cast<WebSocketServerController*>(this)->handleDisconnect();
    }
    return false;
  }

  httpd_ws_frame_t frame;
  std::memset(&frame, 0, sizeof(frame));
  frame.type = type;
  frame.payload = const_cast<uint8_t*>(payload);
  frame.len = length;

  const esp_err_t error = httpd_ws_send_frame_async(server_, activeClientFd_, &frame);
  if (error != ESP_OK) {
    Serial.printf("[ws] send failed: 0x%x\n", static_cast<unsigned>(error));
    handleDisconnect();
    return false;
  }
  return true;
}
