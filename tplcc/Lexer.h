#ifndef TPLCC_LEXER_H
#define TPLCC_LEXER_H

#include <istream>
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include <variant>
#include <concepts>

#include "error-reporting.h"

struct Punctuator {
	std::string str;
	bool operator==(const Punctuator&) const = default;
};

struct Identifier {
	std::string str;
	bool operator==(const Identifier&) const = default;
};

// The type of a number literal without a type suffix is uncertain
// until we've parsed the expression surrounds it. for example:
// 
// ``` c
// char a = 128;
// int b = 128 * a; 
// ```
// 
// Because the variable "a" is a "char", the literal should be a "char" too, and the 
// whole expression should overflow even though we assign it to a larger type "int" later.
// We cannot know what type the literal "128" is until we've parsed the whole expression.
// So instead converting the string to a number here, it is better to store the string we've parsed
// and let the parser to convert it to any type it needs.

struct NumberLiteral {
	std::string str;
	bool operator==(const NumberLiteral&) const = default;
};

enum class CharSequenceLiteralPrefix {
	None,
	L
};

// We don't preform any type and encoding conversion here on the literal's characters. No matter
// what prefix the string literal has, the content is always an identical copy from the original
// source code. 
struct StringLiteral {
	std::string str;
	CharSequenceLiteralPrefix prefix;
	bool operator==(const StringLiteral&) const = default;
};

// Same as StringLiteral, no conversions are made.
struct CharacterLiteral {
	std::string str;
	CharSequenceLiteralPrefix prefix;
	bool operator==(const CharacterLiteral&) const = default;
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

struct EndOfInput {
	bool operator==(const EndOfInput& other) const = default;
};

constexpr const EndOfInput EOI;

using Token = std::variant<
	Punctuator,
	Identifier,
	NumberLiteral,
	StringLiteral,
	CharacterLiteral,
	Keyword,
	char,
	EndOfInput
>;

class InvalidNumberSuffix: public Error {
	std::string invalidSuffix;
	std::string numberLiteralNoSuffix;
public:
	InvalidNumberSuffix(std::string numberLiteralNoSuffix, std::string invalidSuffix) : numberLiteralNoSuffix(numberLiteralNoSuffix), invalidSuffix(invalidSuffix) {};
	std::string errorMessage() {
		return "\"" + invalidSuffix + "\" is not a valid suffix for the number literal " + numberLiteralNoSuffix + ".";
	}
};

class ExponentHasNoDigit: public Error {};
class HexFloatHasNoExponent: public Error {};
class InvalidNumber: public Error {};
class InvalidOctalNumber: public Error {};
class StringMissEndingQuote : public Error {};
class StringInvalidEscape : public Error {};
class HexEscapeSequenceOutOfRange : public Error {};
class InvalidStringOrCharacterPrefix : public Error {};
class MissEndingQuote : public Error {};
class InvalidCharacter : public Error {};

// provide a forward-only n-lookahead stream-like input interface
struct ILexerInput {
	virtual int get() = 0;
	virtual int peek() = 0;
	virtual std::vector<int> peekN(size_t n) = 0;
	virtual void ignore() = 0;
	virtual void ignoreN(size_t n) = 0;
	virtual bool eof() = 0;
	virtual size_t numberOfConsumedChars() = 0;
	virtual ~ILexerInput() = default;
};

class Lexer {
private:
	ILexerInput& input;
	IReportError& errOut;
public:
	Lexer(ILexerInput& is, IReportError& errOut): input(is), errOut(errOut){}
	std::optional<Token> next();
private:
	std::string readIdentString();
	std::optional<Keyword> findKeyword(const std::string& str);
	std::string scanCharSequenceContent(const CharSequenceLiteralPrefix prefix, const char quote);
	void skipCharSequenceContent(const char endingQuote);
	std::optional<Token> scanCharSequence(const char quote, const std::string& prefix = "");
	std::optional<Token> scanPunctuator();
};

#endif