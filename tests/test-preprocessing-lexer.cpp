#pragma execution_character_set("utf-8")

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <numeric>
#include <string_view>

#include "mocking/report-error-stub.h"
#include "tplcc/encoding.h"
#include "tplcc/preprocessing-lexer.h"

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
  std::unique_ptr<ReportErrorStub> errOut;
  std::unique_ptr<PreprocessingLexer<decltype(decodeUTF8)>> pplex;

  std::vector<Token> scanInput(const std::string& inputStr) {
    setUpPreprocessor(inputStr);
    return exhaustTokens();
  }

  void setUpPreprocessor(const std::string& inputStr) {
    errOut = std::make_unique<ReportErrorStub>();
    pplex = std::make_unique<PreprocessingLexer<decltype(decodeUTF8)>>(
        inputStr, decodeUTF8, *errOut);
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