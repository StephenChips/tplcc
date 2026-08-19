#pragma execution_character_set("utf-8")

#include <gtest/gtest.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <string_view>

#include "tplcc/preprocessing-lexer.h"

/**
 * TODO Miss tests for diagnstic output
 */

struct DiagnosticStub : Diagnostic {
  std::vector<DiagnosticInfo> collectedInfos;
  virtual void report(DiagnosticInfo info) { collectedInfos.push_back(info); }
};

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

std::ostream& operator<<(std::ostream& os, Identifier identifier) {
  return os << "Identifier { text = " << identifier.text
            << "; name = " << identifier.name << "; };" << std::endl;
}

std::ostream& operator<<(std::ostream& os, Punctuator punct) {
  return os << "Punctuator { kind = " << punct.kind << "; text = \""
            << punct.text << "\"; };" << std::endl;
}

std::ostream& operator<<(std::ostream& os, InvalidToken token) {
  return os << "InvalidToken { text = \"" << token.text << "\"; }" << std::endl;
}

class TestPreprocessingLexer : public ::testing::Test {
 protected:
  std::unique_ptr<PreprocessingLexer> pplex;
  std::unique_ptr<DiagnosticStub> diag;

  std::vector<Token> scanInput(const std::string& inputStr) {
    setUpPreprocessor(inputStr);
    return exhaustTokens();
  }

  void setUpPreprocessor(std::string inputStr) {
    diag = std::make_unique<DiagnosticStub>();
    pplex = std::make_unique<PreprocessingLexer>(inputStr, *diag);
  }

 private:
  std::vector<Token> exhaustTokens() {
    std::vector<Token> tokens;

    while (!pplex->isEof()) {
      tokens.push_back(pplex->getToken());
    }

    return tokens;
  }
};

TEST_F(TestPreprocessingLexer, test_empty_input) {
  setUpPreprocessor("");
  EXPECT_TRUE(pplex->isEof());
  EXPECT_TRUE(std::holds_alternative<EofToken>(pplex->getToken()));
}

TEST_F(TestPreprocessingLexer, test_punctuators) {
  struct PunctuatorPair {
    PunctuatorKind kind;
    const char* str;
  };

  std::vector<PunctuatorPair> punctuatorPairs{
      {PunctuatorKind::LParen, "("},
      {PunctuatorKind::BitwiseAndAssign, "&="},
      {PunctuatorKind::GreaterOrEqual, ">="},
      {PunctuatorKind::LShift, "<<"},
      {PunctuatorKind::BitwiseXorAssign, "^="},
      {PunctuatorKind::LogicalAnd, "&&"},
      {PunctuatorKind::Hash, "%:"},
      {PunctuatorKind::LessThan, "<"},
      {PunctuatorKind::Semicolon, ";"},
      {PunctuatorKind::RShift, ">>"},
      {PunctuatorKind::Modulo, "%"},
      {PunctuatorKind::DivideAssign, "/="},
      {PunctuatorKind::Ellipsis, "..."},
      {PunctuatorKind::Slash, "/"},
      {PunctuatorKind::HashHash, "%:%:"},
      {PunctuatorKind::Arrow, "->"},
      {PunctuatorKind::Dot, "."},
      {PunctuatorKind::Minus, "-"},
      {PunctuatorKind::RBracket, "]"},
      {PunctuatorKind::GreaterThan, ">"},
      {PunctuatorKind::MinusAssign, "-="},
      {PunctuatorKind::RParen, ")"},
      {PunctuatorKind::PlusPlus, "++"},
      {PunctuatorKind::Star, "*"},
      {PunctuatorKind::HashHash, "##"},
      {PunctuatorKind::Equal, "=="},
      {PunctuatorKind::LBracket, "<:"},
      {PunctuatorKind::LBracket, "["},
      {PunctuatorKind::MinusMinus, "--"},
      {PunctuatorKind::PlusAssign, "+="},
      {PunctuatorKind::Comma, ","},
      {PunctuatorKind::BitwiseOrAssign, "|="},
      {PunctuatorKind::RBrace, "}"},
      {PunctuatorKind::NotEqual, "!="},
      {PunctuatorKind::LBrace, "{"},
      {PunctuatorKind::LessOrEqual, "<="},
      {PunctuatorKind::RShiftAssign, ">>="},
      {PunctuatorKind::Colon, ":"},
      {PunctuatorKind::Plus, "+"},
      {PunctuatorKind::BitwiseOr, "|"},
      {PunctuatorKind::Hash, "#"},
      {PunctuatorKind::MultiplyAssign, "*="},
      {PunctuatorKind::LShiftAssign, "<<="},
      {PunctuatorKind::Question, "?"},
      {PunctuatorKind::LBrace, "<%"},
      {PunctuatorKind::RBracket, ":>"},
      {PunctuatorKind::RBrace, "%>"},
      {PunctuatorKind::LogicalNot, "!"},
      {PunctuatorKind::LogicalOr, "||"},
      {PunctuatorKind::BitwiseXor, "^"},
      {PunctuatorKind::ModuloAssign, "%="},
      {PunctuatorKind::Assign, "="},
      {PunctuatorKind::BitwiseNot, "~"},
      {PunctuatorKind::BitwiseAnd, "&"},
  };

  std::string input;
  for (auto pair : punctuatorPairs) {
    input += pair.str;
    input += " ";
  }

  setUpPreprocessor(input);

  for (auto pair : punctuatorPairs) {
    size_t len = std::strlen(pair.str);
    Token token = pplex->getToken();

    if (std::holds_alternative<EofToken>(token)) {
      FAIL() << "lexer reaches EOF too early.";
      return;
    }

    auto* punctuator = std::get_if<Punctuator>(&token);
    if (punctuator == nullptr) {
      FAIL() << "lexer should scan a punctuator " << pair.str << "\n";
    }

    ASSERT_EQ(punctuator->kind, pair.kind);
    ASSERT_EQ(punctuator->text, std::string{pair.str});
  }

  EXPECT_TRUE(pplex->isEof());
  EXPECT_TRUE(std::holds_alternative<EofToken>(pplex->getToken()));
}

