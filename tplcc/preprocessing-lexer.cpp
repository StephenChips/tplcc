#include "preprocessing-lexer.h"

#include <cassert>
#include <limits>
#include <string>

static constexpr char32_t MAX_UNICODE_CODEPOINT = 0x10FFF;

// The MSVC's std::isspace will throw a runtime error when we pass a
// codepoint that is larger than 255. We have to write our own version of
// isspace here to avoid this error.
static bool isSpace(char32_t ch) {
  return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
         ch == '\v';
}
static bool isNonNewlineSpace(char32_t ch) {
  return ch == ' ' || ch == '\f' || ch == '\t' || ch == '\v';
}
static bool isNewlineCharacter(char32_t ch) { return ch == '\r' || ch == '\n'; }

static bool isOctDigit(char32_t ch) { return ch >= '0' && ch <= '7'; }

std::string& getTokenText(PreprocessingToken& token) {
  return std::visit([](auto& t) -> std::string& { return t.text; }, token);
}

std::string& getTokenText(Token& token) {
  return std::visit([](auto& t) -> std::string& { return t.text; }, token);
}

#define X(PascalName, name) {KeywordKind::PascalName, #name},
constexpr std::array<Keyword, keywordKindCount> keywordArray{
    {KEYWORDS_X_MACRO_LIST}};
#undef X

#define X(name, str) Punctuator{PunctuatorKind::name, str},

static std::vector<Punctuator> createPunctuatorList() {
  std::vector<Punctuator> list = {
      {PUNCTUATORS_X_MACRO_LIST DIGRAPH_PUNCTUATORS_X_MACRO_LIST}};

  std::sort(list.begin(), list.end(), [](Punctuator first, Punctuator second) {
    if (first.text.size() != second.text.size()) {
      return first.text.size() > second.text.size();
    } else {
      return first.text > second.text;
    }
  });

  return list;
}

const std::vector<Punctuator> listOfEveryPunctuators = createPunctuatorList();

#undef X

DecodeUTF8Result decodeUTF8(const char* buffer) {
  char32_t ch;
  unsigned long len;

  auto buf = reinterpret_cast<const unsigned char*>(buffer);

  if (buf[0] >> 7 == 0) {
    len = 1;
    ch = buf[0];
  } else if (buf[0] >> 5 == 0b00000110) {
    len = 2;
    ch = (buf[0] & 0b00011111) << 6;
    ch |= buf[1] & 0b00111111;
  } else if (buf[0] >> 4 == 0b00001110) {
    len = 3;
    ch = (buf[0] & 0b00001111) << 12;
    ch |= (buf[1] & 0b00111111) << 6;
    ch |= buf[2] & 0b00111111;
  } else if (buf[0] >> 3 == 0b00011110) {
    len = 4;
    ch = (buf[0] & 0b00000111) << 18;
    ch |= (buf[1] & 0b00111111) << 12;
    ch |= (buf[2] & 0b00111111) << 6;
    ch |= buf[3] & 0b00111111;
  }

  return {ch, len};
}

void encodeUTF8(std::string& str, char32_t cp) {
  if (cp <= 0x7F) {
    str.push_back(static_cast<char8_t>(cp));
  } else if (cp <= 0x7FF) {
    str.push_back(static_cast<char8_t>(0xC0 | (cp >> 6)));
    str.push_back(static_cast<char8_t>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    str.push_back(static_cast<char8_t>(0xE0 | (cp >> 12)));
    str.push_back(static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F)));
    str.push_back(static_cast<char8_t>(0x80 | (cp & 0x3F)));
  } else if (cp <= 0x10FFFF) {
    str.push_back(static_cast<char8_t>(0xF0 | (cp >> 18)));
    str.push_back(static_cast<char8_t>(0x80 | ((cp >> 12) & 0x3F)));
    str.push_back(static_cast<char8_t>(0x80 | ((cp >> 6) & 0x3F)));
    str.push_back(static_cast<char8_t>(0x80 | (cp & 0x3F)));
  }
}

ScanSection& PreprocessingLexer::getScanSection(ScanStackFrame& frame) {
  return std::visit([](auto& frame) -> ScanSection& { return frame.section; },
                    frame);
}

bool PreprocessingLexer::popFrame() {
  if (scanStack.empty()) return false;

  scanStack.pop_back();

  while (!scanStack.empty()) {
    skipSpacesAndComments(scanStack.back());
    ScanSection& section = getScanSection(scanStack.back());
    if (section.offset < section.text.size()) break;
    scanStack.pop_back();
  }

  return true;
}

bool PreprocessingLexer::pushFileFrame(std::string text) {
  scanStack.emplace_back(FileFrame{});
  auto& frame = std::get<FileFrame>(scanStack.back());

  frame.buffer = std::move(text);
  frame.section.offset = 0;
  frame.section.text = frame.buffer;
  frame.isAtLineStart = true;

  skipSpacesAndComments(frame);
  if (frame.section.offset == frame.section.text.size()) {
    scanStack.pop_back();
    return false;
  }

  return true;
}

bool PreprocessingLexer::pushMacroFrame(const MacroDef& def,
                                        std::vector<std::string> arguments) {
  scanStack.emplace_back(MacroFrame{});
  auto& frame = std::get<MacroFrame>(scanStack.back());

  frame.def = &def;
  frame.arguments = std::move(arguments);
  frame.buffer =
      HashOperatorEvaluator(*this, *frame.def, frame.arguments).evaluate();
  frame.section = ScanSection{frame.buffer, 0};

  skipSpacesAndComments(frame);
  if (frame.section.offset == frame.section.text.size()) {
    scanStack.pop_back();
    return false;
  }

  return true;
}

std::optional<Punctuator> PreprocessingLexer::scanPunctuator(
    ScanSection& section) {
  if (section.offset == section.text.size()) return std::nullopt;

  for (Punctuator punctuator : listOfEveryPunctuators) {
    size_t offset = section.offset;
    bool isMatch = true;

    for (char ch : punctuator.text) {
      if (offset >= section.text.size() ||
          getCharAtOffset(section, offset) != ch) {
        isMatch = false;
        break;
      }
    }

    if (isMatch) {
      std::string_view text = slice(section.text, section.offset, offset);
      section.offset = offset;
      return Punctuator{punctuator.kind, std::string{text}};
    }
  }

  return std::nullopt;
}

char32_t PreprocessingLexer::getChar(ScanSection& section,
                                     bool willSkipBackslashNewlines) {
  return getCharAtOffset(section, section.offset, willSkipBackslashNewlines);
}

char32_t PreprocessingLexer::getCharAtOffset(const ScanSection& section,
                                             size_t& offset,
                                             bool willSkipBackslashNewlines) {
  // A valid cursor should never points to a '\' '\n' sequence,
  // so we don't need to skip it before decoding a character.
  auto [ch, charlen] = decodeUTF8(section.text.data() + offset);
  offset += charlen;
  if (willSkipBackslashNewlines) {
    skipBackslashNewlinesAtOffset(section, offset);
  }
  return ch;
}

char32_t PreprocessingLexer::peekChar(const ScanSection& section,
                                      size_t* endOffset,
                                      bool willSkipBackslashNewlines) {
  return peekCharAtOffset(section, section.offset, endOffset,
                          willSkipBackslashNewlines);
}

char32_t PreprocessingLexer::peekCharAtOffset(const ScanSection& section,
                                              size_t offset, size_t* endOffset,
                                              bool willSkipBackslashNewlines) {
  char32_t ch = getCharAtOffset(section, offset, willSkipBackslashNewlines);
  if (endOffset) *endOffset = offset;
  return ch;
}

