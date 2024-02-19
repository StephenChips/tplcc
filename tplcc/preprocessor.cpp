#include "preprocessor.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <format>
#include <stdexcept>

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

// The MSVC's std::isspace is troublesome. It will throw a runtime error when we
// pass codepoint that is larger than 255, instead of simply returning false, so
// we have to write our own version of isspace here.
bool isSpace(int ch) {
  return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
         ch == '\v';
}
bool isDirectiveSpace(int ch) { return ch == ' ' || ch == '\t'; }
bool isNewlineCharacter(int ch) { return ch == '\r' || ch == '\n'; }

PPScanner::PPScanner(
    PPImpl& pp, CodeBuffer& codeBuffer,
    std::function<std::tuple<int, int>(const unsigned char*)> readUTF32)
    : PPBaseScanner(codeBuffer, codeBuffer.section(pp.currentSection()),
                    std::move(readUTF32)),
      pp(pp) {
  skipBackslashReturn();
};

bool PPScanner::reachedEndOfInput() const {
  return _offset == pp.currentSectionEnd();
}

PPDirectiveScanner::PPDirectiveScanner(const PPScanner& scanner)
    : PPBaseScanner(scanner), sectionID(scanner.ppImpl().currentSectionID()) {
  skipBackslashReturn();
};

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

/*
 * Problem:
 *
 * If a macro is expanded to an empty string, and it's at the end of the input,
 * for example:
 *
 * ```
 * #define ID(x) x
 *
 * hello ID()
 * ```
 *
 * the preprocessor has reached the end before we expand the macro, yet it
 * cannot know such fact until is actually expand the macro and find out if the
 * macro is expanded to nothing. For now `reachedEndOfInput` checks the internal
 * scanner's offset to determine if it has reached the end. It doesn't changes
 * or copy it scanner. Without doing that, It is not possible for it to know if
 * the pp has reached the end.
 *
 * Solution:
 *
 * Implement the peek() method. Inside the method it will copy the current
 * scanner and try getting the next character. if the pp has reached the end, it
 * will return EOF. Internally we all uses `peek() == EOF` to check if we've
 * reached the end, and `reachedTheEndOfInput` is only used by the user of a
 * preprocessor.
 *
 * Then, the reachedTheEndOfInput will only be a simple function that checks if
 * current `peek()` equals to `EOF`:
 *
 * bool reachedEndOfInput() {
 *     return peek() == EOF;
 * }
 *
 *
 */
PPCharacter PPImpl::get() {
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
  if (isSpace(scanner) || lookaheadMatches(scanner, "/*")) {
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
    auto [sectionID, endOffset] = tryExpandingMacro(scanner, nullptr, nullptr);
    if (sectionID) {
      scanner.setOffset(endOffset);
      enterSection(*sectionID);
    } else {
      identEndOffset = endOffset;
    }
    return get();
  }

  const auto ch = scanner.get();
  const auto offset = scanner.offset();
  exitFullyScannedSections();
  return PPCharacter(ch, offset);
}
/*
identifier <- parse the identifier first

1. expand it as object-like macro if there is an object-like macro definition
whose name is same as the identifier.

2. expand it as function-like macro if there is a funciton-like macro defintion
whose name is same as the identifier, and there is a argument list right
behind the identifier

3. otherwise do not expand the macro.
identScanner = copy current scanner
identifier = parseIdentifier(identScanner)

macro definition := find macro definition with identifier

if macro definition isn't found {
scan the text char by char.
}

if the macro definition is a object-like macro {
expand it.
}

if the macro definition is a function-like macro {
if it is followed by an argument list {
     arguments = parseMacroArgs scanner.
     expand the macro (if it's cached, return the cached string directly)
     enter and scan the new section.
} else {
    scan the text char by cahr.
}
}

func parseMacroArgs (scanner) {
skip '(' at the beginning,
}
*/
std::tuple<std::optional<CodeBuffer::SectionID>, CodeBuffer::Offset>
PPImpl::tryExpandingMacro(const ICopyableOffsetScanner& scanner,
                          const MacroDefinition* const macroDefContext,
                          const std::vector<std::string>* const argContext) {
  assert(macroDefContext == nullptr && argContext == nullptr ||
         macroDefContext != nullptr && argContext != nullptr &&
             macroDefContext->parameters.size() == argContext->size());

  auto copy = copyUnique(scanner);
  std::string identStr = parseIdentifier(*copy);

  const auto macroDef = setOfMacroDefinitions.find(identStr);
  if (macroDef == setOfMacroDefinitions.end() ||
      macroDef->type == MacroType::FUNCTION_LIKE_MACRO &&
          !copy->reachedEndOfInput() && copy->peek() != '(') {
    return {std::nullopt, copy->offset()};
  }

  std::string key;
  std::vector<std::string> arguments;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    arguments =
        parseFunctionLikeMacroArgumentList(*copy, macroDefContext, argContext);
    key = createFunctionLikeMacroCacheKey(macroDef->name, arguments);
  } else {
    key = macroDef->name;
  }

  if (const auto iter = codeCache.find(key); iter != codeCache.end()) {
    return {iter->second, copy->offset()};
  }

  std::string expandedText;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    expandedText = expandFunctionLikeMacro(*macroDef, arguments);
  } else {
    expandedText = macroDef->body.empty() ? " " : macroDef->body;
  }

  CodeBuffer::SectionID sectionID = codeBuffer.addSection(expandedText);
  codeCache.insert({macroDef->name, sectionID});

  return {sectionID, copy->offset()};
}