TEST_F(TestPreprocessingLexer, test_comments) {
  std::string input =
      "// const std::str\\\ning a = \"123\"\n"
      "/* ### this is a comment            \n"
      "   a comment is what it     is !+=*/\n"
      "+\\\n\\\n\\\n\\\n=\n";

  setUpPreprocessor(input);
  Token token = pplex->getToken();

  if (auto* punctuator = std::get_if<Punctuator>(&token)) {
    EXPECT_EQ(punctuator->kind, PunctuatorKind::PlusAssign);
    EXPECT_EQ(punctuator->text, std::string{"+\\\n\\\n\\\n\\\n="});
  } else {
    FAIL() << "expect to scan a " << PunctuatorKind::PlusAssign;
  }
}

TEST_F(TestPreprocessingLexer, test_identifiers) {
  auto testIdentifier = [this](Identifier input) {
    setUpPreprocessor(input.text);
    auto token = pplex->getToken();
    if (auto ident = std::get_if<Identifier>(&token)) {
      ASSERT_EQ(ident->text, input.text);
      ASSERT_EQ(ident->name, input.name);
    } else {
      FAIL() << "Expect an identifier " << input << std::endl;
    }
  };

  testIdentifier({"foo", "foo"});
  testIdentifier({"_foo", "_foo"});
  testIdentifier({"foo12", "foo12"});
  testIdentifier({"你好", "你好"});
  testIdentifier({"\\u4f60\\U0000597D", "你好"});
  testIdentifier({"fo\\\no", "foo"});

  // test invalid identifers

  struct ExpectedDiagnostic {
    DiagnosticLevel level;
    std::string message;
  };

  auto testInvalidIdentifierWithUCN =
      [this](std::string input, Token expectedToken,
             std::vector<ExpectedDiagnostic> expectedDiagnostics) {
        setUpPreprocessor(input);

        Token token = pplex->getToken();
        ASSERT_EQ(token, expectedToken);

        ASSERT_EQ(diag->collectedInfos.size(), expectedDiagnostics.size());

        for (int i = 0; i < diag->collectedInfos.size(); i++) {
          ASSERT_EQ(diag->collectedInfos[i].level,
                    expectedDiagnostics[i].level);
          ASSERT_EQ(diag->collectedInfos[i].message,
                    expectedDiagnostics[i].message);
        }
      };

  /**
   * TODO Fix invalid UCN error recovery in the lexer
   *
   * When an invalid universal character name (UCN) is encountered during
   * scanning, the lexer currently gives up immediately and returns the first
   * character of the UCN as an `InvalidToken` where the text is a backslash.
   * It also emits an error message "stray character '\\' in the program".
   *
   * More importantly, because the lexer does not consume the rest of the
   * invalid UCN, the remaining chracter are scanned into several tokens.
   * These tokens shouldn't even be produced, and they can cause the parser
   * to make incorrect parsing decisions and produce more confusing error
   * messages.
   *
   * The lexer should be refractored to recover from invalid UCNs as a unit.
   * Ideally, an invalid token should contain a entire UCN, so does the
   * error message. Depending on the surrounding context, it may be preferable
   * to consume a larger unit, such as the entire identifier that contains the
   * invalid UCN, to provide better error recovery and diagnostics.
   *
   * The same issue also applies to keyword scanning. For example, after
   * scanning "\\u0063onst", an InvalidToken containing "\\u0063" should be
   * returned, or even the entire keyword "\\u0063onst" (codepoint U+0063 is
   * 'c').
   */

  testInvalidIdentifierWithUCN(
      "\\u333z", InvalidToken{std::string{"\\"}},
      {{DiagnosticLevel::Warning, "incomplete universal character name"},
       {DiagnosticLevel::Error, "stray character '\\' in the program"}});

  // A universal character name whose codepoint is bigger than Unicode's maximum
  // codepoint.
  testInvalidIdentifierWithUCN(
      "\\UffffFFFF", InvalidToken{std::string{"\\"}},
      {{DiagnosticLevel::Error,
        "\\UffffFFFF is not a valid universal character name"},
       {DiagnosticLevel::Error, "stray character '\\' in the program"}});

  // Not all unicode can be used within an identifier.
  testInvalidIdentifierWithUCN(
      "\\u00b0", InvalidToken{std::string{"\\"}},
      {{DiagnosticLevel::Error,
        "universal character name \\u00b0 is not valid in an identifier"},
       {DiagnosticLevel::Error, "stray character '\\' in the program"}});

  /**
   * TODO Fix invalid UCN error recovery in the lexer
   *
   * After refactoring these three tests should be merged into one, with the
   * input "\\u005f\\u0063\\u0030".
   */
  testInvalidIdentifierWithUCN(
      "\\u005f" /*  _ */, InvalidToken{std::string{"\\"}},
      {{DiagnosticLevel::Error,
        "universal character name \\u005f is not valid in an identifier"},
       {DiagnosticLevel::Error, "stray character '\\' in the program"}});

  testInvalidIdentifierWithUCN(
      "\\u0063" /* c */, InvalidToken{std::string{"\\"}},
      {{DiagnosticLevel::Error,
        "universal character name \\u0063 is not valid in an identifier"},
       {DiagnosticLevel::Error, "stray character '\\' in the program"}});

  testInvalidIdentifierWithUCN(
      "\\u0030" /* 0 */, InvalidToken{std::string{"\\"}},
      {{DiagnosticLevel::Error,
        "universal character name \\u0030 is not valid in an identifier"},
       {DiagnosticLevel::Error, "stray character '\\' in the program"}});

  // Emojis as identifier are not supported, even though GCC and Clang do.
  testInvalidIdentifierWithUCN(
      "😀", InvalidToken{std::string{"😀"}},
      {{DiagnosticLevel::Error, "stray character '😀' in the program"}});

  /**
   * Worth to noting that UCNs in string literals and character constants are
   * subject to less constraints. For example, even its codepoint excceeded
   * the maximum Unicode codepoint, no diagnostic is reported.
   */
}