PreprocessingLexer::ResultOfScanPPNumberText
PreprocessingLexer::scanPPNumberText(ScanSection& section) {
  size_t startOffset = section.offset;

  bool hasInvalidUCN = false;

  while (section.offset < section.text.size()) {
    size_t endOffset;
    char32_t ch = peekChar(section, &endOffset);
    if (std::isdigit(ch)) {
      section.offset = endOffset;
    } else if (ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P') {
      /**
       * Note: `sign` (`+` / `-`) is optional for `pp-number`. If there
       * isn't a sign after e/E/p/P, the second rule of the right-recursive
       * grammar is applied.
       */
      section.offset = endOffset;
      if (endOffset >= section.text.size()) break;
      ch = peekChar(section, &endOffset);
      if (ch == '+' || ch == '-') {
        section.offset = endOffset;
      }
    } else if (ch == '.') {
      section.offset = endOffset;
    } else if (isMatchIdentifierNonDigitCharacter(section, &endOffset)) {
      section.offset = endOffset;
    } else if (ch == '\\') {
      if (endOffset >= section.text.size()) break;
      ch = peekCharAtOffset(section, endOffset, &endOffset);
      if (ch == 'u' || ch == 'U') {
        size_t ucnStart = section.offset;
        section.offset = endOffset;
        bool isValidUCN =
            skipUniversalCharacterNameHexQuad(section, ch, ucnStart);
        if (!isValidUCN) hasInvalidUCN = true;
      }
    } else {
      break;
    }
  }

  return {slice(section.text, startOffset, section.offset), hasInvalidUCN};
}

/**
 * This function is very similar to the code in `getToken` that
 * parses tokens, yet they are not exactly the same. For example, the function
 * can optionally parse header names, while `getToken` does not support it.
 *
 * TODO: we may abstract the shared logics to a function/class for parsing
 * processing token. However there is no need to be rush until more
 * differences become clear.
 */
PreprocessingToken PreprocessingLexer::scanPreprocessingTokenInsideDreictive(
    ScanSection& section, bool enableParseHeaderName) {
  size_t nextOffset;
  char32_t ch = peekChar(section, &nextOffset);

  // TODO implement header name parsing
  //
  // This is only needed when handling `#include` directives. The relevant
  // logics will be added when support for `#include` directive is added.

  if (std::isdigit(ch) ||
      ch == '.' && section.offset < section.text.size() &&
          std::isdigit(peekCharAtOffset(section, nextOffset, &nextOffset))) {
    size_t startOffset = section.offset;
    ResultOfScanPPNumberText result = scanPPNumberText(section);

    if (result.hasInvalidUCN) {
      diagnostics.report({DiagnosticLevel::Error,
                          {0, 0},
                          "Found invalid universal character name"});
      return InvalidToken{std::string{result.text}};
    } else {
      return PPNumber{std::string{result.text}};
    }
  } else if (std::optional<Punctuator> punctuator = scanPunctuator(section)) {
    return *punctuator;
  } else if (isMatchIdentifierNonDigitCharacter(section, &nextOffset)) {
    size_t startOffset = section.offset;

    do {
      section.offset = nextOffset;
    } while (section.offset < section.text.size() &&
             isMatchIdentifierCharacter(section, &nextOffset));

    std::string_view text = slice(section.text, startOffset, section.offset);

    // TODO support u, u8 and U string and character constant.
    if (section.offset < section.text.size() && text == "L") {
      ch = peekChar(section, &nextOffset);
      if (ch == '"' || ch == '\'') {
        bool isValid = skipQuotedLiteralContent(section, EncodingPrefix::L);
        text = slice(section.text, startOffset, section.offset);

        if (ch == '"') {
          return StringLiteral{isValid, std::string{text}};
        } else {
          return CharacterConstant{isValid, std::string{text}};
        }
      } else {
        return Identifier{std::string{text}};
      }
    } else {
      auto it = std::ranges::find_if(
          keywordArray, [&](Keyword keyword) { return keyword.text == text; });

      if (it != keywordArray.end()) {
        return Keyword{it->kind, std::string{text}};
      } else {
        return Identifier{std::string{text}};
      }
    }
  } else if (ch == '"' || ch == '\'') {
    size_t startOffset = section.offset;
    bool isValid = skipQuotedLiteralContent(section, EncodingPrefix::None);
    std::string_view text = slice(section.text, startOffset, section.offset);

    if (ch == '"') {
      return StringLiteral{isValid, std::string{text}};
    } else {
      return CharacterConstant{isValid, std::string{text}};
    }
  } else {
    size_t startOffset = section.offset;
    getChar(section);
    std::string_view text = slice(section.text, startOffset, section.offset);
    return InvalidToken{std::string{text}};
  }
}

bool PreprocessingLexer::isMatchNewline(const ScanSection& section,
                                        size_t* endOffset) {
  bool isMatch = false;
  size_t offset = section.offset;
  size_t nextOffset;

  char32_t ch = peekCharAtOffset(section, offset, &nextOffset);
  if (ch == '\r') {
    isMatch = true;
    offset = nextOffset;
    if (offset < section.text.size() &&
        peekCharAtOffset(section, offset, &nextOffset) == '\n') {
      offset = nextOffset;
    }
  } else if (ch == '\n') {
    isMatch = true;
    offset = nextOffset;
  }

  if (endOffset) {
    *endOffset = offset;
  }

  return isMatch;
}

bool PreprocessingLexer::isMatchSpace(const ScanSection& section,
                                      size_t* endOffset) {
  size_t ch = peekChar(section, endOffset);
  return isSpace(ch);
}

bool PreprocessingLexer::isMatchNonNewlineSpace(const ScanSection& section,
                                                size_t* endOffset) {
  size_t ch = peekChar(section, endOffset);
  return isNonNewlineSpace(ch);
}

bool PreprocessingLexer::isMatchString(const ScanSection& section,
                                       const char* s, size_t* endOffset) {
  size_t offset = section.offset;
  while (*s) {
    if (offset >= section.text.size()) break;
    if (getCharAtOffset(section, offset) != *s) break;
    s++;
  }
  if (*s != '\0') return false;
  if (endOffset) *endOffset = offset;
  return true;
}

void PreprocessingLexer::skipBackslashNewlines(ScanSection& section) {
  skipBackslashNewlinesAtOffset(section, section.offset);
}

void PreprocessingLexer::skipBackslashNewlinesAtOffset(
    const ScanSection& section, size_t& offset) {
  size_t nextOffset = offset;
  while (nextOffset < section.text.size()) {
    char32_t ch1 = getCharAtOffset(section, nextOffset, false);
    if (ch1 != '\\' || nextOffset >= section.text.size()) break;
    char32_t ch2 = getCharAtOffset(section, nextOffset, false);
    if (ch2 != '\n') break;
    offset = nextOffset;
  }
}

void PreprocessingLexer::skipDirectiveSpacesAndComments(ScanSection& section) {
  size_t endOffset;
  while (section.offset < section.text.size()) {
    if (isMatchNewline(section, &endOffset)) {
      section.offset = endOffset;
      break;
    } else if (isMatchNonNewlineSpace(section, &endOffset)) {
      /**
       * C99 spec (section 6.10) states that the only whtiespace characters that
       * shall appears between two preprocessing token within a directive are
       * space ' '  and horizontal tab '\t', yet neither GCC nor Clang issues an
       * fatal error. Only when `--pedantic` flags is provided does GCC issue a
       * warning (but not Clang).
       *
       * I think it is perfectly fine to permit form feed '\f' and vertical
       * '\v' to appear within a directive in this implementation.
       */
      section.offset = endOffset;
    } else if (isMatchString(section, "//", &endOffset)) {
      section.offset = endOffset;
      skipToNextLine(section);
    } else if (isMatchString(section, "/*", &endOffset)) {
      size_t startOfComment = section.offset;
      size_t afterCommentStart = endOffset;

      section.offset = endOffset;

      while (section.offset < section.text.size() &&
             !isMatchString(section, "*/", &endOffset)) {
        getChar(section);
      }
      section.offset = endOffset;
      if (section.offset >= section.text.size()) {
        diagnostics.report({DiagnosticLevel::Error,
                            {startOfComment, afterCommentStart},
                            "Miss end of multiline comment"});
      }
    } else {
      break;
    }
  }
}

