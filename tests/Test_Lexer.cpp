#include <sstream>
#include <vector>
#include <gtest/gtest.h>
#include "../tplcc/Lexer.h"

template<typename T>
bool operator==(const Token& token, const T& literal) {
	if (auto lit = std::get_if<T>(&token)) {
		return *lit == literal;
	}
	else {
		return false;
	}
}

struct ReportErrorStub : IReportError {
	std::vector<std::unique_ptr<IErrorOutputItem>> listOfErrors{};

	void reportsError(std::unique_ptr<IErrorOutputItem> error) override {
		listOfErrors.push_back(std::move(error));
	}
};

struct DummyStringLexerInput : ILexerInput {
	std::string inputStr = "";
	size_t cursor = 0;

	DummyStringLexerInput() {};

	void resetInput(std::string input) {
		inputStr = std::move(input);
		cursor = 0;
	}

	DummyStringLexerInput(std::string inputStr): inputStr(std::move(inputStr)) {}
	int get() override {
		return eof() ? EOF : inputStr[cursor++];
	}
	int peek() override {
		return eof() ? EOF : inputStr[cursor];
	}
	std::vector<int> peekN(size_t n) override {
		std::vector<int> output;
		for (size_t i = 0; i < n; i++) {
			if (cursor + i >= inputStr.size()) {
				output.push_back(EOF);
			}
			else {
				output.push_back(inputStr[cursor + i]);
			}
		}
		return output;
	}
	void ignore() override {
		if (!eof()) cursor++;
	}
	void ignoreN(size_t n) override {
		for (size_t i = 0; !eof() && i < n; i++) {
			cursor++;
		}
	}
	bool eof() override {
		return cursor == inputStr.size();
	}
	size_t numberOfConsumedChars() override {
		return cursor;
	}
};

class TestLexer : public testing::Test {
protected:
	DummyStringLexerInput li;
	ReportErrorStub errOut;
	Lexer lexer;

	TestLexer() : lexer(li, errOut) {}

public:
	void testStringLiteral(const std::string& str, const std::string& expectedContent, const CharSequenceLiteralPrefix expectedPrefix) {
		li.resetInput(str + "    ");

		auto result = lexer.next();
		EXPECT_TRUE(result != std::nullopt);

		auto actual = std::get<StringLiteral>(*result);
		EXPECT_EQ(actual.str, expectedContent);
		EXPECT_EQ(actual.prefix, expectedPrefix);
	}

	void testCharacterLiteral(const std::string& str, const std::string& expectedContent, const CharSequenceLiteralPrefix expectedPrefix) {
		li.resetInput(str + "    ");

		auto result = lexer.next();
		EXPECT_TRUE(result != std::nullopt);

		auto actual = std::get<CharacterLiteral>(*result);
		EXPECT_EQ(actual.str, expectedContent);
		EXPECT_EQ(actual.prefix, expectedPrefix);
	}

	void testInvalidStringPrefix(const std::string& prefix) {
		li.resetInput(prefix + "\"hello\"");
		errOut.listOfErrors.clear();

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_TRUE(errOut.listOfErrors.size() == 1);

		auto ptrToError = dynamic_cast<InvalidStringOrCharacterPrefix*>(errOut.listOfErrors[0].get());
		EXPECT_TRUE(ptrToError != nullptr);
	}

	// If we let the lexer continue scanning when it detects this kind of error, it will probably
// generate wrong tokens or even more erros and cause the parser to produce pointless and confusing
// errors, so it is better to print out all errors that we've found now, halt the program, ask the
// programmer fixes the bugs and let him re-run the compiler.
	void testStringMissEndingQuote(const std::string& str) {
		try {
			li.resetInput(str);
			errOut.listOfErrors.clear();
			lexer.next();
			FAIL();
		}
		catch (std::exception&) {
			EXPECT_TRUE(errOut.listOfErrors.size() == 1);
			auto ptrToError = dynamic_cast<StringMissEndingQuote*>(errOut.listOfErrors[0].get());
			EXPECT_TRUE(ptrToError == nullptr);
		}
	}

