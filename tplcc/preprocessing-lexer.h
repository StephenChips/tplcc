#ifndef TPLCC_PP_H
#define TPLCC_PP_H

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "diagnostic.h"

/**
 * TODO Need a complete redesign of the source location system.
 * Currently the source code location in error outputs is not all correct.
 */

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

// 主模板：检测类型 T 是否是 Variant 的子类型
template <typename T, typename Variant>
struct IsAlternativeT : std::false_type {};

// 特化：如果 T 是 Variant 的某个子类型
template <typename T, typename... Types>
struct IsAlternativeT<T, std::variant<Types...>>
    : std::disjunction<std::is_same<T, Types>...> {};

template <typename T, typename Variant>
concept IsAlternativeOf = IsAlternativeT<T, Variant>::value;

enum class MacroKind { ObjectLikeMacro, FunctionLikeMacro };

struct MacroDef {
  MacroKind kind;
  std::string name;
  std::vector<std::string> parameters;
  std::string body;
};

struct ScanSection {
  std::string_view text;
  size_t offset;
};

struct FileFrame {
  std::string buffer;
  ScanSection section;
  bool isAtLineStart;
};

struct MacroFrame {
  MacroDef* def;  // a pointer to the definition of the macro
  std::vector<std::string> arguments;
  // text after `#` and `##` evaluation.
  // Maybe it should be a vector of tokens?
  std::string buffer;
  ScanSection section;  // the section.text is the buffer
};

struct DecodeUTF8Result {
  char32_t codepoint;
  unsigned long charlen;
};

DecodeUTF8Result decodeUTF8(const char* buffer);
void encodeUTF8(std::string& str, char32_t cp);

inline std::string_view slice(std::string_view& sv, size_t start, size_t end) {
  return sv.substr(start, end - start);
}

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
  std::string text;
  bool operator==(const Keyword&) const = default;
};

struct Identifier {
  std::string text;
  bool operator==(const Identifier&) const = default;
};

enum class EncodingPrefix { L, None };

struct StringLiteral {
  bool isValid;
  std::string text;
  bool operator==(const StringLiteral&) const = default;
};

enum class InvalidNumberReason {
  None,
  InvalidOctalDigit,
  InvalidSuffix,
  ExponentNoDigit,
  HexFloatNoExponent
};

struct IntegerConstant {
  bool isValid;
  InvalidNumberReason invalidReason;
  std::string text;
  bool operator==(const IntegerConstant&) const = default;
};

struct PPNumber {
  std::string text;
  bool operator==(const PPNumber&) const = default;
};

struct HeaderName {
  std::string text;
};

struct FloatingConstant {
  bool isValid;
  InvalidNumberReason invalidReason;
  std::string text;
  bool operator==(const FloatingConstant&) const = default;
};

struct CharacterConstant {
  bool isValid;
  std::string text;
  bool operator==(const CharacterConstant&) const = default;
};

struct Punctuator {
  PunctuatorKind kind;
  std::string text;
  bool operator==(const Punctuator&) const = default;
};

struct InvalidToken {
  std::string text;
  bool operator==(const InvalidToken&) const = default;
};

struct EofToken {
  // A EofToken's text carries no meaning.
  // It exists solely to simplify to process of handling token's text.
  std::string text;
  bool operator==(const EofToken&) const = default;
};

using ScanStackFrame = std::variant<FileFrame, MacroFrame>;

using Token = std::variant<Keyword, Identifier, StringLiteral, FloatingConstant,
                           IntegerConstant, CharacterConstant, Punctuator,
                           InvalidToken, EofToken>;

using PreprocessingToken =
    std::variant<HeaderName, Identifier, PPNumber, CharacterConstant,
                 Punctuator, StringLiteral, Keyword, InvalidToken>;

std::string& getTokenText(PreprocessingToken& token);
std::string& getTokenText(Token& token);

class PreprocessingLexer {
  Diagnostic& diagnostics;
  std::vector<ScanStackFrame> scanStack;

  std::map<std::string, MacroDef> macroDefDict;

  ScanSection& getScanSection(ScanStackFrame& frame);

  bool popFrame();

  bool pushFileFrame(std::string text);

  std::optional<Punctuator> scanPunctuator(ScanSection& section);

  char32_t getChar(ScanSection& section, bool willSkipBackslashNewlines = true);

  char32_t getCharAtOffset(const ScanSection& section, size_t& offset,
                           bool willSkipBackslashNewlines = true);

  char32_t peekChar(const ScanSection& section, size_t* endOffset = nullptr,
                    bool willSkipBackslashNewlines = true);

  char32_t peekCharAtOffset(const ScanSection& section, size_t offset,
                            size_t* endOffset = nullptr,
                            bool willSkipBackslashNewlines = true);

  struct ResultOfScanPPNumberText {
    std::string_view text;
    bool hasInvalidUCN;  // UCN: Universal Character Name
  };

