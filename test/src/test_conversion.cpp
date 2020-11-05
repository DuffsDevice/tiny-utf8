#include <gtest/gtest.h>

#include <string>

#include <tinyutf8/tinyutf8.h>

TEST(TinyUTF8, ToWideLiteral)
{
	tiny_utf8::string str(std::string("Löwen, Bären, Vögel und Käfer sind Tiere."));

	std::unique_ptr<char32_t[]> ptr{ new char32_t[str.length() + 1] };
	str.to_wide_literal(ptr.get());

	tiny_utf8::string::const_iterator it_fwd = str.begin();
	for (size_t chcount = 0; chcount < str.length(); ++chcount)
	{
		EXPECT_TRUE((static_cast<uint64_t>(str[chcount])) == (static_cast<uint64_t>(ptr[chcount])));
		++it_fwd;
	}
}
