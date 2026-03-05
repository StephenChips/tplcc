#ifndef TPLCC_PP_H
#define TPLCC_PP_H

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>

#include "encoding.h"
#include "error.h"

#define KEYWORDS_X_MACRO_LIST \
  X(Bool, _Bool)              \
  X(Complex, _Complex)        \
  X(Imaginary, _Imaginary)    \
  X(Auto, auto)               \
  X(Break, break)             \
  X(Case, case)               \
  X(Char, char)               \
  X(Const, const)             \
  X(Continue, continue)       \
  X(Default, default)         \
  X(Do, do)                   \
  X(Double, double)           \
  X(Else, else)               \
  X(Enum, enum)               \
  X(Extern, extern)           \
  X(Float, float)             \
  X(For, for)                 \
  X(Goto, goto)               \
  X(If, if)                   \
  X(Inline, inline)           \
  X(Int, int)                 \
  X(Long, long)               \
  X(Register, register)       \
  X(Restrict, restrict)       \
  X(Return, return)           \
  X(Signed, signed)           \
  X(Sizeof, sizeof)           \
  X(Static, static)           \
  X(Struct, struct)           \
  X(Switch, switch)           \
  X(Typedef, typedef)         \
  X(Union, union)             \
  X(Unsigned, unsigned)       \
  X(Void, void)               \
  X(Volatile, volatile)       \
  X(While, while)

#define PUNCTUATORS_X_MACRO_LIST \
  /* Primary brackets */         \
  X(LBracket, "[")               \
  X(RBracket, "]")               \
  X(LParen, "(")                 \
  X(RParen, ")")                 \
  X(LBrace, "{")                 \
  X(RBrace, "}")                 \
                                 \
  /* Member / pointer */         \
  X(Dot, ".")                    \
  X(Arrow, "->")                 \
                                 \
  /* Increment / decrement */    \
  X(PlusPlus, "++")              \
  X(MinusMinus, "--")            \
                                 \
  /* Arithmetic */               \
  X(Plus, "+")                   \
  X(Minus, "-")                  \
  X(Star, "*")                   \
  X(Slash, "/")                  \
  X(Modulo, "%")                 \
                                 \
  /* Bitwise */                  \
  X(BitwiseAnd, "&")             \
  X(BitwiseOr, "|")              \
  X(BitwiseXor, "^")             \
  X(BitwiseNot, "~")             \
  X(LShift, "<<")                \
  X(RShift, ">>")                \
                                 \
  /* Logical */                  \
  X(LogicalNot, "!")             \
  X(LogicalAnd, "&&")            \
  X(LogicalOr, "||")             \
                                 \
  /* Assignment */               \
  X(Assign, "=")                 \
  X(PlusAssign, "+=")            \
  X(MinusAssign, "-=")           \
  X(MultiplyAssign, "*=")        \
  X(DivideAssign, "/=")          \
  X(ModuloAssign, "%=")          \
  X(BitwiseAndAssign, "&=")      \
  X(BitwiseOrAssign, "|=")       \
  X(BitwiseXorAssign, "^=")      \
  X(LShiftAssign, "<<=")         \
  X(RShiftAssign, ">>=")         \
                                 \
  /* Comparison */               \
  X(Equal, "==")                 \
  X(NotEqual, "!=")              \
  X(LessThan, "<")               \
  X(GreaterThan, ">")            \
  X(LessOrEqual, "<=")           \
  X(GreaterOrEqual, ">=")        \
                                 \
  /* Misc operators */           \
  X(Question, "?")               \
  X(Colon, ":")                  \
  X(Semicolon, ";")              \
  X(Comma, ",")                  \
  X(Ellipsis, "...")             \
                                 \
  /* Preprocessor */             \
  X(Hash, "#")                   \
  X(HashHash, "##")

#define DIGRAPH_PUNCTUATORS_X_MACRO_LIST \
  X(Hash, "%:")                          \
  X(HashHash, "%:%:")                    \
  /* Digraph brackets */                 \
  X(LBracket, "<:")                      \
  X(RBracket, ":>")                      \
  X(LBrace, "<%")                        \
  X(RBrace, "%>")

enum class KeywordKind {
#define X(pascalName, name) pascalName,
  KEYWORDS_X_MACRO_LIST
#undef X
};

enum class PunctuatorKind {
#define X(name, str) name,
  PUNCTUATORS_X_MACRO_LIST
#undef X
};

#define X(...) +1
constexpr size_t punctuatorKindCount = 0 PUNCTUATORS_X_MACRO_LIST;
constexpr size_t keywordKindCount = 0 KEYWORDS_X_MACRO_LIST;
constexpr size_t digraphPunctuatorCount = 0 DIGRAPH_PUNCTUATORS_X_MACRO_LIST;
#undef X

#define X(name, str) str,
constexpr std::array<const char*, punctuatorKindCount> punctuatorCStrings{
    PUNCTUATORS_X_MACRO_LIST};