bool PreprocessingLexer::skipQuotedLiteralContent(
    ScanSection& section, EncodingPrefix encodingPrefix) {
  size_t startOffset = section.offset;
  size_t endOffset;
  char32_t quote = getChar(section);
  bool isValid = true;

  assert(quote == '"' || quote == '\'');

  char32_t ch;
  while (section.offset < section.text.size()) {
    ch = peekChar(section, &endOffset);
    if (ch == '\n') {
      isValid = false;
      break;
    }
    section.offset = endOffset;
    if (ch == quote) {
      break;
    }
    if (ch != '\\') continue;

    /** handle escaping */

    size_t escapeStart = section.offset;
    section.offset = endOffset;
    if (section.offset >= section.text.size()) break;

    ch = getChar(section);

    switch (ch) {
      case '\'':
      case '"':
      case '?':
      case '\\':
      case 'a':
      case 'b':
      case 'f':
      case 'n':
      case 'r':
      case 't':
      case 'v':
        break;
      case 'x': {
        char32_t value = 0;
        size_t firstHexOffset = section.offset;
        bool isOverflow = false;
        while (section.offset < section.text.size()) {
          ch = peekChar(section, &endOffset);
          if (!std::isxdigit(ch)) break;
          section.offset = endOffset;
          if (value >= (std::numeric_limits<char32_t>::max() >> 4) ||
              encodingPrefix == EncodingPrefix::None && value > 0xFF ||
              encodingPrefix == EncodingPrefix::L && value > 0xFFFFFFFF) {
            isOverflow = true;
          }
          if (isOverflow) continue;
          value <<= 4;
          if (ch >= '0' && ch <= '9') {
            value |= ch;
          } else if (ch >= 'a' && ch <= 'f') {
            value |= ch - 'a';
          } else {
            value |= ch = 'A';
          }
        }
        if (firstHexOffset == section.offset) {
          diagnostics.report({DiagnosticLevel::Error,
                              {escapeStart, section.offset},
                              "\\x used with no following hex digits"});
          isValid = false;
          break;
        }

        if (isOverflow) {
          diagnostics.report({DiagnosticLevel::Error,
                              {escapeStart, section.offset},
                              "hex escape sequence out of range"});
          isValid = false;
        }
      } break;

      /**
       * Octal-escape sequence e.g. "\333"
       */
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7': {
        int value = ch;
        for (int i = 0; i < 2; i++) {
          if (section.offset >= section.text.size()) break;
          ch = peekChar(section, &endOffset);
          if (!isOctDigit(ch)) break;
          section.offset = endOffset;
          value *= 10;
          value += ch;
        }

        if (encodingPrefix == EncodingPrefix::None && value > 0xFF) {
          diagnostics.report({DiagnosticLevel::Error,
                              {escapeStart, section.offset},
                              "octal escape sequence out of range"});
          isValid = false;
        }
      } break;

      /**
       * Universal character name escaping e.g. "\uFFFF", "\UFFFFFFFF"
       */
      case 'u':
      case 'U': {
        size_t startOffset = section.offset;
        size_t i;
        skipUniversalCharacterNameHexQuad(section, ch, escapeStart);
      } break;

      /**
       * Unknown escape sequence is found, a warning is thrown.
       */
      default:
        diagnostics.report({DiagnosticLevel::Warning,
                            {escapeStart, section.offset},
                            "unknown escape sequence"});
        isValid = false;
    }
  }

  if (section.offset != startOffset && ch == quote) {
    return isValid;
  }

  if (quote == '"') {
    diagnostics.report({DiagnosticLevel::Error,
                        {startOffset, section.offset},
                        "Unterminated string literal"});
  } else {
    diagnostics.report({DiagnosticLevel::Error,
                        {startOffset, section.offset},
                        "Unterminated character constant"});
  }

  THROW_IRRECOVERABLE_ERROR();
}

/**
 * This function scan any text that matches `pp-number` defined in C99 spec
 * then check if they are a valid integer or a valid floating point number.
 *
 * The grammar of `pp-number` in C99 spec is defined as:
 *
 * pp-number:
 *    digit
 *    . digit
 *    pp-number digit
 *    pp-number identifier-nondigit
 *    pp-number e sign
 *    pp-number E sign
 *    pp-number p sign
 *    pp-number P sign
 *    pp-number .
 *
 * Rewritten as an equivalent right-recursive grammar for top-down parsing:
 *
 * pp-number:
 *    digit pp-tail
 *    '.' digit pp-tail
 *
 * pp-tail:
 *    digit pp-tail
 *    identifier-nondigit pp-tail
 *    'e' sign pp-tail
 *    'E' sign pp-tail
 *    'p' sign pp-tail
 *    'P' sign pp-tail
 *    '.' pp-tail
 *     ε
 *
 * From this grammar we can observe that a `pp-number` begins with either a
 * bare digit or a '.' followed by a digit. Recognizing such prefix is what
 * determines whether a `pp-number` is present; only confirming it should
 * this function be called. At the point the function is called, the
 * `section.offset` points to the first character of a `pp-number`, i.e., a
 * digit or a '.' followed by a digit.
 */
PreprocessingLexer::NumberTextScanResult PreprocessingLexer::scanNumberText(
    ScanSection& section) {
  using NumberKind = PreprocessingLexer::NumberTextScanResult::NumberKind;
  using RadixKind = NumberTextScanResult::RadixKind;
  size_t startOffset = section.offset;
  size_t endOffset;

  /**
   * Since pp-number is a superset of integer and floaing constant,
   * we can parse the number with pp-number's grammar first.
   */

  PreprocessingLexer::ResultOfScanPPNumberText ppResult =
      scanPPNumberText(section);
  if (ppResult.hasInvalidUCN) {
    diagnostics.report({DiagnosticLevel::Error,
                        {0, 0},
                        "Found invalid universal character name"});
    return {NumberKind::Invalid};
  }

  /**
   * Then we rescan the text, classify the number and check its validity.
   */

  bool foundInvalidOctalDigit = false;
  bool hasIntegralPart = false;
  bool hasExponentPart = false;

  std::string_view sv = ppResult.text;
  NumberTextScanResult result{
      NumberKind::Int, RadixKind::Dec, sv, InvalidNumberReason::None, {}};

  int cursor = 0;

  // Scan radix prefix
  if (sv.size() > 2 && sv[0] == '0') {
    if (sv[1] == 'x' || sv[1] == 'X') {
      result.radix = RadixKind::Hex;
      cursor = 2;
    } else {
      result.radix = RadixKind::Oct;
      cursor = 1;
    }
  }

  /**
   * C lacks octal floating point literals. If the radix is marked as octal
   * but a decimal point or a exponent mark is found later, the radix must be
   * change to RadixKind::Dec. Yet even a number has a leading zero, the final
   * radix is uncertain until the scan finishes. For example, `012.34` is a
   * valid decimal floating point number with a leading zero, so we assume it
   * is a octal integer until we scan the decimal point later. Therefore,
   * invalid digits (like '9') can only be reported when the scan is done,
   * because if we find the number is a decimal floating point, those
   * "invalid" octal digits will suddenly become valid.
   */

  // Scan integral part
  while (cursor < sv.size() &&
         (result.radix == RadixKind::Hex && std::isxdigit(sv[cursor]) ||
          std::isdigit(sv[cursor]))) {
    if (result.radix == RadixKind::Oct && !isOctDigit(sv[cursor])) {
      foundInvalidOctalDigit = true;
    }
    hasIntegralPart = true;
    cursor++;
  }

  // Scan decimal point and decimal part
  if (cursor < sv.size() && sv[cursor] == '.') {
    result.kind = NumberKind::Float;
    cursor++;

    while (cursor < sv.size() &&
           (result.radix == RadixKind::Hex && std::isxdigit(sv[cursor]) ||
            std::isdigit(sv[cursor]))) {
      if (result.radix == RadixKind::Oct) {
        result.radix = RadixKind::Dec;
      }
      cursor++;
    }
  }

  // Scan exponent part
  if (cursor < sv.size() &&
      (result.radix == RadixKind::Hex && std::tolower(sv[cursor]) == 'p' ||
       std::tolower(sv[cursor]) == 'e')) {
    result.kind = NumberKind::Float;
    hasExponentPart = true;

    if (result.radix == RadixKind::Oct) {
      result.radix = RadixKind::Dec;
    }

    cursor++;
    if (cursor < sv.size() && (sv[cursor] == '-' || sv[cursor] == '+')) {
      cursor++;
    }

    if (cursor == sv.size() ||
        result.radix == RadixKind::Hex && !std::isxdigit(sv[cursor]) ||
        !std::isdigit(sv[cursor])) {
      result.invalidReason = InvalidNumberReason::ExponentNoDigit;
      return result;
    }

    while (cursor < sv.size() &&
           (result.radix == RadixKind::Hex && std::isxdigit(sv[cursor]) ||
            std::isdigit(sv[cursor]))) {
      cursor++;
    }
  }

  // Scan number suffixes.

  size_t suffixStart = cursor;

  if (result.kind == NumberKind::Int) {
    if (cursor < sv.size() && std::tolower(sv[cursor]) == 'u') {
      cursor++;
      if (cursor < sv.size() - 1 &&
          (sv[cursor] == 'l' && sv[cursor + 1] == 'l' ||
           sv[cursor] == 'L' && sv[cursor + 1] == 'L')) {
        cursor += 2;
      } else if (cursor < sv.size() && std::tolower(sv[cursor]) == 'l') {
        cursor++;
      }
    } else if (cursor < sv.size() - 1 &&
               (sv[cursor] == 'l' && sv[cursor + 1] == 'l' ||
                sv[cursor] == 'L' && sv[cursor + 1] == 'L')) {
      cursor += 2;
      if (cursor < sv.size() && std::tolower(sv[cursor]) == 'u') {
        cursor++;
      }
    } else if (cursor < sv.size() && std::tolower(sv[cursor]) == 'l') {
      cursor++;
      if (cursor < sv.size() && std::tolower(sv[cursor]) == 'u') {
        cursor++;
      }
    }
  } else if (result.kind == NumberKind::Float && cursor < sv.size() &&
             (sv[cursor] == 'f' || sv[cursor] == 'l' || sv[cursor] == 'F' ||
              sv[cursor] == 'L')) {
    cursor++;
  }

  if (cursor < sv.size()) {
    result.invalidReason = InvalidNumberReason::InvalidSuffix;
    result.invalidSuffix = sv.substr(cursor);
    return result;
  }

  if (result.kind == NumberKind::Float && result.radix == RadixKind::Hex &&
      !hasExponentPart) {
    result.invalidReason = InvalidNumberReason::HexFloatNoExponent;
  } else if (result.radix == RadixKind::Oct && foundInvalidOctalDigit) {
    result.invalidReason = InvalidNumberReason::InvalidOctalDigit;
  }

  return result;
}

