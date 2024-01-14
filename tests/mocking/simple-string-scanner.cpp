#include "./simple-string-scanner.h"

int SimpleStringScanner::get() {
	return reachedEndOfInput() ? EOF : input[cursor++];
}

int SimpleStringScanner::peek() {
	return reachedEndOfInput() ? EOF : input[cursor];
}

std::string SimpleStringScanner::peekN(size_t n) {
	std::string output;

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

void SimpleStringScanner::ignore() {
	get();
}

void SimpleStringScanner::ignoreN(size_t n) {
	for (size_t i = 0; !reachedEndOfInput() && i < n; i++) {
		get();
	}
}
bool SimpleStringScanner::reachedEndOfInput() {
	return cursor == input.size();
}

std::uint32_t SimpleStringScanner::offset() {
	return cursor;
}