#undef X

#define X(pascalName, name) #name,
constexpr std::array<const char*, keywordKindCount> keywordCStrings{
    KEYWORDS_X_MACRO_LIST};
#undef X

struct Keyword {
  KeywordKind kind;
  std::string_view text;
  bool operator==(const Keyword&) const = default;
};

struct Identifier {
  std::string_view text;
  bool operator==(const Identifier&) const = default;
};

struct StringLiteral {
  std::string_view text;
  bool operator==(const StringLiteral&) const = default;
};

struct IntegerConstant {
  std::string_view text;
  bool operator==(const IntegerConstant&) const = default;
};

struct FloatingConstant {
  std::string_view text;
  bool operator==(const FloatingConstant&) const = default;
};

struct CharacterConstant {
  std::string_view text;
  bool operator==(const CharacterConstant&) const = default;
};

struct Punctuator {
  PunctuatorKind kind;
  std::string_view text;
  bool operator==(const Punctuator&) const = default;
};

struct InvalidToken {
  std::string_view text;
  bool operator==(const InvalidToken&) const = default;
};

struct EofToken {
  bool operator==(const EofToken&) const = default;
};

using Token = std::variant<Keyword, Identifier, StringLiteral, IntegerConstant,
                           FloatingConstant, CharacterConstant, Punctuator,
                           InvalidToken, EofToken>;

constexpr auto punctuatorArraySortedByLenDesc = []() {
#define X(name, str) Punctuator{PunctuatorKind::name, str},
  std::array<Punctuator, punctuatorKindCount + digraphPunctuatorCount> copy{
      {PUNCTUATORS_X_MACRO_LIST DIGRAPH_PUNCTUATORS_X_MACRO_LIST}};
#undef X
  std::sort(copy.begin(), copy.end(), [](Punctuator first, Punctuator second) {
    if (first.text.size() != second.text.size()) {
      return first.text.size() > second.text.size();
    } else {
      return first.text > second.text;
    }
  });
  return copy;
}();

enum class MacroKind { ObjectLikeMacro, FunctionLikeMacro };

struct MacroDefinition {
  MacroKind kind;
  std::string name;
  std::vector<std::string> parameters;
  std::string body;

  MacroDefinition(std::string name, std::string body,
                  MacroKind type = MacroKind::ObjectLikeMacro)
      : name(std::move(name)), body(std::move(body)), kind(type) {};

  MacroDefinition(std::string name, std::vector<std::string> parameters,
                  std::string body,
                  MacroKind kind = MacroKind::FunctionLikeMacro)
      : name(std::move(name)),
        body(std::move(body)),
        parameters(std::move(parameters)),
        kind(kind) {};

  bool operator==(const MacroDefinition& other) const {
    return this->name == other.name;
  }
};

template <>
struct std::hash<MacroDefinition> {
  size_t operator()(const MacroDefinition& macroDef) const noexcept {
    return std::hash<std::string>{}(macroDef.name);
  }
};

template <CharDecodeFunc CharDecodeFunc>
class PreprocessingLexer {
  using Offset = uint64_t;

  struct ScanCursor {
    std::string_view text;
    size_t offset;
  };

  using ScanStack = std::vector<ScanCursor>;

  IReportError& errorOut;
  CharDecodeFunc& decodeFunc;
  ScanStack scanStack;
  std::string input;

  std::unordered_set<MacroDefinition> macroDefs;

  void enterSection(std::string_view text) {
    ScanCursor cursor = initScanCursor(text);
    if (isAtEnd(cursor)) return;
    scanStack.push_back(cursor);
  }

  void exitSection() { scanStack.pop_back(); }

  std::optional<Punctuator> scanPunctuator();

  ScanCursor initScanCursor(std::string_view text) {
    ScanCursor cursor{text, 0};
    skipBackslashNewlines(cursor);
    return cursor;
  }

  char32_t getChar(ScanCursor& cursor, size_t* len = nullptr,
                   bool willSkipBackslashNewlines = true) const {
    // A valid cursor should never points to a '\' '\n' sequence,
    // so we don't need to skip it before decoding a character.
    auto [ch, charlen] = decodeFunc(cursor.text.data() + cursor.offset);
    cursor.offset += charlen;
    if (len) *len = charlen;
    if (willSkipBackslashNewlines) skipBackslashNewlines(cursor);
    return ch;
  }

  char32_t peekChar(ScanCursor cursor, size_t* len = nullptr) const {
    auto [ch, charlen] = decodeFunc(cursor.text.data() + cursor.offset);
    if (len) *len = charlen;
    return ch;
  }

