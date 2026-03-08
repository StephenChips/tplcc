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

#define X(PascalName, name) {KeywordKind::PascalName, #name},
constexpr std::array<Keyword, keywordKindCount> keywordArray{
    {KEYWORDS_X_MACRO_LIST}};
#undef X

constexpr auto punctuatorArraySortedByLenDesc = []() {
#define X(name, str) Punctuator{PunctuatorKind::name, str},
  std::array<Punctuator, punctuatorKindCount + digraphPunctuatorCount> array{
      {PUNCTUATORS_X_MACRO_LIST DIGRAPH_PUNCTUATORS_X_MACRO_LIST}};
#undef X
  std::sort(array.begin(), array.end(),
            [](Punctuator first, Punctuator second) {
              if (first.text.size() != second.text.size()) {
                return first.text.size() > second.text.size();
              } else {
                return first.text > second.text;
              }
            });
  return array;
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

  struct ScanSection {
    std::string_view text;
    size_t offset;
  };

  using ScanStack = std::vector<ScanSection>;

  IReportError& errorOut;
  CharDecodeFunc& decodeFunc;
  ScanStack scanStack;
  std::string input;

  std::unordered_set<MacroDefinition> macroDefs;

  void enterSection(std::string_view text) {
    ScanSection section = initScanSection(text);
    if (section.offset >= section.text.size()) return;
    scanStack.push_back(section);
  }

  void exitSection() { scanStack.pop_back(); }

  std::optional<Punctuator> scanPunctuator() {
    if (scanStack.empty()) return std::nullopt;

    for (Punctuator punctuator : punctuatorArraySortedByLenDesc) {
      ScanSection& section = scanStack.back();
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
        std::string_view str =
            section.text.substr(section.offset, offset - section.offset);
        section.offset = offset;
        return Punctuator{punctuator.kind, str};
      }
    }

    return std::nullopt;
  }

  // The MSVC's std::isspace will throw a runtime error when we pass a
  // codepoint that is larger than 255. We have to write our own version of
  // isspace here to avoid this error.
  inline bool isSpace(char32_t ch) {
    return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
           ch == '\v';
  }
  inline bool isDirectiveSpace(char32_t ch) { return ch == ' ' || ch == '\t'; }
  inline bool isNewlineCharacter(char32_t ch) {
    return ch == '\r' || ch == '\n';
  }

  inline bool isStartOfIdentifier(char32_t ch) {
    return ch == '_' || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z';
  }

  ScanSection initScanSection(std::string_view text) {
    ScanSection cursor{text, 0};
    skipBackslashNewlines(cursor);
    return cursor;
  }

  char32_t getChar(ScanSection& section,
                   bool willSkipBackslashNewlines = true) const {
    return getCharAtOffset(section, section.offset, willSkipBackslashNewlines);
  }

  char32_t getCharAtOffset(const ScanSection& section, size_t& offset,
                           bool willSkipBackslashNewlines = true) const {
    // A valid cursor should never points to a '\' '\n' sequence,
    // so we don't need to skip it before decoding a character.
    auto [ch, charlen] = decodeFunc(section.text.data() + offset);
    offset += charlen;
    if (willSkipBackslashNewlines) {
      skipBackslashNewlinesAtOffset(section, offset);
    }
    return ch;
  }

  char32_t peekChar(const ScanSection& section, size_t* endOffset = nullptr,
                    bool willSkipBackslashNewlines = true) const {
    return peekCharAtOffset(section, section.offset, endOffset,
                            willSkipBackslashNewlines);
  }

  char32_t peekCharAtOffset(const ScanSection& section, size_t offset,
                            size_t* endOffset = nullptr,
                            bool willSkipBackslashNewlines = true) const {
    char32_t ch = getCharAtOffset(section, offset, willSkipBackslashNewlines);
    if (endOffset) *endOffset = offset;
    return ch;
  }

  bool isMatchNewline(const ScanSection& section, size_t* endOffset = nullptr) {
    if (section.offset >= section.text.size()) {
      return false;
    }

    bool isMatch = false;
    size_t offset = section.offset;
    size_t nextOffset;

    char32_t ch = peekCharAtOffset(section, offset, &nextOffset);
    if (ch == '\r') {
      isMatch = true;
      offset = nextOffset;
      ch = peekCharAtOffset(section, offset, &nextOffset);
      if (ch == '\n') offset = nextOffset;
    } else if (ch == '\n') {
      isMatch = true;
      offset = nextOffset;
    }

    if (isMatch && endOffset) {
      *endOffset = offset;
    }

    return isMatch;
  }

  bool isMatchSpace(const ScanSection& section, size_t* endOffset = nullptr) {
    size_t ch = peekChar(section, endOffset);
    return isSpace(ch);
  }

  bool isMatchString(const ScanSection& section, const char* s,
                     size_t* endOffset = nullptr) {
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

  void skipBackslashNewlines(ScanSection& section) {
    skipBackslashNewlinesAtOffset(section, section.offset);
  }

  void skipBackslashNewlinesAtOffset(const ScanSection& section,
                                     size_t& offset) const {
    size_t nextOffset = offset;
    while (nextOffset < section.text.size()) {
      char32_t ch1 = getCharAtOffset(section, nextOffset, false);
      if (ch1 != '\\' || nextOffset >= section.text.size()) break;
      char32_t ch2 = getCharAtOffset(section, nextOffset, false);
      if (ch2 != '\n') break;
      offset = nextOffset;
    }
  }

  void skipSpacesAndComments() {
    while (!scanStack.empty()) {
      ScanSection& section = scanStack.back();
      size_t endOffset;

      while (section.offset < section.text.size()) {
        if (isMatchSpace(section, &endOffset)) {
          section.offset = endOffset;
        } else if (isMatchString(section, "//", &endOffset)) {
          section.offset = endOffset;
          while (section.offset < section.text.size() &&
                 !isMatchNewline(section, &endOffset)) {
            getChar(section);
          }
          section.offset = endOffset;
        } else if (isMatchString(section, "/*", &endOffset)) {
          section.offset = endOffset;
          while (section.offset < section.text.size() &&
                 !isMatchString(section, "*/", &endOffset)) {
            getChar(section);
          }
          section.offset = endOffset;
          if (section.offset >= section.text.size()) {
            // output not find multiline comment ending.
          }
        } else {
          break;
        }
      }

      if (section.offset >= section.text.size()) {
        exitSection();
      } else {
        break;
      }

      // TODO try replacing isMatchXXX functions with consumeIf, consumeUntil
      // functions, which needs some advance template skills.
      //
      // for example: the code can be refractored to:
      //
      // while (offset < text.size()) {
      //   if (consumeIf<&isSpace>(sv, offset)) {
      //   } else if (consumeIf<"/*">(sv, offset, "//")) {
      //     consumeUntil<&isNewline>(sv, offset, true);
      //   } else if (consumeIf<"/*">(sv, offset)) {
      //     consumeUntil<"*/">(sv, offset, true);
      //   } else {
      //     break;
      //   }
      // }
    }
  }

 public:
  PreprocessingLexer(std::string inputStr, CharDecodeFunc& decodeFunc,
                     IReportError& errorOut)
      : input(std::move(inputStr)), decodeFunc(decodeFunc), errorOut(errorOut) {
    enterSection(input);
    skipSpacesAndComments();
  };

  Token getToken() {
    char32_t ch;
    size_t endOffset;
    ScanSection& section = scanStack.back();

    if (scanStack.empty()) {
      return EofToken{};
    }

    if (std::optional<Punctuator> punctuator = scanPunctuator()) {
      skipSpacesAndComments();
      return *punctuator;
    }

    ch = peekChar(section, &endOffset);
    if (std::isalpha(ch) || ch == '_') {
      auto startOffset = section.offset;

      do {
        section.offset = endOffset;
        ch = peekChar(section, &endOffset);
      } while (section.offset < section.text.size() &&
               (std::isdigit(ch) || std::isalpha(ch) || ch == '_'));

      std::string_view text =
          section.text.substr(startOffset, section.offset - startOffset);

      skipSpacesAndComments();

      auto it = std::ranges::find_if(
          keywordArray, [=](Keyword keyword) { return keyword.text == text; });
      if (it != std::end(keywordArray)) {
        return Keyword{it->kind, text};
      } else {
        return Identifier{text};
      }
    }

    size_t startOffset = section.offset;
    getChar(section);
    skipSpacesAndComments();
    return InvalidToken{
        {section.text.substr(startOffset, section.offset - startOffset)}};
  }

  Token peekToken();
  bool isEof() { return scanStack.empty(); }
};

#endif  // !TPLCC_PP_H