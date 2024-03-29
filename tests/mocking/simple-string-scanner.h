#ifndef TPLCC_TESTS_MOCKING_SIMPLE_STRING_SCANNER_H
#define TPLCC_TESTS_MOCKING_SIMPLE_STRING_SCANNER_H

#include "tplcc/lexer.h"

// A simple string scanner mainly for testing.
class SimpleStringScanner : public ILexerScanner {
	const std::string input;
	size_t cursor = 0;
	std::vector<size_t> startOfLineIndice;
public:
	SimpleStringScanner(std::string input) : input(std::move(input)) {}
	virtual int get() override;
	virtual int peek() const override;
	virtual std::string peekN(size_t n) override;
	virtual void ignore() override;
	virtual void ignoreN(size_t n) override;
	virtual bool reachedEndOfInput() const override;
	virtual std::uint32_t offset() override;
};

#endif