  bool isNewlineAt(ScanCursor cursor, size_t* len) {
    size_t charlen = 0;
    bool isMatch = false;

    if (!isAtEnd(cursor)) {
      switch (peekChar(cursor, &charlen)) {
        case '\n':
        case '\r':
          isMatch = true;
          if (len) *len = charlen;
          cursor.offset += charlen;
          if (!isAtEnd(cursor) && peekChar(cursor, &charlen) == '\n') {
            if (len) *len += charlen;
            cursor.offset += charlen;
          }
      }
    }

    return isMatch;
  }

  bool isSpaceAt(ScanCursor cursor, size_t* len = nullptr) const {
    size_t ch = peekChar(cursor, len);
    return isSpace(ch);
  }

  bool isStringAt(ScanCursor cursor, const char* s,
                  size_t* len = nullptr) const {
    size_t oldOffset = cursor.offset;
    while (*s && !isAtEnd(cursor) && getChar(cursor) == *s) {
      s++;
    }
    if (len) *len = cursor.offset - oldOffset;
    return *s == '\0';
  }

  void skipBackslashNewlines(ScanCursor& cursor) const {
    for (;;) {
      ScanCursor preview = cursor;
      if (isAtEnd(preview)) break;
      char32_t ch1 = getChar(preview, nullptr, false);
      if (ch1 != '\\' || isAtEnd(preview)) break;
      char32_t ch2 = getChar(preview, nullptr, false);
      if (ch2 != '\n') break;
      cursor = preview;
    }
  }

  bool isAtEnd(ScanCursor cursor) const {
    return cursor.offset >= cursor.text.size();
  }

  // The MSVC's std::isspace will throw a runtime error when we pass a
  // codepoint that is larger than 255. We have to write our own version of
  // isspace here to avoid this error.
  bool isSpace(char32_t ch) const {
    return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
           ch == '\v';
  }
  bool isDirectiveSpace(int ch) const { return ch == ' ' || ch == '\t'; }
  bool isNewlineCharacter(int ch) const { return ch == '\r' || ch == '\n'; }

  bool isStartOfIdentifier(int ch) const {
    return ch == '_' || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z';
  }

  void skipSpacesAndComments();

 public:
  PreprocessingLexer(std::string inputStr, CharDecodeFunc& decodeFunc,
                     IReportError& errorOut)
      : input(std::move(inputStr)), decodeFunc(decodeFunc), errorOut(errorOut) {
    enterSection(input);
    skipSpacesAndComments();
  };

  Token getToken();
  Token peekToken();
  bool isEof();
};

template <CharDecodeFunc CharDecodeFunc>
Token PreprocessingLexer<CharDecodeFunc>::getToken() {
  if (scanStack.empty()) {
    return EofToken{};
  }

  Token token;

  if (std::optional<Punctuator> punctuator = scanPunctuator()) {
    token = *punctuator;
  } else {
    ScanCursor& cursor = scanStack.back();
    size_t charlen;
    size_t oldOffset = cursor.offset;
    getChar(cursor, &charlen);
    token = InvalidToken{{cursor.text.substr(oldOffset, charlen)}};
  }

  skipSpacesAndComments();

  return token;
}

template <CharDecodeFunc CharDecodeFunc>
std::optional<Punctuator> PreprocessingLexer<CharDecodeFunc>::scanPunctuator() {
  if (scanStack.empty()) return std::nullopt;

  for (Punctuator punctuator : punctuatorArraySortedByLenDesc) {
    ScanCursor& current = scanStack.back();
    ScanCursor preview = current;
    bool isMatch = true;

    for (char ch : punctuator.text) {
      if (isAtEnd(preview) || getChar(preview) != ch) {
        isMatch = false;
        break;
      }
    }

    if (isMatch) {
      std::string_view str =
          current.text.substr(current.offset, preview.offset - current.offset);
      current = preview;
      return Punctuator{punctuator.kind, str};
    }
  }

  return std::nullopt;
}

template <CharDecodeFunc CharDecodeFunc>
inline void PreprocessingLexer<CharDecodeFunc>::skipSpacesAndComments() {
  while (!scanStack.empty()) {
    ScanCursor& cursor = scanStack.back();
    size_t len;

    while (!isAtEnd(cursor)) {
      if (isSpaceAt(cursor, &len)) {
        cursor.offset += len;
      } else if (isStringAt(cursor, "//", &len)) {
        cursor.offset += len;
        while (!isNewlineAt(cursor, &len)) {
          getChar(cursor);
        }
        cursor.offset += len;
      } else if (isStringAt(cursor, "/*", &len)) {
        cursor.offset += len;
        while (!isStringAt(cursor, "*/", &len)) {
          getChar(cursor, &len);
        }
        cursor.offset += len;
      } else {
        break;
      }
    }

    if (isAtEnd(cursor)) {
      exitSection();
    } else {
      break;
    }
  }
}

template <CharDecodeFunc CharDecodeFunc>
bool PreprocessingLexer<CharDecodeFunc>::isEof() {
  return scanStack.empty();
}

#endif  // !TPLCC_PP_H