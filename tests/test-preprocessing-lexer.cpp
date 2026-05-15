#pragma execution_character_set("utf-8")

#include <gtest/gtest.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#include <string_view>

#include "tplcc/encoding.h"
#include "tplcc/preprocessing-lexer.h"

/**
 * TODO Miss tests for diagnstic output
 */

struct DiagnosticStub : Diagnostic {
  virtual void report(DiagnosticInfo info) {}
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

class TestPreprocessingLexer : public ::testing::Test {
 protected:
  std::unique_ptr<PreprocessingLexer<decltype(decodeUTF8)>> pplex;
  std::unique_ptr<Diagnostic> diag;

  std::vector<Token> scanInput(const std::string& inputStr) {
    setUpPreprocessor(inputStr);
    return exhaustTokens();
  }

  void setUpPreprocessor(std::string inputStr) {
    diag = std::make_unique<DiagnosticStub>();
    pplex = std::make_unique<PreprocessingLexer<decltype(decodeUTF8)>>(
        inputStr, decodeUTF8, *diag);
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
  // TODO Add more tests
  std::vector<std::string> identifiers{"foo", "_foo", "Foo", "foo12",
                                       "fo\\\no"};

  std::string input;
  for (size_t i = 0; i < identifiers.size(); i++) {
    input += identifiers[i];
    input += " ";
  }

  setUpPreprocessor(input);

  for (const auto id : identifiers) {
    auto token = pplex->getToken();
    auto ident = std::get_if<Identifier>(&token);
    if (ident) {
      ASSERT_EQ(ident->text, id);
    } else {
      FAIL() << "Expect an identifer " << id << std::endl;
    }
  }

  EXPECT_TRUE(pplex->isEof());
  EXPECT_TRUE(std::holds_alternative<EofToken>(pplex->getToken()));
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
  auto testStringLiteral = [&, this](StringLiteral expected) {
    setUpPreprocessor(std::string(expected.text));

    Token token = pplex->getToken();
    StringLiteral* actual = std::get_if<StringLiteral>(&token);

    if (actual) {
      EXPECT_EQ(actual->isValid, expected.isValid);
      EXPECT_EQ(actual->text, expected.text);
    } else {
      FAIL() << "expected a string literal " << expected.text << std::endl;
    }
  };

  testStringLiteral({true, "\"a\""});
  testStringLiteral({true, "\"你 😀\""});

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
