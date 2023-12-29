#ifndef TCC_LEXER_H
#define TCC_LEXER_H

#include <istream>
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include <variant>

using Token = char;

struct TokenKind {
	static const char Ident = 0;
	static const char Keyword = 1;
	static const char NumberLiteral = 2;
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

// The type of a number literal without a type suffix is uncurtain
// until we've parsed the expression surrounds it. for example:
// 
// ``` c
// char a = 128;
// int b = 128 * a; 
// ```
// 
// Because the variable "a" is a "char", the literal should be deduce to a "char" type too,
// and the whole expression will overflow even if we will assign it to a larger type "int" later.
// We cannot know what type the literal "128" is until we've parsed the whole expression.
// So instead converting the string to a number here, it is better to store the string we've parsed
// and let the parser to convert it to any type it needs.

// 
struct FloatingConstant {
	std::string& literalString;
};

struct LexerError {
	enum ErrorReason {
		INVALID_NUMBER_SUFFIX,
		EXPONENT_HAS_NO_DIGIT,
		HEX_FLOAT_HAS_NO_EXPONENT,
		INVALID_NUMBER,
		INVALID_OCTAL_NUMBER,
	} reason;

	bool operator==(const LexerError&) const = default;
};



class Lexer {
private:
	std::string buffer;
	Keyword keyword = Keyword::Auto;
	char character = '\0';
public:
	Lexer() {}

	/// <summary>
	/// Get the next token from the input stream, which is a number that has following meaning:
	/// 
	/// 1. If it euqals TokenKind::Identifier, it is an identifier. We can get its string from `getIdentStr`
	/// 2. If it equals TokenKind::Keyword, it is a keyword. We can know what keyword it is from `getKeyword`
	/// 3. If it equals TokenKind::IntLiteral, it is any kind of integer literal. We can get the literal string from `getIntStr`
	/// 4. If it equals TokenKind::FloatLiteral, it is a double or a float. We can get the literal string from `getFloatStr` 
	/// 4. Otherwise it is a character, where the number itself is the charcode.
	/// 
	/// </summary>
	/// <param name="is">the input stream</param>
	/// <returns>A "token"</returns>
	std::variant<Token, LexerError> next(std::istream& is);

	char getCharacter();
	std::string getIdentStr();
	Keyword getKeyword();
	std::string getNumLiteralStr();
private:
	void readIdentifierString(std::istream& inputStream);
	std::optional<LexerError> readNumber(std::istream& is);
	std::optional<Keyword> findKeyword(const std::string& str);
	std::optional<LexerError> scanSuffixes(std::istream& is, std::initializer_list<const std::vector<std::string>*> availableSuffixes);
	std::optional<LexerError> scanIntegerSuffixes(std::istream& is);
	std::optional<LexerError> scanDoubleAndFloatSuffixes(std::istream& is);
	std::optional<LexerError> scanExponentPart(std::istream& is);
};

#endif