#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include <tinyutf8/tinyutf8.h>

TEST(TinyUTF8, AppendString)
{
	// Test 2
	tiny_utf8::string str = U"Hello ツ";
	str.append(U" ♫ World");

	EXPECT_EQ(str.length(), 15);
	EXPECT_EQ(str.size(), 19);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());

	tiny_utf8::string tmp = U" ♫ World";
	str.append(tmp);

	EXPECT_EQ(str.length(), 23);
	EXPECT_EQ(str.size(), 29);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());
}

TEST(TinyUTF8, AppendAndShrinkString)
{
	tiny_utf8::string str(U"TEST: ツ♫");

	EXPECT_EQ(str.capacity(), 31);

	for (uint64_t iteration = 0; iteration < 5; ++iteration)
	{
		str.append(str); // yes, yes...self-append!

		switch (iteration)
		{
		case 0:
			EXPECT_EQ(str.capacity(), 31);
			EXPECT_EQ(str.length(), 16);
			EXPECT_EQ(str.size(), 24);
			EXPECT_FALSE(str.lut_active());
			break;
		case 1:
			EXPECT_EQ(str.capacity(), 72);
			EXPECT_EQ(str.length(), 32);
			EXPECT_EQ(str.size(), 48);
			EXPECT_TRUE(str.lut_active());
			break;
		case 2:
			EXPECT_EQ(str.capacity(), 72);
			EXPECT_EQ(str.length(), 64);
			EXPECT_EQ(str.size(), 96);
			EXPECT_TRUE(str.lut_active());
			break;
		case 3:
			EXPECT_EQ(str.capacity(), 231);
			EXPECT_EQ(str.length(), 128);
			EXPECT_EQ(str.size(), 192);
			EXPECT_TRUE(str.lut_active());
			break;
		case 4:
			EXPECT_EQ(str.capacity(), 519);
			EXPECT_EQ(str.length(), 256);
			EXPECT_EQ(str.size(), 384);
			EXPECT_TRUE(str.lut_active());
			break;
		case 5:
			EXPECT_EQ(str.capacity(), 519);
			EXPECT_EQ(str.length(), 512);
			EXPECT_EQ(str.size(), 768);
			EXPECT_TRUE(str.lut_active());
			break;
		}
	}

	str.shrink_to_fit();

	EXPECT_EQ(str.capacity(), 259); 
	EXPECT_EQ(str.length(), 256);
	EXPECT_EQ(str.size(), 384);
	EXPECT_TRUE(str.lut_active());
}

TEST(TinyUTF8, EraseString)
{
	tiny_utf8::string str = U"Hello ツ World ♫";

	EXPECT_EQ(str.length(), 15);
	EXPECT_EQ(str.size(), 19);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());

	str.erase(14);

	EXPECT_EQ(str.length(), 14);
	EXPECT_EQ(str.size(), 16);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());

	str.erase(str.begin(), str.begin() + 9);

	EXPECT_EQ(str.length(), 5);
	EXPECT_EQ(str.size(), 5);
	EXPECT_FALSE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
}

TEST(TinyUTF8, SubString)
{
	const tiny_utf8::string fullstr(U"Hello ツ World rg rth rt he rh we gxgre");

	const tiny_utf8::string str = fullstr.substr(3, 16);
	const tiny_utf8::string substr(U"lo ツ World rg rt");
		
	tiny_utf8::string::const_iterator it_str = str.begin();
	tiny_utf8::string::const_iterator it_sub = substr.begin();
	for ( /* NOTHING HERE */; it_sub != substr.end(); /* NOTHING HERE */)
	{
		EXPECT_TRUE((static_cast<uint64_t>(*it_str)) == (static_cast<uint64_t>(*it_sub)));

		++it_str;
		++it_sub;
	}

	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());

	EXPECT_EQ(static_cast<uint64_t>(str[6]), static_cast<uint64_t>(U'o'));
}

TEST(TinyUTF8, ReplaceString)
{
	tiny_utf8::string str(U"Hello ツ World");

	const std::string str_repl1("Hello World");
	const tiny_utf8::string str_repl2(U"Hello~ 🤝 ~World");

	const char32_t ch_repl1 = U'ツ';
	const char32_t ch_repl2 = U'🤝';

	EXPECT_EQ(static_cast<uint64_t>(str[6]), static_cast<uint64_t>(ch_repl1));
	EXPECT_EQ(str.length(), 13);
	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());
	EXPECT_FALSE(str.lut_active());

	str[6] = ch_repl2;
	EXPECT_EQ(static_cast<uint64_t>(str[6]), static_cast<uint64_t>(ch_repl2));
	EXPECT_EQ(str.length(), 13);
	EXPECT_TRUE(str.requires_unicode());

	str.replace(5, 3, " ");
	EXPECT_TRUE(
		std::equal(str.begin(), str.end(), str_repl1.begin(),
			[](const tiny_utf8::string::value_type& a, const tiny_utf8::string::value_type& b) -> bool
			{
				return ((static_cast<uint64_t>(a)) == (static_cast<uint64_t>(b)));
			}
		)
	);
	EXPECT_EQ(str.length(), 11);
	EXPECT_FALSE(str.requires_unicode());

	str.replace(str.begin() + 5, str.begin() + 6, U"~ 🤝 ~"); // Orig: 
	EXPECT_TRUE(
		std::equal(str.begin(), str.end(), str_repl2.begin(),
			[](const tiny_utf8::string::value_type& a, const tiny_utf8::string::value_type& b) -> bool
			{
				return ((static_cast<uint64_t>(a)) == (static_cast<uint64_t>(b)));
			}
		)
	);
	EXPECT_EQ(str.length(), 15);
	EXPECT_TRUE(str.requires_unicode());
}
