#include <vector>

#include "scanner.h"

TextScanner::TextScanner(std::string _input) : input(std::move(_input)) {
	findAndRecordStartOfLineIndice();
}

int TextScanner::get() {
	auto lookaheads = peekN(2);

	if (enteredNextLine()) {
		pos.lineNumber++;
		pos.charOffset = 0;
	}
	else {
		pos.charOffset++;
	}

	return reachedEndOfInput() ? EOF : input[cursor++];
}

int TextScanner::peek() {
	return reachedEndOfInput() ? EOF : input[cursor];
}

std::vector<int> TextScanner::peekN(size_t n) {
	std::vector<int> output;

	for (size_t i = 0; i < n; i++) {
		if (cursor + i >= input.size()) {
			output.push_back(EOF);
		}
		else {
			output.push_back(input[cursor + i]);
		}
	}
	return output;
}

void TextScanner::ignore() {
	if (!reachedEndOfInput()) cursor++;
}

void TextScanner::ignoreN(size_t n) {
	for (size_t i = 0; !reachedEndOfInput() && i < n; i++) {
		cursor++;
	}
}
bool TextScanner::reachedEndOfInput() {
	return cursor == input.size();
}

size_t TextScanner::numberOfConsumedChars() {
	return cursor;
}

CodePos TextScanner::currentCodePos() {
	return pos;
}

size_t TextScanner::currentLineNumber() {
	return pos.lineNumber;
}

size_t TextScanner::currentCharOffset() {
	return pos.charOffset;
}

bool TextScanner::enteredNextLine() {
	if (cursor == 0) return false;
	auto previousChar = input[cursor - 1];
	return previousChar == '\r' || previousChar == '\n';
}

std::string TextScanner::getText(size_t startLine, size_t endLine) {
	auto from = startOfLineIndice[startLine];
	auto count = startOfLineIndice[endLine + 1] - from;
	return input.substr(from, count);
}

void TextScanner::findAndRecordStartOfLineIndice() {
	startOfLineIndice.push_back(0);

	for (size_t i = 0; i < input.size();) {
		if (input[i] == '\r') {
			i++;
			if (i < input.size() && input[i] == '\n') {
				i++;
			}
			startOfLineIndice.push_back(i);
		}
		else if (input[i] == '\n') {
			i++;
			startOfLineIndice.push_back(i);
		}
		else {
			i++;
		}
	}
}
