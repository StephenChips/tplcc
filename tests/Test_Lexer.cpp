#include <sstream>
#include <vector>
#include <gtest/gtest.h>
#include "../tplcc/Lexer.h"

struct ReportErrorStub : IReportError {
	std::vector<std::unique_ptr<Error>> listOfErrors{};

	void reportsError(std::unique_ptr<Error> error) override {
		listOfErrors.push_back(std::move(error));
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
		std::stringstream ss(keywordNames[i] + "    ");
		ReportErrorStub stub;

		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<Keyword>(*lexer.next()), keywordValues[i]);
		EXPECT_EQ(ss.tellg(), keywordNames[i].size());
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
		std::stringstream ss(id + "    ");

		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<Ident>(*lexer.next()), Ident { id });
		EXPECT_EQ(ss.tellg(), id.size());
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
		std::stringstream ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(std::get<NumberLiteral>(*lexer.next()), NumberLiteral{ literals[i]});
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
		std::stringstream ss(literals[i] + "    ");
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
		std::stringstream ss(literals[i] + "    ");
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
		std::stringstream ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidNumberSuffix *>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.tellg(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_exponent_has_no_digit) {
	std::vector<std::string> literals{
		"4e+uf",
		"4e",
		"0Xa.1p-a",
	};

	for (size_t i = 0; i < literals.size(); i++) {
		std::stringstream ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<ExponentHasNoDigit*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.tellg(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_hex_float_has_no_exponent) {
	std::vector<std::string> literals{
		"0Xa.1cu",
		"0xa.1f",
		"0X.1F"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		std::stringstream ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<HexFloatHasNoExponent *>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.tellg(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_invalid_number) {
	std::vector<std::string> literals{
		".e10f",
		".ll",
		".ace"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		std::stringstream ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidNumber*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.tellg(), literals[i].size());
	}
}

TEST(TestLexer, test_lexer_error_invalid_octal_number) {
	std::vector<std::string> literals{
		"0897",
		"08"
	};

	for (size_t i = 0; i < literals.size(); i++) {
		std::stringstream ss(literals[i] + "    ");
		ReportErrorStub stub;
		Lexer lexer(ss, stub);

		EXPECT_EQ(lexer.next(), std::nullopt);
		EXPECT_EQ(stub.listOfErrors.size(), 1);
		EXPECT_TRUE(dynamic_cast<InvalidOctalNumber*>(&(*stub.listOfErrors.front())) != nullptr);
		EXPECT_EQ(ss.tellg(), literals[i].size());
	}
}

TEST(TestLexer, test_character) {
	const std::string characters{ "{=+-*" };
	std::stringstream ss(characters);
	
	ReportErrorStub stub;
	Lexer lexer(ss, stub);

	for (const char& ch : characters) {
		EXPECT_EQ(std::get<char>(*lexer.next()), ch);
	}
}