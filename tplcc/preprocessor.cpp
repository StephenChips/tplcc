#include "preprocessor.h"

#include <cctype>
#include <cstdint>

bool isDirectiveSpace(int ch) { return ch == ' ' || ch == '\t'; }
bool isNewlineCharacter(int ch) { return ch == '\r' || ch == '\n'; }

DirectiveContentScanner::DirectiveContentScanner(Preprocessor* pp) : pp(pp){};
int DirectiveContentScanner::get() {
  const char ch = pp->cursor.currentChar();
  pp->cursor.next();
  return ch;
}
int DirectiveContentScanner::peek() { return pp->cursor.currentChar(); };
bool DirectiveContentScanner::reachedEndOfInput() {
  return !pp->reachedEndOfSection(pp->cursor) &&
         isNewlineCharacter(pp->cursor.currentChar());
}

SectionContentScanner::SectionContentScanner(Preprocessor& pp, PPCursor& _c)
    : pp(pp), cursor(_c) {}
int SectionContentScanner::get() {
  const auto ch = cursor.currentChar();
  cursor.next();
  return ch;
}
int SectionContentScanner::peek() { return cursor.currentChar(); }
bool SectionContentScanner::reachedEndOfInput() {
  return cursor.offset() >= pp.currentSectionEnd();
}

class IdentStrLexer {
  IBaseScanner& scanner;