	void testStringInvalidEscape(const std::string& str) {
		li.resetInput(str);
		errOut.listOfErrors.clear();

		EXPECT_EQ(lexer.next(), std::nullopt);
		ASSERT_TRUE(!errOut.listOfErrors.empty() && errOut.listOfErrors.size() == 1);

		auto ptrToError = dynamic_cast<StringInvalidEscape*>(errOut.listOfErrors[0].get());
		EXPECT_TRUE(ptrToError == nullptr);
	}

	void testHexEscapeSequenceOutOfRange(const std::string& str) {
		li.resetInput(str);
		errOut.listOfErrors.clear();

		ASSERT_EQ(lexer.next(), std::nullopt);
		EXPECT_TRUE(errOut.listOfErrors.size() == 1);

		auto ptrToError = dynamic_cast<HexEscapeSequenceOutOfRange*>(errOut.listOfErrors[0].get());
		EXPECT_TRUE(ptrToError == nullptr);
	}

	// like string misses its ending quote, we throw an exception here too.
	void testCharacterMissEndingQuote(const std::string& str) {
		li.resetInput(str);
		errOut.listOfErrors.clear();

		try {
			lexer.next();
			FAIL();
		}
		catch (std::exception&) {
			EXPECT_TRUE(errOut.listOfErrors.size() == 1);
			auto ptrToError = dynamic_cast<MissEndingQuote*>(errOut.listOfErrors[0].get());
			EXPECT_TRUE(ptrToError != nullptr);
		}
	}

	void testInvalidCharacterPrefix(const std::string& prefix) {
		li.resetInput(prefix + "'0'");
		errOut.listOfErrors.clear();

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_TRUE(errOut.listOfErrors.size() == 1);
		auto ptrToError = dynamic_cast<InvalidStringOrCharacterPrefix*>(errOut.listOfErrors[0].get());
		EXPECT_TRUE(ptrToError != nullptr);
	}
};

TEST_F(TestLexer, test_keyword) {
	std::vector<std::string> keywordNames{
		"static",
		"int",
		"extern",
		"goto"
	};

	std::vector<Keyword> keywordValues{
		Keyword::Static,
		Keyword::Int,
		Keyword::Extern,
		Keyword::Goto
	};

	for (size_t i = 0; i < keywordNames.size(); i++) {
		li.resetInput(keywordNames[i]);
		EXPECT_EQ(std::get<Keyword>(*lexer.next()), keywordValues[i]);
		EXPECT_EQ(li.numberOfConsumedChars(), keywordNames[i].size());
	}

}

TEST_F(TestLexer, test_identifier) {
	std::vector<std::string> identifiers{
		"foo",
		"_foo",
		"Foo",
		"foo12"
	};

	for (const auto& id : identifiers) {
		li.resetInput(id);
		EXPECT_EQ(std::get<Ident>(*lexer.next()), Ident{ id });
		EXPECT_EQ(li.numberOfConsumedChars(), id.size());
	}
}

TEST_F(TestLexer, test_integer) {
	std::vector<std::string> literals{
		"0171uLL",
		"017",
		"171uLL",
		"171",
		"0x171ABCuLL",
		"0x171ABC"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i]);
		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i] });
	}
}

TEST_F(TestLexer, test_decimal_floating_numbers) {
	std::vector<std::string> literals{
		"100.33e10f",
		"100.33E10f",
		"100.33e-10f",
		"100.33e+10f",
		"100.33e10",
		"100.33",
		"100.33f",
		".33e-10f",
		".33f",
		"0123.123"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i]);
		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i] });
	}
}

TEST_F(TestLexer, test_hexadecimal_floating_numbers) {
	std::vector<std::string> literals{
		"0xabc.3defp10f",
		"0xABC.3DEFp10f",
		"0xabc.3defP10f",
		"0xabc.3defp-10f",
		"0xabc.3defp+10f",
		"0xabc.3defp10",
		"0xabcp10f",
		"0x.3defp10f",
		"0x.3defp10",
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i]);
		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i] });
	}
}