std::vector<std::string> PPImpl::parseFunctionLikeMacroArgumentList(
    ICopyableOffsetScanner& scanner,
    const MacroDefinition* const macroDefContext,
    const std::vector<std::string>* const argContext) {
  std::vector<std::string> argumentList;

  scanner.get();  // skip the beginning '('

  if (scanner.peek() == ')') {
    scanner.get();  // skip the ending ')'

    // If there is nothing inside an argument list, e.g. ID(), the
    // macro will still have a empty string as its only argument.
    argumentList.push_back("");
    return argumentList;
  }

  std::string arg =
      parseFunctionLikeMacroArgument(scanner, macroDefContext, argContext);
  argumentList.push_back(std::move(arg));

  while (scanner.peek() != ')') {
    if (scanner.peek() != ',') {
      // should return error to the parent
      throw std::runtime_error("expects a ','");
    }
    scanner.get();  // skip ','

    std::string arg =
        parseFunctionLikeMacroArgument(scanner, macroDefContext, argContext);
    argumentList.push_back(std::move(arg));
  }

  scanner.get();  // skip the ending ')'
  return argumentList;
}

std::string PPImpl::expandFunctionLikeMacro(
    const MacroDefinition& macroDef,
    const std::vector<std::string>& arguments) {
  if (macroDef.parameters.size() != arguments.size()) {
    throw std::runtime_error(std::format(
        "The macro {} requires {} argument(s), but got {}.", macroDef.name,
        macroDef.parameters.size(), arguments.size()));
  }

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

      const auto [sectionID, endOffset] =
          tryExpandingMacro(rbs, &macroDef, &arguments);
      CodeBuffer::Offset startOffset;
      std::size_t size;

      if (sectionID) {
        startOffset = codeBuffer.section(*sectionID);
        size = codeBuffer.sectionSize(*sectionID);
      } else {
        startOffset = rbs.offset();
        size = endOffset - rbs.offset();
      }

      const auto startPos = codeBuffer.pos(startOffset);
      output.append(reinterpret_cast<const char*>(startPos), size);

      rbs.setOffset(endOffset);
    } else {
      output += rbs.get();
    }
  }

  return output;
}

std::string PPImpl::parseFunctionLikeMacroArgument(
    ICopyableOffsetScanner& scanner,
    const MacroDefinition* const macroDefContext,
    const std::vector<std::string>* const argContext) {
  std::string output;
  int parenthesisLevel = 0;

  // skip leading spaces
  while (isSpace(scanner)) scanner.get();

  while (!scanner.reachedEndOfInput()) {
    if (parenthesisLevel == 0) {
      if (scanner.peek() == ',') break;
      if (scanner.peek() == ')') break;
    }

    if (scanner.peek() == ')') {
      parenthesisLevel--;
    } else if (scanner.peek() == '(') {
      parenthesisLevel++;
    }

    if (isSpace(scanner)) {
      while (isSpace(scanner)) scanner.get();
      if (!scanner.reachedEndOfInput()) {
        output += ' ';
      }
      continue;
    }

    if (isStartOfIdentifier(scanner.peek())) {
      const auto ident = parseIdentifier(*copyUnique(scanner));
      if (macroDefContext) {
        if (auto index = findIndexOfParameter(*macroDefContext, ident)) {
          output += argContext->operator[](*index);
          scanner.setOffset(scanner.offset() + ident.size());
          continue;
        }
      }

      const auto [sectionID, endOffset] =
          tryExpandingMacro(scanner, macroDefContext, argContext);
      CodeBuffer::Offset startOffset;
      std::size_t size;
      if (sectionID) {
        startOffset = codeBuffer.section(*sectionID);
        size = codeBuffer.sectionSize(*sectionID);
      } else {
        startOffset = scanner.offset();
        size = endOffset - scanner.offset();
      }

      const auto startPos = codeBuffer.pos(startOffset);
      output.append(reinterpret_cast<const char*>(startPos), size);

      scanner.setOffset(endOffset);
    } else {
      output += scanner.get();
    }
  }

  return output;
}

