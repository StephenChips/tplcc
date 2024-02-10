#include "encoding.h"

std::tuple<int, int> utf8(const unsigned char *buffer) {
  unsigned int codepoint;
  int charlen;
  if (buffer[0] >> 7 == 0) {
    charlen = 1;
    codepoint = buffer[0];
  } else if (buffer[0] >> 5 == 0b00000110) {
    charlen = 2;
    codepoint = (buffer[0] & 0b00011111) << 6;
    codepoint |= buffer[1] & 0b00111111;
  } else if (buffer[0] >> 4 == 0b00001110) {
    charlen = 3;
    codepoint = (buffer[0] & 0b00001111) << 12;
    codepoint |= (buffer[1] & 0b00111111) << 6;
    codepoint |= buffer[2] & 0b00111111;
  } else if (buffer[0] >> 3 == 0b00011110) {
    charlen = 4;
    codepoint = (buffer[0] & 0b00000111) << 18;
    codepoint |= (buffer[1] & 0b00111111) << 12;
    codepoint |= (buffer[2] & 0b00111111) << 6;
    codepoint |= buffer[3] & 0b00111111;
  }

  return {codepoint, charlen};
}
