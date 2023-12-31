#ifndef TCC_LEXER_H
#define TCC_LEXER_H

#include <istream>
#include <vector>
#include <memory>
#include <cstdint>
#include <optional>
#include <variant>
#include <concepts>

struct Ident {
	std::string str;

	bool operator==(const Ident&) const = default;
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

using Token = std::variant<
	Ident,
	NumberLiteral,
	Keyword,
	char
>;

class Error {
public:
	Error() = default;
	virtual ~Error() = default;
};

class InvalidNumberSuffix: public Error {};
class ExponentHasNoDigit: public Error {};
class HexFloatHasNoExponent: public Error {};
class InvalidNumber: public Error {};
class InvalidOctalNumber: public Error {};

struct IReportError {
	virtual void reportsError(std::unique_ptr<Error> error) = 0;
	virtual ~IReportError() = default;
};

template<typename T, typename... Args>
void reportsError(IReportError& errOut, Args&&... args) {
	errOut.reportsError(std::make_unique<T>(std::forward(args)...));
}

class Lexer {
private:
	std::istream& is;
	IReportError& errOut;
public:
	Lexer(std::istream& is, IReportError& errOut): is(is), errOut(errOut){}
	std::optional<Token> next();
private:
	std::string readIdentString();
	std::optional<Keyword> findKeyword(const std::string& str);
};

#endif