bool PreprocessingLexer::isValidUniversalCharacterNameCodepoint(char32_t ch) {
  // C99 Annex D defines valid UCN codepoints for identifiers
  // Based on ISO/IEC 9899:1999 (E) Annex D

  // Latin
  if (ch == 0x00AA || ch == 0x00BA) return true;
  if (ch >= 0x00C0 && ch <= 0x00D6) return true;
  if (ch >= 0x00D8 && ch <= 0x00F6) return true;
  if (ch >= 0x00F8 && ch <= 0x01F5) return true;
  if (ch >= 0x01FA && ch <= 0x0217) return true;
  if (ch >= 0x0250 && ch <= 0x02A8) return true;
  if (ch >= 0x1E00 && ch <= 0x1E9B) return true;
  if (ch >= 0x1EA0 && ch <= 0x1EF9) return true;
  if (ch == 0x207F) return true;

  // Greek
  if (ch == 0x0386) return true;
  if (ch >= 0x0388 && ch <= 0x038A) return true;
  if (ch == 0x038C) return true;
  if (ch >= 0x038E && ch <= 0x03A1) return true;
  if (ch >= 0x03A3 && ch <= 0x03CE) return true;
  if (ch >= 0x03D0 && ch <= 0x03D6) return true;
  if (ch == 0x03DA || ch == 0x03DC || ch == 0x03DE || ch == 0x03E0) return true;
  if (ch >= 0x03E2 && ch <= 0x03F3) return true;
  if (ch >= 0x1F00 && ch <= 0x1F15) return true;
  if (ch >= 0x1F18 && ch <= 0x1F1D) return true;
  if (ch >= 0x1F20 && ch <= 0x1F45) return true;
  if (ch >= 0x1F48 && ch <= 0x1F4D) return true;
  if (ch >= 0x1F50 && ch <= 0x1F57) return true;
  if (ch == 0x1F59 || ch == 0x1F5B || ch == 0x1F5D) return true;
  if (ch >= 0x1F5F && ch <= 0x1F7D) return true;
  if (ch >= 0x1F80 && ch <= 0x1FB4) return true;
  if (ch >= 0x1FB6 && ch <= 0x1FBC) return true;
  if (ch >= 0x1FC2 && ch <= 0x1FC4) return true;
  if (ch >= 0x1FC6 && ch <= 0x1FCC) return true;
  if (ch >= 0x1FD0 && ch <= 0x1FD3) return true;
  if (ch >= 0x1FD6 && ch <= 0x1FDB) return true;
  if (ch >= 0x1FE0 && ch <= 0x1FEC) return true;
  if (ch >= 0x1FF2 && ch <= 0x1FF4) return true;
  if (ch >= 0x1FF6 && ch <= 0x1FFC) return true;

  // Cyrillic
  if (ch >= 0x0401 && ch <= 0x040C) return true;
  if (ch >= 0x040E && ch <= 0x044F) return true;
  if (ch >= 0x0451 && ch <= 0x045C) return true;
  if (ch >= 0x045E && ch <= 0x0481) return true;
  if (ch >= 0x0490 && ch <= 0x04C4) return true;
  if (ch >= 0x04C7 && ch <= 0x04C8) return true;
  if (ch >= 0x04CB && ch <= 0x04CC) return true;
  if (ch >= 0x04D0 && ch <= 0x04EB) return true;
  if (ch >= 0x04EE && ch <= 0x04F5) return true;
  if (ch >= 0x04F8 && ch <= 0x04F9) return true;

  // Armenian
  if (ch >= 0x0531 && ch <= 0x0556) return true;
  if (ch >= 0x0561 && ch <= 0x0587) return true;

  // Hebrew
  if (ch >= 0x05B0 && ch <= 0x05B9) return true;
  if (ch >= 0x05BB && ch <= 0x05BD) return true;
  if (ch == 0x05BF) return true;
  if (ch >= 0x05C1 && ch <= 0x05C2) return true;
  if (ch >= 0x05D0 && ch <= 0x05EA) return true;
  if (ch >= 0x05F0 && ch <= 0x05F2) return true;

  // Arabic
  if (ch >= 0x0621 && ch <= 0x063A) return true;
  if (ch >= 0x0640 && ch <= 0x0652) return true;
  if (ch >= 0x0670 && ch <= 0x06B7) return true;
  if (ch >= 0x06BA && ch <= 0x06BE) return true;
  if (ch >= 0x06C0 && ch <= 0x06CE) return true;
  if (ch >= 0x06D0 && ch <= 0x06DC) return true;
  if (ch >= 0x06E5 && ch <= 0x06E8) return true;
  if (ch >= 0x06EA && ch <= 0x06ED) return true;

  // Devanagari
  if (ch >= 0x0901 && ch <= 0x0903) return true;
  if (ch >= 0x0905 && ch <= 0x0939) return true;
  if (ch >= 0x093E && ch <= 0x094D) return true;
  if (ch >= 0x0950 && ch <= 0x0952) return true;
  if (ch >= 0x0958 && ch <= 0x0963) return true;

  // Bengali
  if (ch >= 0x0981 && ch <= 0x0983) return true;
  if (ch >= 0x0985 && ch <= 0x098C) return true;
  if (ch >= 0x098F && ch <= 0x0990) return true;
  if (ch >= 0x0993 && ch <= 0x09A8) return true;
  if (ch >= 0x09AA && ch <= 0x09B0) return true;
  if (ch == 0x09B2) return true;
  if (ch >= 0x09B6 && ch <= 0x09B9) return true;
  if (ch >= 0x09BE && ch <= 0x09C4) return true;
  if (ch >= 0x09C7 && ch <= 0x09C8) return true;
  if (ch >= 0x09CB && ch <= 0x09CD) return true;
  if (ch >= 0x09DC && ch <= 0x09DD) return true;
  if (ch >= 0x09DF && ch <= 0x09E3) return true;
  if (ch >= 0x09F0 && ch <= 0x09F1) return true;

  // Gurmukhi
  if (ch == 0x0A02) return true;
  if (ch >= 0x0A05 && ch <= 0x0A0A) return true;
  if (ch >= 0x0A0F && ch <= 0x0A10) return true;
  if (ch >= 0x0A13 && ch <= 0x0A28) return true;
  if (ch >= 0x0A2A && ch <= 0x0A30) return true;
  if (ch >= 0x0A32 && ch <= 0x0A33) return true;
  if (ch >= 0x0A35 && ch <= 0x0A36) return true;
  if (ch >= 0x0A38 && ch <= 0x0A39) return true;
  if (ch >= 0x0A3E && ch <= 0x0A42) return true;
  if (ch >= 0x0A47 && ch <= 0x0A48) return true;
  if (ch >= 0x0A4B && ch <= 0x0A4D) return true;
  if (ch >= 0x0A59 && ch <= 0x0A5C) return true;
  if (ch == 0x0A5E || ch == 0x0A74) return true;

  // Gujarati
  if (ch >= 0x0A81 && ch <= 0x0A83) return true;
  if (ch >= 0x0A85 && ch <= 0x0A8B) return true;
  if (ch == 0x0A8D) return true;
  if (ch >= 0x0A8F && ch <= 0x0A91) return true;
  if (ch >= 0x0A93 && ch <= 0x0AA8) return true;
  if (ch >= 0x0AAA && ch <= 0x0AB0) return true;
  if (ch >= 0x0AB2 && ch <= 0x0AB3) return true;
  if (ch >= 0x0AB5 && ch <= 0x0AB9) return true;
  if (ch >= 0x0ABD && ch <= 0x0AC5) return true;
  if (ch >= 0x0AC7 && ch <= 0x0AC9) return true;
  if (ch >= 0x0ACB && ch <= 0x0ACD) return true;
  if (ch == 0x0AD0 || ch == 0x0AE0) return true;

  // Oriya
  if (ch >= 0x0B01 && ch <= 0x0B03) return true;
  if (ch >= 0x0B05 && ch <= 0x0B0C) return true;
  if (ch >= 0x0B0F && ch <= 0x0B10) return true;
  if (ch >= 0x0B13 && ch <= 0x0B28) return true;
  if (ch >= 0x0B2A && ch <= 0x0B30) return true;
  if (ch >= 0x0B32 && ch <= 0x0B33) return true;
  if (ch >= 0x0B36 && ch <= 0x0B39) return true;
  if (ch >= 0x0B3E && ch <= 0x0B43) return true;
  if (ch >= 0x0B47 && ch <= 0x0B48) return true;
  if (ch >= 0x0B4B && ch <= 0x0B4D) return true;
  if (ch >= 0x0B5C && ch <= 0x0B5D) return true;
  if (ch >= 0x0B5F && ch <= 0x0B61) return true;

  // Tamil
  if (ch >= 0x0B82 && ch <= 0x0B83) return true;
  if (ch >= 0x0B85 && ch <= 0x0B8A) return true;
  if (ch >= 0x0B8E && ch <= 0x0B90) return true;
  if (ch >= 0x0B92 && ch <= 0x0B95) return true;
  if (ch >= 0x0B99 && ch <= 0x0B9A) return true;
  if (ch == 0x0B9C) return true;
  if (ch >= 0x0B9E && ch <= 0x0B9F) return true;
  if (ch >= 0x0BA3 && ch <= 0x0BA4) return true;
  if (ch >= 0x0BA8 && ch <= 0x0BAA) return true;
  if (ch >= 0x0BAE && ch <= 0x0BB5) return true;
  if (ch >= 0x0BB7 && ch <= 0x0BB9) return true;
  if (ch >= 0x0BBE && ch <= 0x0BC2) return true;
  if (ch >= 0x0BC6 && ch <= 0x0BC8) return true;
  if (ch >= 0x0BCA && ch <= 0x0BCD) return true;

  // Telugu
  if (ch >= 0x0C01 && ch <= 0x0C03) return true;
  if (ch >= 0x0C05 && ch <= 0x0C0C) return true;
  if (ch >= 0x0C0E && ch <= 0x0C10) return true;
  if (ch >= 0x0C12 && ch <= 0x0C28) return true;
  if (ch >= 0x0C2A && ch <= 0x0C33) return true;
  if (ch >= 0x0C35 && ch <= 0x0C39) return true;
  if (ch >= 0x0C3E && ch <= 0x0C44) return true;
  if (ch >= 0x0C46 && ch <= 0x0C48) return true;
  if (ch >= 0x0C4A && ch <= 0x0C4D) return true;
  if (ch >= 0x0C60 && ch <= 0x0C61) return true;

  // Kannada
  if (ch >= 0x0C82 && ch <= 0x0C83) return true;
  if (ch >= 0x0C85 && ch <= 0x0C8C) return true;
  if (ch >= 0x0C8E && ch <= 0x0C90) return true;
  if (ch >= 0x0C92 && ch <= 0x0CA8) return true;
  if (ch >= 0x0CAA && ch <= 0x0CB3) return true;
  if (ch >= 0x0CB5 && ch <= 0x0CB9) return true;
  if (ch >= 0x0CBE && ch <= 0x0CC4) return true;
  if (ch >= 0x0CC6 && ch <= 0x0CC8) return true;
  if (ch >= 0x0CCA && ch <= 0x0CCD) return true;
  if (ch == 0x0CDE) return true;
  if (ch >= 0x0CE0 && ch <= 0x0CE1) return true;

  // Malayalam
  if (ch >= 0x0D02 && ch <= 0x0D03) return true;
  if (ch >= 0x0D05 && ch <= 0x0D0C) return true;
  if (ch >= 0x0D0E && ch <= 0x0D10) return true;
  if (ch >= 0x0D12 && ch <= 0x0D28) return true;
  if (ch >= 0x0D2A && ch <= 0x0D39) return true;
  if (ch >= 0x0D3E && ch <= 0x0D43) return true;
  if (ch >= 0x0D46 && ch <= 0x0D48) return true;
  if (ch >= 0x0D4A && ch <= 0x0D4D) return true;
  if (ch >= 0x0D60 && ch <= 0x0D61) return true;

  // Thai
  if (ch >= 0x0E01 && ch <= 0x0E3A) return true;
  if (ch >= 0x0E40 && ch <= 0x0E5B) return true;

  // Lao
  if (ch >= 0x0E81 && ch <= 0x0E82) return true;
  if (ch == 0x0E84) return true;
  if (ch >= 0x0E87 && ch <= 0x0E88) return true;
  if (ch == 0x0E8A || ch == 0x0E8D) return true;
  if (ch >= 0x0E94 && ch <= 0x0E97) return true;
  if (ch >= 0x0E99 && ch <= 0x0E9F) return true;
  if (ch >= 0x0EA1 && ch <= 0x0EA3) return true;
  if (ch == 0x0EA5 || ch == 0x0EA7) return true;
  if (ch >= 0x0EAA && ch <= 0x0EAB) return true;
  if (ch >= 0x0EAD && ch <= 0x0EAE) return true;
  if (ch >= 0x0EB0 && ch <= 0x0EB9) return true;
  if (ch >= 0x0EBB && ch <= 0x0EBD) return true;
  if (ch >= 0x0EC0 && ch <= 0x0EC4) return true;
  if (ch == 0x0EC6) return true;
  if (ch >= 0x0EC8 && ch <= 0x0ECD) return true;
  if (ch >= 0x0EDC && ch <= 0x0EDD) return true;

  // Tibetan
  if (ch == 0x0F00) return true;
  if (ch >= 0x0F18 && ch <= 0x0F19) return true;
  if (ch == 0x0F35 || ch == 0x0F37 || ch == 0x0F39) return true;
  if (ch >= 0x0F3E && ch <= 0x0F47) return true;
  if (ch >= 0x0F49 && ch <= 0x0F69) return true;
  if (ch >= 0x0F71 && ch <= 0x0F84) return true;
  if (ch >= 0x0F86 && ch <= 0x0F8B) return true;
  if (ch >= 0x0F90 && ch <= 0x0F95) return true;
  if (ch == 0x0F97) return true;
  if (ch >= 0x0F99 && ch <= 0x0FAD) return true;
  if (ch >= 0x0FB1 && ch <= 0x0FB7) return true;
  if (ch == 0x0FB9) return true;

  // Georgian
  if (ch >= 0x10A0 && ch <= 0x10C5) return true;
  if (ch >= 0x10D0 && ch <= 0x10F6) return true;

  // Hiragana
  if (ch >= 0x3041 && ch <= 0x3093) return true;
  if (ch >= 0x309B && ch <= 0x309C) return true;

  // Katakana
  if (ch >= 0x30A1 && ch <= 0x30F6) return true;
  if (ch >= 0x30FB && ch <= 0x30FC) return true;

  // Bopomofo
  if (ch >= 0x3105 && ch <= 0x312C) return true;

  // CJK Unified Ideographs
  if (ch >= 0x4E00 && ch <= 0x9FA5) return true;

  // Hangul
  if (ch >= 0xAC00 && ch <= 0xD7A3) return true;

  // Digits
  if (ch >= 0x0660 && ch <= 0x0669) return true;
  if (ch >= 0x06F0 && ch <= 0x06F9) return true;
  if (ch >= 0x0966 && ch <= 0x096F) return true;
  if (ch >= 0x09E6 && ch <= 0x09EF) return true;
  if (ch >= 0x0A66 && ch <= 0x0A6F) return true;
  if (ch >= 0x0AE6 && ch <= 0x0AEF) return true;
  if (ch >= 0x0B66 && ch <= 0x0B6F) return true;
  if (ch >= 0x0BE7 && ch <= 0x0BEF) return true;
  if (ch >= 0x0C66 && ch <= 0x0C6F) return true;
  if (ch >= 0x0CE6 && ch <= 0x0CEF) return true;
  if (ch >= 0x0D66 && ch <= 0x0D6F) return true;
  if (ch >= 0x0E50 && ch <= 0x0E59) return true;
  if (ch >= 0x0ED0 && ch <= 0x0ED9) return true;
  if (ch >= 0x0F20 && ch <= 0x0F33) return true;

  // Special characters
  if (ch == 0x00B5 || ch == 0x00B7) return true;
  if (ch >= 0x02B0 && ch <= 0x02B8) return true;
  if (ch == 0x02BB) return true;
  if (ch >= 0x02BD && ch <= 0x02C1) return true;
  if (ch >= 0x02D0 && ch <= 0x02D1) return true;
  if (ch >= 0x02E0 && ch <= 0x02E4) return true;
  if (ch == 0x037A || ch == 0x0559) return true;
  if (ch == 0x093D || ch == 0x0B3D || ch == 0x1FBE) return true;
  if (ch >= 0x203F && ch <= 0x2040) return true;
  if (ch == 0x2102 || ch == 0x2107) return true;
  if (ch >= 0x210A && ch <= 0x2113) return true;
  if (ch == 0x2115) return true;
  if (ch >= 0x2118 && ch <= 0x211D) return true;
  if (ch == 0x2124 || ch == 0x2126 || ch == 0x2128) return true;
  if (ch >= 0x212A && ch <= 0x2131) return true;
  if (ch >= 0x2133 && ch <= 0x2138) return true;
  if (ch >= 0x2160 && ch <= 0x2182) return true;
  if (ch >= 0x3005 && ch <= 0x3007) return true;
  if (ch >= 0x3021 && ch <= 0x3029) return true;

  return false;
}

