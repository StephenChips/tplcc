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

TEST_F(TestPreprocessor, input_nothing) { EXPECT_EQ(scanInput(""), ""); }

TEST_F(TestPreprocessor, multiple_blankspaces_char_will_be_collapsed_into_one) {
  EXPECT_EQ(scanInput("FOO  \r\n   \t  BAR"), "FOO BAR");
}

TEST_F(TestPreprocessor, define_object_macro) {
  // Exitting macro expansion and finishing scanning in a same place,
  // it should be able to handle this situation properly.
  EXPECT_EQ(scanInput("#define FOO 1\n"
                      "int a = FOO"),
  //                              ^ both exit here.
            "int a = 1");

  // Expect it can discern macro name among characters
  EXPECT_EQ(scanInput("#define FOO 1\n"
                      "a=FOO;\n"),
            "a=1; ");

  // Expect it can handle nested expansion and non-macro identifier
  // whose substring is one of the macro's name.
  EXPECT_EQ(scanInput("#define FOO 10\n"
                      "#define BAR FOO  +  FOO  + FOO\n"
                      "#define BUS BARBAR(BAR)\n"
                      "int a = BUS;"),
            "int a = BARBAR(10 + 10 + 10);");

  // define but doesn't use it
  EXPECT_EQ(scanInput("#define FOO 10 "), "");
}

TEST_F(TestPreprocessor, every_comment_will_become_a_space) {
  EXPECT_EQ(scanInput("/*     #if */      #define /* FOO */ FOO /* 3 */ 3\n"
                      "FO/**/O/* */FOO"),
            "FO O 3");
}

TEST_F(TestPreprocessor, backslash_return_should_be_discarded) {
  const auto str = R"(\
#define FOO a =\
            20 \

#define BAR int
BA\
\
\
\
R F\
OO)";

  EXPECT_EQ(scanInput(str), "int a = 20");
}
