#include "preprocessor.h"

#include <cctype>
#include <cstdint>

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

PPScanner::PPScanner(PPImpl& pp, CodeBuffer& codeBuffer,
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
  const auto [ch, _] = readUTF32(_codeBuffer.pos(_offset));
  return isNewlineCharacter(ch);
}

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

/* PPImpl */

int PPImpl::get() {
  if (reachedEndOfInput()) {
    return EOF;
  }
  if (scanner.reachedEndOfInput()) {
    exitSection();
    return get();
  }
  if (identScanner) {
    const auto ch = scanner.get();
    if (scanner.offset() == identScanner->offset()) {
      identScanner = nullptr;
      exitFullyScannedSections();
    }
    return ch;
  }
  if (isSpace(scanner) || lookaheadMatches(scanner, "/*")) {
    skipSpacesAndComments(scanner);

    if (scanner.peek() == '#' && canParseDirectives) {
      parseDirective();
      return get();
    } else {
      exitFullyScannedSections();
      return ' ';
    }
  }
  if (scanner.peek() == '#' && canParseDirectives) {
    parseDirective();
    return get();
  }

  canParseDirectives = false;

  if (IdentStrLexer::isStartOfAIdentifier(scanner.peek())) {
    identScanner = std::make_unique<PPScanner>(scanner);
    std::string identStr = IdentStrLexer(*identScanner).scan();

    const auto macroDef = setOfMacroDefinitions.find(identStr);
    if (macroDef == setOfMacroDefinitions.end()) {
      // fallback and get each characters
      return get();
    }

    scanner.setOffset(identScanner->offset());
    identScanner = nullptr;

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

  const auto ch = scanner.get();
  exitFullyScannedSections();
  return ch;
}

int PPImpl::peek() const { return 0; }

bool PPImpl::reachedEndOfInput() const {
  return stackOfSectionID.empty() && ppReachedEnd(scanner.offset());
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
    skipSpacesAndComments(ppds, isDirectiveSpace);

    if (!IdentStrLexer::isStartOfAIdentifier(ppds.peek())) {
      errorMsg = hint = "macro names must be identifiers";
      goto fail;
    }

    std::string macroName = IdentStrLexer(ppds).scan();
    skipSpacesAndComments(ppds, isDirectiveSpace);
    std::string macroBody = readAll(ppds);
    setOfMacroDefinitions.insert(MacroDefinition(macroName, macroBody));

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
  const auto [ch1, len1] = readUTF32(_codeBuffer.pos(_offset));
  if (ch1 != '\\' || reachedEndOfInput()) return;
  _offset += len1;
  const auto [ch2, len2] = readUTF32(_codeBuffer.pos(_offset));
  if (ch2 != '\n') return;
  _offset += len1;
  skipBackslashReturn();
}
