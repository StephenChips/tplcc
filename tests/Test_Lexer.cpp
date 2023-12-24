#include <sstream>
#include <vector>
#include <gtest/gtest.h>
#include "../tcc/Lexer.h"

TEST(Test_Lexer, test_keyword) {
	std::vector<const char*> keywordNames {
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
		std::stringstream ss(keywordNames[i]);

		Lexer lexer;
		const TokenKind tok = lexer.next(ss);

		EXPECT_EQ(tok, TokenKind::Keyword);
		EXPECT_EQ(lexer.getKeyword(), keywordValues[i]);
	}

}

TEST(TestLexer, test_identifier) {
	std::vector<const char*> identifiers {
	"foo",
	"_foo",
	"Foo",
	"foo12"
	};

	for (const auto& id : identifiers) {
		std::stringstream ss(id);

		Lexer lexer;
		
		EXPECT_EQ(lexer.next(ss), TokenKind::Identifier);
		EXPECT_EQ(lexer.getIdentifierString(), id);
	}
}

TEST(TestLexer, test_integer) {
	std::vector<const char*> literals{
		"1",
		"12",
		"123"
	};

	std::vector<long> expectedValue{
		1,
		12,
		123
	};

	for (size_t i = 0; i < literals.size(); i++) {
		std::stringstream ss(literals[i]);
		Lexer lexer;

		EXPECT_EQ(lexer.next(ss), TokenKind::IntegerLiteral);
		EXPECT_EQ(lexer.getInteger(), expectedValue[i]);
	}
}

TEST(TestLexer, test_character) {
	const std::string characters{ "{=+-*" };
	std::stringstream ss(characters);
	Lexer lexer;

	for (const char& ch : characters) {
		EXPECT_EQ(lexer.next(ss), TokenKind::Character);
		EXPECT_EQ(lexer.getCharacter(), ch);
	}
}