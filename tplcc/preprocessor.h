#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <compare>
#include <concepts>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "code-buffer.h"
#include "encoding.h"
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

bool isSpace(int ch);
bool isDirectiveSpace(int ch);
bool isNewlineCharacter(int ch);

class PPImpl;

template <typename T>
concept CreatescannerFunc =
    requires(T func, const CodeBuffer& buffer, CodeBuffer::Offset offset) {
      { func(buffer, offset) };
    };

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

class IdentStrLexer {
  IBaseScanner& scanner;

 public:
  IdentStrLexer(IBaseScanner& scanner) : scanner(scanner){};
  std::string scan();

  static bool isStartOfAIdentifier(const char ch);
};

class PPBaseScanner : public IBaseScanner {
 protected:
  CodeBuffer& _codeBuffer;
  CodeBuffer::Offset _offset;
  std::function<std::tuple<int, int>(const unsigned char*)> readUTF32;

 public:
  PPBaseScanner(CodeBuffer& codeBuffer, CodeBuffer::Offset startOffset,
            std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : _codeBuffer(codeBuffer),
        _offset(startOffset),
        readUTF32(std::move(readUTF32)) {}

  int get() override {
    if (reachedEndOfInput()) return EOF;
    const auto [codepoint, codelen] = readUTF32(_codeBuffer.pos(_offset));
    _offset += codelen;
    skipBackslashReturn();
    return codepoint;
  }

  int peek() const override {
    // TODO collapses comments and spaces into one.
    if (reachedEndOfInput()) return EOF;
    const auto [codepoint, _] = readUTF32(_codeBuffer.pos(_offset));
    return codepoint;
  }

  void setOffset(CodeBuffer::Offset offset) { _offset = offset; }
  CodeBuffer::Offset offset() const { return _offset; }

  CodeBuffer& codeBuffer() { return _codeBuffer; }
  const CodeBuffer& codeBuffer() const { return _codeBuffer; }

  virtual std::unique_ptr<PPBaseScanner> copy() const = 0;

 protected:
  void skipBackslashReturn();
};

class PPScanner : public PPBaseScanner {
  PPImpl& pp;

 public:
  PPScanner(PPImpl& pp, CodeBuffer& codeBuffer,
            std::function<std::tuple<int, int>(const unsigned char*)> readUTF32);

  bool reachedEndOfInput() const override;
  PPImpl& ppImpl() { return pp; }
  const PPImpl& ppImpl() const { return pp; }
  std::unique_ptr<PPBaseScanner> copy() const {
    return std::make_unique<PPScanner>(*this);
  }
};

class PPDirectiveScanner : public PPBaseScanner {
  CodeBuffer::SectionID sectionID;

 public:
  PPDirectiveScanner(const PPScanner& scanner);
  bool reachedEndOfInput() const override;

  std::unique_ptr<PPBaseScanner> copy() const {
    return std::make_unique<PPDirectiveScanner>(*this);
  }
};

class PPImpl : public IBaseScanner {
  CodeBuffer& codeBuffer;
  IReportError& errOut;
  std::set<MacroDefinition, CompareMacroDefinition>& setOfMacroDefinitions;
  const std::vector<std::unique_ptr<std::string>>* macroArguments;
  std::map<std::string, CodeBuffer::SectionID>& codeCache;

  std::function<bool(CodeBuffer::Offset)> ppReachedEnd;
  std::function<bool(CodeBuffer::Offset)> doesScannerReachedEnd;

  std::vector<CodeBuffer::SectionID> stackOfSectionID;
  std::vector<CodeBuffer::Offset> stackOfStoredOffsets;

  std::unique_ptr<PPScanner> identScanner;
  PPScanner scanner;
  
  bool canParseDirectives;

  friend class PPDirectiveScanner;
  friend class PPScanner;

