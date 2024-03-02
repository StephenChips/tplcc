#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <algorithm>
#include <cassert>
#include <cctype>
#include <compare>
#include <concepts>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "code-buffer.h"
#include "encoding.h"
#include "error.h"
#include "helper.h"

template <typename F>
concept ByteDecoderConcept = requires(F func, const unsigned char* addr) {
  { func(addr) } -> std::same_as<std::tuple<int, int>>;
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

template <ByteDecoderConcept F>
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

// A wrapper that store a character and all information about it.
class PPCharacter {
  int _codepoint;
  CodeBuffer::Offset _offset;

 public:
  PPCharacter(int codepoint, CodeBuffer::Offset offset)
      : _codepoint(codepoint), _offset(offset) {}
  operator int() const { return _codepoint; }
  int offset() const { return _offset; }

  static PPCharacter eof() { return PPCharacter(EOF, 0); }
};

struct IOffsetScanner : IBaseScanner {
  virtual CodeBuffer::Offset offset() const = 0;
  virtual ~IOffsetScanner() {}
};

template <typename T>
struct ILookaheadScanner : IOffsetScanner {
  virtual T lookaheadScanner() const = 0;
  virtual ~ILookaheadScanner() {}
};

template <std::derived_from<IBaseScanner> T>
bool lookaheadMatches(ILookaheadScanner<T>& scanner, const std::string& str) {
  auto lookaheadScanner = scanner.lookaheadScanner();
  for (const auto& ch : str) {
    if (lookaheadScanner.get() != ch) return false;
  }
  return true;
}

class CharOffsetRecorder : public IBaseScanner {
  IOffsetScanner& _scanner;
  std::vector<CodeBuffer::Offset> _offsets;

 public:
  CharOffsetRecorder(IOffsetScanner& scanner) : _scanner(scanner) {}
  int get() {
    _offsets.push_back(_scanner.offset());
    const auto ch = _scanner.get();
    return ch;
  }
  int peek() const override { return _scanner.peek(); }
  bool reachedEndOfInput() const override {
    return _scanner.reachedEndOfInput();
  }

  std::vector<CodeBuffer::Offset> offsets() { return _offsets; }
  std::vector<CodeBuffer::Offset> moveOffsets() { return std::move(_offsets); }
};

template <ByteDecoderConcept F>
class OffsetCharScanner : public IOffsetScanner {
  CodeBuffer& _codeBuffer;
  std::vector<CodeBuffer::Offset> _offsets;
  F& _decodeByte;
  std::size_t _count = 0;

 public:
  OffsetCharScanner(CodeBuffer& codeBuffer,
                    std::vector<CodeBuffer::Offset> offsets, F& decodeByte)
      : _codeBuffer(codeBuffer),
        _offsets(std::move(offsets)),
        _decodeByte(decodeByte) {}

  int get() override {
    if (_count == _offsets.size()) return EOF;
    const auto [codepoint, charlen] =
        _decodeByte(_codeBuffer.pos(_offsets[_count]));
    _count++;
    return codepoint;
  }

  int peek() const override {
    if (_count == _offsets.size()) return EOF;
    const auto [codepoint, charlen] =
        _decodeByte(_codeBuffer.pos(_offsets[_count]));
    return codepoint;
  }

  bool reachedEndOfInput() const override { return _count == _offsets.size(); }

  CodeBuffer::Offset offset() const override { return _offsets[_count]; }
};

template <ByteDecoderConcept F>
class PPScanner;

template <ByteDecoderConcept F>
class PPLookaheadScanner : public IBaseScanner {
  const PPScanner<F>& _pps;

  int _indexOfStackItem;
  CodeBuffer::Offset _offset;

 public:
  PPLookaheadScanner(const PPScanner<F>& pps)
      : _pps(pps),
        _indexOfStackItem(pps.stackOfSectionID.size() - 1),
        _offset(pps._offset) {
    skipBackslashReturn();
  }

  int get();
  int peek() const override;
  bool reachedEndOfInput() const override;
  void refresh();

 private:
  void skipBackslashReturn();
  CodeBuffer::SectionID currentSectionID();
  CodeBuffer::Offset currentSectionEnd();
};

template <ByteDecoderConcept F>
class PPScanner : public ILookaheadScanner<PPLookaheadScanner<F>> {
  friend class PPLookaheadScanner<F>;

  CodeBuffer& _codeBuffer;
  CodeBuffer::Offset _offset = 0;
  F& _decodeChar;

  std::vector<CodeBuffer::SectionID> stackOfSectionID;
  std::vector<CodeBuffer::Offset> stackOfStoredOffsets;

 public:
  PPScanner(CodeBuffer& codeBuffer, F& readUTF32)
      : _codeBuffer(codeBuffer), _decodeChar(readUTF32) {}

  int get() {
    if (reachedEndOfInput()) return EOF;
    const auto charOffset = _offset;
    const auto [codepoint, codelen] = _decodeChar(_codeBuffer.pos(_offset));
    _offset += codelen;
    if (currentSectionID() == 0) skipBackslashReturn();
    while (!stackOfSectionID.empty() && _offset == currentSectionEnd()) {
      exitSection();
    }
    return codepoint;
  }

  int peek() const { return PPLookaheadScanner(*this).peek(); }

  bool reachedEndOfInput() const {
    return stackOfSectionID.empty() && _offset == _codeBuffer.sectionEnd(0);
  }

  PPLookaheadScanner<F> lookaheadScanner() const {
    return PPLookaheadScanner<F>(*this);
  }

  CodeBuffer::Offset offset() const { return _offset; }

  F& byteDecoder() { return _decodeChar; }

  void enterSection(CodeBuffer::SectionID id) {
    if (_codeBuffer.sectionSize(id) == 0) return;
    stackOfSectionID.push_back(id);
    stackOfStoredOffsets.push_back(_offset);
    _offset = _codeBuffer.section(id);
  }

 private:
  void exitSection() {
    stackOfSectionID.pop_back();
    _offset = stackOfStoredOffsets.back();
    stackOfStoredOffsets.pop_back();
  }

  void skipBackslashReturn() {
    while (_offset != currentSectionEnd()) {
      const auto [ch1, l1] = _decodeChar(_codeBuffer.pos(_offset));
      if (ch1 != '\\' || _offset + l1 == currentSectionEnd()) return;
      const auto [ch2, l2] = _decodeChar(_codeBuffer.pos(_offset + l1));
      if (ch2 != '\n') return;
      _offset += l1 + l2;
    }
  }

  CodeBuffer::SectionID currentSectionID() {
    return stackOfSectionID.empty() ? 0 : stackOfSectionID.back();
  }

  CodeBuffer::Offset currentSectionEnd() {
    return _codeBuffer.sectionEnd(currentSectionID());
  }
};

template <ByteDecoderConcept F>
int PPLookaheadScanner<F>::get() {
  if (reachedEndOfInput()) return EOF;
  const auto [codepoint, codelen] =
      _pps._decodeChar(_pps._codeBuffer.pos(_offset));
  _offset += codelen;
  if (currentSectionID() == 0) skipBackslashReturn();
  while (_indexOfStackItem >= 0 && _offset == currentSectionEnd()) {
    _offset = _pps.stackOfStoredOffsets[_indexOfStackItem];
    _indexOfStackItem--;
  }
  return codepoint;
}

template <ByteDecoderConcept F>
int PPLookaheadScanner<F>::peek() const {
  PPLookaheadScanner copy(*this);
  return copy.get();
}

template <ByteDecoderConcept F>
bool PPLookaheadScanner<F>::reachedEndOfInput() const {
  return _indexOfStackItem == 0 && _offset == _pps._codeBuffer.sectionEnd(0);
}

template <ByteDecoderConcept F>
void PPLookaheadScanner<F>::refresh() {
  _indexOfStackItem = _pps.stackOfSectionID.size() - 1;
  _offset = _pps._offset;
}

template <ByteDecoderConcept F>
CodeBuffer::SectionID PPLookaheadScanner<F>::currentSectionID() {
  return _indexOfStackItem == -1 ? 0 : _pps.stackOfSectionID[_indexOfStackItem];
}

template <ByteDecoderConcept F>
CodeBuffer::Offset PPLookaheadScanner<F>::currentSectionEnd() {
  return _pps._codeBuffer.sectionEnd(currentSectionID());
}

template <ByteDecoderConcept F>
void PPLookaheadScanner<F>::skipBackslashReturn() {
  while (_offset != currentSectionEnd()) {
    const auto [ch1, l1] = _pps._decodeChar(_pps._codeBuffer.pos(_offset));
    if (ch1 != '\\' || _offset + l1 == currentSectionEnd()) return;
    const auto [ch2, l2] = _pps._decodeChar(_pps._codeBuffer.pos(_offset + l1));
    if (ch2 != '\n') return;
    _offset += l1 + l2;
  }
}

template <ByteDecoderConcept F>
class PPDirectiveScanner;

template <ByteDecoderConcept F>
class PPDirectiveLookaheadScanner : public IBaseScanner {
  PPLookaheadScanner<F> _ppls;

 public:
  PPDirectiveLookaheadScanner(PPLookaheadScanner<F> ppls) : _ppls(ppls){};

  int get() override;
  int peek() const override;
  bool reachedEndOfInput() const override;
  void refresh();
};

template <ByteDecoderConcept F>
class PPDirectiveScanner
    : public ILookaheadScanner<PPDirectiveLookaheadScanner<F>> {
  friend class PPDirectiveLookaheadScanner<F>;
  PPScanner<F>& _scanner;

 public:
  PPDirectiveScanner(PPScanner<F>& scanner) : _scanner(scanner) {}

  int get() override {
    if (reachedEndOfInput()) return EOF;
    return _scanner.get();
  }

  int peek() const override {
    if (reachedEndOfInput()) return EOF;
    return _scanner.peek();
  }

  bool reachedEndOfInput() const override {
    return _scanner.reachedEndOfInput() || isNewlineCharacter(_scanner.peek());
  }

  CodeBuffer::Offset offset() const { return _scanner.offset(); }

  PPDirectiveLookaheadScanner<F> lookaheadScanner() const {
    return PPDirectiveLookaheadScanner<F>(_scanner.lookaheadScanner());
  }
};

template <ByteDecoderConcept F>
int PPDirectiveLookaheadScanner<F>::get() {
  if (reachedEndOfInput()) return EOF;
  return _ppls.get();
}

template <ByteDecoderConcept F>
int PPDirectiveLookaheadScanner<F>::peek() const {
  if (reachedEndOfInput()) return EOF;
  return _ppls.peek();
}

template <ByteDecoderConcept F>
bool PPDirectiveLookaheadScanner<F>::reachedEndOfInput() const {
  return _ppls.reachedEndOfInput() || isNewlineCharacter(_ppls.peek());
}

template <ByteDecoderConcept F>
void PPDirectiveLookaheadScanner<F>::refresh() {
  _ppls.refresh();
}

class RawBufferLookaheadScanner : public IBaseScanner {};

template <ByteDecoderConcept F>
class RawBufferScanner : public ILookaheadScanner<RawBufferScanner<F>> {
  const char* const _buffer;
  const std::size_t _bufferSize;
  F& _decodeChar;

  const char* _cursor;

 public:
  RawBufferScanner(
      const char* buffer, std::size_t bufferSize,
      F& readUTF32)
      : _buffer(buffer),
        _bufferSize(bufferSize),
        _decodeChar(readUTF32),
        _cursor(buffer){};

  int get() override {
    if (_cursor == _buffer + _bufferSize) return EOF;
    const auto [codepoint, charlen] =
        _decodeChar(reinterpret_cast<const unsigned char*>(_cursor));
    _cursor += charlen;
    return codepoint;
  }
  int peek() const override {
    if (_cursor == _buffer + _bufferSize) return EOF;
    const auto [codepoint, charlen] =
        _decodeChar(reinterpret_cast<const unsigned char*>(_cursor));
    return codepoint;
  }

  bool reachedEndOfInput() const override {
    return _cursor == _buffer + _bufferSize;
  }

  RawBufferScanner<F> lookaheadScanner() const { return *this; }

  CodeBuffer::Offset offset() const { return _cursor - _buffer; }
};

namespace MacroExpansionResult {
struct Ok {
  CodeBuffer::SectionID sectionID;
};
struct Fail {};

using Type =
    std::variant<MacroExpansionResult::Ok, MacroExpansionResult::Fail, Error>;
};  // namespace MacroExpansionResult

template <ByteDecoderConcept F>
class PPImpl {
  CodeBuffer& codeBuffer;
  IReportError& errOut;

  // A cache for macro that has been expanded before.
  std::map<std::string, CodeBuffer::SectionID> codeCache;
  std::set<MacroDefinition, CompareMacroDefinition> setOfMacroDefinitions;

  std::unique_ptr<OffsetCharScanner<F>> identScanner;
  PPScanner<F> scanner;

  std::optional<PPCharacter> lookaheadBuffer;

  bool canParseDirectives;

 public:
  PPImpl(CodeBuffer& codeBuffer, IReportError& errOut, F&& readUTF32)
      : codeBuffer(codeBuffer),
        errOut(errOut),
        setOfMacroDefinitions(setOfMacroDefinitions),
        codeCache(codeCache),
        scanner(codeBuffer, std::forward<F>(readUTF32)) {
    fastForwardToFirstOutputCharacter();
  }

  PPCharacter get();
  PPCharacter peek() const;
  bool reachedEndOfInput() const;

 private:
  bool reachedEndOfCurrentSection() const;

  template <std::derived_from<IBaseScanner> T, typename U>
  std::optional<Error> skipSpacesAndComments(ILookaheadScanner<T>& scanner,
                                             U&& isSpaceFn);
  template <std::derived_from<IBaseScanner> T>
  std::optional<Error> skipSpacesAndComments(ILookaheadScanner<T>& scanner) {
    return skipSpacesAndComments(scanner, ::isSpace);
  }
  void fastForwardToFirstOutputCharacter();
  void parseDirective();
  void skipNewline(IBaseScanner& scanner) {
    if (scanner.peek() == '\r') scanner.get();
    if (scanner.peek() == '\n') scanner.get();
  }
  template <typename T>
  void skipSpaces(IOffsetScanner& scanner, T&& isSpace);
  void skipSpaces(IOffsetScanner& scanner) {
    return skipSpaces(scanner, ::isSpace);
  }

  template <std::derived_from<IBaseScanner> T>
  std::variant<std::vector<std::string>, Error>
  parseFunctionLikeMacroParameters(const std::string& macroName,
                                   ILookaheadScanner<T>& scanner);

  template <std::derived_from<IBaseScanner> T>
  MacroExpansionResult::Type tryExpandingMacro(
      const std::string& macroName, ILookaheadScanner<T>& scanner,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  template <std::derived_from<IBaseScanner> T>
  std::variant<std::vector<std::string>, Error>
  parseFunctionLikeMacroArgumentList(
      ILookaheadScanner<T>& scanner, const MacroDefinition& macroDef,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  template <std::derived_from<IBaseScanner> T>
  std::string parseFunctionLikeMacroArgument(
      ILookaheadScanner<T>& scanner,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  std::string expandFunctionLikeMacro(
      const MacroDefinition& macroDef,
      const std::vector<std::string>& arguments);
};

template <ByteDecoderConcept F>
template <std::derived_from<IBaseScanner> T, typename U>
std::optional<Error> PPImpl<F>::skipSpacesAndComments(
    ILookaheadScanner<T>& scanner, U&& isSpaceFn) {
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

template <ByteDecoderConcept F>
template <typename T>
void PPImpl<F>::skipSpaces(IOffsetScanner& scanner, T&& isSpaceFn) {
  while (!scanner.reachedEndOfInput() && isSpaceFn(scanner.peek())) {
    scanner.get();
  }
}

template <ByteDecoderConcept F = decltype(utf8)>
class Preprocessor {
  mutable PPImpl<F> ppImpl;
  mutable std::optional<PPCharacter> lookaheadBuffer;

 public:
  Preprocessor(CodeBuffer& codeBuffer, IReportError& errOut,
               F&& readUTF32 = utf8)
      : ppImpl(codeBuffer, errOut, std::move(readUTF32)) {}

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

std::optional<std::size_t> findIndexOfParameter(
    const MacroDefinition& macroDef, const std::string& parameterName) {
  const auto& parameters = macroDef.parameters;
  const auto it =
      std::find(parameters.begin(), parameters.end(), parameterName);
  if (it == parameters.end()) {
    return std::nullopt;
  } else {
    return it - parameters.begin();
  }
}

std::string createFunctionLikeMacroCacheKey(
    const std::string& macroName, const std::vector<std::string>& arguments) {
  if (arguments.empty()) return macroName + "()";

  std::string output = macroName + "(" + arguments[0];

  for (std::size_t i = 1; i < arguments.size(); i++) {
    output += ',';
    output += arguments[i];
  }

  return output + ")";
}

void skipAll(IBaseScanner& scanner) {
  while (scanner.get() != EOF)
    ;
}

std::string readAll(IBaseScanner& scanner) {
  std::string content;
  for (auto ch = scanner.get(); ch != EOF; ch = scanner.get()) {
    content.push_back(ch);
  }
  return content;
}

// The MSVC's std::isspace will throw a runtime error when we pass a codepoint
// that is larger than 255. We have to write our own version of isspace here to
// avoid this error.
bool isSpace(int ch) {
  return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
         ch == '\v';
}
bool isDirectiveSpace(int ch) { return ch == ' ' || ch == '\t'; }
bool isNewlineCharacter(int ch) { return ch == '\r' || ch == '\n'; }

bool isStartOfIdentifier(int ch) {
  return ch == '_' || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z';
}

std::string parseIdentifier(IBaseScanner& scanner) {
  std::string result;

  result.push_back(scanner.get());

  while (!scanner.reachedEndOfInput() && std::isalnum(scanner.peek())) {
    result.push_back(scanner.get());
  }

  return result;
}

bool isFirstCharOfIdentifier(const char ch) {
  return std::isalpha(ch) || ch == '_';
}

/* PPImpl */

template <ByteDecoderConcept F>
PPCharacter PPImpl<F>::get() {
  if (identScanner) {
    const auto offset = identScanner->offset();
    const auto ch = identScanner->get();
    if (identScanner->reachedEndOfInput()) {
      identScanner = nullptr;
    }
    return PPCharacter(ch, offset);
  }

  if (scanner.reachedEndOfInput()) {
    return PPCharacter::eof();
  }

  // A row of spaces and comments will be merge into one space. That means
  // whenever we read a space or a comment, we will skip as far as possible then
  // return a space to the caller.
  if (isSpace(scanner.peek()) || lookaheadMatches(scanner, "/*") ||
      lookaheadMatches(scanner, "//")) {
    // Actually an error range will not start or end with a space or a comment,
    // so it doesn't matter what offset we return to the caller.

    const auto offset = scanner.offset();
    skipSpacesAndComments(scanner);

    if (scanner.peek() == '#' && canParseDirectives) {
      parseDirective();
      return get();
    } else {
      return PPCharacter(' ', offset);
    }
  }
  if (scanner.peek() == '#' && canParseDirectives) {
    parseDirective();
    return get();
  }

  // A directive can be only written at the starting of a line, optionally
  // preceeded by spaces and comments, so if we find a character that is not
  // space nor a start of a comment, we cannot parse directives any longer
  // until reaching the next line.
  canParseDirectives = false;

  if (isStartOfIdentifier(scanner.peek())) {
    CharOffsetRecorder recorder(scanner);
    const auto identifier = parseIdentifier(recorder);
    auto res = tryExpandingMacro(identifier, scanner, nullptr, nullptr);

    if (const auto ptr = std::get_if<MacroExpansionResult::Ok>(&res)) {
      scanner.enterSection(ptr->sectionID);
    } else if (const auto ptr = std::get_if<MacroExpansionResult::Fail>(&res)) {
      identScanner = std::make_unique<OffsetCharScanner<F>>(
          codeBuffer, recorder.offsets(), scanner.byteDecoder());
    } else {
      errOut.reportsError(std::get<Error>(std::move(res)));
    }

    return get();
  }

  const auto ch = scanner.get();
  const auto offset = scanner.offset();
  return PPCharacter(ch, offset);
}

template <ByteDecoderConcept F>
template <std::derived_from<IBaseScanner> T>
MacroExpansionResult::Type PPImpl<F>::tryExpandingMacro(
    const std::string& macroName, ILookaheadScanner<T>& scanner,
    const MacroDefinition* const macroDefContext,
    const std::vector<std::string>* const argContext) {
  assert(macroDefContext == nullptr && argContext == nullptr ||
         macroDefContext != nullptr && argContext != nullptr &&
             macroDefContext->parameters.size() == argContext->size());

  const auto startOffset = scanner.offset();

  const auto macroDef = setOfMacroDefinitions.find(macroName);
  if (macroDef == setOfMacroDefinitions.end() ||
      macroDef->type == MacroType::FUNCTION_LIKE_MACRO &&
          scanner.peek() != '(') {
    return MacroExpansionResult::Fail();
  }

  std::string key;
  std::vector<std::string> arguments;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    auto result = parseFunctionLikeMacroArgumentList(
        scanner, *macroDef, macroDefContext, argContext);

    if (const auto error = std::get_if<Error>(&result)) {
      return std::move(*error);
    }

    arguments = std::get<std::vector<std::string>>(std::move(result));
    // If there is nothing inside an argument list, e.g. ID(), the
    // macro will still have a empty string as its only argument.
    if (macroDef->parameters.size() == 1 && arguments.empty()) {
      arguments.push_back("");
    }
    if (macroDef->parameters.size() != arguments.size()) {
      return Error{
          {startOffset, scanner.offset()},
          std::format("The macro \"{}\" requires {} argument(s), but got {}.",
                      macroDef->name, macroDef->parameters.size(),
                      arguments.size()),
          ""};
    }
    key = createFunctionLikeMacroCacheKey(macroDef->name, arguments);
  } else {
    key = macroDef->name;
  }

  if (const auto iter = codeCache.find(key); iter != codeCache.end()) {
    return MacroExpansionResult::Ok{iter->second};
  }

  std::string expandedText;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    expandedText = expandFunctionLikeMacro(*macroDef, arguments);
  } else {
    expandedText = macroDef->body.empty() ? " " : macroDef->body;
  }

  CodeBuffer::SectionID sectionID = codeBuffer.addSection(expandedText);
  codeCache.insert({macroDef->name, sectionID});

  return MacroExpansionResult::Ok{sectionID};
}

template <ByteDecoderConcept F>
template <std::derived_from<IBaseScanner> T>
std::variant<std::vector<std::string>, Error>
PPImpl<F>::parseFunctionLikeMacroArgumentList(
    ILookaheadScanner<T>& scanner, const MacroDefinition& macroDef,
    const MacroDefinition* const macroDefContext,
    const std::vector<std::string>* const argContext) {
  std::vector<std::string> argumentList;

  scanner.get();  // skip the beginning '('

  if (scanner.peek() == ')') {
    scanner.get();  // skip the ending ')'
    return argumentList;
  }

  std::string arg =
      parseFunctionLikeMacroArgument(scanner, macroDefContext, argContext);
  argumentList.push_back(std::move(arg));

  while (scanner.peek() != ')') {
    if (scanner.peek() == ',') {
      scanner.get();
      auto args =
          parseFunctionLikeMacroArgument(scanner, macroDefContext, argContext);
      argumentList.push_back(std::move(args));
      continue;
    }

    // While parsing a function-like macro's argument, the scanner will only
    // stop when it has read a right parenthesis (that doesn't pair with a
    // former-parsed left parenthis), a comma or when it have reached the end
    // of input (in this case it will return a EOF). Because we have excluded
    // the first two possibilities, We are sure that we will get an EOF in the
    // following code.

    const auto startOffset = scanner.offset();
    const auto ch = scanner.get();  // will get an EOF here
    const auto endOffset = scanner.offset();

    return Error{{startOffset, endOffset},
                 std::format("unterminated argument list invoking macro \"{}\"",
                             macroDef.name),
                 ""};
  }

  scanner.get();  // skip the ending ')'
  return argumentList;
}

template <ByteDecoderConcept F>
std::string PPImpl<F>::expandFunctionLikeMacro(
    const MacroDefinition& macroDef,
    const std::vector<std::string>& arguments) {
  using namespace MacroExpansionResult;

  if (macroDef.body.empty()) return " ";

  RawBufferScanner<F> rbs(macroDef.body.c_str(), macroDef.body.size(),
                       scanner.byteDecoder());
  std::string output;

  while (!rbs.reachedEndOfInput()) {
    if (isStartOfIdentifier(rbs.peek())) {
      const auto offsetBeforePasing = rbs.offset();
      const auto identifier = parseIdentifier(rbs);
      if (const auto index = findIndexOfParameter(macroDef, identifier)) {
        output += arguments[*index];
        continue;
      }

      auto res = tryExpandingMacro(identifier, rbs, &macroDef, &arguments);

      if (const auto ptr = std::get_if<Ok>(&res)) {
        const auto sectionStart = codeBuffer.section(ptr->sectionID);
        const auto strPos = codeBuffer.pos(sectionStart);
        const auto strLen = codeBuffer.sectionSize(ptr->sectionID);
        output.append(reinterpret_cast<const char*>(strPos), strLen);
      } else if (const auto ptr = std::get_if<Fail>(&res)) {
        output += identifier;
      } else {
        errOut.reportsError(std::get<Error>(std::move(res)));
        continue;
      }

    } else {
      output += rbs.get();
    }
  }

  return output;
}

template <ByteDecoderConcept F>

template <std::derived_from<IBaseScanner> T>
std::string PPImpl<F>::parseFunctionLikeMacroArgument(
    ILookaheadScanner<T>& scanner, const MacroDefinition* const macroDefContext,
    const std::vector<std::string>* const argContext) {
  using namespace MacroExpansionResult;

  std::string output;
  int parenthesisLevel = 0;

  // skip leading spaces
  while (isSpace(scanner.peek())) scanner.get();

  while (!scanner.reachedEndOfInput()) {
    if (parenthesisLevel == 0) {
      if (scanner.peek() == ',') break;
      if (scanner.peek() == ')') break;
    }

    if (scanner.peek() == '(' || scanner.peek() == ')') {
      if (scanner.peek() == '(') parenthesisLevel++;
      if (scanner.peek() == ')') parenthesisLevel--;
      output += scanner.get();
      continue;
    }

    if (isSpace(scanner.peek())) {
      while (isSpace(scanner.peek())) scanner.get();
      output += scanner.get();
      continue;
    }

    if (isStartOfIdentifier(scanner.peek())) {
      const auto startOffset = scanner.offset();
      const auto identifier = parseIdentifier(scanner);

      if (macroDefContext) {
        if (auto index = findIndexOfParameter(*macroDefContext, identifier)) {
          output += (*argContext)[*index];
          continue;
        }
      }

      auto res =
          tryExpandingMacro(identifier, scanner, macroDefContext, argContext);

      if (const auto ptr = std::get_if<Error>(&res)) {
        errOut.reportsError(std::get<Error>(std::move(res)));
        continue;
      }

      if (const auto ptr = std::get_if<Ok>(&res)) {
        const auto strOffset = codeBuffer.section(ptr->sectionID);
        const auto strPos = codeBuffer.pos(strOffset);
        const auto strLen = codeBuffer.sectionSize(ptr->sectionID);
        output.append(reinterpret_cast<const char*>(strPos), strLen);
      } else {  // Fail
        output.append(identifier);
      }
    } else {
      output += scanner.get();
    }
  }

  return output;
}

template <ByteDecoderConcept F>
void PPImpl<F>::fastForwardToFirstOutputCharacter() {
  for (;;) {
    skipSpacesAndComments(scanner);
    if (scanner.peek() != '#') break;
    parseDirective();
  }
}

template <ByteDecoderConcept F>
void PPImpl<F>::parseDirective() {
  PPDirectiveScanner ppds{scanner};

  const auto startOffset = ppds.offset();
  Error error;

  ppds.get();  // ignore the leading #
  skipSpaces(ppds, isDirectiveSpace);

  const auto offsetBeforeParsingDirectiveName = scanner.offset();
  std::string directiveName = parseIdentifier(ppds);
  const auto offsetAfterParsingDirectiveName = scanner.offset();

  if (directiveName == "") {
    skipNewline(scanner);
    return;
  }

  if (directiveName == "define") {
    MacroType macroType = MacroType::OBJECT_LIKE_MACRO;
    std::vector<std::string> parameters;

    skipSpacesAndComments(ppds, isDirectiveSpace);
    if (!isStartOfIdentifier(ppds.peek())) {
      const auto startOffset = ppds.offset();
      ppds.get();
      const auto endOffset = ppds.offset();
      error = Error({startOffset, endOffset}, "macro names must be identifiers",
                    "");
      goto fail;
    }

    std::string macroName = parseIdentifier(ppds);
    if (ppds.peek() == '(') {
      auto result = parseFunctionLikeMacroParameters(macroName, ppds);
      if (auto e = std::get_if<Error>(&result)) {
        error = std::move(*e);
        goto fail;
      }

      macroType = MacroType::FUNCTION_LIKE_MACRO;
      parameters = std::move(std::get<std::vector<std::string>>(result));
    }

    skipSpacesAndComments(ppds, isDirectiveSpace);
    std::string macroBody = readAll(ppds);

    if (macroType == MacroType::OBJECT_LIKE_MACRO) {
      setOfMacroDefinitions.insert(MacroDefinition(macroName, macroBody));
    } else {
      setOfMacroDefinitions.insert(
          MacroDefinition(macroName, parameters, macroBody));
    }

    skipNewline(scanner);
  } else {
    error = Error{
        {offsetBeforeParsingDirectiveName, offsetAfterParsingDirectiveName},
        "Unknown preprocessing directive " + directiveName,
        ""};
    goto fail;
  }

  return;

fail:
  skipAll(ppds);
  skipNewline(scanner);
  errOut.reportsError(error);
}

// paraList -> ( )
// paraList | ( id restOfParameters )
// restOfParameters -> ''
//      |  , id restOfParameters
//

template <ByteDecoderConcept F>
template <std::derived_from<IBaseScanner> T>
std::variant<std::vector<std::string>, Error>
PPImpl<F>::parseFunctionLikeMacroParameters(const std::string& macroName,
                                            ILookaheadScanner<T>& scanner) {
  std::vector<std::string> parameters;
  const auto parameterHasDefined =
      [&parameters](const std::string& identifier) {
        return std::find(parameters.begin(), parameters.end(), identifier) !=
               parameters.end();
      };

  scanner.get();  // skip the beginning '('
  skipSpacesAndComments(scanner);

  if (scanner.peek() == ')') {
    scanner.get();
    return parameters;
  }

  if (scanner.reachedEndOfInput()) {
    return Error{{scanner.offset(), scanner.offset() + 1},
                 "Expected parameter name before end of line",
                 ""};
  }

  if (!isStartOfIdentifier(scanner.peek())) {
    const auto startOffset = scanner.offset();
    scanner.get();
    const auto endOffset = scanner.offset();
    return Error{{startOffset, endOffset}, "Expected ',' or ')' here.", ""};
  }

  const auto identStartOffset = scanner.offset();
  parameters.push_back(parseIdentifier(scanner));
  skipSpacesAndComments(scanner);

  while (scanner.peek() != ')') {
    if (scanner.reachedEndOfInput()) {
      return Error{{scanner.offset(), scanner.offset() + 1},
                   "Expected ')' before end of line",
                   ""};
    }

    if (scanner.peek() != ',') {
      const auto startOffset = scanner.offset();
      scanner.get();
      const auto endOffset = scanner.offset();
      return Error{{startOffset, endOffset}, "Expected ',' or ')' here.", ""};
    }

    scanner.get();  // skip , (ignore error conditions for now)
    skipSpacesAndComments(scanner);

    if (scanner.reachedEndOfInput()) {
      return Error{{scanner.offset(), scanner.offset() + 1},
                   "Expected parameter name before end of line",
                   ""};
    }

    if (!isStartOfIdentifier(scanner.peek())) {
      const auto startOffset = scanner.offset();
      scanner.get();
      const auto endOffset = scanner.offset();
      return Error{{startOffset, endOffset}, "Expected ',' or ')' here.", ""};
    }

    const auto parameter = parseIdentifier(scanner);
    if (parameterHasDefined(parameter)) {
      const auto identEndOffset = scanner.offset();
      return Error{
          {identStartOffset, identEndOffset},
          std::format(
              "Duplicated parameter \"{}\" in the function-like macro \"{}\".",
              parameter, macroName)};
    }
    parameters.push_back(parameter);
  }

  scanner.get();
  return parameters;
}

#endif  // !TPLCC_PREPROCESSOR_H
