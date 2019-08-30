// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include <fbjni/detail/utf8.h>

using namespace std;
using namespace facebook::jni;

TEST(Utf16toUTF8_test, nullUtf16String) {
  EXPECT_TRUE(detail::utf16toUTF8(NULL, 10).empty());
}

TEST(Utf16toUTF8_test, negativeUtf16StringLength) {
  std::vector<uint16_t> utf16String = {'a'};
  EXPECT_TRUE(detail::utf16toUTF8(utf16String.data(), -1).empty());
}

TEST(Utf16toUTF8_test, zeroUtf16StringLength) {
  std::vector<uint16_t> utf16String = {'a'};
  EXPECT_TRUE(detail::utf16toUTF8(utf16String.data(), 0).empty());
}

TEST(Utf16toUTF8_test, badFormedUtf16String) {
  std::vector<uint16_t> utf16String = {'a', 'b', 'c', 0xD800};
  auto utf8String = detail::utf16toUTF8(utf16String.data(), utf16String.size());
  EXPECT_EQ(utf8String.size(), 6);
  EXPECT_EQ(utf8String, "abc\xED\xA0\x80");
}

TEST(Utf16toUTF8_test, goodUtf16String) {
  std::vector<uint16_t> utf16String = {'a', 0x0123, 0x1234, 0xD812, 0xDC34};
  auto utf8String = detail::utf16toUTF8(utf16String.data(), utf16String.size());
  EXPECT_EQ(utf8String.size(), 10);
  EXPECT_EQ(utf8String, "a\xC4\xA3\xE1\x88\xB4\xF0\x94\xA0\xB4");
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
