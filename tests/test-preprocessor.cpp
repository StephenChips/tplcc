#include <gtest/gtest.h>

#include <memory>

#include "./mocking/report-error-stub.h"
#include "tplcc/code-buffer.h"
#include "tplcc/preprocessor.h"

class TestPreprocessor : public ::testing::Test {
  std::unique_ptr<ReportErrorStub> errOut;
  std::unique_ptr<Preprocessor> pp;
  std::unique_ptr<CodeBuffer> codeBuffer;

 protected:
  std::string scanInput(const std::string& inputStr) {
    codeBuffer = std::make_unique<CodeBuffer>(inputStr);
    errOut = std::make_unique<ReportErrorStub>();
    pp = std::make_unique<Preprocessor>(*codeBuffer, *errOut);

    return exhaustPreprocessor();
  }

 private:
  std::string exhaustPreprocessor() {
    std::string output;

    while (!pp->reachedEndOfInput()) {
      output += pp->get();
    }

    return output;
  }
};

TEST_F(TestPreprocessor, multiple_blankspaces_char_will_be_collapsed_into_one) {
  EXPECT_EQ(scanInput("FOO  \r\n   \t  BAR"), "FOO BAR");
}

TEST_F(TestPreprocessor, define_object_macro) {
  const auto expanded = scanInput(
      "#define FOO 1 \n"
      "int a = FOO");
  EXPECT_EQ(expanded, "int a = 1");
}
