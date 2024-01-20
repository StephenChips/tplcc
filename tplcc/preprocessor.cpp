#include "preprocessor.h"

#include <cctype>

bool isControlLineSpace(int ch) { return ch == ' ' || ch == '\t'; }
bool isNewlineCharacter(int ch) { return ch == '\r' || ch == '\n'; }

DirectiveContentScanner::DirectiveContentScanner(Preprocessor* pp) : pp(pp){};
int DirectiveContentScanner::get() { return *(pp->cursor++); }
int DirectiveContentScanner::peek() { return *pp->cursor; };
bool DirectiveContentScanner::reachedEndOfInput() {
  return !pp->reachedEndOfSection() && isNewlineCharacter(*pp->cursor);
}

SectionContentScanner::SectionContentScanner(Preprocessor& pp, PPCursor& _c)
    : pp(pp), cursor(_c) {}
int SectionContentScanner::get() { return pp.codeBuffer[cursor++]; }
int SectionContentScanner::peek() { return pp.codeBuffer[cursor]; }
bool SectionContentScanner::reachedEndOfInput() {
  return cursor >= pp.currentSectionEnd();
}

class IdentStrLexer {
  IGetPeekOnlyScanner& scanner;

 public:
  IdentStrLexer(IGetPeekOnlyScanner& scanner) : scanner(scanner){};
  std::string scan();

  static bool isStartOfAIdentifier(const char ch);
};

bool IdentStrLexer::isStartOfAIdentifier(const char ch) {
  return ch == '_' || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z';
}

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

bool Preprocessor::isStartOfAComment(const PPCursor& cursor) {
  return lookaheadMatches(cursor, "/*");
}

bool Preprocessor::isSpace(const PPCursor& cursor, bool isInsideControlLine) {
  return isInsideControlLine ? isControlLineSpace(*cursor) : std::isspace(*cursor);
}
void Preprocessor::skipComment(PPCursor& cursor) {
  cursor++;
  cursor++;

  while (!reachedEndOfSection() && !lookaheadMatches(cursor, "*/")) {
    cursor++;
  }

  cursor++;
  cursor++;
}

