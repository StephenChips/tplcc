#include "preprocessor.h"

#include <cctype>

bool isControlLineSpace(const char ch) { return ch == ' ' || ch == '\t'; }
bool isNewlineCharacter(const char ch) { return ch == '\r' || ch == '\n'; }

DirectiveContentScanner::DirectiveContentScanner(Preprocessor* pp) : pp(pp){};
int DirectiveContentScanner::get() { return *(pp->cursor++); }
int DirectiveContentScanner::peek() { return *pp->cursor; };
bool DirectiveContentScanner::reachedEndOfInput() {
  return !pp->reachedEndOfSection() && isNewlineCharacter(*pp->cursor);
}

class IdentStrLexer {
  IGetPeekOnlyScanner& scanner;

 public:
  IdentStrLexer(IGetPeekOnlyScanner& scanner) : scanner(scanner){};
  std::string scan();
};

std::string IdentStrLexer::scan() {
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

void Preprocessor::skipWhitespaces() {
  while (!reachedEndOfSection() && std::isspace(*cursor)) {
    cursor++;
  }
}

int Preprocessor::get() {
  if (reachedEndOfInput()) {
    return EOF;
  }
  if (reachedEndOfSection()) {
    exitSection();
    return get();
  }
  if (endOfNonMacroPPToken) {
    const char ch = *(cursor++);
    if (cursor == endOfNonMacroPPToken) {
      endOfNonMacroPPToken = nullptr;
    }
    return ch;
  }
  if (std::isspace(peek())) {
    skipWhitespaces();
    return ' ';
  }
  if (peek() == '#') {
    parseControlLine();
    return get();
  }

  std::string preprocessingToken;
  while (!reachedEndOfLine() && !std::isspace(*cursor)) {
    preprocessingToken.push_back(*cursor);
    cursor++;
  }

  const auto macroDef = setOfMacroDefinitions.find(preprocessingToken);
  if (macroDef == setOfMacroDefinitions.end()) {
    // fallback and get each characters
    endOfNonMacroPPToken = cursor;
    cursor -= preprocessingToken.size();
    return get();
  }

  const auto iter = codeCache.find(macroDef->name);
  auto sectionID = iter != codeCache.end()
                       ? iter->second
                       : codeBuffer.addSection(macroDef->body);

  enterSection(sectionID);

  return *(cursor++);
}

int Preprocessor::peek() { return *cursor; }

std::string Preprocessor::peekN(size_t n) { return ""; }

void Preprocessor::ignore() { get(); }

void Preprocessor::ignoreN(size_t n) {
  for (size_t i = 0; i < n; i++) {
    get();
  }
}

bool Preprocessor::reachedEndOfInput() {
  while (!stackOfSectionID.empty() && reachedEndOfSection()) {
    exitSection();
  }

  return stackOfSectionID.empty() && cursor == codeBuffer.secitonEnd(0);
}

std::uint32_t Preprocessor::offset() { return std::uint32_t(); }

std::vector<MacroExpansionRecord>& Preprocessor::macroExpansionRecords() {
  return vectorOfMacroExpansion;
}

void Preprocessor::parseControlLine() {
  using namespace std::literals::string_literals;
  const auto startOffset = codeBuffer.offset(currentSection());
  std::string errorMsg;
  std::string hint;

  cursor++;  // ignore the leading #
  skipControlLineSpaces();

  std::string directiveName;
  while (!reachedEndOfInput() && !std::isspace(*cursor)) {
    directiveName.push_back(*cursor);
    cursor++;
  }

  if (directiveName == "define") {
    skipControlLineSpaces();
    if (!isFirstCharOfIdentifier(*cursor)) {
      errorMsg = hint = "macro names must be identifiers";
      goto fail;
    }
    std::string macroName = IdentStrLexer(dcs).scan();
    skipControlLineSpaces();
    std::string macroBody = scanControlLine();
    skipNewline();
    setOfMacroDefinitions.insert(MacroDefinition(macroName, macroBody));
  } else {
    errorMsg = hint = "Unknown preprocessing directive "s + directiveName;
    goto fail;
  }

  return;

fail:
  skipControlLine();
  const auto endOffset = codeBuffer.offset(cursor);
  const auto error = Error({startOffset, endOffset}, errorMsg, errorMsg);
  errOut.reportsError(error);
}

void Preprocessor::skipNewline() {
  if (*cursor == '\r') cursor++;
  if (*cursor == '\n') cursor++;
}

void Preprocessor::skipControlLineSpaces() {
  while (isControlLineSpace(*cursor)) {
    cursor++;
  }
}

void Preprocessor::skipControlLine() {
  while (!reachedEndOfLine() && !isNewlineCharacter(*cursor)) {
    cursor++;
  }
}

void Preprocessor::skipControlLineWhitespaces() {
  while (!reachedEndOfLine() && isControlLineSpace(*cursor)) {
    cursor++;
  }
}

std::string Preprocessor::scanControlLine() {
  std::string output;

  while (!reachedEndOfLine()) {
    while (!reachedEndOfLine() && !isControlLineSpace(*cursor)) {
      output.push_back(*cursor);
      cursor++;
    }

    skipControlLineWhitespaces();

    if (!reachedEndOfLine()) {
      output.push_back(' ');
    }
  }

  return output;
}

bool Preprocessor::reachedEndOfSection() {
  const auto id = currentSectionID();
  return cursor == codeBuffer.section(id) + codeBuffer.sectionSize(id);
}

void Preprocessor::enterSection(CodeBuffer::SectionID id) {
  stackOfSectionID.push_back(id);
  stackOfStoredCursor.push_back(cursor);
  cursor = codeBuffer.section(id);
}

void Preprocessor::exitSection() {
  stackOfSectionID.pop_back();
  if (!stackOfStoredCursor.empty()) {
    cursor = stackOfStoredCursor.back();
    stackOfStoredCursor.pop_back();
  }
}

CodeBuffer::SectionID Preprocessor::currentSectionID() {
  return stackOfSectionID.empty() ? 0 : stackOfSectionID.back();
}

const char* Preprocessor::currentSection() {
  return codeBuffer.section(currentSectionID());
}

bool Preprocessor::reachedEndOfLine() {
  return reachedEndOfSection() || isNewlineCharacter(*cursor);
}
