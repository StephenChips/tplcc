#include <memory>
#include <gtest/gtest.h>

#include "./mocking/report-error-stub.h"
#include "./mocking/simple-string-scanner.h"
#include "tplcc/preprocessor.h"



class TestPreprocessor: public ::testing::Test {
  std::unique_ptr<SimpleStringScanner> scanner;
  std::unique_ptr<CodeBuffer> cb;
  std::unique_ptr<ReportErrorStub> errOut;
  std::unique_ptr<Preprocessor> pp;

protected:
  std::string scanInput(const std::string& inputStr) {
    scanner = std::make_unique<SimpleStringScanner>(inputStr);
    cb = std::make_unique<CodeBuffer>();
    errOut = std::make_unique<ReportErrorStub>();
    pp = std::make_unique<Preprocessor>(*scanner, *cb, *errOut);

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

TEST_F(TestPreprocessor, define_object_macro) {
  const auto expanded = scanInput(
      "#define FOO 1 \n"
      "int a = FOO"
  );
  EXPECT_EQ(expanded, "int a = 1");
}
