#ifndef TPLCC_TESTS_UTILS_HELPERS_H
#define TPLCC_TESTS_UTILS_HELPERS_H

#include <string>

std::string fromUTF8(std::u8string);
std::string fromUTF16(std::u16string);
std::string fromUTF32(std::u32string);

#endif