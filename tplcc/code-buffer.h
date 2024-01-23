#ifndef TPLCC_CODE_BUFFER_H
#define TPLCC_CODE_BUFFER_H

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>

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
  CodeBuffer::Offset section(SectionID id) const;
  CodeBuffer::Offset sectionEnd(SectionID id) const;
  CodeBuffer::Offset sectionSize(SectionID id) const;
  CodeBuffer::Offset sectionCount() const;
  SectionID addSection(std::string content);
  std::uint8_t operator[](CodeBuffer::Offset index) const;
};

#endif