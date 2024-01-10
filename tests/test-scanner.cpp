#include <gtest/gtest.h>
#include "../tplcc/scanner.h"

TEST(TestTextScanner, scanner_get) {
	const std::string input { "hello, world" };
	TextScanner textScanner(input);

	for (const char ch : input) {
		EXPECT_EQ(textScanner.get(), ch);
	}

	EXPECT_EQ(textScanner.get(), EOF);
}