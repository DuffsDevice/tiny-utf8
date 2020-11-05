#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include <tinyutf8/tinyutf8.h>

#include "helpers/helpers_ssotestutils.h"

TEST(TinyUTF8, CTor_TakeALiteral)
{
	tiny_utf8::string str(U"TEST: ツ♫");

	EXPECT_EQ(str.length(), 8);
	EXPECT_EQ(str.size(), 12);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());
	EXPECT_EQ(static_cast<uint64_t>(str[6]), 12484);
}

TEST(TinyUTF8, CTor_TakeALiteral_SSOAndNoSSO)
{
	using test_utf8_string = tiny_utf8::basic_string<char32_t, char, std::allocator<char>>;

	constexpr uint32_t TEST_LITERAL_U_LENGTH = 1;
	constexpr uint32_t TEST_STRING_LENGTH = 100;

	const char TEST_LITERAL_T = 'T';
	const char TEST_LITERAL_U[TEST_LITERAL_U_LENGTH] = { 'U' };
	const char TEST_STRING[TEST_STRING_LENGTH] = "This is a test string...This is a test string...This is a test string...This is a test string...";

	// Calls -> Constructor that fills the string with the supplied character
	test_utf8_string str_literal(TEST_LITERAL_T, std::allocator<char>());

	// Calls -> Constructor taking an utf8 char literal (SSO)
	ASSERT_LT(TEST_LITERAL_U_LENGTH, Helpers_SSOTestUtils::SSO_Capacity<test_utf8_string>::get_sso_capacity());
	test_utf8_string str_literal_sso(TEST_LITERAL_U, std::allocator<char>());
	EXPECT_EQ(TEST_LITERAL_U_LENGTH, str_literal_sso.length());
	EXPECT_EQ(TEST_LITERAL_U_LENGTH, str_literal_sso.size());

	// Calls -> Constructor taking an utf8 char literal (NO SSO)
	ASSERT_GT(TEST_STRING_LENGTH, Helpers_SSOTestUtils::SSO_Capacity<test_utf8_string>::get_sso_capacity());
	test_utf8_string str_literal_nosso(TEST_STRING, std::allocator<char>());
	EXPECT_EQ(TEST_STRING_LENGTH-1, str_literal_nosso.length());	// Note that -1 accounts for null terminator
	EXPECT_EQ(TEST_STRING_LENGTH-1, str_literal_nosso.size());		// Note that -1 accounts for null terminator

}

TEST(TinyUTF8, CTor_TakeALiteralWithMaxCodePoints)
{
	tiny_utf8::string str(U"ツ♫", 1);

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
	tiny_utf8::string str(ansi_str);

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
	tiny_utf8::string str_orig(U"Hello  ツ  World");
	tiny_utf8::string str(str_orig);

	EXPECT_EQ(str.length(), 15);
	EXPECT_EQ(str.size(), 17);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());
	EXPECT_EQ(static_cast<uint64_t>(str[8]), 32);
}
