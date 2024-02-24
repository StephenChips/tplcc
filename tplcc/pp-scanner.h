#ifndef TPLCC_PPSCANNER_H
#define TPLCC_PPSCANNER_H

#include <concepts>
#include <memory>
#include <string>
#include <tuple>

#include "./code-buffer.h"
#include "./scanner.h"

template <typename F>
concept ByteDecoderConcept = requires(F func, const unsigned char* addr) {
  { func(addr) } -> std::same_as<std::tuple<int, int>>;
};
//
// class PPLookaheadScanner : public IBaseScanner {};

// A wrapper of PPScanner. It cache look-ahead characters.

// template <ByteDecoderConcept F>
// class PPScannerImpl : public IBaseScanner {
//   CodeBuffer& codeBuffer;
//   F byteDecoder;
//
//  public:
//   PPScannerImpl(CodeBuffer& codeBuffer, F&& readUTF32)
//       : codeBuffer(codeBuffer), byteDecoder(std::forward<F>(readUTF32)){};
//   bool reachedEndOfInput() const override;
//   CodeBuffer::Offset offset() { return 0; }
//   F& byteDecoder() { return _readUTF32; }
// };

template <ByteDecoderConcept F>
class PPScanner : public IBaseScanner {
  F& readUTF32;

 public:
  PPScanner(CodeBuffer& codeBuffer, F& readUTF32) : readUTF32(readUTF32){};
  int get() override { return 0; }
  int peek() const override { return 0; }
  bool reachedEndOfInput() const override { return true; }
  CodeBuffer::Offset offset() { return 0; }
  F& byteDecoder() { return readUTF32; }
};

#endif