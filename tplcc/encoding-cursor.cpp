#include "encoding-cursor.h"

#include <cstdint>

#include "memory"

std::unique_ptr<ICursor> UTF8Cursor::create(const CodeBuffer &buffer, const CodeBuffer::Offset offset) {
  return std::make_unique<UTF8Cursor>(buffer, offset);
}

unsigned char UTF8Cursor::charByteLength() const { return charLength; }

void UTF8Cursor::readChar() {
  if (buffer[cursor] >> 7 == 0) {
    charLength = 1;
    curCh = buffer[cursor];
  } else if (buffer[cursor] >> 5 == 0b00000110) {
    charLength = 2;
    curCh = (buffer[cursor] & 0b00011111) << 6;
    curCh |= buffer[cursor + 1] & 0b00111111;
  } else if (buffer[cursor] >> 4 == 0b00001110) {
    charLength = 3;
    curCh = (buffer[cursor] & 0b00001111) << 12;
    curCh |= (buffer[cursor + 1] & 0b00111111) << 6;
    curCh |= buffer[cursor + 2] & 0b00111111;
  } else if (buffer[cursor] >> 3 == 0b00011110) {
    charLength = 4;
    curCh = (buffer[cursor] & 0b00000111) << 18;
    curCh |= (buffer[cursor + 1] & 0b00111111) << 12;
    curCh |= (buffer[cursor + 2] & 0b00111111) << 6;
    curCh |= buffer[cursor + 3] & 0b00111111;
  }
}

ICursor &UTF8Cursor::next() {
  // TODO: insert return statement here
  cursor += charLength;
  readChar();
  return *this;
}

std::uint32_t UTF8Cursor::currentChar() const { return curCh; }

CodeBuffer::Offset UTF8Cursor::offset() const { return cursor; }

ICursor &UTF8Cursor::setOffset(CodeBuffer::Offset offset) {
  cursor = offset;
  readChar();
  return *this;
}

std::unique_ptr<ICursor> UTF8Cursor::clone() const {
  return std::make_unique<UTF8Cursor>(*this);
}
