#include <vector>
#include <algorithm>
#include <numeric>
#include <cstdint>
#include <utility>
#include <optional>
#include <variant>
#include <functional>
#include <sstream>

#include "./Lexer.h"

namespace {
	template<std::ranges::range Container, typename Element>
	bool containsElement(Container&& container, Element&& el) {
		return std::find(container.begin(), container.end(), el) != container.end();
	}

	std::vector<Punctuator> allPunctuators() {
		std::vector<Punctuator> punctuators;
		std::istringstream ss(
			"[ ] ( ) { } . -> "
			"++ -- & * + - ~ ! "
			"/ % << >> < > <= >= == != ^ | && || "
			"? : ; "
			"= *= /= %= += -= <<= >>= &= ^= |= "
			", <: :> <% %>"
			// Preprocessor-only punctuators ... %:, %:%:, # and ##
			// are scanned and filtered by the preprocessor, the lexer
			// won't and shouldn't be received these punctuators, so we
			// can simply omit them.
		);
		std::transform(
			std::istream_iterator<std::string>(ss),
			std::istream_iterator<std::string>(),
			std::back_inserter(punctuators),
			[](const std::string& punc) { return Punctuator{ punc }; }
		);
		std::sort(punctuators.begin(), punctuators.end(),
			[](const Punctuator& p1, const Punctuator& p2) {
				return p1.str >= p2.str;
			});
		return punctuators;
	}

	static const std::vector<Punctuator> ALL_PUNCTUATORS = allPunctuators();

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
		IScanner& scanner;
		IReportError& errOut;
		std::vector<std::unique_ptr<Error>> listOfErrors;
		uint32_t startOffset;

	public:
		NumberLiteralScanner(IScanner& s, IReportError& e)
			: scanner(s), errOut(e), startOffset(s.offset()) {};
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
		while (std::isalpha(scanner.peek())) {
			buffer.push_back(scanner.get());
		}

		for (size_t i = beginIndexOfSuffix; i < buffer.size();) {
			const std::string* matched = nullptr;

			for (size_t j = 0; j < availableSuffixes.size(); j++) {
				if (hasSeenSuffix[j]) {
					goto fail;
				}

				auto& vecOfSuffixes = **(availableSuffixes.begin() + j);
				if (const auto matched = matchesSuffix(buffer, i, vecOfSuffixes)) {
					hasSeenSuffix[j] = true;
					i += matched->size();
					continue;
				}

				goto fail;
			}
		}

		return true;

