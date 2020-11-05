#include <gtest/gtest.h>

#include <tinyutf8/tinyutf8.h>

TEST(TinyUTF8, FindSubstr)
{
	tiny_utf8::string str = U"Hello World ツ♫";

	const char32_t* find_last_not_of = U"ツ♫";
	const char32_t* find_last_of = U"e";
	const char32_t find = U'l';
	const char32_t rfind = U'l';

	EXPECT_EQ(str.find_last_not_of(find_last_not_of), 11);
	EXPECT_EQ(str.find_last_of(find_last_of), 1);
	EXPECT_EQ(str.find(find), 2);
	EXPECT_EQ(str.rfind(rfind), 9);
}

TEST(TinyUTF8, StartsEndsWith)
{
	tiny_utf8::string str = U"Hello World ツ♫";

	const char32_t* ends_with_positive = U"ツ♫";
	const char32_t* ends_with_negative = U"e";
	const char32_t* starts_with_positive = U"Hello ";
	const char32_t* starts_with_negative = U"Hell ";

	EXPECT_EQ(str.ends_with(ends_with_positive), true);
	EXPECT_EQ(str.ends_with(ends_with_negative), false);
	EXPECT_EQ(str.ends_with(tiny_utf8::string(ends_with_positive)), true);
	EXPECT_EQ(str.ends_with(tiny_utf8::string(ends_with_negative)), false);
	EXPECT_EQ(str.starts_with(starts_with_positive), true);
	EXPECT_EQ(str.starts_with(starts_with_negative), false);
	EXPECT_EQ(str.starts_with(tiny_utf8::string(starts_with_positive)), true);
	EXPECT_EQ(str.starts_with(tiny_utf8::string(starts_with_negative)), false);
}