bool PreprocessingLexer::skipUniversalCharacterNameHexQuad(ScanSection& section,
                                                           char32_t ch,
                                                           size_t ucnStart) {
  size_t endOffset;
  std::optional<char32_t> result =
      parseUniversalCharacterNameHexQuad(section, ch, ucnStart, &endOffset);
  section.offset = endOffset;
  return result != std::nullopt;
}

std::optional<char32_t> PreprocessingLexer::parseUniversalCharacterNameHexQuad(
    const ScanSection& section, char32_t ch, size_t ucnStart,
    size_t* endOffset) {
  char32_t result = 0;
  int hexCount = ch == 'u' ? 4 : 8;
  size_t cursor = section.offset;
  int i;
  for (i = 0; i < hexCount; i++) {
    if (cursor >= section.text.size()) break;
    ch = peekCharAtOffset(section, cursor, &cursor);
    if (!std::isxdigit(ch)) break;
    char32_t hexValue;
    if (ch >= '0' && ch <= '9') {
      hexValue = ch - '0';
    } else {
      hexValue = 10 + std::tolower(ch) - 'a';
    }
    result = result * 0x10 + hexValue;
  }

  if (i < hexCount) {
    diagnostics.report({DiagnosticLevel::Warning,
                        {ucnStart, cursor},
                        "incomplete universal character name"});
    return std::nullopt;
  }

  if (endOffset) *endOffset = cursor;
  return result;
}

