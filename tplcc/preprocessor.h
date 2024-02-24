#ifndef TPLCC_PREPROCESSOR_H
#define TPLCC_PREPROCESSOR_H

#include <algorithm>
#include <cassert>
#include <cctype>
#include <compare>
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
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
#include "pp-scanner.h"

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

struct ICopyable {
  virtual ICopyable* copy() const = 0;
};

// A wrapper that store a character and all information about it.
class PPCharacter {
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

struct ICopyableOffsetScanner : IBaseScanner, ICopyable {
  virtual void setOffset(CodeBuffer::Offset offset) = 0;
  virtual CodeBuffer::Offset offset() const = 0;
  virtual ICopyableOffsetScanner* copy() const = 0;
};

struct IOffsetScanner : IBaseScanner {
  virtual CodeBuffer::Offset offset() const = 0;
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

class PPDirectiveScanner : public PPBaseScanner {
  CodeBuffer::SectionID sectionID;

 public:
  template <ByteDecoderConcept F>
  PPDirectiveScanner(const PPScanner<F>& scanner)
      : PPBaseScanner(scanner), sectionID(scanner.ppImpl().currentSectionID()) {
    skipBackslashReturn();
  }
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

template <ByteDecoderConcept F>
class PPImpl {
  CodeBuffer& codeBuffer;
  IReportError& errOut;

  // A cache for macro that has been expanded before.
  std::map<std::string, CodeBuffer::SectionID> codeCache;
  std::set<MacroDefinition, CompareMacroDefinition> setOfMacroDefinitions;

  std::vector<CodeBuffer::SectionID> stackOfSectionID;
  std::vector<CodeBuffer::Offset> stackOfStoredOffsets;

  std::optional<CodeBuffer::Offset> identEndOffset;
  PPScanner<F> scanner;

  std::optional<PPCharacter> lookaheadBuffer;

  bool canParseDirectives;

