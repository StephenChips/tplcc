#include "preprocessor.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <format>
#include <stdexcept>

#include "helper.h"

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

std::tuple<MacroExpansionResult::Type, CodeBuffer::Offset>
PPImpl::tryExpandingMacro(const ICopyableOffsetScanner& scanner,
                          const MacroDefinition* const macroDefContext,
                          const std::vector<std::string>* const argContext) {
  assert(macroDefContext == nullptr && argContext == nullptr ||
         macroDefContext != nullptr && argContext != nullptr &&
             macroDefContext->parameters.size() == argContext->size());

  auto copy = copyUnique(scanner);
  const auto startOffset = copy->offset();
  std::string identStr = parseIdentifier(*copy);

  const auto macroDef = setOfMacroDefinitions.find(identStr);
  if (macroDef == setOfMacroDefinitions.end() ||
      macroDef->type == MacroType::FUNCTION_LIKE_MACRO && copy->peek() != '(') {
    return {MacroExpansionResult::Fail(), copy->offset()};
  }

  std::string key;
  std::vector<std::string> arguments;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    auto result = parseFunctionLikeMacroArgumentList(
        *copy, *macroDef, macroDefContext, argContext);

    if (auto args = std::get_if<std::vector<std::string>>(&result)) {
      arguments = std::move(*args);
    } else {
      return {std::get<Error>(result), scanner.offset()};
    }

    // If there is nothing inside an argument list, e.g. ID(), the
    // macro will still have a empty string as its only argument.
    if (macroDef->parameters.size() == 1 && arguments.empty()) {
      arguments.push_back("");
    }

    if (macroDef->parameters.size() != arguments.size()) {
      return {Error{{startOffset, copy->offset()},
                    std::format(
                        "The macro \"{}\" requires {} argument(s), but got {}.",
                        macroDef->name, macroDef->parameters.size(),
                        arguments.size()),
                    ""},
              copy->offset()};
    }

    key = createFunctionLikeMacroCacheKey(macroDef->name, arguments);
  } else {
    key = macroDef->name;
  }

  if (const auto iter = codeCache.find(key); iter != codeCache.end()) {
    return {MacroExpansionResult::Ok{iter->second, copy->offset()},
            copy->offset()};
  }

  std::string expandedText;
  if (macroDef->type == MacroType::FUNCTION_LIKE_MACRO) {
    expandedText = expandFunctionLikeMacro(*macroDef, arguments);
  } else {
    expandedText = macroDef->body.empty() ? " " : macroDef->body;
  }

  CodeBuffer::SectionID sectionID = codeBuffer.addSection(expandedText);
  codeCache.insert({macroDef->name, sectionID});

  return {MacroExpansionResult::Ok{sectionID, copy->offset()}, copy->offset()};
}

std::variant<std::vector<std::string>, Error>
PPImpl::parseFunctionLikeMacroArgumentList(
    ICopyableOffsetScanner& scanner, const MacroDefinition& macroDef,
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

std::string PPImpl::expandFunctionLikeMacro(
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

std::string PPImpl::parseFunctionLikeMacroArgument(
    ICopyableOffsetScanner& scanner,
    const MacroDefinition* const macroDefContext,
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
      const auto ident = parseIdentifier(*copyUnique(scanner));
      if (macroDefContext) {
        if (auto index = findIndexOfParameter(*macroDefContext, ident)) {
          output += argContext->operator[](*index);
          scanner.setOffset(scanner.offset() + ident.size());
          continue;
        }
      }

      auto [res, endOffset] =
          tryExpandingMacro(scanner, macroDefContext, argContext);
      CodeBuffer::Offset startOffset;
      std::size_t size;

      if (const auto ptr = std::get_if<Ok>(&res)) {
        startOffset = codeBuffer.section(ptr->sectionID);
        size = codeBuffer.sectionSize(ptr->sectionID);
        scanner.setOffset(ptr->endOffset);
      } else if (const auto ptr = std::get_if<Fail>(&res)) {
        startOffset = scanner.offset();
        size = endOffset - scanner.offset();
        scanner.setOffset(endOffset);
      } else {
        errOut.reportsError(std::get<Error>(std::move(res)));
        continue;
      }

      const auto startPos = codeBuffer.pos(startOffset);
      output.append(reinterpret_cast<const char*>(startPos), size);
    } else {
      output += scanner.get();
    }
  }

  return output;
}

void PPImpl::fastForwardToFirstOutputCharacter() {
  auto copy = copyUnique(scanner);
  skipSpacesAndComments(*copy);

  while (copy->peek() == '#') {
    scanner.setOffset(copy->offset());
    parseDirective();
    copy->setOffset(scanner.offset());
    skipSpacesAndComments(*copy);
  }
}

void PPImpl::parseDirective() {
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
std::variant<std::vector<std::string>, Error>
PPImpl::parseFunctionLikeMacroParameters(const std::string& macroName,
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