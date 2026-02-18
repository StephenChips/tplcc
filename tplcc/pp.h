#ifndef TPLCC_PP_H
#define TPLCC_PP_H

#include <array>
#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>

#include "error.h"

using UTF32Char = char32_t;

enum class KeywordKind {
  Bool,
  Complex,
  Imaginary,
  Auto,
  Break,
  Case,
  Char,
  Const,
  Continue,
  Default,
  Do,
  Double,
  Else,
  Enum,
  Extern,
  Float,
  For,
  Goto,
  If,
  Inline,
  Int,
  Long,
  Register,
  Restrict,
  Return,
  Signed,
  Sizeof,
  Static,
  Struct,
  Switch,
  Typedef,
  Union,
  Unsigned,
  Void,
  Volatile,
  While
};

enum class PunctuatorKind {
  LeftBracket,
  Rightbracket,
  LeftParenthesis,
  RightParenthesis,
  LeftBrace,
  RightBrace,
  Dot,
  Arrow,
  SelfIncrement,
  SelfDecrement,
  Ampersand,
  Star,
  Add,
  Minus,
  BitwiseNot,
  Not,
  Divide,
  Modulo,
  LeftShift,
  RightShift,
  LessThan,
  GreaterThan,
  LessOrEqual,
  GreaterOrEqual,
  Equal,
  NotEqual,
  BitwiseXor,
  BitwiseOr,
  LogicalAnd,
  QuestionMark,
  Colon,
  Semicolon,
  DotDotDot,
  Assign,
  MultiplyAssign,
  DivideAssign,
  ModuloAssign,
  AddAssign,
  MinusAssign,
  LeftShiftAssign,
  RightShiftAssign,
  BitwiseAndAssign,
  BitwiseXorAssign,
  BitwiseOrAssign,
  Comma
};

struct Keyword {
  KeywordKind kind;
  std::string_view range;
};

struct Identifier {
  std::string_view range;
};

struct StringLiteral {
  std::string_view range;
};

struct IntegerConstant {
  std::string_view range;
};

struct FloatingConstant {
  std::string_view range;
};

struct CharacterConstant {
  std::string_view range;
};

struct Punctuator {
  PunctuatorKind kind;
  std::string_view range;
};

struct Eof {};

using Token = std::variant<
  Keyword,
  Identifier,
  StringLiteral,
  IntegerConstant,
  FloatingConstant,
  CharacterConstant,
  Punctuator,
  Eof
>;

template <typename T>
concept CharDecoder = requires(T decoder, const unsigned char* addr) {
  { decoder.decode(addr) } -> std::same_as<UTF32Char>;
};

enum class MacroType { ObjectLikeMacro, FunctionLikeMacro };

struct MacroDefinition {
  MacroType type;
  std::string name;
  std::vector<std::string> parameters;
  std::string body;

  MacroDefinition(std::string name, std::string body,
                  MacroType type = MacroType::ObjectLikeMacro)
      : name(std::move(name)), body(std::move(body)), type(type) {};

  MacroDefinition(std::string name, std::vector<std::string> parameters,
                  std::string body,
                  MacroType type = MacroType::FunctionLikeMacro)
      : name(std::move(name)),
        body(std::move(body)),
        parameters(std::move(parameters)),
        type(type) {};

  bool operator==(const MacroDefinition& other) const {
    return this->name == other.name;
  }
};

namespace std {
template <>
struct hash<MacroDefinition> {
  size_t operator()(const MacroDefinition& macroDef) const noexcept {
    return std::hash<std::string>{}(macroDef.name);
  }
};
}  // namespace std

template <CharDecoder CharDecoder>
class PreprocessingLexer {
  using Offset = uint64_t;

  struct ScanStackItem {
    enum { SourceFile, MacroBody, MacroArgument } type;
    std::string_view range;
    std::string_view::size_type returnOffset;
  };

  using ScanStack = std::vector<ScanStackItem>;

  struct ScanStackCursor {
    Offset offset;
    ScanStack::size_type level;
  };

  IReportError& errorOut;
  CharDecoder decoder;
  ScanStack scanStack;
  std::string input;
  Offset offset;

  std::unordered_set<MacroDefinition> macroDefs;

 public:
  PreprocessingLexer(const std::string input, CharDecoder decoder,
               IReportError& errorOut)
      : input(std::move(input)),
        decoder(std::move(decoder)),
        errorOut(errorOut) {
    // It's the root, it doesn't have a return type.
    ScanStackItem rootSection{ScanStackItem::SOURCE_FILE,
                              std::string_view(input)};
    scanSectionStack.push_back(rootSection);
  };

  Token get();
  Token peek();
  bool isEof();

  static constexpr UTF32Char EOF = static_cast<UTF32Char>(-1);

 private:
  void enterSection(ScanStackItem);
  void exitSection();

  UTF32Char getChar(Offset&) const;
  UTF32Char getChar(ScanStackCursor&) const;
  UTF32Char getChar();

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
    return {offset, scanSectionStack.size() - 1};
  }
};

template <CharDecoder CharDecoder>
Token PreprocessingLexer<CharDecoder>::get() {
  if (isEof()) return Eof{};
}

template <CharDecoder CharDecoder>
bool PreprocessingLexer<CharDecoder>::isEof() {
  return scanStack.empty();
}

#endif  // !TPLCC_PP_H