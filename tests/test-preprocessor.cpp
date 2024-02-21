#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "./mocking/report-error-stub.h"
#include "./utils/helpers.h"
#include "tplcc/code-buffer.h"
#include "tplcc/preprocessor.h"

class TestPreprocessor : public ::testing::Test {
 protected:
  std::unique_ptr<ReportErrorStub> errOut;
  std::unique_ptr<Preprocessor> pp;
  std::unique_ptr<CodeBuffer> codeBuffer;

  std::string scanInput(const std::string& inputStr) {
    setUpPreprocessor(inputStr);
    return exhaustPreprocessor();
  }

  void setUpPreprocessor(const std::string& inputStr) {
    codeBuffer = std::make_unique<CodeBuffer>(inputStr);
    errOut = std::make_unique<ReportErrorStub>();
    pp = std::make_unique<Preprocessor>(*codeBuffer, *errOut);
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
            //                    ^ both exit here.
            "int a = 1");

  // Expect it can discern macro name among characters
  EXPECT_EQ(scanInput("#define FOO 1\n"
                      "a=FOO;\n\n\n\n\n\n"),
            //               ^
            //               Remember sequence of spaces characters will be
            //               merged into one ' ' (0u0020) and \n is a space
            //               character.
            "a=1; ");
  //             ^
  //             That's why there is a space at the end of the output.

  // Expect it can handle nested expansion and non-macro identifier
  // whose substring is one of the macro's name.
  EXPECT_EQ(scanInput("#define FOO 10\n"
                      "#define BAR FOO  +  FOO  + FOO\n"
                      "#define BUS BARBAR(BAR)\n"
                      "int a = BUS;"),
            "int a = BARBAR(10 + 10 + 10);");

  // define but doesn't use it
  EXPECT_EQ(scanInput("#define FOO 10 "), "");

  // A macro with empty body will be expanded to a space.
  EXPECT_EQ(scanInput("#define EMPTY\n"
                      "EMPTY;"),
            " ;");
}

TEST_F(TestPreprocessor, test_comments_and_spaces) {
  // Consecutive spaces and comments will be replaced by one space.
  EXPECT_EQ(scanInput("/*     #if */        #define/* FOO */    FOO/* 3 */3\n"
                      "FO   /**/O/* */   FOO"),
            "FO O 3");

  // Simliary, multiple rows of line comment will be replaced by one space.
  EXPECT_EQ(scanInput("// line comment\n"
                      "// another line comment\n"
                      "// yet another line comment"),
            " ");
}

TEST_F(TestPreprocessor, backslash_return_should_be_discarded) {
  const auto str = R"(\
#define FOO a =\
            20 \

#define BAR int
BA\
\
\
R F\
OO)";

  EXPECT_EQ(scanInput(str), "int a = 20 ");

  EXPECT_EQ(scanInput("//\\\n"
                      "int a = 3;"),
            " ");
}

TEST_F(TestPreprocessor, directive_should_be_at_the_start_of_the_line) {
  const auto s =
      "int a = 10; #define FOO 10\n"
      "int b = FOO";

  EXPECT_EQ(scanInput(s), "int a = 10; #define FOO 10 int b = FOO");
}

TEST_F(TestPreprocessor, empty_directive_line) {
  const auto s =
      "#       \n"
      "int a = 10;";

  EXPECT_EQ(scanInput(s), "int a = 10;");
}

TEST_F(TestPreprocessor, define_directive_in_a_string) {
  const auto s = "\"#define FOO 1\"";
  EXPECT_EQ(scanInput(s), s);
}

TEST_F(TestPreprocessor, test_encoding) {
  const auto s = fromUTF8(std::u8string{u8"Äã"});
  setUpPreprocessor(s);

  std::vector<std::uint32_t> characters;

  while (!pp->reachedEndOfInput()) {
    characters.push_back(pp->get());
  }

  if (characters == std::vector{0xe4u, 0xbdu, 0xa0u}) {
    FAIL() << "Outputted the multibyte UTF-8 character Äã as three single byte "
              "characters.";
    return;
  }

  ASSERT_EQ(characters.size(), 1);
  EXPECT_EQ(characters[0], 0x4f60);
  EXPECT_EQ(errOut->listOfErrors.empty(), true);
}

