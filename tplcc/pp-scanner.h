#ifndef TPLCC_PPSCANNER_H
#define TPLCC_PPSCANNER_H

#include <concepts>
#include <tuple>
#include <string>
#include <memory>

#include "./scanner.h"
#include "./code-buffer.h"

template <typename F>
concept ByteDecoderConcept = requires(F func, const unsigned char* addr) {
  { func(addr) } -> std::same_as<std::tuple<int, int>>;
};

class PPLookaheadScanner : public IBaseScanner {};

// A wrapper of PPScanner. It cache look-ahead characters.

template <ByteDecoderConcept F>
class PPScanner : public IBaseScanner {
  PPScannerImpl ppsi;

 public:
  PPScannerImpl(CodeBuffer& codeBuffer, F&& readUTF32)
      : codeBuffer(codeBuffer), byteDecoder(std::forward<F>(readUTF32)){};
  bool reachedEndOfInput() const override;
  CodeBuffer::Offset offset() { }
  F& byteDecoder() { }
};

template <ByteDecoderConcept F>
class PPScannerImpl : public IBaseScanner {
  CodeBuffer& codeBuffer;
  F byteDecoder;

 public:
  PPScannerImpl(CodeBuffer& codeBuffer, F&& readUTF32)
      : codeBuffer(codeBuffer), byteDecoder(std::forward<F>(readUTF32)){};
  bool reachedEndOfInput() const override;
  CodeBuffer::Offset offset() { return 0; }
  F& byteDecoder() { return _readUTF32; }
};

#endif