 public:
  PPImpl(
      CodeBuffer& codeBuffer, IReportError& errOut,
      std::set<MacroDefinition, CompareMacroDefinition>& setOfMacroDefinitions,
      const std::vector<std::unique_ptr<std::string>>* macroArguments,
      std::map<std::string, CodeBuffer::SectionID>& codeCache,
      CodeBuffer::Offset startOffset,
      std::function<bool(CodeBuffer::Offset)>& ppReachedEnd,
      std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : codeBuffer(codeBuffer),
        errOut(errOut),
        setOfMacroDefinitions(setOfMacroDefinitions),
        macroArguments(macroArguments),
        codeCache(codeCache),
        ppReachedEnd(ppReachedEnd),
        doesScannerReachedEnd([this](CodeBuffer::Offset offset) {
          return this->ppReachedEnd(offset) || offset == currentSectionEnd();
        }),
        scanner(*this, codeBuffer, readUTF32) {
    fastForwardToFirstOutputCharacter(scanner);
  }

  int get() override;
  int peek() const override;
  bool reachedEndOfInput() const override;

 private:
  bool reachedEndOfCurrentSection() const;
  template <typename T>
  bool isSpace(const PPBaseScanner& scanner, T&& isSpaceFn) {
    return isSpaceFn(scanner.peek());
  }
  bool isSpace(const PPBaseScanner& scanner) { return isSpace(scanner, ::isSpace); }

  template <typename T>
  void skipSpacesAndComments(PPBaseScanner& scanner, T&& isSpaceFn);
  void skipSpacesAndComments(PPBaseScanner& scanner) {
    return skipSpacesAndComments(scanner, ::isSpace);
  }
  void fastForwardToFirstOutputCharacter(PPBaseScanner& scanner);
  void parseDirective();
  void skipNewline(PPBaseScanner& scanner);
  template <typename T>
  void skipSpaces(PPBaseScanner& scanner, T&& isSpace);
  void skipSpaces(PPBaseScanner& scanner) { return skipSpaces(scanner, ::isSpace); }
  void enterSection(CodeBuffer::SectionID id);
  void exitSection();
  CodeBuffer::SectionID currentSectionID() const;
  CodeBuffer::Offset currentSection() const;
  CodeBuffer::Offset currentSectionEnd() const;
  bool lookaheadMatches(const PPBaseScanner& scanner, const std::string& s);
  void exitFullyScannedSections();
};

template <typename T>
void PPImpl::skipSpacesAndComments(PPBaseScanner& scanner, T&& isSpaceFn) {
  while (!scanner.reachedEndOfInput()) {
    if (isNewlineCharacter(scanner.peek())) {
      canParseDirectives = true;
      skipNewline(scanner);
      continue;
    }

    if (isSpaceFn(scanner.peek())) {
      scanner.get();
      continue;
    }

    if (lookaheadMatches(scanner, "/*")) {
      scanner.get();
      scanner.get();

      while (!scanner.reachedEndOfInput() && !lookaheadMatches(scanner, "*/")) {
        scanner.get();
      }

      if (scanner.reachedEndOfInput()) {
        // TODO: THROW ERROR HERE
        return;
      }

      scanner.get();
      scanner.get();
      continue;
    }

    break;
  }
}

template <typename T>
void PPImpl::skipSpaces(PPBaseScanner& scanner, T&& isSpaceFn) {
  while (!scanner.reachedEndOfInput() && isSpaceFn(scanner.peek())) {
    scanner.get();
  }
}

class Preprocessor : IBaseScanner {
  CodeBuffer& codeBuffer;
  IReportError& errOut;
  std::function<std::tuple<int, int>(const unsigned char*)> readUTF32;
  std::function<bool(CodeBuffer::Offset)> toEndOfFirstSection;

  // A cache for macro that has been expanded before.
  std::map<std::string, CodeBuffer::SectionID> codeCache;
  std::set<MacroDefinition, CompareMacroDefinition> setOfMacroDefinitions;

  PPImpl ppImpl;

 public:
  Preprocessor(CodeBuffer& codeBuffer, IReportError& errOut,
               std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : codeBuffer(codeBuffer),
        errOut(errOut),
        readUTF32(std::move(readUTF32)),
        toEndOfFirstSection([this](CodeBuffer::Offset offset) {
          return offset == this->codeBuffer.sectionEnd(0);
        }),
        ppImpl(codeBuffer, errOut, setOfMacroDefinitions, nullptr, codeCache,
               codeBuffer.section(0), toEndOfFirstSection, this->readUTF32) {}

  Preprocessor(CodeBuffer& codeBuffer, IReportError& errOut)
      : Preprocessor(codeBuffer, errOut, utf8) {}

  // Inherited via IScanner
  int get() override { return ppImpl.get(); }
  int peek() const override { return ppImpl.peek(); }
  bool reachedEndOfInput() const override { return ppImpl.reachedEndOfInput(); }
};

void skipAll(PPBaseScanner& scanner);
std::string readAll(PPBaseScanner& scanner);

#endif  // !TPLCC_PREPROCESSOR_H
