#include <vector>
#include <algorithm>

#include "./Lexer.h"

struct KeywordPair {
	char* str;
	Keyword kind;
};

static std::vector<KeywordPair> keywordPairs{
	{ "_Bool", Keyword::_Bool },
	{ "_Complex", Keyword::_Complex },
	{ "_Imaginary", Keyword::_Imaginary },
	{ "auto", Keyword::Auto },
	{ "break", Keyword::Break },
	{ "case", Keyword::Case },
	{ "const", Keyword::Const },
	{ "continue", Keyword::Continue },
	{ "default", Keyword::Default },
	{ "do", Keyword::Do },
	{ "double", Keyword::Double },
	{ "else", Keyword::Else },
	{ "enum", Keyword::Enum },
	{ "extern", Keyword::Extern },
	{ "float", Keyword::Float },
	{ "for", Keyword::For },
	{ "goto", Keyword::Goto },
	{ "if", Keyword::If },
	{ "inline", Keyword::Inline },
	{ "int", Keyword::Int },
	{ "long", Keyword::Long },
	{ "register", Keyword::Register },
	{ "restrict", Keyword::Restrict },
	{ "return", Keyword::Return },
	{ "signed", Keyword::Signed },
	{ "sizeof", Keyword::Sizeof },
	{ "static", Keyword::Static },
	{ "struct", Keyword::Struct },
	{ "switch", Keyword::Switch },
	{ "typedef", Keyword::Typedef },
	{ "union", Keyword::Union },
	{ "unsigned", Keyword::Unsigned },
	{ "void", Keyword::Void },
	{ "volatile", Keyword::Volatile },
	{ "while", Keyword::While }
};

constexpr bool isStartOfIdentifier(const char ch);
constexpr bool isStartOfNumber(const char ch);

TokenKind Lexer::next(std::istream& is)
{
	if (isStartOfIdentifier(is.peek())) {
		readIdentifierString(is);

		auto pos = findKeywordPair();
		if (pos != keywordPairs.cend()) {
			keyword = pos->kind;
			return TokenKind::Keyword;
		}
		else {
			return TokenKind::Identifier;
		}
	}
	else if (isStartOfNumber(is.peek())) {
		readNumber(is);
	}

	is >> character;

	return TokenKind::Character;
}

char Lexer::getCharacter()
{
	return character;
}

std::string Lexer::getIdentifierString()
{
	return identifierString;
}

Keyword Lexer::getKeyword()
{
	return keyword;
}

constexpr bool isStartOfIdentifier(const char ch) {
	return ch >= 'a' && ch <= 'z'
		|| ch >= 'A' && ch <= 'Z'
		|| ch == '_';
}

constexpr bool isIdentiferCharacater(const char ch) {
	return isStartOfIdentifier(ch) ||
		ch >= '0' && ch <= '9';
}

constexpr bool isStartOfNumber(const char ch) {
	return ch >= '0' && ch <= '9' || ch == '.';
}

void Lexer::readIdentifierString(std::istream& inputStream)
{
	char ch;

	identifierString.clear();

	while (inputStream >> ch && isIdentiferCharacater(ch)) {
		identifierString.push_back(ch);
	}
}

long Lexer::getInteger()
{
	return integerValue;
}

std::vector<KeywordPair>::const_iterator Lexer::findKeywordPair()
{
	auto pos = std::lower_bound(
		keywordPairs.begin(),
		keywordPairs.end(),
		identifierString,
		[](const KeywordPair& pair, const std::string& targetKeywordString) {
			return pair.str < targetKeywordString;
		});

	if (pos != keywordPairs.end() && pos->str == identifierString) {
		return pos;
	}
	else {
		return keywordPairs.end();
	}
}

void Lexer::readNumber(std::istream& is) {
	bool isInteger = true;
	char ch;
	std::string buffer;

	while (is >> ch && ch >= '0' && ch <= '9') {
		buffer.push_back(ch);
	}

	if (is.eof() || is.peek() != '.') {
		integerValue = std::atol(buffer.c_str());
	}
}
