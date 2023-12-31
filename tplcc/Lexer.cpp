#include <vector>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <optional>
#include <variant>
#include <functional>

#include "./Lexer.h"

namespace {
	static const std::vector<std::string> UNSIGNED_INT_SUFFIX{
		"u",
		"U"
	};

	// The order matters! See the code that uses the constant and you'll know why.
	static const std::vector<std::string> LONG_INT_SUFFIX{
		"ll",
		"LL",
		"l",
		"L",
	};

	static const std::vector<std::string> TYPE_FLOAT_SUFFIX{
		"F",
		"f"
	};

	static const std::vector<std::string> TYPE_FLOAT_AND_DOUBLE_SUFFIX{
		"F",
		"f",
		"l",
		"L"
	};

	static std::vector<std::pair<const char*, Keyword>> keywordPairs{
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

	enum class LiteralNumberBase {
		Octal,
		Decimal,
		Hexadecimal
	};

	const std::string* matchesSuffix(const std::string& str, size_t offset, const std::vector<std::string>& validAlternatives) {
		auto result = std::find_if(validAlternatives.begin(), validAlternatives.end(), [&str, offset](const std::string& suffix) {
			return str.substr(offset).rfind(suffix, 0) == 0;
			});

		return result == validAlternatives.end() ? nullptr : &(*result);
	}

	bool isOctalDigit(char ch) {
		return ch >= '0' && ch <= '7';
	}

	bool isValidDigit(const char ch, LiteralNumberBase numberBase) {
		if (numberBase == LiteralNumberBase::Octal) {
			return isOctalDigit(ch);
		}
		else if (numberBase == LiteralNumberBase::Decimal) {
			return std::isdigit(ch);
		}
		else {
			return std::isdigit(ch)
				|| ch >= 'a' && ch <= 'f'
				|| ch >= 'A' && ch <= 'F';
		}
	}

	bool isFirstCharOfExponentPart(const char ch, LiteralNumberBase numberBase) {
		if (numberBase == LiteralNumberBase::Decimal) {
			return ch == 'e' || ch == 'E';
		}
		else {
			// Otherwise it must be a hexadecimal number, since any C octal number can only be an integer.
			return ch == 'p' || ch == 'P';
		}
	}
	
	class NumberLiteralScanner {
		std::string buffer;
		std::istream& is;
		IReportError &errOut;
		std::vector<std::unique_ptr<Error>> listOfErrors; 

	public:
		NumberLiteralScanner(std::istream& is, IReportError& errOut) : is(is), errOut(errOut) {};
		std::optional<NumberLiteral> scan();
	private:
		
		// Following functions will return false if they find errors when scanning.
		bool scanSuffixes(std::initializer_list<const std::vector<std::string>*> availableSuffixes);
		bool scanIntegerSuffixes();
		bool scanDoubleAndFloatSuffixes();
		bool scanExponentPart();
		std::optional<NumberLiteral> scanAndCreateIntegerLiteral();
		std::optional<NumberLiteral> scanAndCreateFloatLiteral(bool hasExponentPart);
	};

	bool NumberLiteralScanner::scanSuffixes(std::initializer_list<const std::vector<std::string>*> availableSuffixes) {
		std::vector<bool> hasSeenSuffix(availableSuffixes.size(), false);

		size_t beginIndexOfSuffix = buffer.size();
		while (std::isalpha(is.peek())) {
			buffer.push_back(is.get());
		}

		for (size_t i = beginIndexOfSuffix; i < buffer.size();) {
			const std::string* matched = nullptr;

			for (size_t j = 0; j < availableSuffixes.size(); j++) {
				if (hasSeenSuffix[j]) {
					reportsError<InvalidNumberSuffix>(errOut);
					return false;
				}

				auto& vecOfSuffixes = **(availableSuffixes.begin() + j);
				if (const auto matched = matchesSuffix(buffer, i, vecOfSuffixes)) {
					hasSeenSuffix[j] = true;
					i += matched->size();
					continue;
				}

				reportsError<InvalidNumberSuffix>(errOut);
				return false;
			}
		}

		return true;
	}

	bool NumberLiteralScanner::scanIntegerSuffixes() {
		return scanSuffixes({ &UNSIGNED_INT_SUFFIX, &LONG_INT_SUFFIX });
	}

	bool NumberLiteralScanner::scanDoubleAndFloatSuffixes() {
		return scanSuffixes({ &TYPE_FLOAT_AND_DOUBLE_SUFFIX });
	}

	bool NumberLiteralScanner::scanExponentPart()
	{
		bool exponentHasNoDigit = true;

		buffer.push_back(is.get()); // read the 'e'/'E'/'p'/'P' at the beginning

		if (is.peek() == '+' || is.peek() == '-') {
			buffer.push_back(is.get());
		}

		while (std::isdigit(is.peek())) {
			buffer.push_back(is.get());
			exponentHasNoDigit = false;
		}

		if (exponentHasNoDigit) {
			while (std::isalnum(is.peek())) buffer.push_back(is.get());
			reportsError<ExponentHasNoDigit>(errOut);
			return false;
		}
		else {
			return true;
		}
	}

	std::optional<NumberLiteral> NumberLiteralScanner::scanAndCreateIntegerLiteral() {
		if (bool hasError = !scanIntegerSuffixes()) return std::nullopt;
		return NumberLiteral{ std::move(buffer) };
	}

	std::optional<NumberLiteral> NumberLiteralScanner::scanAndCreateFloatLiteral(bool hasExponentPart) {
		if (hasExponentPart && !scanExponentPart()) return std::nullopt;
		if (!scanDoubleAndFloatSuffixes()) return std::nullopt;
		return NumberLiteral{ std::move(buffer) };
	}

	// The number first set of a c-number is {'0' - '9'} �� {'.'}. This function should only be called when current
	// `is.peek()` is one of the characters in the first set.
	std::optional<NumberLiteral> NumberLiteralScanner::scan() {
		bool hasIntegerPart = false;
		bool hasFractionPart = false;
		auto numberBase = LiteralNumberBase::Decimal;

		buffer.clear();

		// Exam if it's a hexadecimal number.
		if (is.peek() == '0') {
			buffer.push_back(is.get());

			if (is.peek() == 'x' || is.peek() == 'X') {
				buffer.push_back(is.get());
				numberBase = LiteralNumberBase::Hexadecimal;
			}
		}

		// We don't know if a number is octal even if the first character is zero.
		// It could be a decimal floating point number, in case it has a fraction part. 
		// e.g. 0987.654 is a valid number. We should keep scanning digits until we
		// meet a non-digit, then we can drop a conclusion.

		while (isValidDigit(is.peek(), numberBase)) {
			buffer.push_back(is.get());
			hasIntegerPart = true;
		}

		// This branch handles literals that only have the integer part.
		if (is.peek() != '.') {
			if (isFirstCharOfExponentPart(is.peek(), numberBase)) {
				return scanAndCreateFloatLiteral(true);
			}

			if (buffer[0] == '0' && 
				numberBase != LiteralNumberBase::Hexadecimal && 
				std::any_of(buffer.begin(), buffer.end(), std::not_fn(isOctalDigit))
			) {
				reportsError<InvalidOctalNumber>(errOut);
				return std::nullopt;
			}

			return scanAndCreateIntegerLiteral();
		}

		// Following code handles following two cases:
		// 
		// 1. !hasIntegerPart && is.peek() == '.', e.g. .33e10f
		// 2. hasIntegerPart && is.peek() == '.', e.g. 100.33e10f
		//
		// The case (!hasIntegerPart && is.peek() != '.') is impossible. If a number doesn't
		// have an integer part, its first character must be a decimal point '.', otherwise we
		// we call the function even when the current character isn't a digit nor a '.',
		// which is garentee not gonna happen.

		buffer.push_back(is.get()); // read the decimal point

		// Read the fraction part.
		while (isValidDigit(is.peek(), numberBase)) {
			buffer.push_back(is.get());
			hasFractionPart = true;
		}

		if (!hasIntegerPart && !hasFractionPart) {
			// invalid case e.g. .e10f, .ll, .ace.
			while (std::isalnum(is.peek())) is.get();
			reportsError<InvalidNumber>(errOut);
			return std::nullopt;
		}

		const bool hasExponentPart = isFirstCharOfExponentPart(is.peek(), numberBase);

		if (numberBase == LiteralNumberBase::Hexadecimal && !hasExponentPart) {
			while (std::isalnum(is.peek())) buffer.push_back(is.get());
			reportsError<HexFloatHasNoExponent>(errOut);
			return std::nullopt;
		}

		return hasFractionPart || hasExponentPart
			? scanAndCreateFloatLiteral(hasExponentPart)
			: scanAndCreateIntegerLiteral();
	}
}

// Get the next token from given input stream.
std::optional<Token> Lexer::next()
{
	while (std::isblank(is.peek())) is.ignore();

	if (std::isalpha(is.peek()) || is.peek() == '_') {
		auto buffer = readIdentString();

		if (auto result = findKeyword(buffer)) {
			return *result;
		}
		else {
			return Ident{ std::move(buffer) };
		}
	}
	else if (std::isdigit(is.peek()) || is.peek() == '.') {
		return NumberLiteralScanner(is, errOut).scan();
	}
	else {
		return (char) is.get();
	}
}

std::string Lexer::readIdentString()
{
	std::string buffer;

	while (std::isdigit(is.peek()) || std::isalpha(is.peek()) || is.peek() == '_') {
		buffer.push_back(is.get());
	}

	return buffer;
}

std::optional<Keyword> Lexer::findKeyword(const std::string& str)
{
	auto pos = std::lower_bound(
		keywordPairs.begin(),
		keywordPairs.end(),
		str,
		[](const auto& pair, const std::string& targetKeywordString) {
			return pair.first < targetKeywordString;
		});

	if (pos != keywordPairs.end() && pos->first == str) {
		return pos->second;
	}
	else {
		return {};
	}
}
