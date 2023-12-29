#include <vector>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <optional>
#include <variant>

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
			// Otherwise it must be a hexadecimal number, since any octal number in c can only be an integer.
			return ch == 'p' || ch == 'P';
		}
	}
}

/// <summary>
/// Get the next token from given input stream.
/// </summary>
/// <param name="is"></param>
/// <returns></returns>
std::variant<Token, LexerError> Lexer::next(std::istream& is)
{
	if (std::isalpha(is.peek()) || is.peek() == '_') {
		readIdentifierString(is);

		auto result = findKeyword(buffer);
		if (result) keyword = *result;
		return result ? TokenKind::Keyword : TokenKind::Ident;
	}
	else if (std::isdigit(is.peek()) || is.peek() == '.') {
		if (auto err = readNumber(is)) return *err;
		else return TokenKind::NumberLiteral;
	}
	else {
		return character = is.get();
	}
}

char Lexer::getCharacter()
{
	return character;
}

std::string Lexer::getIdentStr()
{
	return buffer;
}

Keyword Lexer::getKeyword()
{
	return keyword;
}

void Lexer::readIdentifierString(std::istream& is)
{
	buffer.clear();

	while (std::isdigit(is.peek()) || std::isalpha(is.peek()) || is.peek() == '_') {
		buffer.push_back(is.get());
	}
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


std::optional<LexerError> Lexer::scanSuffixes(std::istream& is, std::initializer_list<const std::vector<std::string>*> availableSuffixes) {
	std::vector<bool> hasSeenSuffix(availableSuffixes.size(), false);

	size_t beginIndexOfSuffix = buffer.size();
	while (std::isalpha(is.peek())) {
		buffer.push_back(is.get());
	}

	for (size_t i = beginIndexOfSuffix; i < buffer.size();) {
		const std::string* matched = nullptr;

		for (size_t j = 0; j < availableSuffixes.size(); j++) {
			if (hasSeenSuffix[j]) {
				return LexerError{ LexerError::INVALID_NUMBER_SUFFIX };
			}

			auto& vecOfSuffixes = **(availableSuffixes.begin() + j);
			if (const auto matched = matchesSuffix(buffer, i, vecOfSuffixes)) {
				hasSeenSuffix[j] = true;
				i += matched->size();
				continue;
			}

			return LexerError{ LexerError::INVALID_NUMBER_SUFFIX };
		}
	}

	return std::nullopt;
}

std::optional<LexerError> Lexer::scanIntegerSuffixes(std::istream& is) {
	return scanSuffixes(is, { &UNSIGNED_INT_SUFFIX, &LONG_INT_SUFFIX });
}

std::optional<LexerError> Lexer::scanDoubleAndFloatSuffixes(std::istream& is) {
	return scanSuffixes(is, { &TYPE_FLOAT_AND_DOUBLE_SUFFIX });
}

std::optional<LexerError> Lexer::scanExponentPart(std::istream& is) {
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
		return LexerError{ LexerError::EXPONENT_HAS_NO_DIGIT };
	}
	else {
		return {};
	}
}

// The number first set of a c-number is {'0' - '9'} กษ {'.'}. This function should only be called when current
// `is.peek()` is one of the characters in the first set.
std::optional<LexerError> Lexer::readNumber(std::istream& is) {
	bool hasIntegerPart = false;
	bool hasFractionPart = false;
	LiteralNumberBase numberBase = LiteralNumberBase::Decimal;

	buffer.clear();

	// Exam if it's a hexadecimal number.
	if (is.peek() == '0') {
		buffer.push_back(is.get());

		if (is.peek() == 'x' || is.peek() == 'X') {
			buffer.push_back(is.get());
			numberBase = LiteralNumberBase::Hexadecimal;
		}
	}

	// We cannot know if a number is octal even if the first character is zero.
	// It could be a decimal floating point number, in case it has a fraction part. 
	// e.g. 0987.654 is a valid number. We should keep scanning digits until we
	// meet a non-digit, then we can drop a conclusion.

	while (isValidDigit(is.peek(), numberBase)) {
		buffer.push_back(is.get());
		hasIntegerPart = true;
	}

	// If we can enter this if branch, the number must have an integer part.
	// (but not necessarily have a fraction part).
	// We needn't add the condition `hasIntegerPart` to ensure that.
	if (is.peek() != '.') {
		if (buffer[0] == '0' && numberBase != LiteralNumberBase::Hexadecimal) {
			if (std::all_of(buffer.begin(), buffer.end(), isOctalDigit)) {
				numberBase = LiteralNumberBase::Octal;
				return scanIntegerSuffixes(is);
			}
			else {
				return LexerError{ LexerError::INVALID_OCTAL_NUMBER };
			}
		}
		else if (isFirstCharOfExponentPart(is.peek(), numberBase)) {
			if (auto e = scanExponentPart(is)) return e;
			return scanDoubleAndFloatSuffixes(is);
		}
		else {
			return scanIntegerSuffixes(is);
		}

	}

	// Following code handles following two cases:
	// 
	// 1. !hasIntegerPart && is.peek() == '.', e.g. .33e10f
	// 2. hasIntegerPart && is.peek() == '.', e.g. 100.33e10f
	//
	// Case !hasIntegerPart && is.peek() != '.' is impossible. Because if a number doesn't
	// have an integer part, its first character must be a decimal point '.'. If it weren't
	// that means we've called the function even when the current character isn't a digit nor a
	// decimal point, which is garentee won't going to happen.

	buffer.push_back(is.get()); // read the decimal point

	// Read the fraction part.
	while (isValidDigit(is.peek(), numberBase)) {
		buffer.push_back(is.get());
		hasFractionPart = true;
	}

	// It is a floating point number as long as it has a fraction part.
	if (hasFractionPart || hasIntegerPart) {
		if (isFirstCharOfExponentPart(is.peek(), numberBase)) {
			if (auto e = scanExponentPart(is)) return e;
			return scanDoubleAndFloatSuffixes(is);
		}
		else if (numberBase == LiteralNumberBase::Hexadecimal) {
			while (std::isalnum(is.peek())) buffer.push_back(is.get());
			return LexerError{ LexerError::HEX_FLOAT_HAS_NO_EXPONENT };
		}
		else {
			return scanDoubleAndFloatSuffixes(is);
		}
	}
	else {
		// !hasIntegerPart && !hasFractionPart
		// invalid case e.g. .e10f, .ll, .ace.
		while (std::isalnum(is.peek())) is.get();
		return LexerError{ LexerError::INVALID_NUMBER };
	}
}

std::string Lexer::getNumLiteralStr() {
	return buffer;
}