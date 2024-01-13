#ifndef TPLCC_SCANNER_H
#define TPLCC_SCANNER_H

#include <vector>
#include <string>
#include <cstdint>

struct IScanner {
	virtual int get() = 0;
	virtual int peek() = 0;
	virtual std::vector<int> peekN(size_t n) = 0;
	virtual void ignore() = 0;
	virtual void ignoreN(size_t n) = 0;
	virtual bool reachedEndOfInput() = 0;
	virtual std::uint32_t offset() = 0;
	virtual ~IScanner() = default;
};


class TextScanner : public IScanner {
	const std::string input;
	size_t cursor = 0;
	std::vector<size_t> startOfLineIndice;
public:
	TextScanner(std::string input);
	virtual int get() override;
	virtual int peek() override;
	virtual std::vector<int> peekN(size_t n) override;
	virtual void ignore() override;
	virtual void ignoreN(size_t n) override;
	virtual bool reachedEndOfInput() override;
	virtual std::uint32_t offset() override;
};

#endif // !TPLCC_SCANNER_H
