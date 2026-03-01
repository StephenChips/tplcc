#pragma execution_character_set("utf-8")

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <numeric>
#include <string_view>

#include "mocking/report-error-stub.h"
#include "tplcc/encoding.h"
#include "tplcc/preprocessing-lexer.h"

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

    while (pplex->isEof()) {
      tokens.push_back(pplex->getToken());
    }

    return tokens;
  }
};

// TEST_F(TestPreprocessingLexer, test_empty_input) {
//   EXPECT_EQ(scanInput(""), std::vector<Token>{});
// }

TEST_F(TestPreprocessingLexer, test_punctuators) {
  struct PunctuatorPair {
    PunctuatorKind kind;
    const char* str;
  };

  std::vector<PunctuatorPair> punctuatorPairs{
      {PunctuatorKind::LParen, "("},
      {PunctuatorKind::AmpEq, "&="},
      {PunctuatorKind::Ge, ">="},
      {PunctuatorKind::LShift, "<<"},
      {PunctuatorKind::CaretEq, "^="},
      {PunctuatorKind::LogicalAnd, "&&"},
      {PunctuatorKind::HashDigraph, "%:"},
      {PunctuatorKind::Lt, "<"},
      {PunctuatorKind::Semicolon, ";"},
      {PunctuatorKind::RShift, ">>"},
      {PunctuatorKind::Percent, "%"},
      {PunctuatorKind::SlashEq, "/="},
      {PunctuatorKind::Ellipsis, "..."},
      {PunctuatorKind::Slash, "/"},
      {PunctuatorKind::DoubleHashDigraph, "%:%:"},
      {PunctuatorKind::Arrow, "->"},
      {PunctuatorKind::Dot, "."},
      {PunctuatorKind::Minus, "-"},
      {PunctuatorKind::Gt, ">"},
      {PunctuatorKind::RBracket, "]"},
      {PunctuatorKind::MinusEq, "-="},
      {PunctuatorKind::RParen, ")"},
      {PunctuatorKind::PlusPlus, "++"},
      {PunctuatorKind::Star, "*"},
      {PunctuatorKind::DoubleHash, "##"},
      {PunctuatorKind::Eq, "=="},
      {PunctuatorKind::LBracketDigraph, "<:"},
      {PunctuatorKind::LBracket, "["},
      {PunctuatorKind::MinusMinus, "--"},
      {PunctuatorKind::PlusEq, "+="},
      {PunctuatorKind::Comma, ","},
      {PunctuatorKind::PipeEq, "|="},
      {PunctuatorKind::RBrace, "}"},
      {PunctuatorKind::Neq, "!="},
      {PunctuatorKind::LBrace, "{"},
      {PunctuatorKind::Le, "<="},
      {PunctuatorKind::Colon, ":"},
      {PunctuatorKind::RShiftEq, ">>="},
      {PunctuatorKind::Plus, "+"},
      {PunctuatorKind::Pipe, "|"},
      {PunctuatorKind::Hash, "#"},
      {PunctuatorKind::StarEq, "*="},
      {PunctuatorKind::LShiftEq, "<<="},
      {PunctuatorKind::Question, "?"},
      {PunctuatorKind::LBraceDigraph, "<%"},
      {PunctuatorKind::RBracketDigraph, ":>"},
      {PunctuatorKind::RBraceDigraph, "%>"},
      {PunctuatorKind::LogicalNot, "!"},
      {PunctuatorKind::LogicalOr, "||"},
      {PunctuatorKind::PercentEq, "%="},
      {PunctuatorKind::Caret, "^"},
      {PunctuatorKind::Assign, "="},
      {PunctuatorKind::Tilde, "~"},
      {PunctuatorKind::Amp, "&"},
  };

  std::string input;
  for (auto pair : punctuatorPairs) {
    input += pair.str;
  }

  scanInput(input);

  size_t offset = 0;
  for (auto pair : punctuatorPairs) {
    size_t len = std::strlen(pair.str);
    Token token = pplex->getToken();

    if (std::holds_alternative<Eof>(token)) {
      FAIL() << "lexer reaches to the end too early.";
      return;
    }

    auto* punctuator = std::get_if<Punctuator>(&token);
    if (punctuator == nullptr) {
      FAIL() << "lexer should scan a punctuator " << pair.str << "\n";
    }

    EXPECT_EQ(punctuator->kind, pair.kind);
    EXPECT_EQ(punctuator->range, std::string_view(input).substr(offset, len));

    offset += len;
  }
}