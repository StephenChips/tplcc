#ifndef TPLCC_CODE_BUFFER_H
#define TPLCC_CODE_BUFFER_H

#include <string>
#include <tuple>
#include <vector>

struct ICodeBuffer {
  using SectionID = size_t;
  virtual const char* section(SectionID) = 0;
  virtual size_t sectionSize(SectionID) = 0;
  virtual size_t sectionCount() = 0;
  virtual std::tuple<const char*, SectionID> addSection(
      std::string content) = 0;
  ~ICodeBuffer() = default;
};

class CodeBuffer : public ICodeBuffer {
  std::string buffer;
  std::vector<size_t> sectionOffsets;

 public:
  CodeBuffer() = default;

  virtual const char* section(SectionID id) override {
    return buffer.c_str() + sectionOffsets[id];
  }

  virtual size_t sectionSize(SectionID id) override {
    const size_t sectionStart = sectionOffsets[id];
    size_t sectionEnd;

    if (id == sectionOffsets.size() - 1) {
      sectionEnd = buffer.size();
    } else {
      sectionEnd = sectionOffsets[id + 1];
    }

    return sectionEnd - sectionStart;
  }

  virtual size_t sectionCount() override { return sectionOffsets.size(); }

  virtual std::tuple<const char*, SectionID> addSection(
      std::string content) override {
    const size_t sectionStart = buffer.size();
    const size_t sectionID = sectionOffsets.size();
    buffer += std::move(content);
    sectionOffsets.push_back(sectionStart);
    return std::make_tuple(section(sectionID), sectionOffsets.size() - 1);
  }
};

#endif