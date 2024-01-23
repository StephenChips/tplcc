#ifndef TPLCC_CURSOR_H
#define TPLCC_CURSOR_H

#include <memory>
#include <cstdint>

#include "code-buffer.h"

struct ICursor {
  virtual ICursor& next() = 0;
  virtual std::uint32_t currentChar() const = 0;
  virtual CodeBuffer::Offset offset() const = 0;
  virtual ICursor& setOffset(CodeBuffer::Offset offset) = 0;
  virtual std::unique_ptr<ICursor> clone() const = 0;
  virtual ~ICursor() = default;
};

#endif