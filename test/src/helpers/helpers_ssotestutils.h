#ifndef HELPERS_SSOTESTUTILS
#define HELPERS_SSOTESTUTILS

#include <gmock/gmock.h>

#include <memory>

#include <tinyutf8/tinyutf8.h>

namespace Helpers_SSOTestUtils
{

template<class BASIC_STRING>
class SSO_Capacity : public BASIC_STRING
{
public:
	static constexpr typename BASIC_STRING::size_type get_sso_capacity() noexcept
	{
		return BASIC_STRING::get_sso_capacity();
	}
};

}
// namespace Helpers_SSOTestUtils

#endif // HELPERS_SSOTESTUTILS
