#ifndef HELPERS_SSOTESTUTILS
#define HELPERS_SSOTESTUTILS

#include <gmock/gmock.h>

#include <memory>

#include <tinyutf8/tinyutf8.h>

namespace Helpers_SSOTestUtils
{

template<class BASIC_UTF8_STRING>
class SSO_Capacity : public BASIC_UTF8_STRING
{
public:
	static constexpr typename BASIC_UTF8_STRING::size_type get_sso_capacity() noexcept
	{
		return BASIC_UTF8_STRING::get_sso_capacity();
	}
};

}
// namespace Helpers_SSOTestUtils

#endif // HELPERS_SSOTESTUTILS