bool PreprocessingLexer::isMatchIdentifierCharacter(const ScanSection& section,
                                                    bool includesDigit,
                                                    size_t* endOffset) {
  size_t startOffset = section.offset;
  ScanSection copy = section;
  char32_t ch = getChar(copy);
  char32_t codepoint;
  bool isUCN = false;

  if (ch == '\\') {
    ch = getChar(copy);
    if (ch != 'u' && ch != 'U') {
      return false;
    }
    std::optional<char32_t> res =
        parseUniversalCharacterNameHexQuad(copy, ch, startOffset, &copy.offset);
    if (!res) return false;
    codepoint = *res;
    isUCN = true;
  } else {
    codepoint = ch;
  }

  if (endOffset) *endOffset = copy.offset;

  if (codepoint == '_' || std::isalpha(codepoint)) {
    return true;
  }

  if (includesDigit && std::isdigit(codepoint)) {
    return true;
  }

  if (isValidUniversalCharacterNameCodepoint(codepoint)) {
    return true;
  }

  if (isUCN) {
    if (codepoint > MAX_UNICODE_CODEPOINT) {
      std::string msg{slice(section.text, startOffset, copy.offset)};
      msg += " is not a valid universal character name";
      diagnostics.report(
          {DiagnosticLevel::Error, {startOffset, copy.offset}, msg});
    } else {
      std::string msg = "universal character name ";
      msg += slice(section.text, startOffset, copy.offset);
      msg += " is not valid in an identifier";
      diagnostics.report(
          {DiagnosticLevel::Error, {startOffset, copy.offset}, msg});
    }
  }

  return false;
}

bool PreprocessingLexer::isMatchIdentifierCharacter(const ScanSection& section,
                                                    size_t* endOffset) {
  return isMatchIdentifierCharacter(section, true, endOffset);
}

bool PreprocessingLexer::isMatchIdentifierNonDigitCharacter(
    const ScanSection& section, size_t* endOffset) {
  return isMatchIdentifierCharacter(section, false, endOffset);
}

bool PreprocessingLexer::isMatchIdentifier(ScanSection section,
                                           size_t* endOffset) {
  size_t nextOffset;

  if (!isMatchIdentifierNonDigitCharacter(section, &nextOffset)) {
    return false;
  }

  section.offset = nextOffset;

  while (section.offset < section.text.size() &&
         isMatchIdentifierCharacter(section, &nextOffset)) {
    section.offset = nextOffset;
  }

  if (endOffset) *endOffset = section.offset;

  return true;
}

void PreprocessingLexer::skipToNextLine(ScanSection& section) {
  size_t endOffset;
  while (section.offset < section.text.size() &&
         !isMatchNewline(section, &endOffset)) {
    getChar(section);
  }
  if (section.offset < section.text.size()) {
    section.offset = endOffset;
  }
}

#define REPORT_HASH_IS_NOT_FOLLOW_BY_A_MACRO_PARAMETER \
  diagnostics.report({DiagnosticLevel::Error,          \
                      {0, 0},                          \
                      "\"#\" is not followed by a macro parameter"})