TEST_F(TestPreprocessingLexer, test_keywords) {
  struct KeywordPair {
    KeywordKind kind;
    std::string text;
  };

  std::vector<KeywordPair> identifiers{
#define X(PascalName, name) {KeywordKind::PascalName, #name},
      KEYWORDS_X_MACRO_LIST
#undef X
  };

  std::string input;
  for (size_t i = 0; i < identifiers.size(); i++) {
    input += identifiers[i].text;
    input += " ";
  }

  setUpPreprocessor(input);

  for (const auto id : identifiers) {
    auto token = pplex->getToken();
    auto ident = std::get_if<Keyword>(&token);
    if (ident) {
      ASSERT_EQ(ident->kind, id.kind);
      ASSERT_EQ(ident->text, id.text);
    } else {
      FAIL() << "Expect a keyword " << id.kind << std::endl;
    }
  }
}

TEST_F(TestPreprocessingLexer, test_invalid_keyword) {
  setUpPreprocessor("\\u0063onst");

  Token token = pplex->getToken();
  ASSERT_EQ(token, Token{InvalidToken{"\\"}});

  ASSERT_EQ(diag->collectedInfos.size(), 2);

  ASSERT_EQ(diag->collectedInfos[0].level, DiagnosticLevel::Error);
  ASSERT_EQ(diag->collectedInfos[0].message,
            "universal character name \\u0063 is not valid in an identifier");

  ASSERT_EQ(diag->collectedInfos[1].level, DiagnosticLevel::Error);
  ASSERT_EQ(diag->collectedInfos[1].message,
            "stray character '\\' in the program");
}

