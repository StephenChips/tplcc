#ifndef TPLCC_SCANNER_H
#define TPLCC_SCANNER_H

#include <vector>
#include <string>
#include <cstdint>

struct IGetPeekOnlyScanner {
  virtual int get() = 0;
  virtual int peek() = 0;
  virtual bool reachedEndOfInput() = 0;
};

struct IScanner : IGetPeekOnlyScanner {
	virtual std::string peekN(size_t n) = 0;
	virtual void ignore() = 0;
	virtual void ignoreN(size_t n) = 0;
	virtual std::uint32_t offset() = 0;
	virtual ~IScanner() = default;
};

#endif // !TPLCC_SCANNER_H
