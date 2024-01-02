#include <sstream>
#include <vector>
#include <gtest/gtest.h>
#include "../tplcc/Lexer.h"

struct ReportErrorStub : IReportError {
	std::vector<std::unique_ptr<IErrorOutputItem>> listOfErrors{};

	void reportsError(std::unique_ptr<IErrorOutputItem> error) override {
		listOfErrors.push_back(std::move(error));
	}
};

class DummyStringLexerInput : public ILexerInput {
	std::string inputStr;
	size_t cursor = 0;
public:
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
	bool eof() override {
		return cursor == inputStr.size();
	}
	size_t numberOfConsumedChars() override {
		return cursor;
	}
};

TEST(Test_Lexer, test_keyword) {
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
		DummyStringLexerInput ss(keywordNames[i] + "    ");
		ReportErrorStub stub;

		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<Keyword>(*lexer.next()), keywordValues[i]);
		EXPECT_EQ(ss.numberOfConsumedChars(), keywordNames[i].size());
	}

}

TEST(TestLexer, test_identifier) {
	std::vector<std::string> identifiers{
		"foo",
		"_foo",
		"Foo",
		"foo12"
	};

	for (const auto& id : identifiers) {
		DummyStringLexerInput ss(id + "    ");

		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<Ident>(*lexer.next()), Ident{ id });
		EXPECT_EQ(ss.numberOfConsumedChars(), id.size());
	}
}

TEST(TestLexer, test_integer) {
	std::vector<std::string> literals{
		"0171uLL",
		"017",
		"171uLL",
		"171",
		"0x171ABCuLL",
		"0x171ABC"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i] });
	}
}

TEST(TestLexer, test_decimal_floating_numbers) {
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
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i] });
	}
}

TEST(TestLexer, test_hexadecimal_floating_numbers) {
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
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i] });
	}
}

TEST(TestLexer, test_lexer_error_invalid_number_suffix) {
	std::vector<std::string> literals{
		"4f",
		"4.0ul",
		"4.abc",
		"4abc",
	};

	for (size_t i = 0; i < literals.size(); i++) {
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidNumberSuffix*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.numberOfConsumedChars(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_exponent_has_no_digit) {
	std::vector<std::string> literals{
		"4e+uf",
		"4e",
		"0Xa.1p-a",
	};

	for (size_t i = 0; i < literals.size(); i++) {
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<ExponentHasNoDigit*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.numberOfConsumedChars(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_hex_float_has_no_exponent) {
	std::vector<std::string> literals{
		"0Xa.1cu",
		"0xa.1f",
		"0X.1F"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<HexFloatHasNoExponent*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.numberOfConsumedChars(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_invalid_number) {
	std::vector<std::string> literals{
		".e10f",
		".ll",
		".ace"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidNumber*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.numberOfConsumedChars(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_invalid_octal_number) {
	std::vector<std::string> literals{
		"0897",
		"08"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		DummyStringLexerInput ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidOctalNumber*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.numberOfConsumedChars(), literals[i].size());
	}
}

TEST(TestLexer, test_character) {
	const std::string characters{ "{=+-*" };
	DummyStringLexerInput ss(characters);

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	for (const char& ch : characters) {
		EXPECT_EQ(std::get<char>(*lexer.next()), ch);
	}
}

static void testStringLiteral(const std::string& str, const std::string& expectedContent, const CharSequenceLiteralPrefix expectedPrefix) {
	DummyStringLexerInput ss(str + "    ");

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	auto result = lexer.next();
	EXPECT_TRUE(result != std::nullopt);

	auto actual = std::get<StringLiteral>(*result);
	EXPECT_EQ(actual.str, expectedContent);
	EXPECT_EQ(actual.prefix, expectedPrefix);
}

static void testCharacterLiteral(const std::string& str, const std::string& expectedContent, const CharSequenceLiteralPrefix expectedPrefix) {
	DummyStringLexerInput ss(str + "    ");

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	auto result = lexer.next();
	EXPECT_TRUE(result != std::nullopt);

	auto actual = std::get<CharacterLiteral>(*result);
	EXPECT_EQ(actual.str, expectedContent);
	EXPECT_EQ(actual.prefix, expectedPrefix);
}

static void testInvalidStringPrefix(const std::string& prefix) {
	DummyStringLexerInput ss(prefix + "\"hello\"");

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	EXPECT_EQ(lexer.next(), std::nullopt);
	EXPECT_TRUE(stub.listOfErrors.size() == 1);

	auto ptrToError = dynamic_cast<InvalidStringOrCharacterPrefix*>(stub.listOfErrors[0].get());
	EXPECT_TRUE(ptrToError != nullptr);
}

// If we let the lexer continue scanning when it detects this kind of error, it will probably
// generate wrong tokens or even more erros and cause the parser to produce pointless and confusing
// errors, so it is better to print out all errors that we've found now, halt the program, ask the
// programmer fixes the bugs and let him re-run the compiler.
static void testStringMissEndingQuote(const std::string& str) {
	DummyStringLexerInput ss(str);

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	try {
		lexer.next();
		FAIL();
	}
	catch (std::exception&) {
		EXPECT_TRUE(stub.listOfErrors.size() == 1);
		auto ptrToError = dynamic_cast<StringMissEndingQuote*>(stub.listOfErrors[0].get());
		EXPECT_TRUE(ptrToError == nullptr);
	}
}

static void testStringInvalidEscape(const std::string& str) {
	DummyStringLexerInput ss(str);

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	EXPECT_EQ(lexer.next(), std::nullopt);
	ASSERT_TRUE(!stub.listOfErrors.empty() && stub.listOfErrors.size() == 1);

	auto ptrToError = dynamic_cast<StringInvalidEscape*>(stub.listOfErrors[0].get());
	EXPECT_TRUE(ptrToError == nullptr);
}

static void testHexEscapeSequenceOutOfRange(const std::string& str) {
	DummyStringLexerInput ss(str);

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	ASSERT_EQ(lexer.next(), std::nullopt);
	EXPECT_TRUE(stub.listOfErrors.size() == 1);

	auto ptrToError = dynamic_cast<HexEscapeSequenceOutOfRange*>(stub.listOfErrors[0].get());
	EXPECT_TRUE(ptrToError == nullptr);
}

// like string misses its ending quote, we throw an exception here too.
static void testCharacterMissEndingQuote(const std::string& str) {
	DummyStringLexerInput ss(str);

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	try {
		lexer.next();
		FAIL();
	}
	catch (std::exception&) {
		EXPECT_TRUE(stub.listOfErrors.size() == 1);
		auto ptrToError = dynamic_cast<MissEndingQuote*>(stub.listOfErrors[0].get());
		EXPECT_TRUE(ptrToError != nullptr);
	}
}

static void testInvalidCharacterPrefix(const std::string& prefix) {
	DummyStringLexerInput ss(prefix + "'0'");

	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	EXPECT_EQ(lexer.next(), std::nullopt);
	EXPECT_TRUE(stub.listOfErrors.size() == 1);

	auto ptrToError = dynamic_cast<InvalidStringOrCharacterPrefix*>(stub.listOfErrors[0].get());
	EXPECT_TRUE(ptrToError != nullptr);
}

static std::string fromUTF32(std::u32string&& s) {
	return std::string(s.begin(), s.end());
}

TEST(TestLexer, test_string_literal) {
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

TEST(TestLexer, test_character_literal) {
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