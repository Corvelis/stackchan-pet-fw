#pragma once

#include <Arduino.h>

namespace UsbSerialProtocol {

constexpr uint8_t kMagic[4] = {'S', 'C', 'U', '1'};
constexpr uint8_t kVersion = 0x01;
constexpr uint8_t kTypeJson = 0x01;
constexpr uint8_t kTypeTtsPcm = 0x02;
constexpr uint8_t kTypeMicPcm = 0x03;
constexpr uint8_t kTypeCaptureRequest = 0x04;
constexpr uint8_t kTypeCaptureImageChunk = 0x05;
constexpr uint8_t kTypeAck = 0x06;
constexpr uint8_t kTypeError = 0x07;
constexpr uint8_t kTypePing = 0x08;
constexpr uint8_t kTypePong = 0x09;

uint32_t readLe32(const uint8_t* data);
void writeLe32(uint8_t* data, uint32_t value);
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length);

} // namespace UsbSerialProtocol