TEST_F(TestPreprocessingLexer, test_floating_point_constants) {
  auto testFloatingConstant = [&, this](FloatingConstant constant) {
    setUpPreprocessor(std::string(constant.text));

    Token token = pplex->getToken();
    FloatingConstant* actual = std::get_if<FloatingConstant>(&token);
    if (actual) {
      ASSERT_EQ(actual->text, constant.text);
      ASSERT_EQ(actual->isValid, constant.isValid);
      ASSERT_EQ(actual->invalidReason, constant.invalidReason);
    } else {
      FAIL() << "Expect a floating constant " << constant.text << std::endl;
    }
  };

  // Valid - basic decimal
  testFloatingConstant({true, InvalidNumberReason::None, "1.2"});
  testFloatingConstant({true, InvalidNumberReason::None, "1.0"});
  testFloatingConstant({true, InvalidNumberReason::None, "123.456"});
  testFloatingConstant({true, InvalidNumberReason::None, ".5"});
  testFloatingConstant({true, InvalidNumberReason::None, "5."});
  testFloatingConstant({true, InvalidNumberReason::None, "0."});
  testFloatingConstant({true, InvalidNumberReason::None, ".0"});

  // Valid - exponent
  testFloatingConstant({true, InvalidNumberReason::None, "1e10"});
  testFloatingConstant({true, InvalidNumberReason::None, "1E10"});
  testFloatingConstant({true, InvalidNumberReason::None, "1e+10"});
  testFloatingConstant({true, InvalidNumberReason::None, "1e-10"});
  testFloatingConstant({true, InvalidNumberReason::None, "123.456e7"});
  testFloatingConstant({true, InvalidNumberReason::None, ".5e2"});
  testFloatingConstant({true, InvalidNumberReason::None, "5.e-3"});

  // Valid - suffix
  testFloatingConstant({true, InvalidNumberReason::None, "1.0f"});
  testFloatingConstant({true, InvalidNumberReason::None, "1.0F"});
  testFloatingConstant({true, InvalidNumberReason::None, "1.0l"});
  testFloatingConstant({true, InvalidNumberReason::None, "1.0L"});
  testFloatingConstant({true, InvalidNumberReason::None, "123.456e7f"});
  testFloatingConstant({true, InvalidNumberReason::None, ".5L"});

  // Valid - hexadecimal float
  testFloatingConstant({true, InvalidNumberReason::None, "0x1.0p0"});
  testFloatingConstant({true, InvalidNumberReason::None, "0x1.8p1"});
  testFloatingConstant({true, InvalidNumberReason::None, "0x1p10"});
  testFloatingConstant(
      {true, InvalidNumberReason::None, "0x1.921fb54442d18p+1"});
  testFloatingConstant({true, InvalidNumberReason::None, "0x.8p0"});
  testFloatingConstant({true, InvalidNumberReason::None, "0x1.p2"});

  // Invalid - hexadecimal float no exponent part
  testFloatingConstant(
      {false, InvalidNumberReason::HexFloatNoExponent, "0x1.0"});

  // Invalid - exponent has no digit
  testFloatingConstant({false, InvalidNumberReason::ExponentNoDigit, "1e"});
  testFloatingConstant({false, InvalidNumberReason::ExponentNoDigit, "1e+"});
  testFloatingConstant({false, InvalidNumberReason::ExponentNoDigit, "1e+f"});
  // Invalid - exponent has no digit + invalid suffix
  testFloatingConstant(
      {false, InvalidNumberReason::ExponentNoDigit, "1e+ic.3"});

  // Invalid - invalid suffix
  testFloatingConstant({false, InvalidNumberReason::InvalidSuffix, "1.34abc"});
  testFloatingConstant({false, InvalidNumberReason::InvalidSuffix, "1.34ic.3"});
  testFloatingConstant({false, InvalidNumberReason::InvalidSuffix, "1.2.3.4"});
  testFloatingConstant({false, InvalidNumberReason::InvalidSuffix, ".1.2"});
}

