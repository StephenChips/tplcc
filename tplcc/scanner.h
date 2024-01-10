#ifndef TPLCC_SCANNER_H
#define TPLCC_SCANNER_H

#include <vector>
#include <string>

struct CodePos {
	size_t lineNumber;
	size_t charOffset;
};

struct CodeRange {
	CodePos start, end;
};

struct IGetText {
	virtual std::string getText(size_t startLine, size_t endLine) = 0;
	virtual std::string getText(size_t line) { return this->getText(line, line); }

	~IGetText() = default;
};

struct IScanner {
	virtual int get() = 0;
	virtual int peek() = 0;
	virtual std::vector<int> peekN(size_t n) = 0;
	virtual void ignore() = 0;
	virtual void ignoreN(size_t n) = 0;
	virtual bool reachedEndOfInput() = 0;
	virtual size_t numberOfConsumedChars() = 0;
	virtual CodePos currentCodePos() = 0;
	virtual size_t currentCharOffset() = 0;
	virtual size_t currentLineNumber() = 0;
	virtual ~IScanner() = default;
};


class TextScanner : public IScanner, IGetText {
	const std::string input;
	size_t cursor = 0;
	CodePos pos{0, 0};
	std::vector<size_t> startOfLineIndice;
public:
	TextScanner(std::string input);
	virtual int get() override;
	virtual int peek() override;
	virtual std::vector<int> peekN(size_t n) override;
	virtual void ignore() override;
	virtual void ignoreN(size_t n) override;
	virtual bool reachedEndOfInput() override;
	virtual size_t numberOfConsumedChars() override;
	virtual CodePos currentCodePos() override;
	virtual size_t currentCharOffset() override;
	virtual size_t currentLineNumber() override;
	virtual std::string getText(size_t startLine, size_t endLine) override;
private:
	bool enteredNextLine();
	void findAndRecordStartOfLineIndice();
};

#endif // !TPLCC_SCANNER_H
