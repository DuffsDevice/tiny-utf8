#include <gtest/gtest.h>

#include <string>

#include <tinyutf8/tinyutf8.h>

TEST(TinyUTF8, CTor_TakeALiteral)
{
	tiny_utf8::utf8_string str(U"TEST: ツ♫");

	EXPECT_EQ(str.length(), 8);
	EXPECT_EQ(str.size(), 12);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());
	EXPECT_EQ(static_cast<uint64_t>(str[6]), 12484);
}

TEST(TinyUTF8, CTor_TakeALiteralWithMaxCodePoints)
{
	tiny_utf8::utf8_string str(U"ツ♫", 1);

	EXPECT_EQ(str.length(), 1);
	EXPECT_EQ(str.size(), 3);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());
	EXPECT_EQ(static_cast<uint64_t>(str[0]), 12484);
}

TEST(TinyUTF8, CTor_TakeAnAnsiString)
{
	const std::string ansi_str("Loewen, Boeren, Voegel und Koefer sind Tiere.");
	tiny_utf8::utf8_string str(ansi_str);

	EXPECT_EQ(ansi_str.length(), 45);
	EXPECT_EQ(ansi_str.size(), 45);
	EXPECT_EQ(str.length(), 45);
	EXPECT_EQ(str.size(), 45);

	EXPECT_FALSE(str.requires_unicode());
	EXPECT_FALSE(str.sso_active());
	EXPECT_TRUE(str.lut_active());
}

TEST(TinyUTF8, CopyCTor)
{
	tiny_utf8::utf8_string str_orig(U"Hello  ツ  World");
	tiny_utf8::utf8_string str(str_orig);

	EXPECT_EQ(str.length(), 15);
	EXPECT_EQ(str.size(), 17);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());
	EXPECT_EQ(static_cast<uint64_t>(str[8]), 32);
}
