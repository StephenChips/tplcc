#include "code-buffer.h"

CodeBuffer::Offset CodeBuffer::section(SectionID id) {
  return sectionOffsets[id];
}

CodeBuffer::Offset CodeBuffer::sectionEnd(SectionID id) {
  return id == sectionOffsets.size() - 1 ? buf.size() : sectionOffsets[id + 1];
}
CodeBuffer::Offset CodeBuffer::sectionSize(SectionID id) {
  return sectionEnd(id) - section(id);
}

CodeBuffer::Offset CodeBuffer::sectionCount() { return sectionOffsets.size(); }

CodeBuffer::SectionID CodeBuffer::addSection(std::string content) {
  const size_t sectionStart = buf.size();
  const size_t sectionID = sectionOffsets.size();
  buf += std::move(content);
  sectionOffsets.push_back(sectionStart);
  return sectionOffsets.size() - 1;
}

char CodeBuffer::operator[](CodeBuffer::Offset index) { return buf[index]; }