#ifndef MOCK_NOTHROWALLOCATOR_H_
#define MOCK_NOTHROWALLOCATOR_H_

#include <cstddef>

template<typename T = char>
class NoThrowAllocator
{
public:
	using value_type = T;

	template <class Other>
	struct rebind
	{
		using other = NoThrowAllocator<Other>;
	};

	NoThrowAllocator() noexcept 
	{
	}

	NoThrowAllocator(const NoThrowAllocator&) noexcept
	{
	}

	NoThrowAllocator(NoThrowAllocator&&) noexcept
	{
	}

	template<typename U>
	NoThrowAllocator(const NoThrowAllocator<U>&) noexcept 
	{
	}

	template<typename U>
	NoThrowAllocator(NoThrowAllocator<U>&&) noexcept
	{
	}

	T* allocate(std::size_t n, const void* hint = 0) noexcept
	{
		(void)n;
		return nullptr;
	}

	void deallocate(T* p, std::size_t n) noexcept
	{
		(void)p;
		(void)n;
	}
};

template<typename T, typename U>
constexpr bool operator== (const NoThrowAllocator<T>&, const NoThrowAllocator<U>&) noexcept
{
	return true;
}

template<typename T, typename U>
constexpr bool operator!= (const NoThrowAllocator<T>&, const NoThrowAllocator<U>&) noexcept
{
	return false;
}

#endif // MOCK_NOTHROWALLOCATOR_H_
