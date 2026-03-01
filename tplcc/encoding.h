#ifndef TPLCC_ENCODING_H
#define TPLCC_ENCODING_H

#include <concepts>

using UTF32Char = char32_t;

struct CharDecodeResult {
  using CharType = char32_t;
  using CharLength = unsigned long;
  CharType codepoint;
  CharLength length;
};

template <typename T>
concept CharDecodeFunc = requires(T decodeFunc, const char* addr) {
  { decodeFunc(addr) } -> std::same_as<CharDecodeResult>;
};

inline CharDecodeResult decodeUTF8(const char* buffer) {
  CharDecodeResult::CharType ch;
  CharDecodeResult::CharLength len;

  auto buf = reinterpret_cast<const unsigned char*>(buffer);

  if (buf[0] >> 7 == 0) {
    len = 1;
    ch = buf[0];
  } else if (buf[0] >> 5 == 0b00000110) {
    len = 2;
    ch = (buf[0] & 0b00011111) << 6;
    ch |= buf[1] & 0b00111111;
  } else if (buf[0] >> 4 == 0b00001110) {
    len = 3;
    ch = (buf[0] & 0b00001111) << 12;
    ch |= (buf[1] & 0b00111111) << 6;
    ch |= buf[2] & 0b00111111;
  } else if (buf[0] >> 3 == 0b00011110) {
    len = 4;
    ch = (buf[0] & 0b00000111) << 18;
    ch |= (buf[1] & 0b00111111) << 12;
    ch |= (buf[2] & 0b00111111) << 6;
    ch |= buf[3] & 0b00111111;
  }

  return {ch, len};
}

#endif