	fail:
		const auto numberLiteralNoSuffix = buffer.substr(0, beginIndexOfSuffix);
		const auto invalidSuffix = buffer.substr(beginIndexOfSuffix);
		errOut.reportsError({
			{ startOffset, scanner.offset()},
			"\"" + invalidSuffix + "\" is not a valid suffix for the number literal " + numberLiteralNoSuffix + ".",
			"invalid suffix."
			});
		return false;
	}

	bool NumberLiteralScanner::scanIntegerSuffixes() {
		return scanSuffixes({ &UNSIGNED_INT_SUFFIX, &LONG_INT_SUFFIX });
	}

	bool NumberLiteralScanner::scanDoubleAndFloatSuffixes() {
		return scanSuffixes({ &TYPE_FLOAT_AND_DOUBLE_SUFFIX });
	}

	bool NumberLiteralScanner::scanExponentPart() {
		bool exponentHasNoDigit = true;

		buffer.push_back(scanner.get()); // read the 'e'/'E'/'p'/'P' at the beginning

		if (scanner.peek() == '+' || scanner.peek() == '-') {
			buffer.push_back(scanner.get());
		}

		while (std::isdigit(scanner.peek())) {
			buffer.push_back(scanner.get());
			exponentHasNoDigit = false;
		}

		if (exponentHasNoDigit) {
			while (std::isalnum(scanner.peek())) buffer.push_back(scanner.get());
			errOut.reportsError({
				{ startOffset, scanner.offset() },
				"Exponent part of number literal " + buffer + " has no digit.",
				"Exponent has no digit."
				});
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

	// The number first set of a c-number is {'0' - '9'} and {'.'}. This function should only be called when current
	// `is.peek()` is one of the characters in the first set.
	std::optional<NumberLiteral> NumberLiteralScanner::scan() {
		bool hasIntegerPart = false;
		bool hasFractionPart = false;
		auto numberBase = LiteralNumberBase::Decimal;

		buffer.clear();

		// Exam if it's a hexadecimal number.
		if (scanner.peek() == '0') {
			buffer.push_back(scanner.get());

			if (scanner.peek() == 'x' || scanner.peek() == 'X') {
				buffer.push_back(scanner.get());
				numberBase = LiteralNumberBase::Hexadecimal;
			}
		}

		// We don't know if a number is octal even if the first character is zero.
		// It could be a decimal floating point number, in case it has a fraction part. 
		// e.g. 0987.654 is a valid number. We should keep scanning digits until we
		// meet a non-digit, then we can drop a conclusion.

		while (isValidDigit(scanner.peek(), numberBase)) {
			buffer.push_back(scanner.get());
			hasIntegerPart = true;
		}

		// This branch handles literals that only have the integer part.
		if (scanner.peek() != '.') {
			if (isFirstCharOfExponentPart(scanner.peek(), numberBase)) {
				return scanAndCreateFloatLiteral(true);
			}

			if (buffer[0] == '0' &&
				numberBase != LiteralNumberBase::Hexadecimal &&
				std::any_of(buffer.begin(), buffer.end(), std::not_fn(isOctalDigit))
				) {
				errOut.reportsError({
					{ startOffset, scanner.offset() },
					"Invalid octal number.",
					"Invalid octal number."
					});
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

		buffer.push_back(scanner.get()); // read the decimal point

		// Read the fraction part.
		while (isValidDigit(scanner.peek(), numberBase)) {
			buffer.push_back(scanner.get());
			hasFractionPart = true;
		}

		// We will not meet invalid case like .e10f, .ll, .ace (!hasIntegerPart && !hasFractionPart)

		const bool hasExponentPart = isFirstCharOfExponentPart(scanner.peek(), numberBase);

		if (numberBase == LiteralNumberBase::Hexadecimal && !hasExponentPart) {
			while (std::isalnum(scanner.peek())) buffer.push_back(scanner.get());
			errOut.reportsError({
				{ startOffset, scanner.offset() },
				"Hexadecimal floating point " + buffer + " has no exponent part.",
				"Hex float has no exponent part."
				});
			return std::nullopt;
		}

		return hasFractionPart || hasExponentPart
			? scanAndCreateFloatLiteral(hasExponentPart)
			: scanAndCreateIntegerLiteral();
	}
}

constexpr bool isStringLiteralPrefix(const std::string& prefix) {
	return prefix == "L";
}

constexpr std::optional<CharSequenceLiteralPrefix> getCharSequencePrefix(const std::string& prefix) {
	using enum CharSequenceLiteralPrefix;
	if (prefix == "L") return L;
	else if (prefix == "") return None;
	else return std::nullopt;
}

// Scan the body of a char sequence (a string literal or a character literal)
void Lexer::scanCharSequenceContent(const char quote, std::string* output) {
	const auto startOffset = scanner.offset();
	scanner.ignore(); // ignore starting quote

	while (!scanner.reachedEndOfInput() && scanner.peek() != quote && scanner.peek() != '\n') {
		if (scanner.peek() == '\\') { // skip if it's a escaping.
			int ch = scanner.get();
			if (output) output->push_back(ch);
		}
		int ch = scanner.get();
		if (output) output->push_back(ch);
	}

	if (scanner.reachedEndOfInput() || scanner.peek() == '\n') {
		errOut.reportsError({
			{ startOffset, scanner.offset() },
			quote == '"'
				? "The string literal has no ending quote."
				: "The character literal has no ending quote.",
			"No ending quote."
			});
		throw std::exception("Irrecoverable error happened, compilation is interrupted.");
	}

	scanner.ignore(); // ignore the ending quote
}

std::optional<Token> Lexer::scanCharSequence(const char quote, const std::string& prefixStr, const std::uint32_t startOffset) {
	if (auto prefix = getCharSequencePrefix(prefixStr)) {
		std::string buffer;
		scanCharSequenceContent(quote, &buffer);
		if (quote == '"') return StringLiteral{ buffer, *prefix };
		else return CharacterLiteral{ buffer, *prefix };
	}
	else {
		scanCharSequenceContent(quote, nullptr);
		errOut.reportsError({
			{ startOffset, scanner.offset() },
			quote == '"'
				? "\"" + prefixStr + "\" is not a valid prefix for a string literal."
				: "\"" + prefixStr + "\" is not a valid prefix for a character literal.",
			"Invalid prefix."
			});
		return std::nullopt;
	}
}

// TODO: linear searching may be slow, try improving the performance later.
std::optional<Token> Lexer::scanPunctuator()
{
	for (const auto& punct : ALL_PUNCTUATORS) {
                const auto lookahead = scanner.peekN(punct.str.size());
                if (punct.str == std::string(lookahead.begin(), lookahead.end())) {
			scanner.ignoreN(punct.str.size());
			return punct;
		}
	}

	return std::nullopt;
}

// Get the next token from given input stream.
std::optional<Token> Lexer::next()
{
	while (std::isspace(scanner.peek())) scanner.ignore();

	if (scanner.reachedEndOfInput()) return EOI;

	if (scanner.peekN(2) == "//") {
		scanner.ignoreN(2);
		while (!scanner.reachedEndOfInput() && scanner.peek() != '\n') scanner.ignore();
		scanner.ignore();
		return next();
	}

	if (scanner.peekN(2) == "/*") {
		scanner.ignoreN(2);
		while (!scanner.reachedEndOfInput() && scanner.peekN(2) != "*/") scanner.ignore();
		scanner.ignoreN(2);
		return next();
	}

	if (std::isalpha(scanner.peek()) || scanner.peek() == '_') {
		auto startOffset = scanner.offset();
		auto buffer = readIdentString();

		if (scanner.peek() == '"' || scanner.peek() == '\'') {
			return scanCharSequence(scanner.peek(), buffer, startOffset);
		}
		if (auto result = findKeyword(buffer)) {
			return *result;
		}

		return Identifier{ std::move(buffer) };
	}

	if (scanner.peek() == '"' || scanner.peek() == '\'') {
		return scanCharSequence(scanner.peek(), "");
	}

	auto lookaheads = scanner.peekN(2);
	if (lookaheads[0] == '.') {
		if (std::isdigit(lookaheads[1])) {
			return NumberLiteralScanner(scanner, errOut).scan();
		}
		else {
			scanner.ignore();
			return Punctuator{ "." };
		}
	}

	if (std::isdigit(scanner.peek())) {
		return NumberLiteralScanner(scanner, errOut).scan();
	}

	if (auto punctuator = scanPunctuator()) {
		return punctuator;
	}

	auto startOffsetOfStrayChar = scanner.offset();
	auto strayChar = scanner.get();

	errOut.reportsError({
		{ startOffsetOfStrayChar, scanner.offset() },
		std::string("Stray \"") + (char)strayChar + "\" in program.",
		"Invalid character."
		});

	throw std::exception(); // report errors and abort if there is invalid character in the source code. e.g. ` and @.
}

std::string Lexer::readIdentString()
{
	std::string buffer;

	while (std::isdigit(scanner.peek()) || std::isalpha(scanner.peek()) || scanner.peek() == '_') {
		buffer.push_back(scanner.get());
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
