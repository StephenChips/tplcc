#include "code-buffer.h"

CodeBuffer::Offset CodeBuffer::section(SectionID id) const {
  return sectionOffsets[id];
}

CodeBuffer::Offset CodeBuffer::sectionEnd(SectionID id) const {
  return id == sectionOffsets.size() - 1 ? buf.size() : sectionOffsets[id + 1];
}
CodeBuffer::Offset CodeBuffer::sectionSize(SectionID id) const {
  return sectionEnd(id) - section(id);
}

CodeBuffer::Offset CodeBuffer::sectionCount() const {
  return sectionOffsets.size();
}

CodeBuffer::SectionID CodeBuffer::addSection(std::string content) {
  const size_t sectionStart = buf.size();
  const size_t sectionID = sectionOffsets.size();
  buf += std::move(content);
  sectionOffsets.push_back(sectionStart);
  return sectionOffsets.size() - 1;
}

std::uint8_t CodeBuffer::operator[](CodeBuffer::Offset index) const {
  return buf[index];
}

const unsigned char* CodeBuffer::pos(CodeBuffer::Offset offset) const {
  return reinterpret_cast<const unsigned char*>(buf.c_str()) + offset;
}