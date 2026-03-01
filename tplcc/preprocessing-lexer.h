#ifndef TPLCC_PP_H
#define TPLCC_PP_H

#include <array>
#include <concepts>
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
  /* Digraph brackets */         \
  X(LBracketDigraph, "<:")       \
  X(RBracketDigraph, ":>")       \
  X(LBraceDigraph, "<%")         \
  X(RBraceDigraph, "%>")         \
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
  X(Tilde, "~")                  \
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
  X(PlusEqual, "+=")             \
  X(MinusEqual, "-=")            \
  X(MultiplyEqual, "*=")         \
  X(DivideEqual, "/=")           \
  X(ModuloEqual, "%=")           \
  X(BitwiseAndEqual, "&=")       \
  X(BitwiseOrEqual, "|=")        \
  X(BitwiseXorEqual, "^=")       \
  X(LShiftEqual, "<<=")          \
  X(RShiftEqual, ">>=")          \
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
  X(HashHash, "##")              \
                                 \
  /* Digraph preprocessor */     \
  X(HashDigraph, "%:")           \
  X(DoubleHashDigraph, "%:%:")

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
#undef X

#define X(name, str) str,
constexpr std::array<const char*, punctuatorKindCount> punctuatorCStrings{
    PUNCTUATORS_X_MACRO_LIST};
#undef X

#define X(pascalName, name) #name,
constexpr std::array<const char*, keywordKindCount> keywordCStrings{
    KEYWORDS_X_MACRO_LIST};
#undef X

std::ostream& operator<<(std::ostream& os, PunctuatorKind kind) {
#define X(EnumName, ...)         \
  case PunctuatorKind::EnumName: \
    return os << "PunctuatorKind::" << #EnumName;

  switch (kind) { PUNCTUATORS_X_MACRO_LIST }
  return os;

#undef X
}

std::ostream& operator<<(std::ostream& os, KeywordKind kind) {
#define X(PascalCaseName, ...)      \
  case KeywordKind::PascalCaseName: \
    return os << "KeywordKind::" << #PascalCaseName;

  switch (kind) { KEYWORDS_X_MACRO_LIST }
  return os;

#undef X
}

struct Keyword {
  KeywordKind kind;
  std::string_view range;
  bool operator==(const Keyword&) const = default;
};

struct Identifier {
  std::string_view range;
  bool operator==(const Identifier&) const = default;
};

struct StringLiteral {
  std::string_view range;
  bool operator==(const StringLiteral&) const = default;
};

struct IntegerConstant {
  std::string_view range;
  bool operator==(const IntegerConstant&) const = default;
};

struct FloatingConstant {
  std::string_view range;
  bool operator==(const FloatingConstant&) const = default;
};

struct CharacterConstant {
  std::string_view range;
  bool operator==(const CharacterConstant&) const = default;
};

struct Punctuator {
  PunctuatorKind kind;
  std::string_view range;
  bool operator==(const Punctuator&) const = default;
};

struct InvalidToken {
  std::string_view range;
  bool operator==(const InvalidToken&) const = default;
};

struct Eof {
  bool operator==(const Eof&) const = default;
};

using Token = std::variant<Keyword, Identifier, StringLiteral, IntegerConstant,
                           FloatingConstant, CharacterConstant, Punctuator,
                           InvalidToken, Eof>;

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

  enum class ScanSectionSourceKind { SourceFile, MacroBody, MacroArgument };

  struct ScanSection {
    ScanSectionSourceKind sourceKind;
    std::string_view source;
    std::string_view::size_type returnOffset;
  };

  using ScanStack = std::vector<ScanSection>;

  struct ScanStackCursor {
    Offset offset;
    ScanStack::size_type level;
  };

  IReportError& errorOut;
  CharDecodeFunc& decodeFunc;
  ScanStack scanStack;
  std::string input;
  Offset offset;

  std::unordered_set<MacroDefinition> macroDefs;

 public:
  PreprocessingLexer(std::string inputStr, CharDecodeFunc& decodeFunc,
                     IReportError& errorOut)
      : input(std::move(inputStr)), decodeFunc(decodeFunc), errorOut(errorOut) {
    std::cout << inputStr << std::endl;
    // It's the root, it doesn't have a return type.
    ScanSection rootSection{ScanSectionSourceKind::SourceFile,
                            std::string_view(input)};
    scanStack.push_back(rootSection);
  };

  Token getToken();
  Token peekToken();
  bool isEof();

 private:
  void enterSection(ScanSection);
  void exitSection();
  std::optional<Punctuator> scanPunctuator();

  CharDecodeResult::CharType getChar(const ScanSection&, Offset&) const;
  CharDecodeResult getCharDecodeResult(const ScanSection&, Offset&) const;

  // The MSVC's std::isspace will throw a runtime error when we pass a codepoint
  // that is larger than 255. We have to write our own version of isspace here
  // to avoid this error.
  bool isSpace(int ch) {
    return ch == ' ' || ch == '\f' || ch == '\n' || ch == '\r' || ch == '\t' ||
           ch == '\v';
  }
  bool isDirectiveSpace(int ch) { return ch == ' ' || ch == '\t'; }
  bool isNewlineCharacter(int ch) { return ch == '\r' || ch == '\n'; }

  bool isStartOfIdentifier(int ch) {
    return ch == '_' || ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z';
  }

  ScanStackCursor createScanStackItemStackCursor() {
    return {offset, scanStack.size() - 1};
  }
};