void PPImpl::fastForwardToFirstOutputCharacter(PPBaseScanner& scanner) {
  while (true) {
    skipSpaces(scanner);

    if (scanner.peek() == '#') {
      parseDirective();
    } else if (scanner.peek() == ' ') {
      skipSpacesAndComments(scanner);
    } else {
      break;
    }
  }
}

void PPImpl::parseDirective() {
  using namespace std::literals::string_literals;
  PPDirectiveScanner ppds{scanner};

  const auto startOffset = currentSection();
  std::string errorMsg;
  std::string hint;

  ppds.get();  // ignore the leading #
  skipSpaces(ppds, isDirectiveSpace);

  std::string directiveName;

  while (!ppds.reachedEndOfInput() && !isDirectiveSpace(ppds.peek())) {
    directiveName.push_back(ppds.get());
  }

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
      errorMsg = hint = "macro names must be identifiers";
      goto fail;
    }

    std::string macroName = parseIdentifier(ppds);
    if (ppds.peek() == '(') {
      auto result = parseFunctionLikeMacroParameters(ppds);
      if (auto msg = std::get_if<ErrorMessage>(&result)) {
        errorMsg = std::move(msg->errorMessage);
        hint = std::move(msg->hint);
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
    errorMsg = hint = "Unknown preprocessing directive "s + directiveName;
    goto fail;
  }

  return;

fail:
  skipAll(scanner);
  const CodeBuffer::Offset endOffset = scanner.offset();
  const auto range = std::make_tuple(startOffset, endOffset);
  const auto error = Error({startOffset, endOffset}, errorMsg, errorMsg);
  errOut.reportsError(error);
}

void PPImpl::skipNewline(PPBaseScanner& scanner) {
  if (scanner.peek() == '\r') scanner.get();
  if (scanner.peek() == '\n') scanner.get();
}

void PPImpl::enterSection(CodeBuffer::SectionID id) {
  stackOfSectionID.push_back(id);
  stackOfStoredOffsets.push_back(scanner.offset());
  scanner.setOffset(codeBuffer.section(id));
}

void PPImpl::exitSection() {
  stackOfSectionID.pop_back();
  if (!stackOfStoredOffsets.empty()) {
    scanner.setOffset(stackOfStoredOffsets.back());
    stackOfStoredOffsets.pop_back();
  }
}

bool PPImpl::reachedEndOfCurrentSection() const {
  return scanner.reachedEndOfInput();
}

void PPImpl::exitFullyScannedSections() {
  while (!stackOfSectionID.empty() && reachedEndOfCurrentSection()) {
    exitSection();
  }
}

/* PPImpl */

CodeBuffer::SectionID PPImpl::currentSectionID() const {
  return stackOfSectionID.empty() ? 0 : stackOfSectionID.back();
}

CodeBuffer::Offset PPImpl::currentSection() const {
  return codeBuffer.section(currentSectionID());
}

CodeBuffer::Offset PPImpl::currentSectionEnd() const {
  return codeBuffer.sectionEnd(currentSectionID());
}

bool PPImpl::lookaheadMatches(const PPBaseScanner& scanner,
                              const std::string& s) {
  auto copy = scanner.copy();
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
std::variant<std::vector<std::string>, ErrorMessage>
PPImpl::parseFunctionLikeMacroParameters(PPBaseScanner& scanner) {
  std::vector<std::string> parameters;

  scanner.get();
  skipSpacesAndComments(scanner);

  if (scanner.peek() == ')') {
    scanner.get();
    return parameters;
  }

  const auto identifier = parseIdentifier(scanner);
  parameters.push_back(identifier);

  while (!scanner.reachedEndOfInput() && scanner.peek() != ')') {
    if (scanner.peek() != ',') {
      return ErrorMessage("Expected ',' or ')' here.");
    }
    scanner.get();  // skip , (ignore error conditions for now)
    skipSpacesAndComments(scanner);
    const auto identifier = parseIdentifier(scanner);
    parameters.push_back(identifier);
  }

  scanner.get();
  return parameters;
}