bool PreprocessingLexer::validateHashOperator(const MacroDef& def) {
  ScanSection section{std::string_view{def.body}, 0};

  skipDirectiveSpacesAndComments(section);

  if (section.offset == section.text.size()) {
    return true;
  }

  bool isFirstToken = true;
  PreprocessingToken token;

  while (section.offset < section.text.size()) {
    skipDirectiveSpacesAndComments(section);
    token = scanPreprocessingTokenInsideDreictive(section);

    Punctuator* punc = std::get_if<Punctuator>(&token);

    if (!punc) {
      isFirstToken = false;
      continue;
    }

    if (isFirstToken && punc->kind == PunctuatorKind::HashHash) {
      diagnostics.report(
          {DiagnosticLevel::Error,
           {0, 0},
           "\"##\" cannot appear at the start of macro expansion"});
      return false;
    }

    if (def.kind == MacroKind::FunctionLikeMacro &&
        punc->kind == PunctuatorKind::Hash) {
      ScanSection copy = section;

      skipDirectiveSpacesAndComments(copy);

      if (section.offset == section.text.size()) {
        REPORT_HASH_IS_NOT_FOLLOW_BY_A_MACRO_PARAMETER;
        return false;
      }

      PreprocessingToken subsequentToken =
          scanPreprocessingTokenInsideDreictive(section);

      if (!std::holds_alternative<Identifier>(subsequentToken) &&
          !std::holds_alternative<Keyword>(subsequentToken)) {
        REPORT_HASH_IS_NOT_FOLLOW_BY_A_MACRO_PARAMETER;
        return false;
      }

      const std::string& text = getTokenText(subsequentToken);

      if (!std::ranges::all_of(def.parameters,
                               [&](const std::string& parameter) {
                                 return parameter == text;
                               })) {
        REPORT_HASH_IS_NOT_FOLLOW_BY_A_MACRO_PARAMETER;
        return false;
      }

      section = copy;
    }

    isFirstToken = false;
  }

  std::string& text = getTokenText(token);

  if (text == "##") {
    diagnostics.report({DiagnosticLevel::Error,
                        {0, 0},
                        "\"##\" cannot appear at the end of macro expansion"});
    return false;
  }

  return true;
}

#undef REPORT_HASH_IS_NOT_FOLLOW_BY_A_MACRO_PARAMETER

std::string PreprocessingLexer::HashOperatorEvaluator::getNextTokenText(
    ScanSection& section) {
  PreprocessingToken token = pplex.scanPreprocessingTokenInsideDreictive(
      section, enableParseHeaderName);
  return getTokenText(token);
}

std::string PreprocessingLexer::HashOperatorEvaluator::stringize(
    ScanSection& section) {
  std::string text = getNextTokenText(section);

  // The input should have been checked, so we will find the argument for
  // sure.
  auto it = std::ranges::find(def.parameters, text);
  const std::string& arg = arguments[it - def.parameters.begin()];

  std::string result{'"'};
  ScanSection argSection{std::string_view{arg}, 0};

  while (argSection.offset < argSection.text.size()) {
    char32_t ch = pplex.getChar(argSection);
    if (ch == '"' || ch == '\\') {
      result += "\\";
      result += ch;
    } else {
      encodeUTF8(result, ch);
    }
  }

  result += '"';

  return result;
}

bool PreprocessingLexer::HashOperatorEvaluator::isValidTokenText(
    const std::string& text) {
  ScanSection section{std::string_view{text}, 0};
  PreprocessingToken token = pplex.scanPreprocessingTokenInsideDreictive(
      section, enableParseHeaderName);
  return !std::holds_alternative<InvalidToken>(token) &&
         section.offset == section.text.size();
}

std::string PreprocessingLexer::HashOperatorEvaluator::evaluate() {
  enum class EvalState { AfterLHS, AfterDoubleHash, AfterRHS };

  std::string result;
  ScanSection section{def.body, 0};
  EvalState state = EvalState::AfterLHS;
  std::string concatenatedText;

  pplex.skipDirectiveSpacesAndComments(section);

  while (section.offset < section.text.size()) {
    std::string previousText;
    std::string text = getNextTokenText(section);
    EvalState nextState = state;

    switch (state) {
      case EvalState::AfterLHS:
        if (text == "##") {
          nextState = EvalState::AfterDoubleHash;
          concatenatedText = previousText;
          break;
        }

        if (text == "#") {
          pplex.skipDirectiveSpacesAndComments(section);
          text += stringize(section);
        }

        result += text;
        break;
      case EvalState::AfterDoubleHash:
        if (text == "##") {
          break;
        }

        if (text == "#") {
          pplex.skipDirectiveSpacesAndComments(section);
          text += stringize(section);
        }

        // adding a stringized token to a concatenated text will
        // definitely produces a invalid token.
        concatenatedText += text;
        if (isValidTokenText(concatenatedText)) {
          nextState = EvalState::AfterRHS;
        } else {
          concatenatedText.clear();
          nextState = EvalState::AfterLHS;
        }
      case EvalState::AfterRHS:
        if (text == "##") {
          nextState = EvalState::AfterDoubleHash;
          concatenatedText += previousText;
          break;
        }

        // `concatenatedText` is checked for its validity everytime a new
        // text is joint, so there isn't need for recheck here.
        result += concatenatedText;
        nextState = EvalState::AfterLHS;

        if (text == "#") {
          pplex.skipDirectiveSpacesAndComments(section);
          text += stringize(section);
        }

        result += text;
        break;
    }

    previousText = std::move(text);

    size_t spaceStart = section.offset;
    pplex.skipDirectiveSpacesAndComments(section);

    if (state != EvalState::AfterDoubleHash &&
        nextState != EvalState::AfterDoubleHash &&
        spaceStart < section.offset && section.offset < section.text.size()) {
      result += ' ';
    }

    state = nextState;
  }

  return result;
}

void PreprocessingLexer::scanDirective(ScanSection& section) {
  assert(!scanStack.empty());

  MacroDef newDef;
  newDef.kind = MacroKind::ObjectLikeMacro;

  char32_t ch = getChar(section);
  assert(ch == '#');

  skipDirectiveSpacesAndComments(section);

  size_t endOffset = section.offset;
  size_t directiveNameStart = section.offset;
  while (!isMatchNonNewlineSpace(section, &endOffset)) {
    section.offset = endOffset;
  }

  std::string_view directiveName =
      slice(section.text, directiveNameStart, section.offset);

  if (directiveName == "define") {
    skipDirectiveSpacesAndComments(section);

    if (section.offset == section.text.size() || isMatchNewline(section)) {
      diagnostics.report({DiagnosticLevel::Error,
                          {section.offset, section.offset},
                          "incomplete #define directive"});

      if (section.offset < section.text.size()) {
        skipToNextLine(section);
      }
      return;
    }

    size_t macroNameStart = section.offset;
    if (!isMatchIdentifier(section, &endOffset)) {
      // TODO write test for this error.
      diagnostics.report({DiagnosticLevel::Error,
                          {macroNameStart, section.offset},
                          "macro name must be an identifier"});
      skipToNextLine(section);
      return;
    }

    section.offset = endOffset;
    newDef.name = slice(section.text, macroNameStart, section.offset);

    if (section.offset < section.text.size() &&
        !isMatchNonNewlineSpace(section) && peekChar(section) != '(') {
      diagnostics.report({DiagnosticLevel::Warning,
                          {section.offset, section.offset},
                          "ISO C99 requires whitespace after the macro name"});
    }

    skipDirectiveSpacesAndComments(section);

    if (section.offset < section.text.size() &&
        peekChar(section, &endOffset) == '(') {
      section.offset = endOffset;

      for (;;) {
        if (!newDef.parameters.empty()) {
          if (section.offset == section.text.size() ||
              isMatchNewline(section)) {
            diagnostics.report({DiagnosticLevel::Error,
                                {section.offset, endOffset},
                                "incomplete #define directive"});
            skipToNextLine(section);
            return;
          }

          ch = peekChar(section, &endOffset);
          if (ch == ',') {
            section.offset = endOffset;
          } else if (ch == ')') {
            section.offset = endOffset;
            break;
          } else {
            std::string message = "expected ',' or ')', found \"";
            encodeUTF8(message, ch);
            message += '"';
            diagnostics.report({DiagnosticLevel::Error,
                                {section.offset, endOffset},
                                std::move(message)});
            skipToNextLine(section);
            return;
          }
        } else if (peekChar(section, &endOffset) == ')') {
          section.offset = endOffset;
          break;
        }

        skipDirectiveSpacesAndComments(section);

        if (section.offset == section.text.size() || isMatchNewline(section)) {
          diagnostics.report({DiagnosticLevel::Error,
                              {section.offset, endOffset},
                              "incomplete #define directive"});
          skipToNextLine(section);
          return;
        }

        if (!isMatchIdentifier(section, &endOffset)) {
          diagnostics.report({DiagnosticLevel::Error,
                              {section.offset, endOffset},
                              "expected a parameter name"});
          skipToNextLine(section);
          return;
        }

        std::string parameterName =
            std::string{slice(section.text, section.offset, endOffset)};

        auto iter = std::ranges::find(newDef.parameters, parameterName);
        if (iter != newDef.parameters.end()) {
          // TODO write test for this error.
          diagnostics.report(
              {DiagnosticLevel::Error,
               {section.offset, endOffset},
               "duplicate macro parameter name '" + parameterName + "'"});
          skipToNextLine(section);
          return;
        }

        section.offset = endOffset;

        newDef.parameters.push_back(std::move(parameterName));
        skipDirectiveSpacesAndComments(section);
      }

      newDef.kind = MacroKind::FunctionLikeMacro;
    }

    skipDirectiveSpacesAndComments(section);

    size_t startOfMacroBody = section.offset;
    while (section.offset < section.text.size() &&
           !isMatchNewline(section, &endOffset)) {
      getChar(section);
    }

    if (section.offset < section.text.size()) {
      section.offset = endOffset;
    }

    newDef.body = slice(section.text, startOfMacroBody, section.offset);

    if (validateHashOperator(newDef)) {
      macroDefDict.insert({newDef.name, newDef});
    }
  } else {
    // TODO Handle other directives.
  }
}

