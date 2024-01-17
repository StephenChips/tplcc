#ifndef TPLCC_CODE_BUFFER_H
#define TPLCC_CODE_BUFFER_H

#include <string>
#include <vector>

class CodeBuffer {
  std::string buf;
  std::vector<size_t> sectionOffsets;

 public:
  typedef size_t SectionID;
  CodeBuffer() = default;
  CodeBuffer(std::string sourceCode)
      : buf(std::move(sourceCode)), sectionOffsets{0} {}
  size_t offset(const char* p);
  const char* section(SectionID id);
  const char* secitonEnd(SectionID id);
  size_t sectionSize(SectionID id);
  size_t sectionCount();
  SectionID addSection(std::string content);
};

#endif