  ResultOfScanPPNumberText scanPPNumberText(ScanSection& section);

  PreprocessingToken scanPreprocessingTokenInsideDreictive(
      ScanSection& section, bool enableParseHeaderName = false);

  bool isMatchNewline(const ScanSection& section, size_t* endOffset = nullptr);

  bool isMatchSpace(const ScanSection& section, size_t* endOffset = nullptr);

  bool isMatchNonNewlineSpace(const ScanSection& section,
                              size_t* endOffset = nullptr);

  bool isMatchString(const ScanSection& section, const char* s,
                     size_t* endOffset = nullptr);

  void skipBackslashNewlines(ScanSection& section);

  void skipBackslashNewlinesAtOffset(const ScanSection& section,
                                     size_t& offset);

  void skipDirectiveSpacesAndComments(ScanSection& section);

  template <IsAlternativeOf<ScanStackFrame> T>
  void skipSpacesAndComments(T& section);

  void skipSpacesAndComments(ScanStackFrame& frame) {
    std::visit([this](auto& frame) { skipSpacesAndComments(frame); }, frame);
  }

  bool isMatchIdentifierNonDigitCharacter(const ScanSection& section,
                                          size_t* endOffset = nullptr);

  bool skipQuotedLiteralContent(ScanSection& section,
                                EncodingPrefix encodingPrefix);
  struct NumberTextScanResult {
    enum class NumberKind { Float, Int, PPNumber, Invalid } kind;
    enum class RadixKind { Dec, Hex, Oct } radix;
    std::string_view text;
    InvalidNumberReason invalidReason;
    std::string_view invalidSuffix;
    bool hasInvalidUCN;  // UCN: Universal Character Name
  };

  NumberTextScanResult scanNumberText(ScanSection& section);

  bool isValidUniversalCharacterNameCodepoint(char32_t ch);

  bool skipUniversalCharacterNameHexQuad(ScanSection& section, char32_t ch,
                                         size_t ucnStart);

  std::optional<char32_t> parseUniversalCharacterNameHexQuad(
      const ScanSection& section, char32_t ch, size_t ucnStart,
      size_t* endOffset);

  bool isIdentifierNonDigitCharacter(char32_t codepoint);

  bool isIdentifierCharacter(char32_t ch);

  bool isIdentifier(std::string_view sv);

  bool isMatchIdentifierCharacter(const ScanSection& section,
                                  size_t* endOffset);

  bool isMatchIdentifier(ScanSection section, size_t* endOffset);

  void skipToNextLine(ScanSection& section);

  bool validateHashOperator(const MacroDef& def);

  /**
   * Because the macro body is always valid, some checks are omitted.
   * For example, there is no need to check if a # operator is followed by a
   * non-identifier token, or if a ## appears at either end of the macro
   * body.
   *
   * The output is a utf-8 string.
   *
   * --
   *
   * TODO
   *
   * I used to think preserving spaces and comments would simplify source
   * location tracking, because deleting them causes tokens to be shifted
   * earlier and would require additional mapping to recover original positions.
   * However, the '#' and '##' operators themselves generate new tokens and
   * insert them into the text, which inherently shift the positions of
   * subsequent tokens. Therefore preserving spaces and comments does not
   * meaningfully simplify location tracking.
   *
   * I should figure out how to do this mapping, maybe the data structure
   * that stores the text needs updated.
   */

  class HashOperatorEvaluator {
    PreprocessingLexer& pplex;
    const MacroDef& def;
    const std::vector<std::string>& arguments;
    bool enableParseHeaderName = false;

    HashOperatorEvaluator(PreprocessingLexer& pplex, const MacroDef& def,
                          const std::vector<std::string>& argumement,
                          bool enableParseHeaderName = false)
        : pplex(pplex),
          def(def),
          arguments(argumement),
          enableParseHeaderName(enableParseHeaderName) {};

    std::string getNextTokenText(ScanSection& section);

    std::string stringize(ScanSection& section);

    bool isValidTokenText(const std::string& text);

   public:
    std::string evaluate();
  };

  void scanDirective(ScanSection& section);

  bool canScanDirective();

 public:
  PreprocessingLexer(std::string input, Diagnostic& diagnostics)
      : diagnostics(diagnostics) {
    pushFileFrame(std::move(input));
  };

  Token getToken();

  Token peekToken();

  bool isEof();
};

template <IsAlternativeOf<ScanStackFrame> T>
void PreprocessingLexer::skipSpacesAndComments(T& frame) {
  ScanSection& section = frame.section;

  size_t endOffset;
  while (section.offset < section.text.size()) {
    if (isMatchNewline(section, &endOffset)) {
      section.offset = endOffset;
      if constexpr (std::is_same_v<T, FileFrame>) {
        frame.isAtLineStart = true;
      }
    } else if (isMatchNonNewlineSpace(section, &endOffset)) {
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
#endif  // !TPLCC_PP_H