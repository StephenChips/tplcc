#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <compare>
#include <map>
#include <optional>
#include <set>
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

bool isDirectiveSpace(int ch);
bool isNewlineCharacter(int ch);

class Preprocessor;

class PPCursor {
  Preprocessor& pp;
  CodeBuffer::Offset cursor;

 public:
  PPCursor(Preprocessor& pp, CodeBuffer::Offset offset);
  PPCursor(const PPCursor& other) : pp(other.pp), cursor(other.cursor) {}
  PPCursor& operator++();
  PPCursor operator++(int);
  const char operator*() const;
  PPCursor& operator=(CodeBuffer::Offset newOffset) {
    cursor = newOffset;
    return *this;
  }
  PPCursor& operator=(const PPCursor& other) {
    cursor = other.cursor;
    return *this;
  }
  char currentChar();
  CodeBuffer::Offset offset();
  friend std::strong_ordering operator<=>(const PPCursor&, CodeBuffer::Offset);
  friend std::strong_ordering operator<=>(CodeBuffer::Offset, const PPCursor&);
  friend std::strong_ordering operator<=>(const PPCursor&, const PPCursor&);

  friend bool operator==(const PPCursor&, CodeBuffer::Offset);
  friend bool operator==(CodeBuffer::Offset, const PPCursor&);
  friend bool operator==(const PPCursor&, const PPCursor&);
};

// Only use it when scanning a preprocessing directive's content.
class DirectiveContentScanner : public IGetPeekOnlyScanner {
  Preprocessor* const pp;

 public:
  DirectiveContentScanner(Preprocessor* const _pp);
  int get();
  int peek();
  bool reachedEndOfInput();
};

class SectionContentScanner : public IGetPeekOnlyScanner {
  Preprocessor& pp;
  PPCursor& cursor;

 public:
  SectionContentScanner(Preprocessor& pp, PPCursor& cursor);
  int get() override;
  int peek() override;
  bool reachedEndOfInput() override;
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

  CodeBuffer& codeBuffer;
  IReportError& errOut;

  PPCursor cursor;
  std::optional<PPCursor> identCursor;
  DirectiveContentScanner dcs;

  // A cache for macro that has been expanded before.
  std::map<std::string, CodeBuffer::SectionID> codeCache;

  std::vector<CodeBuffer::SectionID> stackOfSectionID;
  std::vector<CodeBuffer::Offset> stackOfStoredOffsets;
  std::vector<MacroExpansionRecord> vectorOfMacroExpansion;
  std::set<MacroDefinition, CompareMacroDefinition> setOfMacroDefinitions;

  bool enabledProcessDirectives = true;

  friend class DirectiveContentScanner;
  friend class SectionContentScanner;
  friend class PPCursor;

 public:
  // Change IScanner to IPreprocessor
  Preprocessor(CodeBuffer& codeBuffer, IReportError& errOut)
      : codeBuffer(codeBuffer),
        errOut(errOut),
        cursor(*this, codeBuffer.section(0)),
        dcs(this) {
    fastForwardToFirstOutputCharacter(cursor);
  }

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
  void addPreprocessorDirective(PreprocessorDirective directive);
  CodeBuffer::SectionID currentSectionID();
  CodeBuffer::Offset currentSection();
  CodeBuffer::Offset currentSectionEnd();
  void enterSection(CodeBuffer::SectionID);
  void exitSection();
  void fastForwardToFirstOutputCharacter(PPCursor&);
  void parseDirective(PPCursor&);
  void skipNewline(PPCursor&);
  void skipSpaces(bool isInsideDirective);
  void skipDirective(PPCursor&);
  void skipWhitespacesAndComments(PPCursor& cursor, bool isInsideDirective);
  void skipComment(PPCursor&);
  std::string scanDirective(PPCursor&);
  bool isStartOfASpaceOrAComment(const PPCursor& cursor, bool isInsideDirective);
  bool isSpace(const PPCursor& cursor, bool isInsideDirective);
  bool isStartOfAComment(const PPCursor& cursor);
  bool reachedEndOfSection(const PPCursor&);
  bool reachedEndOfLine(const PPCursor&);
  bool reachedEndOfInput(const PPCursor&);
  bool lookaheadMatches(PPCursor cursor, const std::string& chars);
};

#endif  // !TPLCC_PREPROCESSOR_H
