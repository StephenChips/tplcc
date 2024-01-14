#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "code-buffer.h"
#include "error.h"
#include "scanner.h"

struct Loc {
  size_t lineNumber;
  size_t charOffset;
};

struct MacroExpansionRecord {
  std::vector<std::string> includeStack;
  Loc macroLocation;
  std::string
      nameOfMacroDefFile;  // the name of file where the macro is defined.
  std::string expandedText;
};

struct CodeLocation {
  std::vector<std::string> includeStack;
  Loc location;
  MacroExpansionRecord* macro;
};

struct IMacroExpansionRecords {
  virtual std::vector<MacroExpansionRecord> macroExpansionRecords() = 0;
  ~IMacroExpansionRecords() = default;
};

class Preprocessor : IScanner, IMacroExpansionRecords {
  IScanner& scanner;
  ICodeBuffer& codeBuffer;
  IReportError& errOut;

 public:
  Preprocessor(IScanner& scanner, ICodeBuffer& codeBuffer, IReportError& errOut)
      : scanner(scanner), codeBuffer(codeBuffer), errOut(errOut) {}

  // Inherited via IScanner
  int get() override;
  int peek() override;
  std::vector<int> peekN(size_t n) override;
  void ignore() override;
  void ignoreN(size_t n) override;
  bool reachedEndOfInput() override;
  std::uint32_t offset() override;

  // Inherited via IMacroExpansionRecords
  std::vector<MacroExpansionRecord> macroExpansionRecords() override;
};

#endif  // !TPLCC_PREPROCESSOR_H
