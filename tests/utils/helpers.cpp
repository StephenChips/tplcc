#include <string>

#include "./helpers.h"

std::string fromUTF8(std::u8string s) {
  return std::string(s.begin(), s.end());
}
std::string fromUTF16(std::u16string s) {
  return std::string(s.begin(), s.end());
}
std::string fromUTF32(std::u32string s) {
  return std::string(s.begin(), s.end());
}