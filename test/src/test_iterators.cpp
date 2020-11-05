#include <gtest/gtest.h>

#include <algorithm>

#include <tinyutf8/tinyutf8.h>

TEST(TinyUTF8, IteratorAccess)
{
	const tiny_utf8::string str = U"Hello ツ World ♫";

	EXPECT_TRUE(str.requires_unicode());
	EXPECT_TRUE(str.sso_active());

	tiny_utf8::string str_fwd;
	tiny_utf8::string str_rev;

	std::copy(str.begin(), str.end(), str_fwd.begin());		// Copy using forward iterators
	std::copy(str.rbegin(), str.rend(), str_rev.begin());	// Copy using reverse iterators

	EXPECT_TRUE(
		std::equal(str.begin(), str.end(), str_fwd.begin(),
			[](const tiny_utf8::string::value_type& a, const tiny_utf8::string::value_type& b) -> bool
			{
				return ((static_cast<uint64_t>(a)) == (static_cast<uint64_t>(b)));
			}
		)
	);

	tiny_utf8::string::const_iterator it_src = str.end();
	tiny_utf8::string::const_iterator it_rev = str_rev.begin();
	for ( /* NOTHING HERE */; it_src != str.begin(); /* NOTHING HERE */)
	{
		--it_src;

		EXPECT_TRUE((static_cast<uint64_t>(*it_src)) == (static_cast<uint64_t>(*it_rev)));

		++it_rev;
	}

	tiny_utf8::string::const_iterator it_fwd = str.begin();
	for (size_t chcount = 0; chcount < str.length(); ++chcount)
	{
		EXPECT_TRUE((static_cast<uint64_t>(str[chcount])) == (static_cast<uint64_t>(*it_fwd)));
		++it_fwd;
	}
}
