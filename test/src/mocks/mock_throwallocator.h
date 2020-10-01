#ifndef MOCK_THROWALLOCATOR_H_
#define MOCK_THROWALLOCATOR_H_

#include <cstddef>
#include <exception> // For std::bad_alloc (Linux and Windows)
#include <new>		 // For std::bad_alloc (MacOS)

template<typename T = char>
class ThrowAllocator
{
public:
	using value_type = T;

	template <class Other>
	struct rebind
	{
		using other = ThrowAllocator<Other>;
	};

	ThrowAllocator() 
	{
	}

	ThrowAllocator(const ThrowAllocator&)
	{
	}

	ThrowAllocator(ThrowAllocator&&)
	{
	}

	template<typename U>
	ThrowAllocator(const ThrowAllocator<U>&)
	{
	}

	template<typename U>
	ThrowAllocator(ThrowAllocator<U>&&)
	{
	}

	T* allocate(std::size_t n, const void* hint = 0)
	{
		throw std::bad_alloc();
	}

	void deallocate(T* p, std::size_t n)
	{
		(void)p;
		(void)n;
	}
};

template<typename T, typename U>
constexpr bool operator== (const ThrowAllocator<T>&, const ThrowAllocator<U>&)
{
	return true;
}

template<typename T, typename U>
constexpr bool operator!= (const ThrowAllocator<T>&, const ThrowAllocator<U>&)
{
	return false;
}

#endif // MOCK_THROWALLOCATOR_H_
