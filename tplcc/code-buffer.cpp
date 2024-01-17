#include "code-buffer.h"

size_t CodeBuffer::offset(const char* p) { return p - buf.c_str(); }

const char* CodeBuffer::section(SectionID id) {
  return buf.c_str() + sectionOffsets[id];
}
const char* CodeBuffer::secitonEnd(SectionID id) {
  if (id == sectionOffsets.size() - 1) {
    return buf.c_str() + buf.size();
  } else {
    return buf.c_str() + sectionOffsets[id + 1];
  }
}
size_t CodeBuffer::sectionSize(SectionID id) {
  const size_t sectionStart = sectionOffsets[id];
  size_t sectionEnd;

  if (id == sectionOffsets.size() - 1) {
    sectionEnd = buf.size();
  } else {
    sectionEnd = sectionOffsets[id + 1];
  }

  return sectionEnd - sectionStart;
}

size_t CodeBuffer::sectionCount() { return sectionOffsets.size(); }

CodeBuffer::SectionID CodeBuffer::addSection(
    std::string content) {
  const size_t sectionStart = buf.size();
  const size_t sectionID = sectionOffsets.size();
  buf += std::move(content);
  sectionOffsets.push_back(sectionStart);
  return sectionOffsets.size() - 1;
}