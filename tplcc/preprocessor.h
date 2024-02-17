#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <compare>
#include <concepts>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
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

struct ErrorMessage {
  std::string errorMessage;
  std::string hint = "";

  ErrorMessage(std::string errorMessage)
      : errorMessage(std::move(errorMessage)){};

  ErrorMessage(std::string errorMessage, std::string hint)
      : errorMessage(std::move(errorMessage)), hint(std::move(hint)) {}
};

enum class MacroType { OBJECT_LIKE_MACRO, FUNCTION_LIKE_MACRO };

struct MacroDefinition {
  MacroType type;
  std::string name;
  std::vector<std::string> parameters;
  std::string body;

  MacroDefinition(std::string name, std::string body,
                  MacroType type = MacroType::OBJECT_LIKE_MACRO)
      : name(std::move(name)), body(std::move(body)), type(type){};

  MacroDefinition(std::string name, std::vector<std::string> parameters,
                  std::string body,
                  MacroType type = MacroType::FUNCTION_LIKE_MACRO)
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

std::string parseIdentifier(IBaseScanner& scanner);
bool isStartOfIdentifier(int ch);

struct ICopyable {
  virtual ICopyable* copy() const = 0;
};

template <std::derived_from<ICopyable> T>
std::unique_ptr<T> copyUnique(const T& origin) {
  T* copy = origin.copy();
  return std::unique_ptr<T>(copy);
}

struct ICopyableOffsetScanner : IBaseScanner, ICopyable {
  virtual void setOffset(CodeBuffer::Offset offset) = 0;
  virtual CodeBuffer::Offset offset() const = 0;
  virtual ICopyableOffsetScanner* copy() const = 0;
};

class PPBaseScanner : public ICopyableOffsetScanner {
 protected:
  CodeBuffer& _codeBuffer;
  CodeBuffer::Offset _offset;
  std::function<std::tuple<int, int>(const unsigned char*)> _readUTF32;

 public:
  PPBaseScanner(
      CodeBuffer& codeBuffer, CodeBuffer::Offset startOffset,
      std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : _codeBuffer(codeBuffer),
        _offset(startOffset),
        _readUTF32(std::move(readUTF32)) {}

  int get() override {
    if (reachedEndOfInput()) return EOF;
    const auto [codepoint, codelen] = _readUTF32(_codeBuffer.pos(_offset));
    _offset += codelen;
    skipBackslashReturn();
    return codepoint;
  }

  int peek() const override {
    // TODO collapses comments and spaces into one.
    if (reachedEndOfInput()) return EOF;
    const auto [codepoint, _] = _readUTF32(_codeBuffer.pos(_offset));
    return codepoint;
  }

  void setOffset(CodeBuffer::Offset offset) { _offset = offset; }
  CodeBuffer::Offset offset() const { return _offset; }

  CodeBuffer& codeBuffer() { return _codeBuffer; }
  const CodeBuffer& codeBuffer() const { return _codeBuffer; }

  std::function<std::tuple<int, int>(const unsigned char*)>& byteDecoder() {
    return _readUTF32;
  }

 protected:
  void skipBackslashReturn();
};

class PPScanner : public PPBaseScanner {
  PPImpl& pp;

 public:
  PPScanner(
      PPImpl& pp, CodeBuffer& codeBuffer,
      std::function<std::tuple<int, int>(const unsigned char*)> readUTF32);

  bool reachedEndOfInput() const override;
  PPImpl& ppImpl() { return pp; }
  const PPImpl& ppImpl() const { return pp; }

  PPScanner* copy() const { return new PPScanner(*this); }
};

class PPDirectiveScanner : public PPBaseScanner {
  CodeBuffer::SectionID sectionID;

 public:
  PPDirectiveScanner(const PPScanner& scanner);
  bool reachedEndOfInput() const override;
  PPDirectiveScanner* copy() const { return new PPDirectiveScanner(*this); }
};

class RawBufferScanner : public ICopyableOffsetScanner {
  const char* const _buffer;
  const std::size_t _bufferSize;
  std::function<std::tuple<int, int>(const unsigned char*)> _readUTF32;

  const char* _cursor;

 public:
  RawBufferScanner(
      const char* buffer, std::size_t bufferSize,
      std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : _buffer(buffer),
        _bufferSize(bufferSize),
        _readUTF32(readUTF32),
        _cursor(buffer){};

  int get() override {
    if (reachedEndOfInput()) return EOF;
    const auto [codepoint, charlen] =
        _readUTF32(reinterpret_cast<const unsigned char*>(_cursor));
    _cursor += charlen;
    return codepoint;
  }
  int peek() const override {
    if (reachedEndOfInput()) return EOF;
    const auto [codepoint, charlen] =
        _readUTF32(reinterpret_cast<const unsigned char*>(_cursor));
    return codepoint;
  }

  bool reachedEndOfInput() const override {
    return _cursor == _buffer + _bufferSize;
  }
  CodeBuffer::Offset offset() const { return _cursor - _buffer; }
  void setOffset(CodeBuffer::Offset offset) { _cursor = _buffer + offset; }

  RawBufferScanner* copy() const { return new RawBufferScanner(*this); }
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

  std::optional<CodeBuffer::Offset> identOffset;
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
  template <std::derived_from<IBaseScanner> V, typename T>
  bool isSpace(const V& scanner, T&& isSpaceFn) {
    return isSpaceFn(scanner.peek());
  }
  template <std::derived_from<IBaseScanner> V>
  bool isSpace(const V& scanner) {
    return isSpace(scanner, ::isSpace);
  }

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
  void skipSpaces(PPBaseScanner& scanner) {
    return skipSpaces(scanner, ::isSpace);
  }
  void enterSection(CodeBuffer::SectionID id);
  void exitSection();
  CodeBuffer::SectionID currentSectionID() const;
  CodeBuffer::Offset currentSection() const;
  CodeBuffer::Offset currentSectionEnd() const;
  bool lookaheadMatches(const PPBaseScanner& scanner, const std::string& s);
  void exitFullyScannedSections();
  std::variant<std::vector<std::string>, ErrorMessage>
  parseFunctionLikeMacroParameters(PPBaseScanner& scanner);
  std::tuple<std::optional<CodeBuffer::SectionID>, CodeBuffer::Offset>
  tryExpandingMacro(const ICopyableOffsetScanner& scanner,
                    const MacroDefinition* const macroDefContext,
                    const std::vector<std::string>* const argContext);
  std::vector<std::string> parseFunctionLikeMacroArgumentList(
      ICopyableOffsetScanner& scanner,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);
  std::string parseFunctionLikeMacroArgument(
      ICopyableOffsetScanner& scanner,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  std::string expandFunctionLikeMacro(
      const MacroDefinition& macroDef,
      const std::vector<std::string>& arguments);
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
  Preprocessor(
      CodeBuffer& codeBuffer, IReportError& errOut,
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
