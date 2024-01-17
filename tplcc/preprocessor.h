#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <optional>
#include <set>
#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

#include "code-buffer.h"
#include "error.h"
#include "scanner.h"

struct MacroDefinition {
  enum MacroType { OBJECT_LIKE_MACRO, FUNCTION_LIKE_MACRO } type;
  std::string name;
  std::vector<std::string> parameters;
  std::string body;

  MacroDefinition(std::string name, std::string body,
                  MacroType type = OBJECT_LIKE_MACRO)
      : name(std::move(name)), body(std::move(body)), type(type){};

  MacroDefinition(std::string name, std::vector<std::string> parameters,
                  std::string body, MacroType type = FUNCTION_LIKE_MACRO)
      : name(std::move(name)),
        body(std::move(body)),
        parameters(std::move(parameters)),
        type(type){};
};

using PreprocessorDirective = std::variant<MacroDefinition>;

struct Loc {
  size_t lineNumber;
  size_t charOffset;
};

struct MacroExpansionRecord {
  std::vector<std::string> includeStack;
  size_t macroLocation;
  // the name of file where the macro is defined.
  std::string nameOfMacroDefFile;
  std::string expandedText;
};

struct CodeLocation {
  std::vector<std::string> includeStack;
  Loc location;
  MacroExpansionRecord* macro;
};

struct IMacroExpansionRecords {
  virtual std::vector<MacroExpansionRecord>& macroExpansionRecords() = 0;
  ~IMacroExpansionRecords() = default;
};

bool isControlLineSpace(const char ch);
bool isNewlineCharacter(const char ch);

class Preprocessor;

// Only use it when scanning a preprocessing directive's content.
class DirectiveContentScanner : public IGetPeekOnlyScanner {
  Preprocessor* const pp;

 public:
  DirectiveContentScanner(Preprocessor* _pp);
  virtual int get();
  virtual int peek();
  virtual bool reachedEndOfInput();
};

class Preprocessor : IScanner, IMacroExpansionRecords {
  struct CompareMacroDefinition {
    using is_transparent = std::true_type;

    bool operator()(const MacroDefinition& lhs,
                    const MacroDefinition& rhs) const {
      return lhs.name < rhs.name;
    }

    bool operator()(const std::string& lhs, const MacroDefinition& rhs) const {
      return lhs < rhs.name;
    }

    bool operator()(const MacroDefinition& lhs, const std::string& rhs) const {
      return lhs.name < rhs;
    }
  };

  const char* cursor;
  const char* endOfNonMacroPPToken = nullptr;

  CodeBuffer& codeBuffer;
  IReportError& errOut;

  // A cache for macro that has been expanded before.
  std::map<std::string, CodeBuffer::SectionID> codeCache;

  std::vector<size_t> stackOfSectionID;
  std::vector<const char*> stackOfStoredCursor;
  std::vector<MacroExpansionRecord> vectorOfMacroExpansion;
  std::set<MacroDefinition, CompareMacroDefinition> setOfMacroDefinitions;
  DirectiveContentScanner dcs;

  friend class DirectiveContentScanner;

 public:
  // Change IScanner to IPreprocessor
  Preprocessor(CodeBuffer& codeBuffer, IReportError& errOut)
      : errOut(errOut),
        codeBuffer(codeBuffer),
        cursor(codeBuffer.section(0)),
        dcs(this) {}

  // Inherited via IScanner
  int get() override;
  int peek() override;
  std::string peekN(size_t n) override;
  void ignore() override;
  void ignoreN(size_t n) override;
  bool reachedEndOfInput() override;
  std::uint32_t offset() override;  // TODO Maby change to bufferOffset

  // Inherited via IMacroExpansionRecords
  std::vector<MacroExpansionRecord>& macroExpansionRecords() override;

 private:
  static void initializeBuffer(Preprocessor* pp, CodeBuffer& codeBuffer);
  void parseControlLine();
  void addPreprocessorDirective(PreprocessorDirective directive);
  void skipNewline();
  void skipControlLineSpaces();
  void skipControlLine();
  void skipControlLineWhitespaces();
  void skipWhitespaces();
  std::string scanControlLine();
  bool reachedEndOfSection();
  bool reachedEndOfLine();
  CodeBuffer::SectionID currentSectionID();
  const char* currentSection();
  void enterSection(CodeBuffer::SectionID id);
  void exitSection();
};

#endif  // !TPLCC_PREPROCESSOR_H
