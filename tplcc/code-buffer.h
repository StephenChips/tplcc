#ifndef TPLCC_CODE_BUFFER_H
#define TPLCC_CODE_BUFFER_H

#include <string>
#include <vector>
#include <cstddef>

class CodeBuffer {
 public:
  typedef std::uint32_t SectionID;
  typedef std::uint32_t Offset;

 private:
  std::string buf;
  std::vector<Offset> sectionOffsets;

 public:
  CodeBuffer() = default;
  CodeBuffer(std::string sourceCode)
      : buf(std::move(sourceCode)), sectionOffsets{0} {}
  CodeBuffer::Offset section(SectionID id);
  CodeBuffer::Offset sectionEnd(SectionID id);
  CodeBuffer::Offset sectionSize(SectionID id);
  CodeBuffer::Offset sectionCount();
  SectionID addSection(std::string content);
  char operator[](CodeBuffer::Offset index);
};

#endif