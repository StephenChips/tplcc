#ifndef TCC_LEXER_H
#define TCC_LEXER_H

#include <istream>
#include <vector>

using Token = char;

enum class TokenKind {
	Identifier,
	Keyword,
	Character,
	IntegerLiteral
};

enum class Keyword {
	_Bool,
	_Complex,
	_Imaginary,
	Auto,
	Break,
	Case,
	Char,
	Const,
	Continue,
	Default,
	Do,
	Double,
	Else,
	Enum,
	Extern,
	Float,
	For,
	Goto,
	If,
	Inline,
	Int,
	Long,
	Register,
	Restrict,
	Return,
	Signed,
	Sizeof,
	Static,
	Struct,
	Switch,
	Typedef,
	Union,
	Unsigned,
	Void,
	Volatile,
	While
};

struct KeywordPair;

class Lexer {
private:
	std::string identifierString;
	Keyword keyword;
	char character;
	long integerValue;
public:
	Lexer() {}
	TokenKind next(std::istream& is);
	char getCharacter();
	std::string getIdentifierString();
	Keyword getKeyword();
	long getInteger();

private:
	void readIdentifierString(std::istream& inputStream);
	void Lexer::readNumber(std::istream& is);
	std::vector<KeywordPair>::const_iterator findKeywordPair();
};

#endif