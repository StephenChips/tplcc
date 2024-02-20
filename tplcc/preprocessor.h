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

// A wrapper that store a character and all information about it.
class PPCharacter {
  friend class PPImpl;
  int _codepoint;
  CodeBuffer::Offset _offset;

 private:
  PPCharacter(int codepoint, CodeBuffer::Offset offset)
      : _codepoint(codepoint), _offset(offset) {}

 public:
  operator int() const { return _codepoint; }
  int offset() const { return _offset; }

  static PPCharacter eof() { return PPCharacter(EOF, 0); }
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

namespace MacroExpansionResult {
struct Ok {
  CodeBuffer::SectionID sectionID;
  CodeBuffer::Offset endOffset;
};
struct Fail {};

using Type =
    std::variant<MacroExpansionResult::Ok, MacroExpansionResult::Fail, Error>;
};  // namespace MacroExpansionResult

class PPImpl {
  CodeBuffer& codeBuffer;
  IReportError& errOut;

  // A cache for macro that has been expanded before.
  std::map<std::string, CodeBuffer::SectionID> codeCache;
  std::set<MacroDefinition, CompareMacroDefinition> setOfMacroDefinitions;

  std::vector<CodeBuffer::SectionID> stackOfSectionID;
  std::vector<CodeBuffer::Offset> stackOfStoredOffsets;

  std::optional<CodeBuffer::Offset> identEndOffset;
  PPScanner scanner;

  std::optional<PPCharacter> lookaheadBuffer;

  bool canParseDirectives;

  friend class PPDirectiveScanner;
  friend class PPScanner;

 public:
  PPImpl(CodeBuffer& codeBuffer, IReportError& errOut,
         std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : codeBuffer(codeBuffer),
        errOut(errOut),
        setOfMacroDefinitions(setOfMacroDefinitions),
        codeCache(codeCache),
        scanner(*this, codeBuffer, readUTF32) {
    fastForwardToFirstOutputCharacter();
  }

  PPCharacter get();
  PPCharacter peek() const;
  bool reachedEndOfInput() const;

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
  std::optional<Error> skipSpacesAndComments(PPBaseScanner& scanner,
                                             T&& isSpaceFn);
  std::optional<Error> skipSpacesAndComments(PPBaseScanner& scanner) {
    return skipSpacesAndComments(scanner, ::isSpace);
  }
  void fastForwardToFirstOutputCharacter();
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
  std::variant<std::vector<std::string>, Error>
  parseFunctionLikeMacroParameters(const std::string& macroName,
                                   PPBaseScanner& scanner);
  std::tuple<MacroExpansionResult::Type, CodeBuffer::Offset> tryExpandingMacro(
      const ICopyableOffsetScanner& scanner,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  std::variant<std::vector<std::string>, Error>
  parseFunctionLikeMacroArgumentList(
      ICopyableOffsetScanner& scanner, const MacroDefinition& macroDef,
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
std::optional<Error> PPImpl::skipSpacesAndComments(PPBaseScanner& scanner,
                                                   T&& isSpaceFn) {
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

    if (lookaheadMatches(scanner, "//")) {
      scanner.get();
      scanner.get();

      while (!scanner.reachedEndOfInput() &&
             !isNewlineCharacter(scanner.peek())) {
        scanner.get();
      }

      skipNewline(scanner);
      continue;
    }

    if (lookaheadMatches(scanner, "/*")) {
      const auto startOffset = scanner.offset();
      scanner.get();
      scanner.get();

      while (!scanner.reachedEndOfInput() && !lookaheadMatches(scanner, "*/")) {
        scanner.get();
      }

      if (scanner.reachedEndOfInput()) {
        return Error{
            {startOffset, startOffset + 2}, "Unterminated comment.", ""};
      }

      scanner.get();
      scanner.get();
      continue;
    }

    break;
  }

  return std::nullopt;
}

template <typename T>
void PPImpl::skipSpaces(PPBaseScanner& scanner, T&& isSpaceFn) {
  while (!scanner.reachedEndOfInput() && isSpaceFn(scanner.peek())) {
    scanner.get();
  }
}

class Preprocessor {
  mutable PPImpl ppImpl;
  mutable std::optional<PPCharacter> lookaheadBuffer;

 public:
  Preprocessor(
      CodeBuffer& codeBuffer, IReportError& errOut,
      std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
      : ppImpl(codeBuffer, errOut, std::move(readUTF32)) {}

  Preprocessor(CodeBuffer& codeBuffer, IReportError& errOut)
      : Preprocessor(codeBuffer, errOut, utf8) {}

  PPCharacter get() {
    if (lookaheadBuffer) {
      const auto copy = *lookaheadBuffer;
      lookaheadBuffer = std::nullopt;
      return copy;
    } else {
      return ppImpl.get();
    }
  }

  PPCharacter peek() const {
    if (lookaheadBuffer) return *lookaheadBuffer;
    auto ppCh = ppImpl.get();
    if (ppCh != EOF) {
      lookaheadBuffer = ppCh;
    }
    return ppCh;
  }

  bool reachedEndOfInput() const { return peek() == EOF; }
};

void skipAll(PPBaseScanner& scanner);
std::string readAll(PPBaseScanner& scanner);

#endif  // !TPLCC_PREPROCESSOR_H