 public:
  IdentStrLexer(IBaseScanner& scanner) : scanner(scanner){};
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

bool Preprocessor::isSpace(const PPCursor& cursor, bool isInsideDirective) {
  if (cursor.currentChar() > std::numeric_limits<unsigned char>::max())
    return false;
  if (isInsideDirective) return isDirectiveSpace(cursor.currentChar());
  return std::isspace(cursor.currentChar());
}
void Preprocessor::skipComment(PPCursor& cursor) {
  cursor.next().next();

  while (!reachedEndOfSection(cursor) && !lookaheadMatches(cursor, "*/")) {
    cursor.next();
  }

  cursor.next().next();
}

void Preprocessor::skipWhitespacesAndComments(PPCursor& cursor,
                                              bool isInsideDirective) {
  while (!reachedEndOfSection(cursor)) {
    if (isSpace(cursor, isInsideDirective)) {
      cursor.next();
    } else if (isStartOfAComment(cursor)) {
      skipComment(cursor);
    } else {
      break;
    }
  }
}

int Preprocessor::get() {
  if (reachedEndOfInput(cursor)) {
    return EOF;
  }
  if (reachedEndOfSection(cursor)) {
    exitSection();
    return get();
  }
  if (identCursor) {
    const char ch = cursor.currentChar();
    cursor.next();
    if (cursor.offset() == identCursor->offset()) {
      identCursor = std::nullopt;
    }
    return ch;
  }
  if (isStartOfASpaceOrAComment(cursor, false)) {
    skipWhitespacesAndComments(cursor, false);
    return ' ';
  }
  if (cursor.currentChar() == '#' && enabledProcessDirectives) {
    parseDirective(cursor);
    return get();
  }

  enabledProcessDirectives = false;

  if (IdentStrLexer::isStartOfAIdentifier(cursor.currentChar())) {
    identCursor = cursor;
    SectionContentScanner scs(*this, *identCursor);
    std::string identStr = IdentStrLexer(scs).scan();

    const auto macroDef = setOfMacroDefinitions.find(identStr);
    if (macroDef == setOfMacroDefinitions.end()) {
      // fallback and get each characters
      return get();
    }

    cursor.setOffset(identCursor->offset());
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

  const auto ch = cursor.currentChar();
  cursor.next();
  return ch;
}

int Preprocessor::peek() { return cursor.currentChar(); }

bool Preprocessor::reachedEndOfInput() { return reachedEndOfInput(cursor); }

bool Preprocessor::reachedEndOfInput(const PPCursor& cursor) {
  while (!stackOfSectionID.empty() && reachedEndOfSection(cursor)) {
    exitSection();
  }

  return stackOfSectionID.empty() &&
         cursor.offset() == codeBuffer.sectionEnd(0);
}

std::vector<MacroExpansionRecord>& Preprocessor::macroExpansionRecords() {
  return vectorOfMacroExpansion;
}

void Preprocessor::fastForwardToFirstOutputCharacter(PPCursor& cursor) {
  while (true) {
    skipSpaces(false);

    if (cursor.currentChar() == '#') {
      parseDirective(cursor);
    } else if (isStartOfASpaceOrAComment(cursor, false)) {
      skipWhitespacesAndComments(cursor, false);
    } else {
      break;
    }
  }
}

void Preprocessor::parseDirective(PPCursor& cursor) {
  using namespace std::literals::string_literals;
  const auto startOffset = currentSection();
  std::string errorMsg;
  std::string hint;

  cursor.next();  // ignore the leading #
  skipSpaces(true);

  std::string directiveName;
  while (!reachedEndOfSection(cursor) && !reachedEndOfLine(cursor) &&
         !isSpace(cursor, true)) {
    directiveName.push_back(cursor.currentChar());
    cursor.next();
  }

  if (directiveName == "") {
    skipNewline(cursor);
    return;
  }

  if (directiveName == "define") {
    skipWhitespacesAndComments(cursor, true);

    if (!isFirstCharOfIdentifier(cursor.currentChar())) {
      errorMsg = hint = "macro names must be identifiers";
      goto fail;
    }
    std::string macroName = IdentStrLexer(dcs).scan();
    skipWhitespacesAndComments(cursor, true);
    std::string macroBody = scanDirective(cursor);
    skipNewline(cursor);
    setOfMacroDefinitions.insert(MacroDefinition(macroName, macroBody));
  } else {
    errorMsg = hint = "Unknown preprocessing directive "s + directiveName;
    goto fail;
  }

  return;

fail:
  skipDirective(cursor);
  const CodeBuffer::Offset endOffset = cursor.offset();
  const auto range = std::make_tuple(startOffset, endOffset);
  const auto error = Error({startOffset, endOffset}, errorMsg, errorMsg);
  errOut.reportsError(error);
}

void Preprocessor::skipNewline(PPCursor& cursor) {
  if (cursor.currentChar() == '\r') cursor.next();
  if (cursor.currentChar() == '\n') cursor.next();
}

void Preprocessor::skipSpaces(bool isInsideDirective) {
  while (isSpace(cursor, isInsideDirective)) {
    if (!isInsideDirective && isNewlineCharacter(cursor.currentChar())) {
      enabledProcessDirectives = true;
    }
    cursor.next();
  }
}

void Preprocessor::skipDirective(PPCursor& cursor) {
  while (!reachedEndOfLine(cursor) &&
         !isNewlineCharacter(cursor.currentChar())) {
    cursor.next();
  }
}

std::string Preprocessor::scanDirective(PPCursor& cursor) {
  std::string output;

  while (!reachedEndOfLine(cursor)) {
    while (!reachedEndOfLine(cursor) &&
           !isDirectiveSpace(cursor.currentChar())) {
      output.push_back(cursor.currentChar());
      cursor.next();
    }

    skipWhitespacesAndComments(cursor, true);

    if (!reachedEndOfLine(cursor)) {
      output.push_back(' ');
    }
  }

  return output;
}

bool Preprocessor::reachedEndOfSection(const PPCursor& cursor) {
  const auto id = currentSectionID();
  return cursor.offset() == codeBuffer.sectionEnd(id);
}

void Preprocessor::enterSection(CodeBuffer::SectionID id) {
  stackOfSectionID.push_back(id);
  stackOfStoredOffsets.push_back(cursor.offset());
  cursor.setOffset(codeBuffer.section(id));
}

void Preprocessor::exitSection() {
  stackOfSectionID.pop_back();
  if (!stackOfStoredOffsets.empty()) {
    cursor.setOffset(stackOfStoredOffsets.back());
    stackOfStoredOffsets.pop_back();
  }
}

bool Preprocessor::isStartOfASpaceOrAComment(const PPCursor& cursor,
                                             bool isInsideDirective) {
  return isSpace(cursor, isInsideDirective) || lookaheadMatches(cursor, "/*");
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
  return reachedEndOfSection(c) || isNewlineCharacter(c.currentChar());
}

bool Preprocessor::lookaheadMatches(const ICursor& cursor,
                                    const std::string& s) {
  auto copy = cursor.clone();
  for (const char ch : s) {
    if (copy->currentChar() == EOF || copy->currentChar() != ch) return false;
    copy->next();
  }

  return true;
}

PPCursor::PPCursor(Preprocessor* pp, ICursor* cursor) noexcept
    : pp(pp), cursor(cursor) {
  while (cursor->offset() < pp->currentSectionEnd() - 1 &&
         pp->lookaheadMatches(*cursor, "\\\n")) {
    cursor->next().next();
  }
}

std::uint32_t PPCursor::currentChar() const { return cursor->currentChar(); }

PPCursor& PPCursor::next() {
  const auto sectionID = pp->currentSectionID();
  const auto sectionEnd = pp->codeBuffer.sectionEnd(sectionID);

  if (cursor->offset() == sectionEnd) return *this;

  cursor->next();
  while (cursor->offset() < sectionEnd - 1 &&
         pp->lookaheadMatches(*cursor, "\\\n")) {
    cursor->next().next();
  }

  return *this;
}

PPCursor& PPCursor::setOffset(CodeBuffer::Offset newOffset) {
  cursor->setOffset(newOffset);
  return *this;
}
CodeBuffer::Offset PPCursor::offset() const { return cursor->offset(); }