TEST_F(TestPreprocessingLexer, test_integer_constants) {
  auto testIntegerConstant = [&, this](IntegerConstant constant) {
    setUpPreprocessor(std::string(constant.text));

    Token token = pplex->getToken();
    IntegerConstant* actual = std::get_if<IntegerConstant>(&token);
    if (actual) {
      ASSERT_EQ(actual->text, constant.text);
      ASSERT_EQ(actual->isValid, constant.isValid);
      ASSERT_EQ(actual->invalidReason, constant.invalidReason);
    } else {
      FAIL() << "Expect an floating constant " << constant.text << std::endl;
    }
  };

  // Valid - basic case
  testIntegerConstant({true, InvalidNumberReason::None, "0"});
  testIntegerConstant({true, InvalidNumberReason::None, "1"});
  testIntegerConstant({true, InvalidNumberReason::None, "123"});
  testIntegerConstant({true, InvalidNumberReason::None, "999999"});

  // Valid - octal digits
  testIntegerConstant({true, InvalidNumberReason::None, "00"});
  testIntegerConstant({true, InvalidNumberReason::None, "0123"});
  testIntegerConstant({true, InvalidNumberReason::None, "0777"});

  // Valid - hexadecimal numbers
  testIntegerConstant({true, InvalidNumberReason::None, "0x0"});
  testIntegerConstant({true, InvalidNumberReason::None, "0x1"});
  testIntegerConstant({true, InvalidNumberReason::None, "0xABC"});
  testIntegerConstant({true, InvalidNumberReason::None, "0xabc"});
  testIntegerConstant({true, InvalidNumberReason::None, "0XFF"});
  // Valid - hexadecimal integers that look like a float.
  testIntegerConstant({true, InvalidNumberReason::None, "0xe3"});
  testIntegerConstant({true, InvalidNumberReason::None, "0x1e3"});

  // Valid - suffix
  testIntegerConstant({true, InvalidNumberReason::None, "1u"});
  testIntegerConstant({true, InvalidNumberReason::None, "1U"});
  testIntegerConstant({true, InvalidNumberReason::None, "1l"});
  testIntegerConstant({true, InvalidNumberReason::None, "1L"});
  testIntegerConstant({true, InvalidNumberReason::None, "1ul"});
  testIntegerConstant({true, InvalidNumberReason::None, "1UL"});
  testIntegerConstant({true, InvalidNumberReason::None, "1lu"});
  testIntegerConstant({true, InvalidNumberReason::None, "1Lu"});
  testIntegerConstant({true, InvalidNumberReason::None, "1ull"});
  testIntegerConstant({true, InvalidNumberReason::None, "1ULL"});
  testIntegerConstant({true, InvalidNumberReason::None, "1llu"});
  testIntegerConstant({true, InvalidNumberReason::None, "1LLu"});
  testIntegerConstant({true, InvalidNumberReason::None, "0777u"});
  testIntegerConstant({true, InvalidNumberReason::None, "0xFFUL"});
  testIntegerConstant({true, InvalidNumberReason::None, "0x10llu"});

  // Invalid - suffix
  testIntegerConstant({false, InvalidNumberReason::InvalidSuffix, "1abc"});
  testIntegerConstant({false, InvalidNumberReason::InvalidSuffix, "1a1bc"});
  testIntegerConstant({false, InvalidNumberReason::InvalidSuffix, "1a.1e+3c"});

  // Invalid - octal digits
  testIntegerConstant({false, InvalidNumberReason::InvalidOctalDigit, "01892"});
}

TEST_F(TestPreprocessingLexer, test_string_literals) {
  auto testStringLiteral = [&, this](std::string expected) {
    setUpPreprocessor(expected);

    Token token = pplex->getToken();
    StringLiteral* actual = std::get_if<StringLiteral>(&token);

    if (actual) {
      EXPECT_EQ(actual->isValid, true);
      EXPECT_EQ(actual->text, expected);
    } else {
      FAIL() << "expected a string literal " << expected << std::endl;
    }
  };

  testStringLiteral("\"a\"");
  testStringLiteral("\"你 😀\"");
  testStringLiteral("L\"hello\"");

  // TODO Add more tests on character escaping and error (diagnostic) output.
}

