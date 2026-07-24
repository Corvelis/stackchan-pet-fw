#pragma once

#include <Arduino.h>

namespace Utf8Utils {

uint32_t readCodepoint(const String& text, size_t& index);
void appendCodepoint(String& out, uint32_t codepoint);
void removeLastCodepoint(String& text);
String normalizeHalfwidthKana(const String& text);

} // namespace Utf8Utils
