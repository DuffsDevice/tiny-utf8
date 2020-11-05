#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

#define TINY_UTF8_NOEXCEPT true
#include <tinyutf8/tinyutf8.h>

#include "mocks/mock_nothrowallocator.h"
#include "mocks/mock_throwallocator.h"

TEST(TinyUTF8, CTor_TakeALiteral_WithAllocator_Throw)
{
	// Make sure that the mock allocator will throw an exception...

	ASSERT_FALSE(std::is_nothrow_constructible<ThrowAllocator<char>>::value);
	ASSERT_FALSE(std::is_nothrow_copy_constructible<ThrowAllocator<char>>::value);
	ASSERT_FALSE(std::is_nothrow_move_constructible<ThrowAllocator<char>>::value);

	using alloc_utf8_string = tiny_utf8::basic_string<char32_t, char, ThrowAllocator<char>>;

	const char TEST_LITERAL[1] = { 'R' };
	const char TEST_STRING[29] = "This Is A Const Char* String";

	auto x = alloc_utf8_string(TEST_LITERAL, ThrowAllocator<char>());
	const bool ThrowCTor = std::is_nothrow_constructible<decltype(x)>::value;
	const bool ThrowCopyCTor = std::is_nothrow_constructible<decltype(x)>::value;
	const bool ThrowMoveCTor = std::is_nothrow_move_constructible<decltype(x)>::value;

	EXPECT_FALSE(ThrowCTor);
	EXPECT_FALSE(ThrowCopyCTor);
	EXPECT_FALSE(ThrowMoveCTor);
	alloc_utf8_string str(TEST_LITERAL);
}

TEST(TinyUTF8, CTor_TakeALiteral_WithAllocator_NoThrow)
{
	// Make sure that the mock allocator will NOT throw an exception...

	ASSERT_TRUE(std::is_nothrow_constructible<NoThrowAllocator<char>>::value);
	ASSERT_TRUE(std::is_nothrow_copy_constructible<NoThrowAllocator<char>>::value);
	ASSERT_TRUE(std::is_nothrow_move_constructible<NoThrowAllocator<char>>::value);

	using alloc_utf8_string = tiny_utf8::basic_string<char32_t, char, NoThrowAllocator<char>>;

	const char TEST_LITERAL[1] = { 'R' };
	const char TEST_STRING[29] = "This Is A Const Char* String";

	auto x = alloc_utf8_string(TEST_LITERAL, NoThrowAllocator<char>());
	const bool NoThrowCTor = std::is_nothrow_constructible<decltype(x)>::value;
	const bool NoThrowCopyCTor = std::is_nothrow_constructible<decltype(x)>::value;
	const bool NoThrowMoveCTor = std::is_nothrow_move_constructible<decltype(x)>::value;

	EXPECT_TRUE(NoThrowCTor);
	EXPECT_TRUE(NoThrowCopyCTor);
	EXPECT_TRUE(NoThrowMoveCTor);
	alloc_utf8_string str(TEST_LITERAL);
}