TEST_F(TestLexer, test_lexer_error_invalid_number_suffix) {
	std::vector<std::string> literals{
		"4f",
		"4.0ul",
		"4.abc",
		"4abc",
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i]);
		errOut.listOfErrors.clear();
		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(errOut.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidNumberSuffix*>(&(*errOut.listOfErrors.front())) != nullptr);
		EXPECT_EQ(li.numberOfConsumedChars(), literals[i].size());
	}
}

TEST_F(TestLexer, test_lexer_error_exponent_has_no_digit) {
	std::vector<std::string> literals{
		"4e+uf",
		"4e",
		"0Xa.1p-a",
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i]);
		errOut.listOfErrors.clear();
		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(errOut.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<ExponentHasNoDigit*>(&(*errOut.listOfErrors.front())) != nullptr);
		EXPECT_EQ(li.numberOfConsumedChars(), literals[i].size());
	}
}

TEST_F(TestLexer, test_lexer_error_hex_float_has_no_exponent) {
	std::vector<std::string> literals{
		"0Xa.1cu",
		"0xa.1f",
		"0X.1F"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i] + "    ");
		errOut.listOfErrors.clear();

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(errOut.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<HexFloatHasNoExponent*>(&(*errOut.listOfErrors.front())) != nullptr);
		EXPECT_EQ(li.numberOfConsumedChars(), literals[i].size());
	}
}

TEST_F(TestLexer, test_lexer_error_invalid_octal_number) {
	std::vector<std::string> literals{
		"0897",
		"08"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		li.resetInput(literals[i] + "    ");
		errOut.listOfErrors.clear();

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(errOut.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidOctalNumber*>(&(*errOut.listOfErrors.front())) != nullptr);
		EXPECT_EQ(li.numberOfConsumedChars(), literals[i].size());
	}
}

std::string fromUTF32(std::u32string&& s) {
	return std::string(s.begin(), s.end());
}

TEST_F(TestLexer, test_string_literal) {
	using namespace std::string_literals;

	testStringLiteral("\"\"", "", CharSequenceLiteralPrefix::None);
	testStringLiteral("\"hello, world\"", "hello, world", CharSequenceLiteralPrefix::None);
	testStringLiteral("L\"hello, world\"", "hello, world", CharSequenceLiteralPrefix::L);
	testStringLiteral("\"😀你好世界\"", "😀你好世界", CharSequenceLiteralPrefix::None);
	testStringLiteral("L\"😀你好世界\"", "😀你好世界", CharSequenceLiteralPrefix::L);
	testStringLiteral(fromUTF32(U"\"😀你好世界\""), fromUTF32(U"😀你好世界"), CharSequenceLiteralPrefix::None);
	testStringLiteral("\"hello\0 world\""s, "hello\0 world"s, CharSequenceLiteralPrefix::None);

	testStringLiteral("\"\\'\\\"\\?\\\\\\a\\b\\f\\n\\r\\t\\v\"", "\\'\\\"\\?\\\\\\a\\b\\f\\n\\r\\t\\v", CharSequenceLiteralPrefix::None);
	testStringLiteral("\"\\0\\1\\2\\3\\4\\5\\6\\7\\71\\121\"", "\\0\\1\\2\\3\\4\\5\\6\\7\\71\\121", CharSequenceLiteralPrefix::None);
	testStringLiteral("\"\\xa\\xb\\xc\\xd\\xe\\xf\\xab\"", "\\xa\\xb\\xc\\xd\\xe\\xf\\xab", CharSequenceLiteralPrefix::None);
	testStringLiteral("\"\\7777\\xff\"", "\\7777\\xff", CharSequenceLiteralPrefix::None);

	testStringLiteral("\"\\u1ab2\"", "\\u1ab2", CharSequenceLiteralPrefix::None);
	testStringLiteral("\"\\U1ab2c3d4\"", "\\U1ab2c3d4", CharSequenceLiteralPrefix::None);

	testInvalidStringPrefix("u8"); // UTF-8 string literal, only C11 supports it
	testInvalidStringPrefix("u"); // UTF-16 string literal, only C11 supports it
	testInvalidStringPrefix("U"); // UTF-32 string literal, only C11 supports it
	// nonsense prefixes
	testInvalidStringPrefix("foo");
	testInvalidStringPrefix("_");
	testInvalidStringPrefix("_313");
	testInvalidStringPrefix("_foo");

	testStringMissEndingQuote("\"hello\n");
	testStringMissEndingQuote("\"hello");
	testStringMissEndingQuote("\"hello\v");
	testStringMissEndingQuote("\"hello\f");
	testStringMissEndingQuote("\"hello");
	testStringMissEndingQuote("\"");

	// We scan and create a string literal as long as it has a pair of enclosing quotes.
	// Any error inside the string is ignored on purpose. Those errors will be
	// discovered when we evaluate its numeric value. 

	// contains error: hex digit's value is bigger than INT_MAX
	testStringLiteral("\"0x7777777\"", "0x7777777", CharSequenceLiteralPrefix::None); 
	// invalid escaping
	testStringLiteral("\"\\j\\9\\xz\\1212\\xaj\"", "\\j\\9\\xz\\1212\\xaj", CharSequenceLiteralPrefix::None);
}

