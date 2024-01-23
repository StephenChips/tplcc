#ifndef TPLCC_ENCODING_CURSOR_H
#define TPLCC_ENCODING_CURSOR_H

#include <cstdint>
#include <memory>

#include "code-buffer.h"
#include "cursor.h"

class UTF8Cursor : public ICursor {
  const CodeBuffer& buffer;
  CodeBuffer::Offset cursor = 0;

  std::uint32_t curCh{0xFFFFFFFF};
  int charLength;

 public:
  UTF8Cursor(const CodeBuffer& buffer) : buffer(buffer) {}
  UTF8Cursor(const CodeBuffer& buffer, const CodeBuffer::Offset offset)
      : buffer(buffer), cursor(offset) {
    readChar();
  }
  // Inherited via ICursor
  ICursor& next() override;
  std::uint32_t currentChar() const override;
  CodeBuffer::Offset offset() const override;
  ICursor& setOffset(CodeBuffer::Offset offset) override;
  std::unique_ptr<ICursor> clone() const override;

  static std::unique_ptr<ICursor> create(const CodeBuffer&,
                                         const CodeBuffer::Offset offset);

 private:
  unsigned char charByteLength() const;
  void readChar();
};

#endif