void Preprocessor::skipWhitespacesAndComments(PPCursor& cursor,
                                              bool isInsideControlLine) {
  while (!reachedEndOfSection()) {
    if (isSpace(cursor, isInsideControlLine)) {
      cursor++;
    } else if (isStartOfAComment(cursor)) {
      skipComment(cursor);
    } else {
      break;
    }
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
  if (identCursor) {
    const char ch = *(cursor++);
    if (cursor == identCursor) {
      identCursor = std::nullopt;
    }
    return ch;
  }
  if (isStartOfASpaceOrAComment(cursor, false)) {
    skipWhitespacesAndComments(cursor, false);
    return ' ';
  }
  if (*cursor == '#') {
    parseControlLine();
    return get();
  }

  if (IdentStrLexer::isStartOfAIdentifier(*cursor)) {
    identCursor = cursor;
    SectionContentScanner scs(*this, *identCursor);
    std::string identStr = IdentStrLexer(scs).scan();

    const auto macroDef = setOfMacroDefinitions.find(identStr);
    if (macroDef == setOfMacroDefinitions.end()) {
      // fallback and get each characters
      return get();
    }

    cursor = *identCursor;
    identCursor = std::nullopt;

    CodeBuffer::SectionID sectionID;
    std::string macroBody;
    const auto iter = codeCache.find(macroDef->name);
    if (iter == codeCache.end()) {
      sectionID = codeBuffer.addSection(macroDef->body);
      codeCache.insert({macroDef->name, sectionID});
    } else {
      sectionID = iter->second;
    }

    enterSection(sectionID);

    return get();
  }

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

  return stackOfSectionID.empty() && cursor == codeBuffer.sectionEnd(0);
}

std::uint32_t Preprocessor::offset() { return cursor; }

std::vector<MacroExpansionRecord>& Preprocessor::macroExpansionRecords() {
  return vectorOfMacroExpansion;
}

void Preprocessor::fastForwardToFirstOutputCharacter() {
  while (true) {
    skipSpaces(false);

    if (*cursor == '#') {
      parseControlLine();
    } else if (isStartOfASpaceOrAComment(cursor, false)) {
      skipWhitespacesAndComments(cursor, false);
    } else {
      break;
    }
  }
}

void Preprocessor::parseControlLine() {
  using namespace std::literals::string_literals;
  const auto startOffset = currentSection();
  std::string errorMsg;
  std::string hint;

  cursor++;  // ignore the leading #
  skipSpaces(true);

  std::string directiveName;
  while (!reachedEndOfInput() && !isSpace(cursor, true)) {
    directiveName.push_back(*cursor);
    cursor++;
  }

  if (directiveName == "define") {
    skipWhitespacesAndComments(cursor, true);
    if (!isFirstCharOfIdentifier(*cursor)) {
      errorMsg = hint = "macro names must be identifiers";
      goto fail;
    }
    std::string macroName = IdentStrLexer(dcs).scan();
    skipWhitespacesAndComments(cursor, true);
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
  const CodeBuffer::Offset endOffset = cursor;
  const auto range = std::make_tuple(startOffset, endOffset);
  const auto error = Error({startOffset, endOffset}, errorMsg, errorMsg);
  errOut.reportsError(error);
}

void Preprocessor::skipNewline() {
  if (*cursor == '\r') cursor++;
  if (*cursor == '\n') cursor++;
}

void Preprocessor::skipSpaces(bool isInsideControlLine) {
  while (isSpace(cursor, isInsideControlLine)) {
    cursor++;
  }
}

void Preprocessor::skipControlLine() {
  while (!reachedEndOfLine() && !isNewlineCharacter(*cursor)) {
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

    skipWhitespacesAndComments(cursor, true);

    if (!reachedEndOfLine()) {
      output.push_back(' ');
    }
  }

  return output;
}

bool Preprocessor::reachedEndOfSection(const PPCursor& cursor) {
  const auto id = currentSectionID();
  return cursor == codeBuffer.sectionEnd(id);
}

void Preprocessor::enterSection(CodeBuffer::SectionID id) {
  stackOfSectionID.push_back(id);
  stackOfStoredOffsets.push_back(cursor);
  cursor = codeBuffer.section(id);
}

void Preprocessor::exitSection() {
  stackOfSectionID.pop_back();
  if (!stackOfStoredOffsets.empty()) {
    cursor = stackOfStoredOffsets.back();
    stackOfStoredOffsets.pop_back();
  }
}

bool Preprocessor::isStartOfASpaceOrAComment(const PPCursor& cursor, bool isInsideControlLine) {
  return isSpace(cursor, isInsideControlLine) || lookaheadMatches(cursor, "/*");
}

CodeBuffer::SectionID Preprocessor::currentSectionID() {
  return stackOfSectionID.empty() ? 0 : stackOfSectionID.back();
}

CodeBuffer::Offset Preprocessor::currentSection() {
  return codeBuffer.section(currentSectionID());
}

CodeBuffer::Offset Preprocessor::currentSectionEnd() {
  return codeBuffer.sectionEnd(currentSectionID());
}

bool Preprocessor::reachedEndOfLine(const PPCursor& c) {
  return reachedEndOfSection(c) || isNewlineCharacter(*c);
}

bool Preprocessor::lookaheadMatches(PPCursor cursor, const std::string& s) {
  for (const char ch : s) {
    if (*cursor == EOF || *cursor != ch) return false;
    cursor++;
  }

  return true;
}

PPCursor::PPCursor(Preprocessor& pp, CodeBuffer::Offset initialCursorOffset)
    : pp(pp), cursor(initialCursorOffset) {
  while (cursor < pp.currentSectionEnd() - 1 && pp.codeBuffer[cursor] == '\\' &&
         pp.codeBuffer[cursor + 1] == '\n') {
    cursor += 2;
  }
}

PPCursor& PPCursor::operator++() {
  const auto sectionID = pp.currentSectionID();
  const auto sectionEnd = pp.codeBuffer.sectionEnd(sectionID);

  if (cursor == sectionEnd) return *this;

  cursor++;
  while (cursor < sectionEnd - 1 && pp.codeBuffer[cursor] == '\\' &&
         pp.codeBuffer[cursor + 1] == '\n') {
    cursor += 2;
  }

  return *this;
}

PPCursor PPCursor::operator++(int) {
  PPCursor copy(*this);
  this->operator++();
  return copy;
}

char PPCursor::currentChar() { return pp.codeBuffer[cursor]; }

const char PPCursor::operator*() const { return pp.codeBuffer[cursor]; }

PPCursor::operator CodeBuffer::Offset() { return cursor; }

std::strong_ordering operator<=>(const PPCursor& lhs, CodeBuffer::Offset rhs) {
  return lhs.cursor <=> rhs;
}
std::strong_ordering operator<=>(CodeBuffer::Offset lhs, const PPCursor& rhs) {
  return lhs <=> rhs.cursor;
}
std::strong_ordering operator<=>(const PPCursor& lhs, const PPCursor& rhs) {
  return lhs.cursor <=> rhs.cursor;
}
bool operator==(const PPCursor& lhs, CodeBuffer::Offset rhs) {
  return lhs.cursor == rhs;
}
bool operator==(CodeBuffer::Offset lhs, const PPCursor& rhs) {
  return lhs == rhs.cursor;
}
bool operator==(const PPCursor& lhs, const PPCursor& rhs) {
  return lhs.cursor == rhs.cursor;
}