bool PreprocessingLexer::canScanDirective() {
  if (scanStack.empty()) return false;
  auto fileFrame = std::get_if<FileFrame>(&scanStack.back());
  return fileFrame && fileFrame->isAtLineStart;
}

// PUBLIC FUNCTIONS

Token PreprocessingLexer::getToken() {
  if (scanStack.empty()) {
    return EofToken{};
  }

  /**
   * Invariant: when entering `getToken`, the `section.offset` points to neither
   * a whitespace character nor a comment. the offset is advanced to the first
   * non-space character when the lexer is created, and after a token is scanned
   * the lexer will skip the spaces and comments after the token until it reach
   * the start of the next token.
   */

  if (canScanDirective()) {
    FileFrame& frame = std::get<FileFrame>(scanStack.back());

    while (frame.section.offset < frame.section.text.size() &&
           peekChar(frame.section) == '#') {
      scanDirective(frame.section);
      skipSpacesAndComments(frame);
    }

    if (frame.section.offset == frame.section.text.size()) {
      popFrame();
      return getToken();
    }
  }

  ScanSection& section = getScanSection(scanStack.back());

  Token token;
  size_t nextOffset;
  char32_t ch = peekChar(section, &nextOffset);

  if (std::isdigit(ch) ||
      ch == '.' && section.offset < section.text.size() &&
          std::isdigit(peekCharAtOffset(section, nextOffset, &nextOffset))) {
    size_t startOffset = section.offset;
    NumberTextScanResult result = scanNumberText(section);

    std::string message;

    switch (result.invalidReason) {
      case InvalidNumberReason::InvalidOctalDigit:
        message = "Invalid digit in octal constant";
        break;
      case InvalidNumberReason::InvalidSuffix:
        message = "Invalid suffix ";
        message += result.invalidSuffix;
        message += "on a ";
        message += result.kind == NumberTextScanResult::NumberKind::Int
                       ? "integer constant"
                       : "floating constant";
        break;
      case InvalidNumberReason::ExponentNoDigit:
        message = "Exponent has no digit";
        break;
      case InvalidNumberReason::HexFloatNoExponent:
        message = "Hexadecimal floating constants requires an exponent";
        break;
      case InvalidNumberReason::None:
        break;
    }

    if (result.invalidReason != InvalidNumberReason::None) {
      this->diagnostics.report(
          {DiagnosticLevel::Error, {startOffset, section.offset}, message});
    }

    if (result.kind == NumberTextScanResult::NumberKind::Int) {
      token = IntegerConstant{result.invalidReason == InvalidNumberReason::None,
                              result.invalidReason, std::string{result.text}};
    } else if (result.kind == NumberTextScanResult::NumberKind::Float) {
      token =
          FloatingConstant{result.invalidReason == InvalidNumberReason::None,
                           result.invalidReason, std::string{result.text}};
    } else {
      this->diagnostics.report({DiagnosticLevel::Error,
                                {startOffset, section.offset},
                                "invalid number literal"});

      token = InvalidToken{std::string{result.text}};
    }
  } else if (std::optional<Punctuator> punctuator = scanPunctuator(section)) {
    token = *punctuator;
  } else if (isMatchIdentifierNonDigitCharacter(section, &nextOffset)) {
    size_t startOffset = section.offset;

    do {
      section.offset = nextOffset;
    } while (section.offset < section.text.size() &&
             isMatchIdentifierCharacter(section, &nextOffset));

    std::string_view text = slice(section.text, startOffset, section.offset);

    // TODO support u, u8 and U string and character constant.
    if (section.offset < section.text.size() && text == "L") {
      ch = peekChar(section, &nextOffset);
      if (ch == '"' || ch == '\'') {
        bool isValid = skipQuotedLiteralContent(section, EncodingPrefix::L);
        text = slice(section.text, startOffset, section.offset);

        if (ch == '"') {
          token = StringLiteral{isValid, std::string{text}};
        } else {
          token = CharacterConstant{isValid, std::string{text}};
        }
      } else {
        token = Identifier{std::string{text}};
      }
    } else {
      auto it = std::ranges::find_if(
          keywordArray, [&](Keyword keyword) { return keyword.text == text; });

      if (it != keywordArray.end()) {
        token = Keyword{it->kind, std::string{text}};
      } else {
        token = Identifier{std::string{text}};
      }
    }

    if (auto identifier = std::get_if<Identifier>(&token)) {
      /**
       * TODO I should remove universal character names in a identifiers
       */
      if (auto macroDefIter = macroDefDict.find(identifier->text);
          macroDefIter != macroDefDict.end()) {
        if (auto frameIter = std::find_if(
                scanStack.begin(), scanStack.end(),
                [&](const ScanStackFrame& frame) {
                  auto macroFrame = std::get_if<MacroFrame>(&frame);
                  return macroFrame &&
                         macroFrame->def->name == macroDefIter->second.name;
                });
            frameIter == scanStack.end()) {
          pushMacroFrame(macroDefIter->second);
          return getToken();
        }
      }
    }
  } else if (ch == '"' || ch == '\'') {
    size_t startOffset = section.offset;
    bool isValid = skipQuotedLiteralContent(section, EncodingPrefix::None);
    std::string_view text = slice(section.text, startOffset, section.offset);

    if (ch == '"') {
      token = StringLiteral{isValid, std::string{text}};
    } else {
      token = CharacterConstant{isValid, std::string{text}};
    }
  } else {
    size_t startOffset = section.offset;
    getChar(section);
    std::string_view text = slice(section.text, startOffset, section.offset);
    token = InvalidToken{std::string{text}};

    std::string msg{"stray character '"};
    msg += text;
    msg += "' in the program";

    diagnostics.report(
        {DiagnosticLevel::Error, {startOffset, section.offset}, msg});
  }

  if (auto fileFrame = std::get_if<FileFrame>(&scanStack.back())) {
    fileFrame->isAtLineStart = false;
  }

  skipSpacesAndComments(scanStack.back());

  if (section.offset == section.text.size()) {
    popFrame();
  }

  return token;
}

bool PreprocessingLexer::isEof() { return scanStack.empty(); }