TEST_F(TestPreprocessingLexer, test_character_constant) {
  auto testCharacterConstant = [&, this](CharacterConstant expected) {
    setUpPreprocessor(std::string(expected.text));

    Token token = pplex->getToken();
    CharacterConstant* actual = std::get_if<CharacterConstant>(&token);

    if (actual) {
      EXPECT_EQ(actual->isValid, expected.isValid);
      EXPECT_EQ(actual->text, expected.text);
    } else {
      FAIL() << "expected a character constant " << expected.text << std::endl;
    }
  };

  testCharacterConstant({true, "'a'"});
  testCharacterConstant({true, "'你'"});

  // TODO Add more tests on character escaping and error (diagnostic) output.
}

#define NOT_FOLLOW_BY_MACRO_PARAMETER          \
  Expected{DiagnosticLevel::Error,             \
           "\"#\" is not followed by a macro " \
           "parameter"}

TEST_F(TestPreprocessingLexer, test_invalid_define_directive) {
  struct Expected {
    DiagnosticLevel expectedLevel;
    std::string expectedMsg;
  };

  auto testInvalid = [&, this](std::string input, Expected expected) {
    setUpPreprocessor(input);

    while (!std::holds_alternative<EofToken>(pplex->getToken()));

    ASSERT_EQ(diag->collectedInfos.size(), 1);
    ASSERT_EQ(diag->collectedInfos[0].level, expected.expectedLevel);
    ASSERT_EQ(diag->collectedInfos[0].message, expected.expectedMsg);
  };

  testInvalid("#define   ",
              {DiagnosticLevel::Error, "incomplete #define directive"});
  testInvalid("#define ###(a) ###",
              {DiagnosticLevel::Error, "macro name must be an identifier"});
  testInvalid("#define A(a,    ",
              {DiagnosticLevel::Error, "incomplete #define directive"});
  testInvalid(
      "#define A(a,    \n"
      "abc",
      {DiagnosticLevel::Error, "incomplete #define directive"});
  testInvalid("#define A(a ",
              {DiagnosticLevel::Error, "incomplete #define directive"});
  testInvalid("#define A(a # ",
              {DiagnosticLevel::Error, "expected ',' or ')', found \"#\""});

  testInvalid("#define A(,,) ",
              {DiagnosticLevel::Error, "expected a parameter name"});

  testInvalid("#define A#",
              {DiagnosticLevel::Warning,
               "ISO C99 requires whitespace after the macro name"});

  testInvalid("#define A(a) #", NOT_FOLLOW_BY_MACRO_PARAMETER);
  testInvalid("#define A(a) #=", NOT_FOLLOW_BY_MACRO_PARAMETER);
  testInvalid("#define A(a) #   =", NOT_FOLLOW_BY_MACRO_PARAMETER);

  testInvalid("#define A ## a",
              {DiagnosticLevel::Error,
               "\"##\" cannot appear at the start of macro expansion"});
  testInvalid("#define A a ##",
              {DiagnosticLevel::Error,
               "\"##\" cannot appear at the end of macro expansion"});

  testInvalid("#define A(a, a)",
              {DiagnosticLevel::Error, "duplicate macro parameter name 'a'"});
}

TEST_F(TestPreprocessingLexer, test_macro_expansion) {
  auto testMacro = [this](std::string input, std::string expandedText) {
    setUpPreprocessor(input);

    std::string str;

    while (!pplex->isEof()) {
      auto token = pplex->getToken();
      str += getTokenText(token);
      if (!pplex->isEof()) {
        str += " ";
      }
    }

    EXPECT_EQ(str, expandedText);
  };

  testMacro(
      "#define FOO FOO\r\n"
      "FOO",
      "FOO");

  testMacro(
      "#define FOO {BAR}\r\n"
      "#define BAR FOO\r\n"
      "FOO",
      "{ FOO }");

  testMacro(
      "#define FOO BAR\r\n"
      "#define BAR FOO\r\n"
      "FOO",
      "FOO");

  testMacro(
      "#define 你好 nihao\r\n"
      "#define hello 你好 \\u4f60\\U0000597D\r\n"
      "hello",
      "nihao nihao");

  testMacro(
      "#define \\u4f60\\U0000597D nihao\r\n"
      "你好\r\n",
      "nihao");

  testMacro(
      "#define int double\r\n"
      "int foo()\r\n",
      "double foo ( )");
}

#undef NOT_FOLLOW_BY_MACRO_PARAMETER