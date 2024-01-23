#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "tplcc/code-buffer.h"
#include "tplcc/encoding-cursor.h"
#include "./utils/helpers.h"

class TestUTF8Cursor : public ::testing::Test {
 protected:
  std::unique_ptr<CodeBuffer> buffer;
  std::unique_ptr<UTF8Cursor> cursor;
  CodeBuffer::SectionID sectionID;

  void setUpWithInput(std::string input) {
    buffer = std::make_unique<CodeBuffer>();
    sectionID = buffer->addSection(input);
    cursor = std::make_unique<UTF8Cursor>(*buffer, sectionID);
  }

  bool reachedEndOfInput() {
    return cursor->offset() >= buffer->sectionEnd(sectionID);
  }
};

TEST_F(TestUTF8Cursor, scan_ascii) {
  std::string input{"abcdefg"};
  setUpWithInput(input);

  for (const char ch : input) {
    EXPECT_EQ(ch, cursor->currentChar());
    cursor->next();
  }
}

TEST_F(TestUTF8Cursor, scan_multibytes) {
  setUpWithInput(fromUTF8(u8"aα你😀"));
  std::vector<std::uint32_t> expectedCodePoints{97, 945, 20320, 128512};


  for (std::size_t i = 0; i < expectedCodePoints.size(); i++) {
    EXPECT_EQ(cursor->currentChar(), expectedCodePoints[i]);
    cursor->next();
  }
}