template <CharDecodeFunc CharDecodeFunc>
Token PreprocessingLexer<CharDecodeFunc>::getToken() {
  if (scanStack.empty()) return Eof{};

  if (std::optional<Punctuator> punctuator = scanPunctuator()) {
    return *punctuator;
  }

  return InvalidToken{{scanStack.back().source.substr(offset)}};
}

template <CharDecodeFunc CharDecodeFunc>
std::optional<Punctuator> PreprocessingLexer<CharDecodeFunc>::scanPunctuator() {
  if (scanStack.empty()) return std::nullopt;

  ScanSection& currentScanSection = scanStack.back();

  Offset cursor = this->offset;
  char ch[3];
  for (int i = 0; i < 3; i++) {
    if (currentScanSection.source.size() == cursor) {
      ch[i] == '\0';
    } else {
      ch[i] = getChar(currentScanSection, cursor);
    }
  }

  size_t len;
  PunctuatorKind kind;
  bool isPunctuator = true;

  switch (c0) {
    case '[':
      len = 1;
      kind = PunctuatorKind::LBracket;
      break;
    case ']':
      len = 1;
      kind = PunctuatorKind::RBracket;
      break;
    case '(':
      len = 1;
      kind = PunctuatorKind::LParen;
      break;
    case ')':
      len = 1;
      kind = PunctuatorKind::RParen;
      break;
    case '{':
      len = 1;
      kind = PunctuatorKind::LBrace;
      break;
    case '}':
      len = 1;
      kind = PunctuatorKind::RBrace;
      break;
    case '?':
      len = 1;
      kind = PunctuatorKind::Question;
      break;
    case ':':
      len = 1;
      kind = PunctuatorKind::Colon;
      break;
    case ';':
      len = 1;
      kind = PunctuatorKind::Semicolon;
      break;
    case ',':
      len = 1;
      kind = PunctuatorKind::Comma;
      break;
    case '~':
      len = 1;
      kind = PunctuatorKind::Tilde;
      break;

    case '^':
      if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::BitwiseXorEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::BitwiseXor;
      }
      break;
    case '.':
      if (ch[1] == '.' && ch[2] == '.') {
        len = 3;
        kind = PunctuatorKind::Ellipsis;
      } else {
        len = 1;
        kind = PunctuatorKind::Dot;
      }
      break;
    case '+':
      if (ch[1] == '+') {
        len = 2;
        kind = PunctuatorKind::PlusPlus;
      } else if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::PlusEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::Plus;
      }
      break;
    case '-':
      if (ch[1] == '-') {
        len = 2;
        kind = PunctuatorKind::MinusMinus;
      } else if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::MinusEqual;
      } else if (ch[1] == '>') {
        len = 2;
        kind = PunctuatorKind::Arrow;
      } else {
        len = 1;
        kind = PunctuatorKind::Minus;
      }
      break;
    case '&':
      if (ch[1] == '&') {
        len = 2;
        kind = PunctuatorKind::LogicalAnd;
      } else if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::BitwiseAndEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::BitwiseAnd;
      }
      break;
    case '|':
      if (ch[1] == '|') {
        len = 2;
        kind = PunctuatorKind::LogicalOr;
      } else if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::BitwiseOrEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::BitwiseOr;
      }
      break;
    case '*':
      if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::MultiplyEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::Star;
      }
      break;
    case '/':
      if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::DivideEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::Slash;
      }
      break;
    case '%':
      if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::ModuloEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::Modulo;
      }
      break;
    case '=':
      if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::Equal;
      } else {
        len = 1;
        kind = PunctuatorKind::Assign;
      }
      break;
    case '!':
      if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::NotEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::LogicalNot;
      }
      break;
    case '<':
      if (ch[1] == '<') {
        if (ch[2] == '=') {
          len = 3;
          kind = PunctuatorKind::LShiftEqual;
        } else {
          len = 2;
          kind = PunctuatorKind::LShift;
        }
      } else if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::LessOrEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::LessThan;
      }
      break;
    case '>':
      if (ch[1] == '>') {
        if (ch[2] == '=') {
          len = 3;
          kind = PunctuatorKind::RShiftEqual;
        } else {
          len = 2;
          kind = PunctuatorKind::RShift;
        }
      } else if (ch[1] == '=') {
        len = 2;
        kind = PunctuatorKind::GreaterOrEqual;
      } else {
        len = 1;
        kind = PunctuatorKind::GreaterThan;
      }
      break;
    case '#':
      if (ch[1] == '#') {
        len = 2;
        kind = PunctuatorKind::HashHash;
      } else {
        len = 1;
        kind = PunctuatorKind::Hash;
      }
      break;
    default:
      isPunctuator = false;
  }

  if (isPunctuator) {
    this->offset += len;
    return Punctuator{
        kind, std::string_view(currentScanSection.source).substr(offset, len)};
  } else {
    return std::nullopt;
  }
}

template <CharDecodeFunc CharDecodeFunc>
CharDecodeResult::CharType PreprocessingLexer<CharDecodeFunc>::getChar(
    const ScanSection& section, Offset& offset) const {
  return getCharDecodeResult(section, offset).codepoint;
}

template <CharDecodeFunc CharDecodeFunc>
inline CharDecodeResult PreprocessingLexer<CharDecodeFunc>::getCharDecodeResult(
    const ScanSection& section, Offset& offset) const {
  assert(offset < scanSection.source.size());
  const auto result = decodeFunc(&section.source[offset]);
  offset += result.length;
  return result;
}

template <CharDecodeFunc CharDecodeFunc>
bool PreprocessingLexer<CharDecodeFunc>::isEof() {
  return scanStack.empty();
}

#endif  // !TPLCC_PP_H