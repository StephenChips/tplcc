#ifndef TPLCC_SCANNER_H
#define TPLCC_SCANNER_H

#include <vector>
#include <string>
#include <cstdint>

struct IBaseScanner {
  virtual int get() = 0;
  virtual int peek() const = 0;
  virtual bool reachedEndOfInput() const = 0;
};

#endif // !TPLCC_SCANNER_H