 public:
  PPImpl(CodeBuffer& codeBuffer, IReportError& errOut, F&& readUTF32)
      : codeBuffer(codeBuffer),
        errOut(errOut),
        setOfMacroDefinitions(setOfMacroDefinitions),
        codeCache(codeCache),
        scanner(*this, codeBuffer, std::forward<F>(readUTF32)) {
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
  MacroExpansionResult::Type tryExpandingMacro(
      const std::string& macroName, IOffsetScanner& scanner,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  std::variant<std::vector<std::string>, Error>
  parseFunctionLikeMacroArgumentList(
      IOffsetScanner& scanner, const MacroDefinition& macroDef,
      const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);
  std::string parseFunctionLikeMacroArgument(
      IOffsetScanner& scanner, const MacroDefinition* const macroDefContext,
      const std::vector<std::string>* const argContext);

  std::string expandFunctionLikeMacro(
      const MacroDefinition& macroDef,
      const std::vector<std::string>& arguments);
};

template <ByteDecoderConcept F>
template <typename T>
std::optional<Error> PPImpl<F>::skipSpacesAndComments(PPBaseScanner& scanner,
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

template <ByteDecoderConcept F>
template <typename T>
void PPImpl<F>::skipSpaces(PPBaseScanner& scanner, T&& isSpaceFn) {
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

void skipAll(PPBaseScanner& scanner);
std::string readAll(PPBaseScanner& scanner);

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

void skipAll(PPBaseScanner& scanner) {
  while (scanner.get() != EOF)
    ;
}

std::string readAll(PPBaseScanner& scanner) {
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

bool PPDirectiveScanner::reachedEndOfInput() const {
  if (_offset == _codeBuffer.sectionEnd(sectionID)) return true;
  const auto [ch, _] = _readUTF32(_codeBuffer.pos(_offset));
  return isNewlineCharacter(ch);
}

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
  if (stackOfSectionID.empty() && scanner.reachedEndOfInput()) {
    return PPCharacter::eof();
  }
  if (scanner.reachedEndOfInput()) {
    exitSection();
    return get();
  }

  if (identEndOffset) {
    const auto ch = scanner.get();
    const auto offset = scanner.offset();
    if (offset == identEndOffset) {
      identEndOffset = std::nullopt;
      exitFullyScannedSections();
    }
    return PPCharacter(ch, offset);
  }

  // A row of spaces and comments will be merge into one space. That means
  // whenever we read a space or a comment, we will skip as far as possible then
  // return a space to the caller.
  if (isSpace(scanner) || lookaheadMatches(scanner, "/*") ||
      lookaheadMatches(scanner, "//")) {
    // Actually an error range will not start or end with a space or a comment,
    // so it doesn't matter what offset we return to the caller.

    const auto offset = scanner.offset();
    skipSpacesAndComments(scanner);

    if (scanner.peek() == '#' && canParseDirectives) {
      parseDirective();
      return get();
    } else {
      exitFullyScannedSections();
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
    auto [res, endOffset] = tryExpandingMacro(scanner, nullptr, nullptr);

    if (const auto ptr = std::get_if<MacroExpansionResult::Ok>(&res)) {
      scanner.setOffset(ptr->endOffset);
      enterSection(ptr->sectionID);
    } else if (const auto ptr = std::get_if<MacroExpansionResult::Fail>(&res)) {
      identEndOffset = endOffset;
    } else {
      identEndOffset = endOffset;
      errOut.reportsError(std::get<Error>(std::move(res)));
    }

    return get();
  }

  const auto ch = scanner.get();
  const auto offset = scanner.offset();
  exitFullyScannedSections();
  return PPCharacter(ch, offset);
}

template <ByteDecoderConcept F>
MacroExpansionResult::Type PPImpl<F>::tryExpandingMacro(const std::string& macroName,
                             IOffsetScanner& scanner,
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
    return {MacroExpansionResult::Fail(), scanner.offset()};
  }

  std::string key;
  std::vector<std::string> arguments;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    auto result = parseFunctionLikeMacroArgumentList(
        scanner, *macroDef, macroDefContext, argContext);

    if (const auto error = std::get_if<Error>(&result)) {
      return {*error, scanner.offset()};
    }

    arguments = std::get_if<std::vector<std::string>>(std::move(result));
    // If there is nothing inside an argument list, e.g. ID(), the
    // macro will still have a empty string as its only argument.
    if (macroDef->parameters.size() == 1 && arguments.empty()) {
      arguments.push_back("");
    }
    if (macroDef->parameters.size() != arguments.size()) {
      return {Error{{startOffset, scanner.offset()},
                    std::format(
                        "The macro \"{}\" requires {} argument(s), but got {}.",
                        macroDef->name, macroDef->parameters.size(),
                        arguments.size()),
                    ""},
              scanner.offset()};
    }
    key = createFunctionLikeMacroCacheKey(macroDef->name, arguments);
  } else {
    key = macroDef->name;
  }

  if (const auto iter = codeCache.find(key); iter != codeCache.end()) {
    return {MacroExpansionResult::Ok{iter->second, scanner.offset()},
            scanner.offset()};
  }

  std::string expandedText;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    expandedText = expandFunctionLikeMacro(*macroDef, arguments);
  } else {
    expandedText = macroDef->body.empty() ? " " : macroDef->body;
  }

  CodeBuffer::SectionID sectionID = codeBuffer.addSection(expandedText);
  codeCache.insert({macroDef->name, sectionID});

  return {MacroExpansionResult::Ok{sectionID, scanner.offset()},
          scanner.offset()};
}

template <ByteDecoderConcept F>
std::variant<std::vector<std::string>, Error>
PPImpl<F>::parseFunctionLikeMacroArgumentList(
    IOffsetScanner& scanner, const MacroDefinition& macroDef,
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

  RawBufferScanner rbs(macroDef.body.c_str(), macroDef.body.size(),
                       scanner.byteDecoder());
  std::string output;

  while (!rbs.reachedEndOfInput()) {
    if (isStartOfIdentifier(rbs.peek())) {
      const auto ident = parseIdentifier(*copyUnique(rbs));
      if (const auto index = findIndexOfParameter(macroDef, ident)) {
        output += arguments[*index];
        rbs.setOffset(rbs.offset() + ident.size());
        continue;
      }

      auto [res, endOffset] = tryExpandingMacro(rbs, &macroDef, &arguments);
      CodeBuffer::Offset startOffset;
      std::size_t size;

      if (const auto ptr = std::get_if<Ok>(&res)) {
        startOffset = codeBuffer.section(ptr->sectionID);
        size = codeBuffer.sectionSize(ptr->sectionID);
        rbs.setOffset(ptr->endOffset);
      } else if (const auto ptr = std::get_if<Fail>(&res)) {
        startOffset = rbs.offset();
        size = endOffset - rbs.offset();
        rbs.setOffset(endOffset);
      } else {
        errOut.reportsError(std::get<Error>(std::move(res)));
        continue;
      }

      const auto startPos = codeBuffer.pos(startOffset);
      output.append(reinterpret_cast<const char*>(startPos), size);
    } else {
      output += rbs.get();
    }
  }

  return output;
}

template <ByteDecoderConcept F>
std::string PPImpl<F>::parseFunctionLikeMacroArgument(
    IOffsetScanner& scanner, const MacroDefinition* const macroDefContext,
    const std::vector<std::string>* const argContext) {
  using namespace MacroExpansionResult;

  std::string output;
  int parenthesisLevel = 0;

  // skip leading spaces
  while (isSpace(scanner)) scanner.get();

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

    if (isSpace(scanner)) {
      while (isSpace(scanner)) scanner.get();
      if (!scanner.reachedEndOfInput()) {
        output += ' ';
      }
      continue;
    }

    if (isStartOfIdentifier(scanner.peek())) {
      const auto startOffset = scanner.offset();
      const auto ident = parseIdentifier(scanner);

      if (macroDefContext) {
        if (auto index = findIndexOfParameter(*macroDefContext, ident)) {
          output += (*argContext)[*index];
          continue;
        }
      }

      auto res = tryExpandingMacro(ident, scanner, macroDefContext, argContext);
      CodeBuffer::Offset startOffset;
      std::size_t size;

      if (const auto ptr = std::get_if<Error>(&res)) {
        errOut.reportsError(std::get<Error>(std::move(res)));
        continue;
      }

      if (const auto ptr = std::get_if<Ok>(&res)) {
        startOffset = codeBuffer.section(ptr->sectionID);
        size = codeBuffer.sectionSize(ptr->sectionID);
      } else {  // Fail
        size = ident.size();
      }

      const auto startPos = codeBuffer.pos(startOffset);
      output.append(reinterpret_cast<const char*>(startPos), size);
    } else {
      output += scanner.get();
    }
  }

  return output;
}

template <ByteDecoderConcept F>
void PPImpl<F>::fastForwardToFirstOutputCharacter() {
  auto copy = copyUnique(scanner);
  skipSpacesAndComments(*copy);

  while (copy->peek() == '#') {
    scanner.setOffset(copy->offset());
    parseDirective();
    copy->setOffset(scanner.offset());
    skipSpacesAndComments(*copy);
  }
}

template <ByteDecoderConcept F>
void PPImpl<F>::parseDirective() {
  PPDirectiveScanner ppds{scanner};

  const auto startOffset = currentSection();
  Error error;

  ppds.get();  // ignore the leading #
  skipSpaces(ppds, isDirectiveSpace);

  const auto offsetBeforeDirectiveName = scanner.offset();
  std::string directiveName = parseIdentifier(ppds);
  const auto offsetAfterDirectiveName = scanner.offset();

  if (directiveName == "") {
    scanner.setOffset(ppds.offset());
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

    scanner.setOffset(ppds.offset());
    skipNewline(scanner);
  } else {
    error = Error{{offsetBeforeDirectiveName, offsetAfterDirectiveName},
                  "Unknown preprocessing directive " + directiveName,
                  ""};
    goto fail;
  }

  return;

fail:
  skipAll(ppds);
  scanner.setOffset(ppds.offset());
  skipNewline(scanner);
  errOut.reportsError(error);
}

template <ByteDecoderConcept F>
void PPImpl<F>::skipNewline(PPBaseScanner& scanner) {
  if (scanner.peek() == '\r') scanner.get();
  if (scanner.peek() == '\n') scanner.get();
}

template <ByteDecoderConcept F>
void PPImpl<F>::enterSection(CodeBuffer::SectionID id) {
  stackOfSectionID.push_back(id);
  stackOfStoredOffsets.push_back(scanner.offset());
  scanner.setOffset(codeBuffer.section(id));
}

template <ByteDecoderConcept F>
void PPImpl<F>::exitSection() {
  stackOfSectionID.pop_back();
  if (!stackOfStoredOffsets.empty()) {
    scanner.setOffset(stackOfStoredOffsets.back());
    stackOfStoredOffsets.pop_back();
  }
}

template <ByteDecoderConcept F>
bool PPImpl<F>::reachedEndOfCurrentSection() const {
  return scanner.reachedEndOfInput();
}

template <ByteDecoderConcept F>
void PPImpl<F>::exitFullyScannedSections() {
  while (!stackOfSectionID.empty() && reachedEndOfCurrentSection()) {
    exitSection();
  }
}

/* PPImpl */

template <ByteDecoderConcept F>
CodeBuffer::SectionID PPImpl<F>::currentSectionID() const {
  return stackOfSectionID.empty() ? 0 : stackOfSectionID.back();
}

template <ByteDecoderConcept F>
CodeBuffer::Offset PPImpl<F>::currentSection() const {
  return codeBuffer.section(currentSectionID());
}

template <ByteDecoderConcept F>
CodeBuffer::Offset PPImpl<F>::currentSectionEnd() const {
  return codeBuffer.sectionEnd(currentSectionID());
}

template <ByteDecoderConcept F>
bool PPImpl<F>::lookaheadMatches(const PPBaseScanner& scanner,
                                 const std::string& s) {
  auto copy = copyUnique(scanner);
  for (const char ch : s) {
    if (copy->peek() == EOF || copy->peek() != ch) return false;
    copy->get();
  }

  return true;
}

void PPBaseScanner::skipBackslashReturn() {
  if (reachedEndOfInput()) return;
  const auto [ch1, l1] = _readUTF32(_codeBuffer.pos(_offset));
  if (ch1 != '\\' || reachedEndOfInput()) return;
  _offset++;
  const auto [ch2, l2] = _readUTF32(_codeBuffer.pos(_offset));
  if (ch2 != '\n') return;
  _offset++;
  skipBackslashReturn();
}

// paraList -> ( )
// paraList | ( id restOfParameters )
// restOfParameters -> ''
//      |  , id restOfParameters
//

template <ByteDecoderConcept F>
std::variant<std::vector<std::string>, Error>
PPImpl<F>::parseFunctionLikeMacroParameters(const std::string& macroName,
                                            PPBaseScanner& scanner) {
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