TEST_F(TestPreprocessor, define_function_macro) {
  const std::string macroDIV{"#define DIV(foo, bar) ((foo) / (bar))\n"};
  const std::string macroID{"#define ID(x) x\n"};
  const std::string macroMCALL{"#define MCALL(func, x) func(x)\n"};

  /* painted blue */
  // EXPECT_EQ(scanInput("#define R R"), "R");
  // EXPECT_EQ(scanInput("#define R() R()"), "R()");
  // EXPECT_EQ(
  //     scanInput("#define R() V()\n"
  //               "#define V() R()\n"
  //                     "R()"),
  //           "R()");
  // EXPECT_EQ(scanInput("#define R(a) a()\n"
  //                     "R(R)"),
  //           "R()");
  // EXPECT_EQ(scanInput("#define FOO(x) BAR x\n"
  //                     "FOO(FOO)(2)"),
  //           "BAR FOO(2)");

  /* The simplest situation */
  EXPECT_EQ(scanInput(macroDIV + "DIV(4, 3)"), "((4) / (3))");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* A function-like macro that has no body will be expanded as one space. */
  EXPECT_EQ(scanInput("#define EMPTY()\n"
                      "EMPTY()"),
            " ");

  /* Multiple pp-tokens in an argument */
  EXPECT_EQ(scanInput(macroDIV + "DIV(1 + 2+ !foo.bar, 3)"),
            "((1 + 2+ !foo.bar) / (3))");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* The argumnet is a macro expansion too */
  EXPECT_EQ(scanInput(macroDIV + "DIV(DIV(jo,ca), iad)"),
            "((((jo) / (ca))) / (iad))");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* The argument is a function call rather than a macro expansion */
  EXPECT_EQ(scanInput(macroDIV + "DIV(add(biz, biz), biz)"),
            "((add(biz, biz)) / (biz))");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* The macro body includes other macros and has function calls */
  EXPECT_EQ(scanInput(macroDIV + macroID +
                      "#define X(a) DIV(ID(a), 3)\n"
                      "X(10)"),
            "((10) / (3))");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* Handle empty arguments */
  EXPECT_EQ(scanInput(macroID + "ID()"), "");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);
  EXPECT_EQ(scanInput(macroDIV + "DIV(,)"), "(() / ())");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);
  EXPECT_EQ(scanInput("#define T(a,b,c) (a,b,c)\n"
                      "T(,,) T(a,,) T(,a,) T(,,a) T(a,a,) T(a,,a) T(,a,a)"),
            "(,,) (a,,) (,a,) (,,a) (a,a,) (a,,a) (,a,a)");

  EXPECT_EQ(scanInput(macroID + "#define T(x) x\n"
                                "ID(T(3))"),
            "3");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* MCALL(ID, 123456) -> ID(123456) -> 123456 */
  EXPECT_EQ(scanInput(macroID + macroMCALL + "MCALL(ID, 123456)"), "123456");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* Situation that an expanded text's last token happens to be a function-like
   * macro's name and there is a argument list right after it, result in a
   * further expansion.
   *
   * For simplicity let's call this situation "multiple arglist macro
   * expansion". */
  EXPECT_EQ(scanInput(macroID + macroDIV + "ID(DIV)(3, 4)"), "((3) / (4))");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);
  EXPECT_EQ(scanInput(macroID + "ID(ID)(3)"),
            "ID(3)");  // corner case: painted-blue
  EXPECT_EQ(errOut->listOfErrors.empty(), true);
  EXPECT_EQ(scanInput(macroDIV + "#define X(a) a\n"
                                 "#define FOO X\n"
                                 "FOO(3)"),
            "3");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  EXPECT_EQ(scanInput(macroID + "ID((3,4))"), "(3,4)");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  const std::string macroEMPTY = "#define EMPTY\n";
  const std::string macroDEFER = "#define DEFER(x) x EMPTY\n";
  EXPECT_EQ(scanInput(macroDIV + macroEMPTY + macroDEFER + "DEFER(DIV)(3, 4)"),
            "DIV (3, 4)");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  const std::string macroEXPAND = "#define EXPAND(x) x";
  EXPECT_EQ(scanInput(macroDIV + macroEMPTY + macroDEFER + macroEXPAND +
                      "EXPAND(DEFER(DIV)(3, 4))"),
            "((3) / (4))");

  EXPECT_EQ(scanInput(macroID + macroMCALL + "MCALL(ID,)"), "");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  EXPECT_EQ(scanInput("#define FOO(a, b, c) (a, b, c)\r\n"
                      "FOO(((a), (b)), (), ())"),
            "(((a), (b)), (), ())");
  EXPECT_EQ(errOut->listOfErrors.empty(), true);

  /* Invalid cases */

  // If an function-like macro call is invalid, for example, the number of
  // arguments isn't equal to the number of parameter, the whole expression
  // will not be expanded and output the invalid expression instead.

  EXPECT_EQ(scanInput(macroDIV + "DIV(a) DIV(a,b,c)"), "DIV(a) DIV(a,b,c)");
  EXPECT_EQ(errOut->listOfErrors.size(), 2);
  if (errOut->listOfErrors.size() == 2) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "The macro \"DIV\" requires 2 argument(s), but got 1.");
    EXPECT_EQ(errOut->listOfErrors[1].message(),
              "The macro \"DIV\" requires 2 argument(s), but got 3.");
  }

  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "The macro \"MCALL\" requires 2 argument(s), but got 1.");
  }

  EXPECT_EQ(scanInput(macroID + macroMCALL + "MCALL(ID)"), "MCALL(ID)");
  EXPECT_EQ(errOut->listOfErrors.size(), 1);
  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "The macro \"MCALL\" requires 2 argument(s), but got 1.");
  }

  EXPECT_EQ(scanInput("#define F(a, a) a\n"
                      "F(1, 2) A"),
            "F(1, 2) A");
  EXPECT_EQ(errOut->listOfErrors.size(), 1);
  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "Duplicated parameter \"a\" in the function-like macro \"F\".");
  }

  scanInput(
      "#define F(a)\n"
      "F(adfadwf \n"
      "daf df");
  EXPECT_EQ(errOut->listOfErrors.size(), 1);
  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "unterminated argument list invoking macro \"F\"");
  }

  scanInput(
      "#define F(a\n"
      "#define G(a $)\n");
  EXPECT_EQ(errOut->listOfErrors.size(), 2);
  if (errOut->listOfErrors.size() == 2) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "Expected ')' before end of line");
    EXPECT_EQ(errOut->listOfErrors[1].message(), "Expected ',' or ')' here.");
  }

  scanInput(
      "#define F(a,\n"
      "#define G(\n");
  EXPECT_EQ(errOut->listOfErrors.size(), 2);
  if (errOut->listOfErrors.size() == 2) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "Expected parameter name before end of line");
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "Expected parameter name before end of line");
  }

  scanInput("#define F(G()) G()");
  EXPECT_EQ(errOut->listOfErrors.size(), 1);
  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(), "Expected ',' or ')' here.");
  }

  scanInput(
      "#define FOO(a, b, c) (a, b, c)\r\n"
      "FOO((, (), ()))");
  EXPECT_EQ(errOut->listOfErrors.size(), 1);
  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "The macro \"FOO\" requires 3 argument(s), but got 1.");
  }

  scanInput(macroDIV + "DIV(,,)");
  EXPECT_EQ(errOut->listOfErrors.size(), 1);
  if (errOut->listOfErrors.size() == 1) {
    EXPECT_EQ(errOut->listOfErrors[0].message(),
              "The macro \"DIV\" requires 2 argument(s), but got 3.");
  }

  // TODO __VA_ARGS__
  // TODO paint-blue
  // TODO #/## operator
  // TODO #if #ifdef #ifndef
  // TODO #pragma #line
}