TEST_F(TestLexer, test_character_literal) {
	testCharacterLiteral("'c'", "c", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'!'", "!", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'1'", "1", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("' '", " ", CharSequenceLiteralPrefix::None);	// SPACE
	testCharacterLiteral("'	'", "	", CharSequenceLiteralPrefix::None); // TAB
	testCharacterLiteral("L'c'", "c", CharSequenceLiteralPrefix::L);
	testCharacterLiteral(fromUTF32(U"\'c\'"), fromUTF32(U"c"), CharSequenceLiteralPrefix::None);
	
	testCharacterLiteral("'\\\"'", "\\\"", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\''", "\\'", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\?'", "\\?", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\\\'", "\\\\", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\a'", "\\a", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\b'", "\\b", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\f'", "\\f", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\n'", "\\n", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\r'", "\\r", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\t'", "\\t", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\v'", "\\v", CharSequenceLiteralPrefix::None);

	testCharacterLiteral("'\\xa'", "\\xa", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xb'", "\\xb", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xc'", "\\xc", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xd'", "\\xd", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xe'", "\\xe", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xf'", "\\xf", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xab'", "\\xab", CharSequenceLiteralPrefix::None);

	testCharacterLiteral("'\\123'", "\\123", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\12'", "\\12", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\0'", "\\0", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\xab\\12\\xff\\x34'", "\\xab\\12\\xff\\x34", CharSequenceLiteralPrefix::None);

	testCharacterLiteral("'\\u1ab2'", "\\u1ab2", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("'\\U1ab2c3d4'", "\\U1ab2c3d4", CharSequenceLiteralPrefix::None);

	testCharacterLiteral("\'ab\'", "ab", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("L\'ab\'", "ab", CharSequenceLiteralPrefix::L);

	testCharacterLiteral("\'你\'", "你", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("\'α\'", "α", CharSequenceLiteralPrefix::None);
	testCharacterLiteral("L\'你\'", "你", CharSequenceLiteralPrefix::L);
	testCharacterLiteral("L\'α\'", "α", CharSequenceLiteralPrefix::L);

	testCharacterMissEndingQuote("'h\n");
	testCharacterMissEndingQuote("'h");
	testCharacterMissEndingQuote("'h\v");
	testCharacterMissEndingQuote("'hello\f");
	testCharacterMissEndingQuote("'h");
	testCharacterMissEndingQuote("'");

	testInvalidCharacterPrefix("u8"); // UTF-8 character literal, only C11 supports it
	testInvalidCharacterPrefix("u"); // UTF-16 character literal, only C11 supports it
	testInvalidCharacterPrefix("U"); // UTF-32 character literal, only C11 supports it
	// nonsense invalid suffixes
	testInvalidCharacterPrefix("foo");
	testInvalidCharacterPrefix("_");
	testInvalidCharacterPrefix("_313");
	testInvalidCharacterPrefix("_foo");

	// We scan and create a character literal as long as it has a pair of enclosing quotes.
	// Any error inside the character is ignored on purpose. Those errors will be
	// discovered when we evaluate its numeric value. 

	// contains error: hex digit's value is bigger than INT_MAX
	testCharacterLiteral("'0x7777777'", "0x7777777", CharSequenceLiteralPrefix::None);
	// invalid escaping
	testCharacterLiteral("'\\xaj'", "\\xaj", CharSequenceLiteralPrefix::None);
}

TEST_F(TestLexer, testSingleLineComment) {
	li.resetInput("// hello, world.         ");

	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, testSingleLineCommentFollowsAToken) {
	li.resetInput("313 // THIS IS A INTEGER");

	EXPECT_EQ(*lexer.next(), NumberLiteral{ "313" });
	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, commented_out_tokens_is_ignored) {
	li.resetInput("// foo = 313");
	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());

	li.resetInput("/* foo = 313 */");
	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, token_at_the_next_line_of_the_single_line_comment_should_be_scanned) {
	li.resetInput(
		"//INT\r\n"
		"313\r\n"
	);

	EXPECT_EQ(*lexer.next(), NumberLiteral{"313"});
	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, test_comment) {
	li.resetInput("/* comment */  ");

	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, test_comment_surrounded_by_tokens) {
	li.resetInput("313 /* comment */ foo   ");

	EXPECT_EQ(*lexer.next(), NumberLiteral{ "313" });
	EXPECT_EQ(*lexer.next(), Ident{ "foo" });
	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, test_comment_spans_across_multiple_lines) {
	li.resetInput("313 /* <- A INT \r\n A IDENTIFIER -> */ foo   ");

	EXPECT_EQ(*lexer.next(), NumberLiteral{ "313" });
	EXPECT_EQ(*lexer.next(), Ident{ "foo" });
	EXPECT_EQ(*lexer.next(), EOI);
	EXPECT_TRUE(errOut.listOfErrors.empty());
}


TEST_F(TestLexer, test_punctuators) {
	const char* inputStr =
		"[ ] ( ) { } . -> "
		"++ -- & * + - ~ ! "
		"/ % << >> < > <= >= == != ^ | && || "
		"? : ; "
		"= *= /= %= += -= <<= >>= &= ^= |= "
		", <: :> <% %>";

	li.resetInput(inputStr);

	std::istringstream li(inputStr);
	std::vector<std::string> punctuators {
		std::istream_iterator<std::string>(li),
		std::istream_iterator<std::string>()
	};

	for (const auto& punctuatorString : punctuators) {
		auto punctuator = lexer.next();
		ASSERT_TRUE(punctuator != std::nullopt);
		EXPECT_EQ(std::get<Punctuator>(*punctuator), Punctuator{ punctuatorString });
	}

	EXPECT_TRUE(errOut.listOfErrors.empty());
}

TEST_F(TestLexer, test_dot_that_followed_by_another_token) {
	li.resetInput(".e10f");
	EXPECT_EQ(std::get<Punctuator>(*lexer.next()), Punctuator{ "." });
	EXPECT_EQ(std::get<Ident>(*lexer.next()), Ident{ "e10f" });
	EXPECT_EQ(std::get<EndOfInput>(*lexer.next()), EOI);
}

TEST_F(TestLexer, test_invalid_characters) {
	std::string invalidCharacters{ "`@" };
	for (const auto ch : invalidCharacters) {
		try {
			li.resetInput(std::string{ ch });
			errOut.listOfErrors.clear();
			lexer.next();
			FAIL();
		}
		catch (std::exception& e) {
			EXPECT_TRUE(errOut.listOfErrors.size() == 1);
			EXPECT_TRUE(dynamic_cast<InvalidCharacter *>(&(*errOut.listOfErrors.front())) != nullptr);
		}
	}
}