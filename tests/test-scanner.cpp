#include <gtest/gtest.h>
#include "../tplcc/scanner.h"

TEST(TestTextScanner, scanner_get_and_peek) {
	const std::string input { "hello, world" };
	TextScanner textScanner(input);

	for (const char ch : input) {
		EXPECT_EQ(textScanner.peek(), ch);
		EXPECT_EQ(textScanner.get(), ch);
	}

	EXPECT_EQ(textScanner.get(), EOF);
}

TEST(TestTextScanner, scanner_getText) {
	const std::string input{
		"Lorem ipsum dolor sit amet,\n"
		"consectetur adipiscing elit,\r"
		"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\r\n"
		"Ut enim ad minim veniam,\r\n"
		"quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. \n"
	};

	TextScanner sc(input);
	EXPECT_EQ(sc.getText(0, 0),
		"Lorem ipsum dolor sit amet,\n"
	);

	EXPECT_EQ(
		sc.getText(0, 2),
		"Lorem ipsum dolor sit amet,\n"
		"consectetur adipiscing elit,\r"
		"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\r\n"
	);

	EXPECT_EQ(
		sc.getText(2, 2),
		"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\r\n"
	);

	EXPECT_EQ(
		sc.getText(2, 3),
		"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.\r\n"
		"Ut enim ad minim veniam,\r\n"
	);
}