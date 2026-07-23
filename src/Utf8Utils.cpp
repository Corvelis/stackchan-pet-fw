#include "Utf8Utils.h"

namespace {

uint32_t halfwidthKanaToFullwidth(uint32_t codepoint) {
  static const uint16_t map[] = {
    0x3002, 0x300c, 0x300d, 0x3001, 0x30fb, 0x30f2, 0x30a1, 0x30a3,
    0x30a5, 0x30a7, 0x30a9, 0x30e3, 0x30e5, 0x30e7, 0x30c3, 0x30fc,
    0x30a2, 0x30a4, 0x30a6, 0x30a8, 0x30aa, 0x30ab, 0x30ad, 0x30af,
    0x30b1, 0x30b3, 0x30b5, 0x30b7, 0x30b9, 0x30bb, 0x30bd, 0x30bf,
    0x30c1, 0x30c4, 0x30c6, 0x30c8, 0x30ca, 0x30cb, 0x30cc, 0x30cd,
    0x30ce, 0x30cf, 0x30d2, 0x30d5, 0x30d8, 0x30db, 0x30de, 0x30df,
    0x30e0, 0x30e1, 0x30e2, 0x30e4, 0x30e6, 0x30e8, 0x30e9, 0x30ea,
    0x30eb, 0x30ec, 0x30ed, 0x30ef, 0x30f3, 0x3099, 0x309a,
  };
  if (codepoint < 0xff61 || codepoint > 0xff9f) {
    return codepoint;
  }
  return map[codepoint - 0xff61];
}

uint32_t voicedKatakana(uint32_t base, uint32_t mark) {
  if (mark == 0xff9e) {
    switch (base) {
      case 0x30a6: return 0x30f4;
      case 0x30ab: return 0x30ac;
      case 0x30ad: return 0x30ae;
      case 0x30af: return 0x30b0;
      case 0x30b1: return 0x30b2;
      case 0x30b3: return 0x30b4;
      case 0x30b5: return 0x30b6;
      case 0x30b7: return 0x30b8;
      case 0x30b9: return 0x30ba;
      case 0x30bb: return 0x30bc;
      case 0x30bd: return 0x30be;
      case 0x30bf: return 0x30c0;
      case 0x30c1: return 0x30c2;
      case 0x30c4: return 0x30c5;
      case 0x30c6: return 0x30c7;
      case 0x30c8: return 0x30c9;
      case 0x30cf: return 0x30d0;
      case 0x30d2: return 0x30d3;
      case 0x30d5: return 0x30d6;
      case 0x30d8: return 0x30d9;
      case 0x30db: return 0x30dc;
      default: return base;
    }
  }
  if (mark == 0xff9f) {
    switch (base) {
      case 0x30cf: return 0x30d1;
      case 0x30d2: return 0x30d4;
      case 0x30d5: return 0x30d7;
      case 0x30d8: return 0x30da;
      case 0x30db: return 0x30dd;
      default: return base;
    }
  }
  return base;
}

} // namespace

namespace Utf8Utils {

uint32_t readCodepoint(const String& text, size_t& index) {
  if (index >= text.length()) {
    return 0;
  }

  const uint8_t first = static_cast<uint8_t>(text[index++]);
  if (first < 0x80) {
    return first;
  }

  uint32_t codepoint = 0;
  uint8_t continuationCount = 0;
  if ((first & 0xe0) == 0xc0) {
    codepoint = first & 0x1f;
    continuationCount = 1;
  } else if ((first & 0xf0) == 0xe0) {
    codepoint = first & 0x0f;
    continuationCount = 2;
  } else if ((first & 0xf8) == 0xf0) {
    codepoint = first & 0x07;
    continuationCount = 3;
  } else {
    return 0xfffd;
  }

  for (uint8_t i = 0; i < continuationCount; ++i) {
    if (index >= text.length()) {
      return 0xfffd;
    }
    const uint8_t next = static_cast<uint8_t>(text[index]);
    if ((next & 0xc0) != 0x80) {
      return 0xfffd;
    }
    ++index;
    codepoint = (codepoint << 6) | (next & 0x3f);
  }
  return codepoint;
}

void appendCodepoint(String& out, uint32_t codepoint) {
  if (codepoint <= 0x7f) {
    out += static_cast<char>(codepoint);
  } else if (codepoint <= 0x7ff) {
    out += static_cast<char>(0xc0 | (codepoint >> 6));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  } else if (codepoint <= 0xffff) {
    out += static_cast<char>(0xe0 | (codepoint >> 12));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  } else {
    out += static_cast<char>(0xf0 | (codepoint >> 18));
    out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
    out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
    out += static_cast<char>(0x80 | (codepoint & 0x3f));
  }
}

void removeLastCodepoint(String& text) {
  if (text.isEmpty()) {
    return;
  }
  int32_t index = static_cast<int32_t>(text.length()) - 1;
  while (index > 0 && (static_cast<uint8_t>(text[index]) & 0xc0) == 0x80) {
    --index;
  }
  text.remove(static_cast<unsigned int>(index));
}

String normalizeHalfwidthKana(const String& text) {
  String out;
  out.reserve(text.length());
  for (size_t index = 0; index < text.length();) {
    const uint32_t codepoint = readCodepoint(text, index);
    if (codepoint >= 0xff61 && codepoint <= 0xff9f) {
      const uint32_t base = halfwidthKanaToFullwidth(codepoint);
      if (index < text.length()) {
        size_t markIndex = index;
        const uint32_t mark = readCodepoint(text, markIndex);
        if (mark == 0xff9e || mark == 0xff9f) {
          appendCodepoint(out, voicedKatakana(base, mark));
          index = markIndex;
          continue;
        }
      }
      appendCodepoint(out, base);
    } else {
      appendCodepoint(out, codepoint);
    }
  }
  return out;
}

} // namespace Utf8Utils
