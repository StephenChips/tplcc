#include "preprocessor.h"

int Preprocessor::get() { return 0; }

int Preprocessor::peek() { return 0; }

std::string Preprocessor::peekN(size_t n) { return ""; }

void Preprocessor::ignore() {}

void Preprocessor::ignoreN(size_t n) {}

bool Preprocessor::reachedEndOfInput() { return true; }

std::uint32_t Preprocessor::offset() { return std::uint32_t(); }

std::vector<MacroExpansionRecord> Preprocessor::macroExpansionRecords() {
  return std::vector<MacroExpansionRecord>();
}
