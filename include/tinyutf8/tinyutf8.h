/**
 * Copyright (c) 2015-2021 Jakob Riedle (DuffsDevice)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR 'AS IS' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _TINY_UTF8_H_
#define _TINY_UTF8_H_

// Includes
#include <memory> // for std::unique_ptr
#include <cstring> // for std::memcpy, std::memmove
#include <string> // for std::string
#include <limits> // for std::numeric_limits
#include <functional> // for std::hash
#include <algorithm> // for std::min, std::max
#include <type_traits> // for std::is_*
#include <cstddef> // for std::size_t and offsetof
#include <cstdint> // for std::uint8_t, std::uint16_t, std::uint32_t, std::uint_least16_t, std::uint_fast32_t
#include <initializer_list> // for std::initializer_list
#include <iosfwd> // for std::ostream and std::istream forward declarations
#ifdef _MSC_VER
#include <intrin.h> // for _BitScanReverse, _BitScanReverse64
#endif

//! Determine the mode of error handling
#ifndef TINY_UTF8_THROW
	#if defined(__cpp_exceptions) && !defined(TINY_UTF8_NOEXCEPT)
		#include <stdexcept> // for std::out_of_range
		#define TINY_UTF8_THROW( LOCATION , FAILING_PREDICATE ) throw std::out_of_range( LOCATION ": " #FAILING_PREDICATE )
	#else
		#define TINY_UTF8_THROW( ... ) void()
	#endif
#endif

//! Determine portable __cplusplus macro
#if defined(_MSC_VER) && defined(_MSVC_LANG)
	#define TINY_UTF8_CPLUSPLUS _MSVC_LANG
#else
	#define TINY_UTF8_CPLUSPLUS __cplusplus
#endif

//! Determine the way to inform about fallthrough behavior
#if TINY_UTF8_CPLUSPLUS >= 201703L
	#define TINY_UTF8_FALLTHROUGH [[fallthrough]];
#elif defined(__clang__)
	// Clang does not warn about implicit fallthrough
	#define TINY_UTF8_FALLTHROUGH
#elif defined(__GNUC__) && __GNUG__ > 6
	#define TINY_UTF8_FALLTHROUGH [[gnu::fallthrough]];
#else
	#define TINY_UTF8_FALLTHROUGH /* fall through */
#endif

//! Remove Warnings, since it is wrong for all cases in this file
#if defined(__clang__)
	#pragma clang diagnostic push
	// #pragma clang diagnostic ignored "-Wmaybe-uninitialized" // Clang is missing it. See https://bugs.llvm.org/show_bug.cgi?id=24979
#elif defined(__GNUC__)
	#pragma GCC diagnostic push
#elif defined(_MSC_VER)
	#pragma warning(push)
	#pragma warning(disable:4701) // Maybe unitialized
	#pragma warning(disable:4702) // Unreachable code after call to TINY_UTF8_THROW()
	#pragma warning(disable:4703) // Maybe unitialized
	#pragma warning(disable:26819) // Implicit Fallthrough
#endif

//! Create macro that yields its arguments, if C++17 or later is present (used for "if constexpr")
#if TINY_UTF8_CPLUSPLUS >= 201703L
	#define TINY_UTF8_CPP17( ... ) __VA_ARGS__
#else
	#define TINY_UTF8_CPP17( ... )
#endif

//! Determine noexcept specifications
#if defined(TINY_UTF8_NOEXCEPT)
	#undef TINY_UTF8_NOEXCEPT
	#define TINY_UTF8_NOEXCEPT true
#elif !defined(__cpp_exceptions)
	#define TINY_UTF8_NOEXCEPT true
#else
	#define TINY_UTF8_NOEXCEPT false
#endif

//! Want global declarations?
#ifdef TINY_UTF8_GLOBAL_NAMESPACE
inline
#endif

namespace tiny_utf8
{
	// Forward Declaration
	template<
		typename ValueType = char32_t
		, typename DataType = char
		, typename Allocator = std::allocator<DataType>
	>
	class basic_string;
	
	//! Typedef of string (data type: char)
	using string = basic_string<char32_t, char>;
	using utf8_string = basic_string<char32_t, char>; // For backwards compatibility
	
	//! Typedef of u8string (data type char8_t)
	#if defined(__cpp_char8_t)
		using u8string = basic_string<char32_t, char8_t>;
	#else
		using u8string = utf8_string;
	#endif
	
	//! Implementation Detail
	namespace tiny_utf8_detail
	{
		// Used for tag dispatching in constructor
		struct read_codepoints_tag{};
		struct read_bytes_tag{};
		
		//! Count leading zeros utility
		#if defined(__GNUC__)
			#define TINY_UTF8_HAS_CLZ true
			static inline unsigned int clz( unsigned int value ) noexcept { return (unsigned int)__builtin_clz( value ); }
			static inline unsigned int clz( unsigned long int value ) noexcept { return (unsigned int)__builtin_clzl( value ); }
			static inline unsigned int clz( char32_t value ) noexcept {
				return sizeof(char32_t) == sizeof(unsigned long int) ? (unsigned int)__builtin_clzl( value ) : (unsigned int)__builtin_clz( value );
			}
		#elif defined(_MSC_VER)
			#define TINY_UTF8_HAS_CLZ true
			template<typename T>
			static inline unsigned int lzcnt( T value ) noexcept {
				unsigned long value_log2;
				#if INTPTR_MAX >= INT64_MAX
					_BitScanReverse64( &value_log2 , value );
				#else
					_BitScanReverse( &value_log2 , value );
				#endif
				return sizeof(T) * 8 - value_log2 - 1;
			}
			static inline unsigned int clz( std::uint16_t value ) noexcept { return lzcnt( value ); }
			static inline unsigned int clz( std::uint32_t value ) noexcept { return lzcnt( value ); }
			#if INTPTR_MAX >= INT64_MAX
				static inline unsigned int clz( std::uint64_t value ) noexcept { return lzcnt( value ); }
			#endif
			static inline unsigned int clz( char32_t value ) noexcept { return lzcnt( value ); }
		#else
			#define TINY_UTF8_HAS_CLZ false
		#endif

		
		//! Helper to detect little endian
		class is_little_endian
		{
			constexpr static std::uint32_t	u4 = 1;
			constexpr static std::uint8_t	u1 = (const std::uint8_t &) u4;
		public:
			constexpr static bool value = u1;
		};
		
		//! Helper to modify the last (address-wise) byte of a little endian value of type 'T'
		template<typename T, std::size_t = sizeof(T)>
		union last_byte
		{
			T			number;
			struct{
				char	dummy[sizeof(T)-1];
				char	last;
			} bytes;
		};
		template<typename T>
		union last_byte<T, 1>
		{
			T			number;
			struct{
				char	last;
			} bytes;
		};

		//! strlen for different character types
		template<typename T>
		inline std::size_t strlen( const T* str ){ std::size_t len = 0u; while( *str++ ) ++len; return len; }
		template<> inline std::size_t strlen<char>( const char* str ){ return std::strlen( str ); }
	}


	template<typename Container, bool RangeCheck>
	struct codepoint_reference
	{
		typename Container::size_type	t_index;
		Container*						t_instance;
		
	public:
		
		//! Ctor
		codepoint_reference( typename Container::size_type index , Container* instance ) noexcept :
			t_index( index )
			, t_instance( instance )
		{}
		
		//! Cast to wide char
		operator typename Container::value_type() const noexcept( TINY_UTF8_NOEXCEPT || RangeCheck == false ) {
			if TINY_UTF8_CPP17(constexpr) ( RangeCheck )
				return static_cast<const Container*>(t_instance)->at( t_index );
			else
				return static_cast<const Container*>(t_instance)->at( t_index , std::nothrow );
		}
		
		//! Dereference operator to act as pointer type
		codepoint_reference& operator*() const noexcept { return *this; }
		
		//! Assignment operator
		codepoint_reference& operator=( typename Container::value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) {
			t_instance->replace( t_index , cp );
			return *this;
		}
		codepoint_reference& operator=( const codepoint_reference& ref ) noexcept(TINY_UTF8_NOEXCEPT) { return *this = (typename Container::value_type)ref; }
	};

	template<typename Container, bool RangeCheck>
	struct raw_codepoint_reference
	{
		typename Container::size_type	t_index;
		Container*						t_instance;
		
	public:
		
		//! Ctors
		raw_codepoint_reference( typename Container::size_type raw_index , Container* instance ) noexcept :
			t_index( raw_index )
			, t_instance( instance )
		{}
		template<bool RC>
		explicit raw_codepoint_reference( const codepoint_reference<Container, RC>& reference ) noexcept :
			t_index( reference.t_instance->get_num_bytes_from_start( reference.t_index ) )
			, t_instance( reference.t_instance )
		{}
		
		//! Cast to wide char
		operator typename Container::value_type() const noexcept( TINY_UTF8_NOEXCEPT || RangeCheck == false ) {
			if TINY_UTF8_CPP17(constexpr) ( RangeCheck )
				return static_cast<const Container*>(t_instance)->raw_at( t_index );
			else
				return static_cast<const Container*>(t_instance)->raw_at( t_index , std::nothrow );
		}
		
		//! Dereference operator to act as pointer type
		raw_codepoint_reference& operator*() const noexcept { return *this; }
		
		//! Cast to normal (non-raw) codepoint reference
		template<bool RC>
		explicit operator codepoint_reference<Container, RC>() const noexcept { return { t_instance->get_num_codepoints( 0 , t_index ) , t_instance }; }
		
		//! Assignment operator
		raw_codepoint_reference& operator=( typename Container::value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) {
			t_instance->raw_replace( t_index , t_instance->get_index_bytes( t_index ) , Container( cp ) );
			return *this;
		}
		raw_codepoint_reference& operator=( const raw_codepoint_reference& ref ) noexcept(TINY_UTF8_NOEXCEPT) { return *this = (typename Container::value_type)ref; }
	};

	// Codepoint-based iterator base
	template<typename Container, bool Raw>
	struct iterator_base
	{
		template<typename, typename, typename>
		friend class basic_string;

	public:

		typedef typename Container::value_type			value_type;
		typedef typename Container::difference_type		difference_type;
		typedef codepoint_reference<Container, false>	reference;
		typedef void*									pointer;
		typedef std::random_access_iterator_tag			iterator_category;

		bool operator==( const iterator_base& it ) const noexcept { return t_index == it.t_index; }
		bool operator!=( const iterator_base& it ) const noexcept { return t_index != it.t_index; }

		//! Ctor
		iterator_base( difference_type index , Container* instance ) noexcept :
			t_index( index )
			, t_instance( instance )
		{}

		//! Default function
		iterator_base() noexcept = default;
		iterator_base( const iterator_base& ) noexcept = default;
		iterator_base& operator=( const iterator_base& ) noexcept = default;

		//! Getter for the instance
		Container* get_instance() const noexcept { return t_instance; }

		// Getter for the iterator index
		difference_type get_index() const noexcept { return t_index; }

		//! Get the index of the codepoint the iterator points to
		difference_type get_raw_index() const noexcept { return t_instance->get_num_bytes_from_start( t_index ); }

		//! Get a reference to the codepoint the iterator points to
		reference get_reference() const noexcept { return t_instance->at( t_index , std::nothrow ); }

		//! Get the value that the iterator points to
		value_type get_value() const noexcept { return static_cast<const Container*>(t_instance)->at( t_index ); }

	protected:

		difference_type		t_index;
		Container*			t_instance = nullptr;

	protected:

		//! Advance the iterator n times (negative values allowed!)
		void advance( difference_type n ) noexcept { t_index += n; }

		//! Move the iterator one codepoint ahead
		void increment() noexcept { t_index++; }

		//! Move the iterator one codepoint backwards
		void decrement() noexcept { t_index--; }
	};

	// (Raw) Byte-based iterator base
	template<typename Container>
	struct iterator_base<Container, true>
	{
		template<typename, typename, typename>
		friend class basic_string;
		
	public:	
		
		typedef typename Container::value_type				value_type;
		typedef typename Container::difference_type			difference_type;
		typedef raw_codepoint_reference<Container, false>	reference;
		typedef void*										pointer;
		typedef std::bidirectional_iterator_tag				iterator_category;
		
		bool operator==( const iterator_base& it ) const noexcept { return t_index == it.t_index; }
		bool operator!=( const iterator_base& it ) const noexcept { return t_index != it.t_index; }
		
		//! Ctor
		iterator_base( difference_type index , Container* instance ) noexcept :
			t_index( index )
			, t_instance( instance )
		{}
		
		//! Default function
		iterator_base() noexcept = default;
		iterator_base( const iterator_base& ) noexcept = default;
		iterator_base& operator=( const iterator_base& ) noexcept = default;
		
		//! Getter for the instance
		Container* get_instance() const noexcept { return t_instance; }

		//! Constructor from non-raw iterator
		iterator_base( iterator_base<Container, false> other ) noexcept :
			t_index( other.get_raw_index() )
			, t_instance( other.get_instance() )
		{}

		//! Cast to non-raw iterator
		operator iterator_base<Container, false>() const noexcept { return { this->get_index() , t_instance }; }

		// Getter for the iterator index
		difference_type get_index() const noexcept { return t_instance->get_num_codepoints( 0 , t_index ); }

		//! Get the index of the codepoint the iterator points to
		difference_type get_raw_index() const noexcept { return t_index; }

		//! Get a reference to the codepoint the iterator points to
		reference get_reference() const noexcept { return t_instance->raw_at( t_index , std::nothrow ); }

		//! Get the value that the iterator points to
		value_type get_value() const noexcept { return static_cast<const Container*>(t_instance)->raw_at( t_index ); }

	protected:
		
		difference_type		t_index;
		Container*			t_instance = nullptr;
		
	protected:

		//! Advance the iterator n times (negative values allowed!)
		void advance( difference_type n ) noexcept {
			if( n > 0 )
				do
					increment();
				while( --n > 0 );
			else
				while( n++ < 0 )
					decrement();
		}
		
		//! Move the iterator one codepoint ahead
		void increment() noexcept { t_index += t_instance->get_index_bytes( t_index ); }

		//! Move the iterator one codepoint backwards
		void decrement() noexcept { t_index -= t_instance->get_index_pre_bytes( t_index ); }
	};
	
	template<typename Container, bool Raw> struct iterator;
	template<typename Container, bool Raw> struct const_iterator;
	template<typename Container, bool Raw> struct reverse_iterator;
	template<typename Container, bool Raw> struct const_reverse_iterator;
	
	template<typename Container, bool Raw = false>
	struct iterator : iterator_base<Container, Raw>
	{
		//! Ctor
		iterator( typename iterator_base<Container, Raw>::difference_type index , Container* instance ) noexcept :
			iterator_base<Container, Raw>( index , instance )
		{}
		iterator( const iterator<Container, !Raw>& other ) noexcept :
			iterator_base<Container, Raw>( other )
		{}
		
		//! Default Functions
		iterator() noexcept = default;
		iterator( const iterator& ) noexcept = default;
		iterator& operator=( const iterator& ) noexcept = default;
		
		//! Delete ctor from const iterator types
		iterator( const const_iterator<Container, Raw>& ) = delete;
		iterator( const const_reverse_iterator<Container, Raw>& ) = delete;
		
		//! Increase the Iterator by one
		iterator& operator++() noexcept { // prefix ++iter
			this->increment();
			return *this;
		}
		iterator operator++( int ) noexcept { // postfix iter++
			iterator tmp{ this->t_index , this->t_instance };
			this->increment();
			return tmp;
		}
		
		//! Decrease the iterator by one
		iterator& operator--() noexcept { // prefix --iter
			this->decrement();
			return *this;
		}
		iterator operator--( int ) noexcept { // postfix iter--
			iterator tmp{ this->t_index , this->t_instance };
			this->decrement();
			return tmp;
		}
		
		//! Increase the Iterator n times
		iterator operator+( typename iterator_base<Container, Raw>::difference_type n ) const noexcept {
			iterator it{*this};
			it.advance( n );
			return it;
		}
		iterator& operator+=( typename iterator_base<Container, Raw>::difference_type n ) noexcept {
			this->advance( n );
			return *this;
		}
		
		//! Decrease the Iterator n times
		iterator operator-( typename iterator_base<Container, Raw>::difference_type n ) const noexcept {
			iterator it{*this};
			it.advance( -n );
			return it;
		}
		iterator& operator-=( typename iterator_base<Container, Raw>::difference_type n ) noexcept {
			this->advance( -n );
			return *this;
		}
		
		//! Returns the value of the codepoint behind the iterator
		typename iterator::reference operator*() const noexcept { return this->get_reference(); }
	};

	template<typename Container, bool Raw>
	struct const_iterator : iterator<Container, Raw>
	{
		//! Ctor
		const_iterator( typename iterator_base<Container, Raw>::difference_type index , const Container* instance ) noexcept :
			iterator<Container, Raw>( index , const_cast<Container*>(instance) )
		{}
		
		//! Ctor from non const
		const_iterator( const iterator<Container, Raw>& other ) noexcept :
			iterator<Container, Raw>( other )
		{}
		const_iterator( const iterator<Container, !Raw>& other ) noexcept :
			iterator<Container, Raw>( other )
		{}
		
		//! Default Functions
		const_iterator() noexcept = default;
		const_iterator( const const_iterator& ) noexcept = default;
		const_iterator& operator=( const const_iterator& ) noexcept = default;
		
		//! Returns the (raw) value behind the iterator
		typename iterator<Container, Raw>::value_type operator*() const noexcept { return this->get_value(); }
	};

	template<typename Container, bool Raw>
	struct reverse_iterator : iterator_base<Container, Raw>
	{
		//! Ctor
		reverse_iterator( typename iterator_base<Container, Raw>::difference_type index , Container* instance ) noexcept :
			iterator_base<Container, Raw>( index , instance )
		{}
		
		//! Ctor from normal iterator
		reverse_iterator( const iterator<Container, Raw>& other ) noexcept :
			iterator_base<Container, Raw>( other )
		{}
		reverse_iterator( const iterator<Container, !Raw>& other ) noexcept :
			iterator_base<Container, Raw>( other )
		{}
		
		//! Default Functions
		reverse_iterator() noexcept = default;
		reverse_iterator( const reverse_iterator& ) noexcept = default;
		reverse_iterator& operator=( const reverse_iterator& ) noexcept = default;
		
		//! Delete ctor from const iterator types
		reverse_iterator( const const_iterator<Container, Raw>& ) = delete;
		reverse_iterator( const const_reverse_iterator<Container, Raw>& ) = delete;
		
		//! Increase the iterator by one
		reverse_iterator& operator++() noexcept { // prefix ++iter
			this->decrement();
			return *this;
		}
		reverse_iterator operator++( int ) noexcept { // postfix iter++
			reverse_iterator tmp{ this->t_index , this->t_instance };
			this->decrement();
			return tmp;
		}
		
		//! Decrease the Iterator by one
		reverse_iterator& operator--() noexcept { // prefix --iter
			this->increment();
			return *this;
		}
		reverse_iterator operator--( int ) noexcept { // postfix iter--
			reverse_iterator tmp{ this->t_index , this->t_instance };
			this->increment();
			return tmp;
		}
		
		//! Increase the Iterator n times
		reverse_iterator operator+( typename iterator_base<Container, Raw>::difference_type n ) const noexcept {
			reverse_iterator it{*this};
			it.advance( -n );
			return it;
		}
		reverse_iterator& operator+=( typename iterator_base<Container, Raw>::difference_type n ) noexcept {
			this->advance( -n );
			return *this;
		}
		
		//! Decrease the Iterator n times
		reverse_iterator operator-( typename iterator_base<Container, Raw>::difference_type n ) const noexcept {
			reverse_iterator it{*this};
			it.advance( n );
			return it;
		}
		reverse_iterator& operator-=( typename iterator_base<Container, Raw>::difference_type n ) noexcept {
			this->advance( n );
			return *this;
		}
		
		//! Returns the value of the codepoint behind the iterator
		typename iterator<Container, Raw>::reference operator*() const noexcept { return this->get_reference(); }
		
		//! Get the underlying iterator instance
		iterator<Container, Raw> base() const noexcept { return { this->t_index , this->t_instance }; }
	};

	template<typename Container, bool Raw>
	struct const_reverse_iterator : reverse_iterator<Container, Raw>
	{
		//! Ctor
		const_reverse_iterator( typename iterator_base<Container, Raw>::difference_type index , const Container* instance ) noexcept :
			reverse_iterator<Container, Raw>( index , const_cast<Container*>(instance) )
		{}
		
		//! Ctor from non const
		const_reverse_iterator( const reverse_iterator<Container, Raw>& other ) noexcept :
			reverse_iterator<Container, Raw>( other )
		{}
		const_reverse_iterator( const reverse_iterator<Container, !Raw>& other ) noexcept :
			reverse_iterator<Container, Raw>( other )
		{}
		
		//! Ctor from normal iterator
		const_reverse_iterator( const const_iterator<Container, Raw>& other ) noexcept :
			reverse_iterator<Container, Raw>( other )
		{}
		const_reverse_iterator( const const_iterator<Container, !Raw>& other ) noexcept :
			reverse_iterator<Container, Raw>( other )
		{}
		
		//! Default Functions
		const_reverse_iterator() noexcept = default;
		const_reverse_iterator( const const_reverse_iterator& ) noexcept = default;
		const_reverse_iterator& operator=( const const_reverse_iterator& ) noexcept = default;
		
		//! Returns the (raw) value behind the iterator
		typename iterator<Container, Raw>::value_type operator*() const noexcept { return this->get_value(); }
		
		//! Get the underlying iterator instance
		const_iterator<Container, Raw> base() const noexcept { return { this->t_index , this->t_instance }; }
	};
	
	
	//! Compare two iterators
	// Non-raw and "don't care"
	template<typename Container, bool Raw>
	static inline bool operator>( const const_iterator<Container, false>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() > rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator>( const const_reverse_iterator<Container, false>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() < rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator>=( const const_iterator<Container, false>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() >= rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator>=( const const_reverse_iterator<Container, false>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() <= rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<( const const_iterator<Container, false>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() < rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<( const const_reverse_iterator<Container, false>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() > rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<=( const const_iterator<Container, false>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() <= rhs.get_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<=( const const_reverse_iterator<Container, false>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_index() >= rhs.get_index(); }
	// Raw and "don't care"
	template<typename Container, bool Raw>
	static inline bool operator>( const const_iterator<Container, true>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() > rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator>( const const_reverse_iterator<Container, true>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() < rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator>=( const const_iterator<Container, true>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() >= rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator>=( const const_reverse_iterator<Container, true>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() <= rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<( const const_iterator<Container, true>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() < rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<( const const_reverse_iterator<Container, true>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() > rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<=( const const_iterator<Container, true>& lhs , const const_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() <= rhs.get_raw_index(); }
	template<typename Container, bool Raw>
	static inline bool operator<=( const const_reverse_iterator<Container, true>& lhs , const const_reverse_iterator<Container, Raw>& rhs ) noexcept { return lhs.get_raw_index() >= rhs.get_raw_index(); }
	
	/**
	 * Iterator difference computation functions (difference is in terms of codepoints)
	 */
	// Non-raw iterators
	template<typename Container>
	typename iterator<Container, true>::difference_type operator-( const iterator<Container, false>& lhs , const iterator<Container, false>& rhs ) noexcept {
		return lhs.get_index() - rhs.get_index();
	}
	template<typename Container>
	typename reverse_iterator<Container, true>::difference_type operator-( const reverse_iterator<Container, false>& lhs , const reverse_iterator<Container, false>& rhs ) noexcept {
		return rhs.get_index() - lhs.get_index();
	}
	// Raw Iterators
	template<typename Container>
	typename iterator<Container, true>::difference_type operator-( const iterator<Container, true>& lhs , const iterator<Container, true>& rhs ) noexcept {
		typename iterator<Container, true>::difference_type	minIndex = std::min( lhs.get_raw_index() , rhs.get_raw_index() );
		typename iterator<Container, true>::difference_type	max_index = std::max( lhs.get_raw_index() , rhs.get_raw_index() );
		typename iterator<Container, true>::difference_type	num_codepoints = lhs.get_instance()->get_num_codepoints( minIndex , max_index - minIndex );
		return max_index == lhs.get_raw_index() ? num_codepoints : -num_codepoints;
	}
	template<typename Container>
	typename reverse_iterator<Container, true>::difference_type operator-( const reverse_iterator<Container, true>& lhs , const reverse_iterator<Container, true>& rhs ) noexcept {
		typename reverse_iterator<Container, true>::difference_type	minIndex = std::min( lhs.get_raw_index() , rhs.get_raw_index() );
		typename reverse_iterator<Container, true>::difference_type	max_index = std::max( lhs.get_raw_index() , rhs.get_raw_index() );
		typename reverse_iterator<Container, true>::difference_type	num_codepoints = lhs.get_instance()->get_num_codepoints( minIndex , max_index - minIndex );
		return max_index == rhs.get_raw_index() ? num_codepoints : -num_codepoints;
	}


	// Base class for basic_string
	template<
		typename ValueType
		, typename DataType
		, typename Allocator
	>
	class basic_string : private Allocator
	{
	public:
		
		typedef DataType													data_type;
		typedef typename std::allocator_traits<Allocator>::size_type		size_type;
		typedef typename std::allocator_traits<Allocator>::difference_type	difference_type;
		typedef ValueType													value_type;
		typedef codepoint_reference<basic_string, false> 					reference;
		typedef codepoint_reference<basic_string, true> 					checked_reference;
		typedef raw_codepoint_reference<basic_string, false> 				raw_reference;
		typedef raw_codepoint_reference<basic_string, true> 				raw_checked_reference;
		typedef const value_type&											const_reference;
		typedef std::uint_fast8_t											width_type; // Data type capable of holding the number of code units in a codepoint
		typedef tiny_utf8::iterator<basic_string, false>					iterator;
		typedef tiny_utf8::const_iterator<basic_string, false>				const_iterator;
		typedef tiny_utf8::reverse_iterator<basic_string, false>			reverse_iterator;
		typedef tiny_utf8::const_reverse_iterator<basic_string, false>		const_reverse_iterator;
		typedef tiny_utf8::iterator<basic_string, true>						raw_iterator;
		typedef tiny_utf8::const_iterator<basic_string, true>				raw_const_iterator;
		typedef tiny_utf8::reverse_iterator<basic_string, true>				raw_reverse_iterator;
		typedef tiny_utf8::const_reverse_iterator<basic_string, true>		raw_const_reverse_iterator;
		typedef Allocator													allocator_type;
		typedef size_type													indicator_type; // Typedef for the lut indicator. Note: Don't change this, because else the buffer will not be a multiple of sizeof(size_type)
		enum : size_type{													npos = (size_type)-1 };
		
	protected: //! Layout specifications
		
		/*
		 * To determine, which layout is active, read either t_sso.data_len, or the last byte of t_non_sso.buffer_size:
		 * LSB == 0  =>  SSO
		 * LSB == 1  =>  NON-SSO
		 */
		
		// Layout used, if sso is inactive
		struct NON_SSO
		{
			data_type*		data; // Points to [ <data::char>... | '0'::char | <index::rle>... | <lut_indicator::size_type> ]
			size_type		data_len; // In bytes, excluding the trailing '\0'
			size_type		buffer_size; // Indicates the size of '::data' minus 'lut_width'
			size_type		string_len; // Shadows data_len on the last byte
		};
		
		// Layout used, if sso is active
		struct SSO
		{
			enum : size_type{	size = sizeof(NON_SSO)-1 };
			data_type			data[size];
			unsigned char		data_len; // This field holds ( size - num_characters ) << 1
			
			SSO( data_type value ) noexcept :
				data{ value , '\0' }
				, data_len( (unsigned char)( size - 1u ) << 1 )
			{}
			SSO() noexcept :
				data{ '\0' }
				, data_len( (unsigned char)( size - 0 ) << 1 )
			{}
		};
		
	protected: //! Attributes
		
		union{
			SSO		t_sso;
			NON_SSO	t_non_sso;
		};
		
	protected: //! Static helper methods
		
		//! Get the maximum number of bytes (excluding the trailing '\0') that can be stored within a basic_string object
		static constexpr inline size_type	get_sso_capacity() noexcept { return SSO::size; }
		
		//! SFINAE helpers for constructors
		template<size_type L>
		using enable_if_small_string = typename std::enable_if<( L <= SSO::size ), bool>::type;
		template<size_type L>
		using enable_if_not_small_string = typename std::enable_if<( L > SSO::size ), bool>::type;
		
		// Template to enable overloads, if the supplied type T is a character array without known bounds
		template<typename T, typename CharType, typename _DataType = bool>
		using enable_if_ptr = typename std::enable_if<
			std::is_pointer<typename std::remove_reference<T>::type>::value
			&&
			std::is_same<
				CharType
				, typename std::remove_cv<
					typename std::remove_pointer<
						typename std::remove_reference<T>::type
					>::type
				>::type
			>::value
			, _DataType
		>::type;
		
		//! Check, if the lut is active using the lut base ptr
		static inline bool					is_lut_active( const data_type* lut_base_ptr ) noexcept { return *((const unsigned char*)lut_base_ptr) & 0x1; }
		
		//! Rounds the supplied value to a multiple of sizeof(size_type)
		static inline size_type				round_up_to_align( size_type val ) noexcept {
			return ( val + sizeof(size_type) - 1 ) & ~( sizeof(size_type) - 1 );
		}
		
		//! Get the LUT base pointer from buffer and buffer size
		static inline data_type*			get_lut_base_ptr( data_type* buffer , size_type buffer_size ) noexcept { return buffer + buffer_size; }
		static inline const data_type*		get_lut_base_ptr( const data_type* buffer , size_type buffer_size ) noexcept { return buffer + buffer_size; }
		
		//! Construct the lut mode indicator
		static inline void					set_lut_indiciator( data_type* lut_base_ptr , bool active , size_type lut_len = 0 ) noexcept {
			*(indicator_type*)lut_base_ptr = active ? ( lut_len << 1 ) | 0x1 : 0;
		}
		//! Copy lut indicator
		static inline void					copy_lut_indicator( data_type* dest , const data_type* source ) noexcept {
			*(indicator_type*)dest = *(indicator_type*)source;
		}
		
		//! Determine, whether we will use a 'std::uint8_t', 'std::uint16_t', 'std::uint32_t' or 'std::uint64_t'-based index table.
		//! Returns the number of bytes of the destination data type
		static inline width_type			get_lut_width( size_type buffer_size ) noexcept {
			return buffer_size <= (size_type)std::numeric_limits<std::uint8_t>::max() + 1
				? sizeof(std::uint8_t)
				: buffer_size <= (size_type)std::numeric_limits<std::uint16_t>::max() + 1
					? sizeof(std::uint16_t)
					: buffer_size <= (size_type)std::numeric_limits<std::uint32_t>::max() + 1
						? sizeof(std::uint32_t)
						: sizeof(std::uint64_t)
			;
		}
		
		//! Determine, whether or not a LUT is worth to set up. General case: worth below 25%. If LUT present <33,3%, otherwise <16,7%
		static inline bool					is_lut_worth( size_type pot_lut_len , size_type string_len , bool lut_present , bool biased = true ) noexcept {
			size_type threshold = biased ? ( lut_present ? string_len / 3u : string_len / 6u ) : string_len / 4u;
			// Note pot_lut_len is supposed to underflow at '0'
			return size_type( pot_lut_len - 1 ) < threshold;
		}
		
		//! Determine the needed buffer size and the needed lut width (excluding the trailling LUT indicator)
		static inline size_type				determine_main_buffer_size( size_type data_len , size_type lut_len , width_type* lut_width ) noexcept {
			size_type width_guess	= get_lut_width( ++data_len ); // Don't forget, we need a terminating '\0', distinct from the lut indicator
			data_len += lut_len * width_guess; // Add the estimated number of bytes from the lut
			data_len += lut_len * ( ( *lut_width = get_lut_width( data_len ) ) - width_guess ); // Adjust the added bytes from the lut
			return round_up_to_align( data_len ); // Make the buffer size_type-aligned
		}
		//! Determine the needed buffer size if the lut width is known (excluding the trailling LUT indicator)
		static inline size_type				determine_main_buffer_size( size_type data_len , size_type lut_len , width_type lut_width ) noexcept {
			return round_up_to_align( data_len + 1 + lut_len * lut_width ); // Compute the size_type-aligned buffer size
		}
		//! Determine the needed buffer size if the lut is empty (excluding the trailling LUT indicator)
		static inline size_type				determine_main_buffer_size( size_type data_len ) noexcept {
			return round_up_to_align( data_len + 1 ); // Make the buffer size_type-aligned
		}
		
		//! Same as above but this time including the LUT indicator
		static inline size_type				determine_total_buffer_size( size_type main_buffer_size ) noexcept {
			return main_buffer_size + sizeof(indicator_type); // Add the lut indicator
		}
		
		//! Get the nth index within a multibyte index table
		static inline size_type				get_lut( const data_type* iter , width_type lut_width ) noexcept {
			switch( lut_width ){
			case sizeof(std::uint8_t):	return *(const std::uint8_t*)iter;
			case sizeof(std::uint16_t):	return *(const std::uint16_t*)iter;
			case sizeof(std::uint32_t):	return *(const std::uint32_t*)iter;
			}
			return (size_type)*(const std::uint64_t*)iter;
		}
		static inline void					set_lut( data_type* iter , width_type lut_width , size_type value ) noexcept {
			switch( lut_width ){
			case sizeof(std::uint8_t):	*(std::uint8_t*)iter = (std::uint8_t)value; break;
			case sizeof(std::uint16_t):	*(std::uint16_t*)iter = (std::uint16_t)value; break;
			case sizeof(std::uint32_t):	*(std::uint32_t*)iter = (std::uint32_t)value; break;
			case sizeof(std::uint64_t):	*(std::uint64_t*)iter = (std::uint64_t)value; break;
			}
		}
		
		//! Get the LUT size (given the lut is active!)
		static inline size_type				get_lut_len( const data_type* lut_base_ptr ) noexcept {
			return *(indicator_type*)lut_base_ptr >> 1;
		}
		
		/**
		 * Returns the number of code units (bytes) using the supplied first byte of a utf8 codepoint
		 */
		// Data left is the number of bytes left in the buffer INCLUDING this one
		#if TINY_UTF8_HAS_CLZ
		static inline width_type			get_codepoint_bytes( data_type first_byte , size_type data_left ) noexcept 
		{
			if( first_byte ){
				// Before counting the leading one's we need to shift the byte into the most significant part of the integer
				size_type codepoint_bytes = tiny_utf8_detail::clz( ~((unsigned int)first_byte << (sizeof(unsigned int)-1)*8 ) );
				
				// The test below would actually be ( codepoint_bytes <= data_left && codepoint_bytes ),
				// but codepoint_bytes is unsigned and thus wraps around zero, which makes the following faster:
				if( size_type( codepoint_bytes - 1 ) < size_type(data_left) )
					return (width_type)codepoint_bytes;
			}
			return 1;
		}
		#else
		static width_type					get_codepoint_bytes( data_type first_byte , size_type data_left ) noexcept ; // Defined in source file
		#endif
		
		/**
		 * Returns the number of code units (bytes) a codepoint will translate to in utf8
		 */
		static inline width_type			get_codepoint_bytes( value_type cp ) noexcept
		{
			#if TINY_UTF8_HAS_CLZ
				if( !cp )
					return 1;
				static const width_type lut[32] = {
					1 , 1 , 1 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 3
					, 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 6 , 7
				};
				return lut[ 31 - tiny_utf8_detail::clz( cp ) ];
			#else
				if( cp <= 0x7F )
					return 1;
				else if( cp <= 0x7FF )
					return 2;
				else if( cp <= 0xFFFF )
					return 3;
				else if( cp <= 0x1FFFFF )
					return 4;
				else if( cp <= 0x3FFFFFF )
					return 5;
				else if( cp <= 0x7FFFFFFF )
					return 6;
				else
					return 7;
			#endif
		}
		
		//! Returns the number of bytes to expect before this one (including this one) that belong to this utf8 char
		static width_type					get_num_bytes_of_utf8_char_before( const data_type* data_start , size_type index ) noexcept ;
		
		//! Decodes a given input of rle utf8 data to a unicode codepoint, given the number of bytes it's made of
		static inline value_type			decode_utf8( const data_type* data , width_type num_bytes ) noexcept {
			value_type cp = (unsigned char)*data;
			if( num_bytes > 1 ){
				cp &= 0x7F >> num_bytes; // Mask out the header bits
				for( width_type i = 1 ; i < num_bytes ; i++ )
					cp = ( cp << 6 ) | ( (unsigned char)data[i] & 0x3F );
			}
			return cp;
		}
		
		/**
		 * Decodes a given input of rle utf8 data to a
		 * unicode codepoint and returns the number of bytes it used
		 */
		static inline width_type			decode_utf8_and_len( const data_type* data , value_type& dest , size_type data_left ) noexcept {
			// See 'get_codepoint_bytes' for 'data_left'
			width_type num_bytes = basic_string::get_codepoint_bytes( *data , data_left );
			dest = decode_utf8( data , num_bytes );
			return num_bytes;
		}
		
		/**
		 * Encodes a given codepoint (expected to use 'cp_bytes') to a character
		 * buffer capable of holding that many bytes.
		 */
		inline static void					encode_utf8( value_type cp , data_type* dest , width_type cp_bytes ) noexcept {
			switch( cp_bytes ){
				case 7: dest[cp_bytes-6] = 0x80 | ((cp >> 30) & 0x3F); TINY_UTF8_FALLTHROUGH
				case 6: dest[cp_bytes-5] = 0x80 | ((cp >> 24) & 0x3F); TINY_UTF8_FALLTHROUGH
				case 5: dest[cp_bytes-4] = 0x80 | ((cp >> 18) & 0x3F); TINY_UTF8_FALLTHROUGH
				case 4: dest[cp_bytes-3] = 0x80 | ((cp >> 12) & 0x3F); TINY_UTF8_FALLTHROUGH
				case 3: dest[cp_bytes-2] = 0x80 | ((cp >>  6) & 0x3F); TINY_UTF8_FALLTHROUGH
				case 2: dest[cp_bytes-1] = 0x80 | ((cp >>  0) & 0x3F);
					dest[0] = (unsigned char)( ( std::uint_least16_t(0xFF00uL) >> cp_bytes ) | ( cp >> ( 6 * cp_bytes - 6 ) ) );
					break;
				case 1:
					dest[0] = (unsigned char)cp;
					break;
			}
		}
		
		/**
		 * Encodes a given codepoint to a character buffer of at least 7 bytes
		 * and returns the number of bytes it used
		 */
		inline static width_type			encode_utf8( value_type cp , data_type* dest ) noexcept {
			width_type width = get_codepoint_bytes( cp );
			basic_string::encode_utf8( cp , dest , width );
			return width;
		}
		
	protected: //! Non-static helper methods
		
		//! Set the main buffer size (also disables SSO)
		inline void			set_non_sso_string_len( size_type string_len ) noexcept
		{
			// Check, if NON_SSO is larger than its members, in which case it's not ambiguated by SSO::data_len
			if TINY_UTF8_CPP17(constexpr) ( offsetof(SSO, data_len) > offsetof(NON_SSO, string_len) + sizeof(NON_SSO::string_len) - 1 ){
				t_non_sso.string_len = string_len;
				t_sso.data_len = 0x1; // Manually set flag to deactivate SSO
			}
			else if TINY_UTF8_CPP17(constexpr) ( tiny_utf8_detail::is_little_endian::value ){
				tiny_utf8_detail::last_byte<size_type> lb;
				lb.number = string_len;
				lb.bytes.last <<= 1;
				lb.bytes.last |= 0x1;
				t_non_sso.string_len = lb.number;
			}
			else
				t_non_sso.string_len = ( string_len << 1 ) | size_type(0x1);
		}
		
		//! Get buffer size, if SSO is disabled
		inline size_type	get_non_sso_string_len() const noexcept {
			// Check, if NON_SSO is larger than its members, in which case it's not ambiguated by SSO::data_len
			if( offsetof(SSO, data_len) > offsetof(NON_SSO, string_len) + sizeof(NON_SSO::string_len) - 1 )
				return t_non_sso.string_len;
			else if( tiny_utf8_detail::is_little_endian::value ){
				tiny_utf8_detail::last_byte<size_type> lb;
				lb.number = t_non_sso.string_len;
				lb.bytes.last >>= 1;
				return lb.number;
			}
			else
				return t_non_sso.string_len >> 1;
		}
		
		//! Set the data length (also enables SSO)
		inline void			set_sso_data_len( unsigned char data_len = 0 ) noexcept {
			t_sso.data_len = (unsigned char)( SSO::size - data_len ) << 1;
		}
		
		//! Get the data length (when SSO is active)
		inline size_type	get_sso_data_len() const noexcept { return get_sso_capacity() - ( t_sso.data_len >> 1 ); }
		
		//! Return a good guess of how many codepoints the currently allocated buffer can hold
		size_type			get_non_sso_capacity() const noexcept ;
		
		//! Check, if sso is inactive (this operation doesn't require a negation and is faster)
		inline bool			sso_inactive() const noexcept { return t_sso.data_len & 0x1; }
		
		// Helper for requires_unicode_sso that generates masks of the form 10000000 10000000...
		template<typename T>
		static constexpr T	get_msb_mask( width_type bytes = sizeof(T) ) noexcept { return bytes ? ( T(1) << ( 8 * bytes - 1 ) ) | get_msb_mask<T>( bytes - 1 ) : T(0); }
		
		//! Check, whether the string contains codepoints > 127
		bool				requires_unicode_sso() const noexcept ;
		
		//! Get buffer
		inline const data_type*	get_buffer() const noexcept { return sso_inactive() ? t_non_sso.data : t_sso.data; }
		inline data_type*		get_buffer() noexcept { return sso_inactive() ? t_non_sso.data : t_sso.data; }
		
		//! Get buffer size (excluding the trailing LUT indicator)
		inline size_type	get_buffer_size() const noexcept {
			return sso_inactive() ? t_non_sso.buffer_size : get_sso_capacity();
		}
		
		//! Returns an std::string with the UTF-8 BOM prepended
		std::basic_string<data_type> cpp_str_bom() const noexcept ;
		
		//! Allocates size_type-aligned storage (make sure, total_buffer_size is a multiple of sizeof(size_type)!)
		inline data_type*		allocate( size_type total_buffer_size ) const noexcept {
			using appropriate_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<size_type>;
			appropriate_allocator	casted_allocator = (const Allocator&)*this;
			return reinterpret_cast<data_type*>(
				std::allocator_traits<appropriate_allocator>::allocate(
					casted_allocator
					, total_buffer_size / sizeof(size_type) * sizeof(data_type)
				)
			);
		}
		
		//! Allocates size_type-aligned storage (make sure, buffer_size is a multiple of sizeof(size_type)!)
		inline void			deallocate( data_type* buffer , size_type buffer_size ) const noexcept {
			using appropriate_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<size_type>;
			appropriate_allocator	casted_allocator = (const Allocator&)*this;
			std::allocator_traits<appropriate_allocator>::deallocate(
				casted_allocator
				, reinterpret_cast<size_type*>( buffer )
				, basic_string::determine_total_buffer_size( buffer_size ) / sizeof(size_type) * sizeof(data_type)
			);
		}
		
		//! Constructs an basic_string from a character literal
		basic_string( const data_type* str , size_type pos , size_type count , size_type data_left , const allocator_type& alloc , tiny_utf8_detail::read_codepoints_tag ) noexcept(TINY_UTF8_NOEXCEPT) ;
		basic_string( const data_type* str , size_type count , const allocator_type& alloc , tiny_utf8_detail::read_bytes_tag ) noexcept(TINY_UTF8_NOEXCEPT) ;
		
	public:
		
		/**
		 * Default Ctor
		 * 
		 * @note Creates an Instance of type basic_string that is empty
		 */
		basic_string()
			noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_default_constructible<Allocator>())
			: Allocator()
			, t_sso()
		{}
		/**
		 * Ctor taking an alloc
		 * 
		 * @note Creates an Instance of type basic_string that is empty
		 */
		explicit basic_string( const allocator_type& alloc )
			noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_copy_constructible<Allocator>())
			: Allocator( alloc )
			, t_sso()
		{}
		/**
		 * Constructor taking an utf8 sequence and the maximum length to read from it (in number of codepoints)
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 sequence to fill the basic_string with
		 * @param	pos		(Optional) The codepoint position of the first codepoint to read
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<typename T>
		inline basic_string( T&& str , const allocator_type& alloc = allocator_type() , enable_if_ptr<T, data_type>* = {} ) 
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , 0 , basic_string::npos , basic_string::npos , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		/**
		 * Constructor taking an utf8 sequence and the maximum length to read from it (in number of codepoints)
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 sequence to fill the basic_string with
		 * @param	len		The maximum number of codepoints to read from the sequence
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		inline basic_string( const data_type* str , size_type len , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , 0 , len , basic_string::npos , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		/**
		 * Constructor taking an utf8 sequence, a codepoint position to start reading from the sequence and the maximum length to read from it (in number of codepoints)
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 sequence to fill the basic_string with
		 * @param	pos		The codepoint position of the first codepoint to read
		 * @param	len		The maximum number of codepoints to read from the sequence
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		inline basic_string( const data_type* str , size_type pos , size_type len , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , pos , len , basic_string::npos , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		/**
		 * Constructor taking an utf8 char literal
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 literal to fill the basic_string with
		 * @param	pos		(Optional) The position of the first codepoint to read (bounds-checked)
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<size_type LITLEN>
		inline basic_string( const data_type (&str)[LITLEN] , const allocator_type& alloc = allocator_type() , enable_if_small_string<LITLEN> = {} )
			noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_copy_constructible<Allocator>())
			: Allocator( alloc )
		{
			std::memcpy( t_sso.data , str , LITLEN );
			if( str[LITLEN-1] ){
				t_sso.data[LITLEN] = '\0';
				set_sso_data_len( LITLEN );
			}
			else
				set_sso_data_len( LITLEN - 1 );
		}
		template<size_type LITLEN>
		inline basic_string( const data_type (&str)[LITLEN] , const allocator_type& alloc = allocator_type() , enable_if_not_small_string<LITLEN> = {} )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , LITLEN - ( str[LITLEN-1] ? 0 : 1 ) , alloc , tiny_utf8_detail::read_bytes_tag() )
		{}
		/**
		 * Constructor taking an utf8 char literal and the maximum number of codepoints to read
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 literal to fill the basic_string with
		 * @param	len		The maximum number of codepoints to read from the sequence
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<size_type LITLEN>
		inline basic_string( const data_type (&str)[LITLEN] , size_type len , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , 0 , len , LITLEN - ( str[LITLEN-1] ? 0 : 1 ) , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		/**
		 * Constructor taking an utf8 char literal, a codepoint position to start reading from the sequence and the maximum number of codepoints to read
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 literal to fill the basic_string with
		 * @param	pos		The codepoint position of the first codepoint to read (bounds-checked)
		 * @param	len		The maximum number of codepoints to read from the sequence
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<size_type LITLEN>
		inline basic_string( const data_type (&str)[LITLEN] , size_type pos , size_type len , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , pos , len , LITLEN - ( str[LITLEN-1] ? 0 : 1 ) , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		/**
		 * Constructor taking an std::string
		 * 
		 * @note	Creates an Instance of type basic_string copying the string data from the supplied std::basic_string
		 * @param	str		The string object from which the data will be copied (interpreted as UTF-8)
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<typename C, typename A>
		inline basic_string( std::basic_string<data_type, C, A> str , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str.data() , str.size() , alloc , tiny_utf8_detail::read_bytes_tag() )
		{}
		/**
		 * Constructor taking an std::string
		 * 
		 * @note	Creates an Instance of type basic_string copying the string data from the supplied std::basic_string
		 * @param	str		The string object from which the data will be copied (interpreted as UTF-8)
		 * @param	len		The maximum number of codepoints to read from 'str'
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<typename C, typename A>
		inline basic_string( std::basic_string<data_type, C, A> str , size_type len , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str.data() , 0 , len , str.size() , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		template<typename C, typename A>
		inline basic_string( std::basic_string<data_type, C, A> str , size_type pos , size_type len , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str.data() , pos , len , str.size() , alloc , tiny_utf8_detail::read_codepoints_tag() )
		{}
		/**
		 * Constructor that fills the string with a certain amount of codepoints
		 * 
		 * @note	Creates an Instance of type basic_string that gets filled with 'n' codepoints
		 * @param	n		The number of codepoints generated
		 * @param	cp		The codepoint that the whole buffer will be set to
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		basic_string( size_type n , value_type cp , const allocator_type& alloc = allocator_type() ) noexcept(TINY_UTF8_NOEXCEPT) ;
		/**
		 * Constructor that fills the string with a certain amount of characters
		 * 
		 * @note	Creates an Instance of type basic_string that gets filled with 'n' characters
		 * @param	n		The number of characters generated
		 * @param	cp		The characters that the whole buffer will be set to
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		basic_string( size_type n , data_type cp , const allocator_type& alloc = allocator_type() ) noexcept(TINY_UTF8_NOEXCEPT) ;
		/**
		 * Constructs the string with a portion of the supplied string
		 * 
		 * @param	str		The string that the constructed string shall be a substring of
		 * @param	pos		The codepoint position indicating the start of the string to be used for construction
		 * @param	count	The number of codepoints to be taken from 'str'
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		basic_string( const basic_string& str , size_type pos , size_type count = basic_string::npos , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str.substr( pos , count ) , alloc )
		{}
		/**
		 * Constructs the string from the range of codepoints supplied. The resulting string will equal [first,last)
		 * 
		 * @note	The Range is expected to contain codepoints (rather than code units, i.e. bytes)
		 * @param	first	The start of the range to construct from
		 * @param	last	The end of the range
		 * @param	alloc	(Optional) The allocator instance to use
		 */
		template<typename InputIt>
		basic_string( InputIt first , InputIt last , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: Allocator( alloc ) 
			, t_sso()
		{
			while( first != last ) push_back( *first++ );
		}
		/**
		 * Copy Constructor that copies the supplied basic_string to construct the string
		 * 
		 * @note	Creates an Instance of type basic_string that has the exact same data and allocator as 'str'
		 * @param	str		The basic_string to copy from
		 */
		basic_string( const basic_string& str )
			noexcept(TINY_UTF8_NOEXCEPT)
			: Allocator( (const allocator_type&)str )
		{
			std::memcpy( (void*)&this->t_sso , (void*)&str.t_sso , sizeof(SSO) ); // Copy data
			
			// Create a new buffer, if sso is not active
			if( str.sso_inactive() ){
				size_type total_buffer_size = basic_string::determine_total_buffer_size( t_non_sso.buffer_size );
				t_non_sso.data = this->allocate( total_buffer_size );
				std::memcpy( t_non_sso.data , str.t_non_sso.data , total_buffer_size );
			}
		}
		/**
		 * Copy Constructor that copies the supplied basic_string to construct the string
		 * 
		 * @note	Creates an Instance of type basic_string that has the exact same data as 'str'
		 * @param	str		The basic_string to copy from
		 * @param	alloc	The new allocator instance to use
		 */
		basic_string( const basic_string& str , const allocator_type& alloc )
			noexcept(TINY_UTF8_NOEXCEPT)
			: Allocator( alloc )
		{
			std::memcpy( (void*)&this->t_sso , (void*)&str.t_sso , sizeof(SSO) ); // Copy data
			
			// Create a new buffer, if sso is not active
			if( str.sso_inactive() ){
				size_type total_buffer_size = basic_string::determine_total_buffer_size( t_non_sso.buffer_size );
				t_non_sso.data = this->allocate( total_buffer_size );
				std::memcpy( t_non_sso.data , str.t_non_sso.data , total_buffer_size );
			}
		}
		/**
		 * Constructor taking a wide codepoint literal that will be copied to construct this basic_string
		 * 
		 * @note	Creates an Instance of type basic_string that holds the given codepoints
		 *			The data itself will be first converted to UTF-8
		 * @param	str		The codepoint sequence to fill the basic_string with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		basic_string( const value_type* str , size_type len , const allocator_type& alloc = allocator_type() ) noexcept(TINY_UTF8_NOEXCEPT) ;
		template<typename T>
		basic_string( T&& str , const allocator_type& alloc = allocator_type() , enable_if_ptr<T, value_type>* = {} )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , basic_string::npos , alloc )
		{}
		template<size_type LITLEN>
		inline basic_string( const value_type (&str)[LITLEN] , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( str , LITLEN - ( str[LITLEN-1] ? 0 : 1 ) , alloc )
		{}
		/**
		 * Constructor taking an initializer list of codepoints.
		 * 
		 * @note	The initializer list is expected to contain codepoints (rather than code units, i.e. bytes)
		 * @param	ilist	The initializer list with the contents to be applied to this string
		 */
		inline basic_string( std::initializer_list<value_type> ilist , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: basic_string( ilist.begin() , ilist.end() , alloc )
		{}
		/**
		 * Constructor that fills the string with the supplied codepoint
		 * 
		 * @note	Creates an Instance of type basic_string that gets filled with 'n' codepoints
		 * @param	cp		The codepoint written at the beginning of the buffer
		 */
		explicit inline basic_string( value_type cp , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: Allocator( alloc )
		{
			t_sso.data[ cp = encode_utf8( cp , t_sso.data ) ] = '\0';
			set_sso_data_len( cp );
		}
		/**
		 * Constructor that fills the string with the supplied character
		 * 
		 * @note	Creates an Instance of type basic_string that gets filled with 'n' codepoints
		 * @param	ch		The codepoint written at the beginning of the buffer
		 */
		explicit inline basic_string( data_type ch , const allocator_type& alloc = allocator_type() )
			noexcept(TINY_UTF8_NOEXCEPT)
			: Allocator( alloc )
			, t_sso( ch )
		{}
		/**
		 * Move Constructor that moves the supplied basic_string content into the new basic_string
		 * 
		 * @note	Creates an Instance of type basic_string that takes all data as well as the allocator from 'str'
		 * 			The supplied basic_string is invalid afterwards and may not be used anymore
		 * @param	str		The basic_string to move from
		 */
		inline basic_string( basic_string&& str )
			noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_move_constructible<Allocator>())
			: Allocator( (allocator_type&&)str )
		{
			std::memcpy( (void*)&this->t_sso , (void*)&str.t_sso , sizeof(SSO) ); // Copy data
			str.set_sso_data_len( 0u ); // Reset old string and enable its SSO-mode (which makes it not care about the buffer anymore)
		}
		/**
		 * Move Constructor that moves the supplied basic_string content into the new basic_string
		 * 
		 * @note	Creates an Instance of type basic_string that takes all data from 'str'
		 * 			The supplied basic_string is invalid afterwards and may not be used anymore
		 * @param	str		The basic_string to move from
		 */
		inline basic_string( basic_string&& str , const allocator_type& alloc )
			noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_copy_constructible<Allocator>())
			: Allocator( alloc )
		{
			std::memcpy( (void*)&this->t_sso , (void*)&str.t_sso , sizeof(SSO) ); // Copy data
			str.set_sso_data_len( 0u ); // Reset old string and enable its SSO-mode (which makes it not care about the buffer anymore)
		}
		
		
		/**
		 * Destructor
		 * 
		 * @note	Destructs a basic_string at the end of its lifetime releasing all held memory
		 */
		inline ~basic_string() noexcept { clear(); }
		
		
		/**
		 * Copy Assignment operator that sets the basic_string to a copy of the supplied one
		 * 
		 * @note	Assigns a copy of 'str' to this basic_string deleting all data that previously was in there
		 * @param	str		The basic_string to copy from
		 * @return	A reference to the string now holding the data (*this)
		 */
		basic_string& operator=( const basic_string& str ) noexcept(TINY_UTF8_NOEXCEPT) ;
		/**
		 * Move Assignment operator that moves all data out of the supplied and into this basic_string
		 * 
		 * @note	Moves all data from 'str' into this basic_string deleting all data that previously was in there
		 *			The supplied basic_string is invalid afterwards and may not be used anymore
		 * @param	str		The basic_string to move from
		 * @return	A reference to the string now holding the data (*this)
		 */
		inline basic_string& operator=( basic_string&& str ) noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_move_assignable<Allocator>()) {
			if( &str != this ){
				clear(); // Reset old data
				(allocator_type&)*this = (allocator_type&&)str; // Move allocator
				std::memcpy( (void*)&this->t_sso , (void*)&str.t_sso , sizeof(SSO) ); // Copy data
				str.set_sso_data_len(0); // Reset old string and enable its SSO-mode (which makes it not care about the buffer anymore)
			}
			return *this;
		}
		
		
		/**
		 * Clears the content of this basic_string
		 * 
		 * @note	Resets the data to an empty string ("")
		 */
		inline void clear() noexcept {
			if( sso_inactive() )
				this->deallocate( t_non_sso.data , t_non_sso.buffer_size );
			set_sso_data_len( 0 );
			t_sso.data[0] = 0;
		}
		
		
		/**
		 * Returns a copy of the stored allocator
		 *
		 * @return	A copy of the stored allocator instance
		 */
		allocator_type get_allocator() const noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_copy_constructible<Allocator>()) { return (const allocator_type&)*this; }
		
		
		/**
		 * Requests the removal of unused capacity.
		 */
		void shrink_to_fit() noexcept(TINY_UTF8_NOEXCEPT) ;
		
		
		/**
		 * Swaps the contents of this basic_string with the supplied one
		 * 
		 * @note	Swaps all data with the supplied basic_string
		 * @param	str		The basic_string to swap contents with
		 */
		inline void swap( basic_string& str ) noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_move_assignable<Allocator>()) {
			if( &str != this ){
				data_type tmp[sizeof(SSO)];
				std::memcpy( &tmp , (void*)&str.t_sso , sizeof(SSO) );
				std::memcpy( (void*)&str , (void*)&this->t_sso , sizeof(SSO) );
				std::memcpy( (void*)&this->t_sso , &tmp , sizeof(SSO) );
				std::swap( (allocator_type&)*this , (allocator_type&)str ); // Swap Allocators
			}
		}
		
		
		/**
		 * Returns the current capacity of this string. That is, the number of codepoints it can hold without reallocation
		 * 
		 * @return	The number of bytes currently allocated
		 */
		inline size_type capacity() const noexcept {
			return sso_inactive() ? get_non_sso_capacity() : get_sso_capacity();
		}
		
		
		/**
		 * Returns the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	The codepoint at position 'n'
		 */
		inline value_type at( size_type n ) const noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_at( get_num_bytes_from_start( n ) );
		}
		inline value_type at( size_type n , std::nothrow_t ) const noexcept {
			return raw_at( get_num_bytes_from_start( n ) , std::nothrow );
		}
		inline checked_reference at( size_type n ) noexcept(TINY_UTF8_NOEXCEPT) { return { n , this }; }
		inline reference at( size_type n , std::nothrow_t ) noexcept { return { n , this }; }
		/**
		 * Returns the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast.
		 * @param	byte_index	The byte position of the codepoint to receive
		 * @return	The codepoint at the supplied position
		 */
		inline raw_checked_reference raw_at( size_type byte_index ) noexcept(TINY_UTF8_NOEXCEPT) { return { byte_index , this }; }
		inline raw_reference raw_at( size_type byte_index , std::nothrow_t ) noexcept { return { byte_index , this }; }
		value_type raw_at( size_type byte_index ) const noexcept(TINY_UTF8_NOEXCEPT) {
			size_type size = this->size();
			if( byte_index >= size ){
				TINY_UTF8_THROW( "tiny_utf8::basic_string::(raw_)at" , byte_index >= size );
				return 0;
			}
			const data_type* pos = get_buffer() + byte_index;
			return *pos ? decode_utf8( pos , basic_string::get_codepoint_bytes( *pos , size - byte_index ) ) : 0;
		}
		value_type raw_at( size_type byte_index , std::nothrow_t ) const noexcept {
			const data_type* pos = get_buffer() + byte_index;
			return *pos ? decode_utf8( pos , basic_string::get_codepoint_bytes( *pos , size() - byte_index ) ) : 0;
		}
		
		
		/**
		 * Returns an iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified codepoint index
		 */
		inline iterator get( size_type n ) noexcept { return { (difference_type)n , this }; }
		inline const_iterator get( size_type n ) const noexcept { return { (difference_type)n , this }; }
		/**
		 * Returns an iterator pointing to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified byte position
		 */
		inline raw_iterator raw_get( size_type n ) noexcept { return { (difference_type)n , this }; }
		inline raw_const_iterator raw_get( size_type n ) const noexcept { return { (difference_type)n , this }; }
		
		
		/**
		 * Returns a reverse iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the reverse iterator to
		 * @return	A reverse iterator pointing to the specified codepoint index
		 */
		inline reverse_iterator rget( size_type n ) noexcept { return { (difference_type)n , this }; }
		inline const_reverse_iterator rget( size_type n ) const noexcept { return { (difference_type)n , this }; }
		/**
		 * Returns a reverse iterator pointing to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to get the reverse iterator to
		 * @return	A reverse iterator pointing to the specified byte position
		 */
		inline raw_reverse_iterator raw_rget( size_type n ) noexcept { return { (difference_type)n , this }; }
		inline raw_const_reverse_iterator raw_rget( size_type n ) const noexcept { return { (difference_type)n , this }; }
		
		
		/**
		 * Returns a reference to the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	A reference wrapper to the codepoint at position 'n'
		 */
		inline reference operator[]( size_type n ) noexcept { return { n , this }; }
		inline value_type operator[]( size_type n ) const noexcept { return at( n , std::nothrow ); }
		/**
		 * Returns a reference to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to receive
		 * @return	A reference wrapper to the codepoint at byte position 'n'
		 */
		inline raw_reference operator()( size_type n ) noexcept { return { n , this }; }
		inline value_type operator()( size_type n ) const noexcept { return raw_at( n , std::nothrow ); }
		
		
		/**
		 * Get the raw data contained in this basic_string
		 * 
		 * @note	Returns the UTF-8 formatted content of this basic_string
		 * @return	UTF-8 formatted data, trailled by a '\0'
		 */
		inline const data_type* c_str() const noexcept { return get_buffer(); }
		inline const data_type* data() const noexcept { return get_buffer(); }
		inline data_type* data() noexcept { return get_buffer(); }
		
		
		/**
		 * Get the number of codepoints in this basic_string
		 * 
		 * @note	Returns the number of codepoints that are taken care of
		 *			For the number of bytes, @see size()
		 * @return	Number of codepoints (not bytes!)
		 */
		inline size_type length() const noexcept { return sso_inactive() ? get_non_sso_string_len() : get_num_codepoints( 0 , get_sso_data_len() ); }
		
		
		/**
		 * Get the number of bytes that are used by this basic_string
		 * 
		 * @note	Returns the number of bytes required to hold the contained wide string,
		 *			That is, without counting the trailling '\0'
		 * @return	Number of bytes (not codepoints!)
		 */
		inline size_type size() const noexcept { return sso_inactive() ? t_non_sso.data_len : get_sso_data_len(); }
		
		
		/**
		 * Check, whether this basic_string is empty
		 * 
		 * @note	Returns True, if this basic_string is empty, that also is comparing true with ""
		 * @return	True, if this basic_string is empty, false if its length is >0
		 */
		inline bool empty() const noexcept { return sso_inactive() ? !t_non_sso.data_len : t_sso.data_len == (get_sso_capacity() << 1); }
		
		
		
		
		
		/**
		 * Get an iterator to the beginning of the basic_string
		 * 
		 * @return	An iterator class pointing to the beginning of this basic_string
		 */
		inline iterator begin() noexcept { return { 0 , this }; }
		inline const_iterator begin() const noexcept { return { 0 , this }; }
		inline raw_iterator raw_begin() noexcept { return { 0 , this }; }
		inline raw_const_iterator raw_begin() const noexcept { return { 0 , this }; }
		/**
		 * Get an iterator to the end of the basic_string
		 * 
		 * @return	An iterator class pointing to the end of this basic_string, that is pointing behind the last codepoint
		 */
		inline iterator end() noexcept { return { (difference_type)length() , this }; }
		inline const_iterator end() const noexcept { return { (difference_type)length() , this }; }
		inline raw_iterator raw_end() noexcept { return { (difference_type)size() , this }; }
		inline raw_const_iterator raw_end() const noexcept { return { (difference_type)size() , this }; }
		
		/**
		 * Get a reverse iterator to the end of this basic_string
		 * 
		 * @return	A reverse iterator class pointing to the end of this basic_string,
		 *			that is exactly to the last codepoint
		 */
		inline reverse_iterator rbegin() noexcept { return { (difference_type)length() - 1 , this }; }
		inline const_reverse_iterator rbegin() const noexcept { return { (difference_type)length() - 1 , this }; }
		inline raw_reverse_iterator raw_rbegin() noexcept { return { (difference_type)raw_back_index() , this }; }
		inline raw_const_reverse_iterator raw_rbegin() const noexcept { return { (difference_type)raw_back_index() , this }; }
		/**
		 * Get a reverse iterator to the beginning of this basic_string
		 * 
		 * @return	A reverse iterator class pointing to the end of this basic_string,
		 *			that is pointing before the first codepoint
		 */
		inline reverse_iterator rend() noexcept { return { -1 , this }; }
		inline const_reverse_iterator rend() const noexcept { return { -1 , this }; }
		inline raw_reverse_iterator raw_rend() noexcept { return { -1 , this }; }
		inline raw_const_reverse_iterator raw_rend() const noexcept { return { -1 , this }; }
		
		
		/**
		 * Get a const iterator to the beginning of the basic_string
		 * 
		 * @return	A const iterator class pointing to the beginning of this basic_string,
		 * 			which cannot alter things inside this basic_string
		 */
		inline const_iterator cbegin() const noexcept { return { 0 , this }; }
		inline raw_const_iterator raw_cbegin() const noexcept { return { 0 , this }; }
		/**
		 * Get an iterator to the end of the basic_string
		 * 
		 * @return	A const iterator class, which cannot alter this basic_string, pointing to
		 *			the end of this basic_string, that is pointing behind the last codepoint
		 */
		inline const_iterator cend() const noexcept { return { (difference_type)length() , this }; }
		inline raw_const_iterator raw_cend() const noexcept { return { (difference_type)size() , this }; }
		
		
		/**
		 * Get a const reverse iterator to the end of this basic_string
		 * 
		 * @return	A const reverse iterator class, which cannot alter this basic_string, pointing to
		 *			the end of this basic_string, that is exactly to the last codepoint
		 */
		inline const_reverse_iterator crbegin() const noexcept { return { (difference_type)length() - 1 , this }; }
		inline raw_const_reverse_iterator raw_crbegin() const noexcept { return { (difference_type)raw_back_index() , this }; }
		/**
		 * Get a const reverse iterator to the beginning of this basic_string
		 * 
		 * @return	A const reverse iterator class, which cannot alter this basic_string, pointing to
		 *			the end of this basic_string, that is pointing before the first codepoint
		 */
		inline const_reverse_iterator crend() const noexcept { return { -1 , this }; }
		inline raw_const_reverse_iterator raw_crend() const noexcept { return { -1 , this }; }
		
		
		/**
		 * Returns a reference to the first codepoint in the basic_string
		 * 
		 * @return	A reference wrapper to the first codepoint in the basic_string
		 */
		inline raw_reference front() noexcept { return { 0 , this }; }
		inline value_type front() const noexcept { return raw_at( 0 , std::nothrow ); }
		/**
		 * Returns a reference to the last codepoint in the basic_string
		 * 
		 * @return	A reference wrapper to the last codepoint in the basic_string
		 */
		inline raw_reference back() noexcept { return { raw_back_index() , this }; }
		inline value_type back() const noexcept {
			size_type			my_size = size();
			const data_type*	buffer = get_buffer();
			width_type			bytes = get_num_bytes_of_utf8_char_before( buffer , my_size );
			return decode_utf8( buffer + my_size - bytes , bytes );
		}
		
		
		/**
		 * Replace a codepoint of this basic_string by a number of codepoints
		 * 
		 * @param	index	The codpoint index to be replaced
		 * @param	repl	The wide character that will be used to replace the codepoint
		 * @param	n		The number of codepoint that will be inserted
		 *					instead of the one residing at position 'index'
		 * @return	A reference to this basic_string, which now has the replaced part in it
		 */
		inline basic_string& replace( size_type index , value_type repl , size_type n = 1 ) noexcept(TINY_UTF8_NOEXCEPT) {
			return replace( index , 1 , repl , n );
		}
		/**
		 * Replace a number of codepoints of this basic_string by a number of other codepoints
		 * 
		 * @param	index	The codpoint index at which the replacement is being started
		 * @param	len		The number of codepoints that are being replaced
		 * @param	repl	The wide character that will be used to replace the codepoints
		 * @param	n		The number of codepoint that will replace the old ones
		 * @return	A reference to this basic_string, which now has the replaced part in it
		 */
		inline basic_string& replace( size_type index , size_type len , value_type repl , size_type n ) noexcept(TINY_UTF8_NOEXCEPT) {
			return replace( index , len , basic_string( n , repl ) );
		}
		inline basic_string& replace( size_type index , size_type len , value_type repl ) noexcept(TINY_UTF8_NOEXCEPT) {
			return replace( index , len , basic_string( repl ) );
		}
		/**
		 * Replace a range of codepoints by a number of codepoints
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be replaced
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be replaced
		 * @param	repl	The wide character that will be used to replace the codepoints in the range
		 * @param	n		(Optional) The number of codepoint that will replace the old ones
		 * @return	A reference to this basic_string, which now has the replaced part in it
		 */
		inline basic_string& replace( raw_iterator first , raw_iterator last , value_type repl , size_type n ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_replace( first.get_raw_index() , last.get_raw_index() - first.get_raw_index() , basic_string( n , repl ) );
		}
		inline basic_string& replace( raw_iterator first , raw_iterator last , value_type repl ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_replace( first.get_raw_index() , last.get_raw_index() - first.get_raw_index() , basic_string( repl ) );
		}
		inline basic_string& replace( raw_iterator first , iterator last , value_type repl , size_type n ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( first , (raw_iterator)last , repl , n ); }
		inline basic_string& replace( iterator first , raw_iterator last , value_type repl , size_type n ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( (raw_iterator)first , last , repl , n ); }
		inline basic_string& replace( iterator first , iterator last , value_type repl , size_type n ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( (raw_iterator)first , (raw_iterator)last , repl , n ); }
		inline basic_string& replace( raw_iterator first , iterator last , value_type repl ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( first , (raw_iterator)last , repl ); }
		inline basic_string& replace( iterator first , raw_iterator last , value_type repl ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( (raw_iterator)first , last , repl ); }
		inline basic_string& replace( iterator first , iterator last , value_type repl ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( (raw_iterator)first , (raw_iterator)last , repl ); }
		/**
		 * Replace a range of codepoints with the contents of the supplied basic_string
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be replaced
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be replaced
		 * @param	repl	The basic_string to replace all codepoints in the range
		 * @return	A reference to this basic_string, which now has the replaced part in it
		 */
		inline basic_string& replace( raw_iterator first , raw_iterator last , const basic_string& repl ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_replace( first.get_raw_index() , last.get_raw_index() - first.get_raw_index() , repl );
		}
		inline basic_string& replace( raw_iterator first , iterator last , const basic_string& repl ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( first , (raw_iterator)last , repl ); }
		inline basic_string& replace( iterator first , raw_iterator last , const basic_string& repl ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( (raw_iterator)first , last , repl ); }
		inline basic_string& replace( iterator first , iterator last , const basic_string& repl ) noexcept(TINY_UTF8_NOEXCEPT) { return replace( (raw_iterator)first , (raw_iterator)last , repl ); }
		/**
		 * Replace a number of codepoints of this basic_string with the contents of the supplied basic_string
		 * 
		 * @param	index	The codpoint index at which the replacement is being started
		 * @param	len		The number of codepoints that are being replaced
		 * @param	repl	The basic_string to replace all codepoints in the range
		 * @return	A reference to this basic_string, which now has the replaced part in it
		 */
		inline basic_string& replace( size_type index , size_type count , const basic_string& repl ) noexcept(TINY_UTF8_NOEXCEPT) {
			size_type start_byte = get_num_bytes_from_start( index );
			return raw_replace(
				start_byte
				, count == basic_string::npos ? basic_string::npos : get_num_bytes( start_byte , count )
				, repl
			);
		}
		/**
		 * Replace a number of bytes of this basic_string with the contents of the supplied basic_string
		 * 
		 * @note	As this function is raw, that is not having to compute byte indices,
		 *			it is much faster than the codepoint-base replace function
		 * @param	start_byte	The byte position at which the replacement is being started
		 * @param	byte_count	The number of bytes that are being replaced
		 * @param	repl		The basic_string to replace all bytes inside the range
		 * @return	A reference to this basic_string, which now has the replaced part in it
		 */
		basic_string& raw_replace( size_type start_byte , size_type byte_count , const basic_string& repl ) noexcept(TINY_UTF8_NOEXCEPT) ;
		
		
		/**
		 * Prepend the supplied basic_string to this basic_string
		 * 
		 * @param	prependix	The basic_string to be prepended
		 * @return	A reference to this basic_string, which now has the supplied string prepended
		 */
		inline basic_string& prepend( const basic_string& prependix ) noexcept(TINY_UTF8_NOEXCEPT) { return raw_insert( 0 , prependix ); }
		
		/**
		 * Appends the supplied basic_string to the end of this basic_string
		 * 
		 * @param	appendix	The basic_string to be appended
		 * @return	A reference to this basic_string, which now has the supplied string appended
		 */
		basic_string& append( const basic_string& appendix ) noexcept(TINY_UTF8_NOEXCEPT) ;
		inline basic_string& operator+=( const basic_string& appendix ) noexcept(TINY_UTF8_NOEXCEPT) { return append( appendix ); }
		
		
		/**
		 * Appends the supplied codepoint to the end of this basic_string
		 * 
		 * @param	cp	The codepoint to be appended
		 * @return	A reference to this basic_string, which now has the supplied codepoint appended
		 */
		inline basic_string& push_back( value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) { return append( basic_string( cp ) ); }
		inline basic_string& operator+=( value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) { return append( basic_string( cp ) ); }
		
		
		/**
		 * Appends/prepends the supplied basic_string resp. data_type/value_type (value/literal) to a copy of this basic_string
		 * 
		 * @param	lhs		The basic_string to be added 
		 * @return	The newly created basic_string, consisting of the concatenated parts.
		 */
		// with basic_string in both operands
		inline basic_string operator+( basic_string summand ) const &  noexcept(TINY_UTF8_NOEXCEPT) { summand.prepend( *this ); return summand; }
		inline basic_string operator+( const basic_string& summand ) && noexcept(TINY_UTF8_NOEXCEPT) { append( summand ); return static_cast<basic_string&&>( *this ); }
		
		// with basic_string as first operand
		friend inline basic_string operator+( basic_string lhs , data_type rhs ) noexcept(TINY_UTF8_NOEXCEPT) { lhs.push_back( rhs ); return lhs; }
		friend inline basic_string operator+( basic_string lhs , value_type rhs ) noexcept(TINY_UTF8_NOEXCEPT) { lhs.push_back( rhs ); return lhs; }
		template<typename T> friend inline enable_if_ptr<T, data_type, basic_string> operator+( basic_string lhs , T&& rhs ) noexcept(TINY_UTF8_NOEXCEPT) { lhs.append( basic_string( rhs ) ); return lhs; }
		template<typename T> friend inline enable_if_ptr<T, value_type, basic_string> operator+( basic_string lhs , T&& rhs ) noexcept(TINY_UTF8_NOEXCEPT) { lhs.append( basic_string( rhs ) ); return lhs; }
		template<size_type LITLEN> friend inline basic_string operator+( basic_string lhs , const data_type (&rhs)[LITLEN] ) noexcept(TINY_UTF8_NOEXCEPT) { lhs.append( basic_string( rhs ) ); return lhs; }
		template<size_type LITLEN> friend inline basic_string operator+( basic_string lhs , const value_type (&rhs)[LITLEN] ) noexcept(TINY_UTF8_NOEXCEPT) { lhs.append( basic_string( rhs ) ); return lhs; }
		
		// With basic_string as second operand
		friend inline basic_string operator+( data_type lhs , basic_string rhs ) noexcept(TINY_UTF8_NOEXCEPT) { rhs.raw_insert( 0 , lhs ); return rhs; }
		friend inline basic_string operator+( value_type lhs , basic_string rhs ) noexcept(TINY_UTF8_NOEXCEPT) { rhs.raw_insert( 0 , lhs ); return rhs; }
		template<typename T> friend inline enable_if_ptr<T, data_type, basic_string> operator+( T&& lhs , basic_string rhs ) noexcept(TINY_UTF8_NOEXCEPT) { rhs.prepend( basic_string( lhs ) ); return rhs; }
		template<typename T> friend inline enable_if_ptr<T, value_type, basic_string> operator+( T&& lhs , basic_string rhs ) noexcept(TINY_UTF8_NOEXCEPT) { rhs.prepend( basic_string( lhs ) ); return rhs; }
		template<size_type LITLEN> friend inline basic_string operator+( const data_type (&lhs)[LITLEN] , basic_string rhs ) noexcept(TINY_UTF8_NOEXCEPT) { rhs.prepend( basic_string( lhs ) ); return rhs; }
		template<size_type LITLEN> friend inline basic_string operator+( const value_type (&lhs)[LITLEN] , basic_string rhs ) noexcept(TINY_UTF8_NOEXCEPT) { rhs.prepend( basic_string( lhs ) ); return rhs; }
		
		
		/**
		 * Sets the contents of this string to 'count' times 'cp'
		 * 
		 * @param	count	The number of times the supplied codepoint is to be repeated
		 * @param	cp		The codepoint to repeat 'count' times
		 * @return	A reference to this basic_string, updated to the new string
		 */
		inline basic_string& assign( size_type count , value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( count , cp );
		}
		/**
		 * Sets the contents of this string to a copy of the supplied basic_string
		 * 
		 * @param	str	The basic_string this string shall be made a copy of
		 * @return	A reference to this basic_string, updated to the new string
		 */
		inline basic_string& assign( const basic_string& str ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = str;
		}
		/**
		 * Sets the contents of this string to a substring of the supplied basic_string
		 * 
		 * @param	str	The basic_string this string shall be constructed a substring of
		 * @param	pos	The codepoint position indicating the start of the string to be used for construction
		 * @param	cp	The number of codepoints to be taken from 'str'
		 * @return	A reference to this basic_string, updated to the new string
		 */
		inline basic_string& assign( const basic_string& str , size_type pos , size_type count ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str , pos , count );
		}
		/**
		 * Moves the contents out of the supplied string into this one
		 * 
		 * @param	str	The basic_string this string shall move from
		 * @return	A reference to this basic_string, updated to the new string
		 */
		inline basic_string& assign( basic_string&& str ) noexcept(TINY_UTF8_NOEXCEPT && std::is_nothrow_move_assignable<Allocator>()) {
			return *this = std::move(str);
		}
		/**
		 * Assigns an utf8 sequence and the maximum length to read from it (in number of codepoints) to this string
		 * 
		 * @param	str		The UTF-8 sequence to fill the this basic_string with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		template<typename T>
		inline basic_string& assign( T&& str , enable_if_ptr<T, data_type>* = {} ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str );
		}
		inline basic_string& assign( const data_type* str , size_type len ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str , len );
		}
		/**
		 * Assigns an utf8 char literal to this string (with possibly embedded '\0's)
		 * 
		 * @param	str		The UTF-8 literal to fill the basic_string with
		 */
		template<size_type LITLEN>
		inline basic_string& assign( const data_type (&str)[LITLEN] ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str );
		}
		/**
		 * Assigns an utf-32 sequence and the maximum length to read from it (in number of codepoints) to this string
		 * 
		 * @param	str		The UTF-8 sequence to fill the this basic_string with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		template<typename T>
		inline basic_string& assign( T&& str , enable_if_ptr<T, value_type>* = {} ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str );
		}
		inline basic_string& assign( const value_type* str , size_type len ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str , len );
		}
		/**
		 * Assigns an utf-32 char literal to this string (with possibly embedded '\0's)
		 * 
		 * @param	str		The UTF-32 literal to fill the basic_string with
		 */
		template<size_type LITLEN>
		inline basic_string& assign( const value_type (&str)[LITLEN] ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( str );
		}
		/**
		 * Assigns the range of codepoints supplied to this string. The resulting string will equal [first,last)
		 * 
		 * @note	The Range is expected to contain codepoints (rather than code units, i.e. bytes)
		 * @param	first	The start of the range to read from
		 * @param	last	The end of the range
		 */
		template<typename InputIt>
		inline basic_string& assign( InputIt first , InputIt last ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( first , last );
		}
		/**
		 * Assigns the supplied initializer list of codepoints to this string.
		 * 
		 * @note	The initializer list is expected to contain codepoints (rather than code units, i.e. bytes)
		 * @param	ilist	The initializer list with the contents to be applied to this string
		 */
		inline basic_string& assign( std::initializer_list<value_type> ilist ) noexcept(TINY_UTF8_NOEXCEPT) {
			return *this = basic_string( std::move(ilist) );
		}
		
		
		/**
		 * Inserts a given codepoint into this basic_string at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	cp		The codepoint to be inserted
		 * @return	A reference to this basic_string, with the supplied codepoint inserted
		 */
		inline basic_string& insert( size_type pos , value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_insert( get_num_bytes_from_start( pos ) , cp );
		}
		/**
		 * Inserts a given basic_string into this basic_string at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	str		The basic_string to be inserted
		 * @return	A reference to this basic_string, with the supplied basic_string inserted
		 */
		inline basic_string& insert( size_type pos , const basic_string& str ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_insert( get_num_bytes_from_start( pos ) , str );
		}
		/**
		 * Inserts a given codepoint into this basic_string at the supplied iterator position
		 * 
		 * @param	it	The iterator position to insert at
		 * @param	cp	The codepoint to be inserted
		 * @return	A reference to this basic_string, with the supplied codepoint inserted
		 */
		inline basic_string& insert( raw_iterator it , value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_insert( it.get_raw_index() , basic_string( cp ) );
		}
		/**
		 * Inserts a given basic_string into this basic_string at the supplied iterator position
		 * 
		 * @param	it	The iterator position to insert at
		 * @param	cp	The basic_string to be inserted
		 * @return	A reference to this basic_string, with the supplied codepoint inserted
		 */
		inline basic_string& insert( raw_iterator it , const basic_string& str ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_insert( it.get_raw_index() , str );
		}
		/**
		 * Inserts a given basic_string into this basic_string at the supplied byte position
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that insert()
		 * @param	pos		The byte position index to insert at
		 * @param	str		The basic_string to be inserted
		 * @return	A reference to this basic_string, with the supplied basic_string inserted
		 */
		basic_string& raw_insert( size_type pos , const basic_string& str ) noexcept(TINY_UTF8_NOEXCEPT) ;
		/**
		 * Inserts a given codepoint into this basic_string at the supplied byte position
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that insert()
		 * @param	pos		The byte position index to insert at
		 * @param	cp		The codepoint to be inserted
		 * @return	A reference to this basic_string, with the supplied basic_string inserted
		 */
		inline basic_string& raw_insert( size_type pos , value_type cp ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_insert( pos , basic_string( cp ) );
		}
		
		
		//! Removes the last codepoint in the basic_string
		inline basic_string& pop_back() noexcept(TINY_UTF8_NOEXCEPT) {
			size_type pos = raw_back_index();
			return raw_erase( pos , get_index_bytes( pos ) );
		}
		
		
		/**
		 * Erases the codepoint at the supplied iterator position
		 * 
		 * @param	pos		The iterator pointing to the position being erased
		 * @return	A reference to this basic_string, which now has the codepoint erased
		 */
		inline basic_string& erase( raw_iterator pos ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_erase( pos.get_raw_index() , get_index_bytes( pos.get_raw_index() ) );
		}
		/**
		 * Erases the codepoints inside the supplied range
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be erased
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be erased
		 * @return	A reference to this basic_string, which now has the codepoints erased
		 */
		inline basic_string& erase( raw_iterator first , raw_iterator last ) noexcept(TINY_UTF8_NOEXCEPT) {
			return raw_erase( first.get_raw_index() , last.get_raw_index() - first.get_raw_index() );
		}
		inline basic_string& erase( raw_iterator first , iterator last ) noexcept(TINY_UTF8_NOEXCEPT) { return erase( first , (raw_iterator)last ); }
		inline basic_string& erase( iterator first , raw_iterator last ) noexcept(TINY_UTF8_NOEXCEPT) { return erase( (raw_iterator)first , last ); }
		inline basic_string& erase( iterator first , iterator last ) noexcept(TINY_UTF8_NOEXCEPT) { return erase( (raw_iterator)first , (raw_iterator)last ); }
		/**
		 * Erases a portion of this string
		 * 
		 * @param	pos		The codepoint index to start eraseing from
		 * @param	len		The number of codepoints to be erased from this basic_string
		 * @return	A reference to this basic_string, with the supplied portion erased
		 */
		inline basic_string& erase( size_type pos , size_type len = 1 ) noexcept(TINY_UTF8_NOEXCEPT) {
			size_type start_byte = get_num_bytes_from_start( pos );
			return raw_erase( start_byte , get_num_bytes( start_byte , len ) );
		}
		/**
		 * Erases a byte range of this string
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that erase()
		 * @param	pos		The byte position index to start erasing from
		 * @param	len		The number of bytes to be erased from the basic_string
		 * @return	A reference to this basic_string, with the supplied bytes erased
		 */
		basic_string& raw_erase( size_type pos , size_type len ) noexcept(TINY_UTF8_NOEXCEPT) ;
		
		
		/**
		 * Returns a portion of the basic_string
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be included in the substring
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint in the substring
		 * @return	The basic_string holding the specified range
		 */
		inline basic_string substr( raw_iterator first , raw_iterator last ) const noexcept(TINY_UTF8_NOEXCEPT) {
			size_type byte_count = last.get_raw_index() - first.get_raw_index();
			return raw_substr( first.get_raw_index() , byte_count );
		}
		inline basic_string substr( raw_iterator first , iterator last ) noexcept(TINY_UTF8_NOEXCEPT) { return substr( first , (raw_iterator)last ); }
		inline basic_string substr( iterator first , raw_iterator last ) noexcept(TINY_UTF8_NOEXCEPT) { return substr( (raw_iterator)first , last ); }
		inline basic_string substr( iterator first , iterator last ) noexcept(TINY_UTF8_NOEXCEPT) { return substr( (raw_iterator)first , (raw_iterator)last ); }
		/**
		 * Returns a portion of the basic_string
		 * 
		 * @param	pos		The codepoint index that should mark the start of the substring
		 * @param	len		The number codepoints to be included within the substring
		 * @return	The basic_string holding the specified codepoints
		 */
		basic_string substr( size_type pos , size_type len = basic_string::npos ) const noexcept(TINY_UTF8_NOEXCEPT) {
			size_type byte_start = get_num_bytes_from_start( pos );
			if( len == basic_string::npos )
				return raw_substr( byte_start , basic_string::npos );
			size_type byte_count = get_num_bytes( byte_start , len );
			return raw_substr( byte_start , byte_count );
		}
		/**
		 * Returns a portion of the basic_string (indexed on byte-base)
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that substr()
		 * @param	start_byte		The byte position where the substring shall start
		 * @param	byte_count		The number of bytes that the substring shall have
		 * @return	The basic_string holding the specified bytes
		 */
		basic_string raw_substr( size_type start_byte , size_type byte_count ) const noexcept(TINY_UTF8_NOEXCEPT) ;
		
		
		/**
		 * Finds a specific codepoint inside the basic_string starting at the supplied codepoint index
		 * 
		 * @param	cp				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the codepoint was found or basic_string::npos
		 */
		size_type find( value_type cp , size_type start_codepoint = 0 ) const noexcept {
			if( sso_inactive() && start_codepoint >= length() ) // length() is only O(1), if sso is inactive
				return basic_string::npos;
			for( const_iterator it = get(start_codepoint) , end = cend() ; it != end ; ++it, ++start_codepoint )
				if( *it == cp )
					return start_codepoint;
			return basic_string::npos;
		}
		/**
		 * Finds a specific pattern within the basic_string starting at the supplied codepoint index
		 * 
		 * @param	cp				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the pattern was found or basic_string::npos
		 */
		size_type find( const basic_string& pattern , size_type start_codepoint = 0 ) const noexcept {
			if( sso_inactive() && start_codepoint >= length() ) // length() is only O(1), if sso is inactive
				return basic_string::npos;
			size_type actual_start = get_num_bytes_from_start( start_codepoint );
			const data_type* buffer = get_buffer();
			const data_type* result = std::strstr( buffer + actual_start , pattern.data() );
			if( !result )
				return basic_string::npos;
			return start_codepoint + get_num_codepoints( actual_start , result - ( buffer + actual_start ) );
		}
		/**
		 * Finds a specific pattern within the basic_string starting at the supplied codepoint index
		 * 
		 * @param	cp				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the pattern was found or basic_string::npos
		 */
		size_type find( const data_type* pattern , size_type start_codepoint = 0 ) const noexcept {
			if( sso_inactive() && start_codepoint >= length() ) // length() is only O(1), if sso is inactive
				return basic_string::npos;
			size_type actual_start = get_num_bytes_from_start( start_codepoint );
			const data_type* buffer = get_buffer();
			const data_type* result = std::strstr( buffer + actual_start , pattern );
			if( !result )
				return basic_string::npos;
			return start_codepoint + get_num_codepoints( actual_start , result - ( buffer + actual_start ) );
		}
		/**
		 * Finds a specific codepoint inside the basic_string starting at the supplied byte position
		 * 
		 * @param	cp			The codepoint to look for
		 * @param	start_byte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the codepoint was found or basic_string::npos
		 */
		size_type raw_find( value_type cp , size_type start_byte = 0 ) const noexcept {
			size_type my_size = size();
			if( start_byte >= my_size )
				return basic_string::npos;
			for( const_iterator it = raw_get(start_byte) , end = raw_get(my_size) ; it != end ; ++it )
				if( *it == cp )
					return it - begin();
			return basic_string::npos;
		}
		/**
		 * Finds a specific pattern within the basic_string starting at the supplied byte position
		 * 
		 * @param	needle		The pattern to look for
		 * @param	start_byte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the pattern was found or basic_string::npos
		 */
		size_type raw_find( const basic_string& pattern , size_type start_byte = 0 ) const noexcept {
			if( start_byte >= size() )
				return basic_string::npos;
			const data_type* buffer = get_buffer();
			const data_type* result = std::strstr( buffer + start_byte , pattern.data() );
			if( !result )
				return basic_string::npos;
			return result - buffer;
		}
		/**
		 * Finds a specific pattern within the basic_string starting at the supplied byte position
		 * 
		 * @param	needle		The pattern to look for
		 * @param	start_byte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the pattern was found or basic_string::npos
		 */
		size_type raw_find( const data_type* pattern , size_type start_byte = 0 ) const noexcept {
			if( start_byte >= size() )
				return basic_string::npos;
			const data_type* buffer = get_buffer();
			const data_type* result = std::strstr( buffer + start_byte , pattern );
			if( !result )
				return basic_string::npos;
			return result - buffer;
		}
		
		/**
		 * Finds the last occourence of a specific codepoint inside the
		 * basic_string starting backwards at the supplied codepoint index
		 * 
		 * @param	cp				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from (backwards)
		 * @return	The codepoint index where and if the codepoint was found or basic_string::npos
		 */
		size_type rfind( value_type cp , size_type start_codepoint = basic_string::npos ) const noexcept {
			const_reverse_iterator  end = rend(), it;
			size_type               string_len = length();
			if( start_codepoint >= string_len )
				it = crbegin(), start_codepoint = string_len - 1;
			else
				it = rget( start_codepoint );
			for( ; it != end ; ++it, --start_codepoint )
				if( *it == cp )
					return start_codepoint;
			return basic_string::npos;
		}
		/**
		 * Finds the last occourence of a specific codepoint inside the
		 * basic_string starting backwards at the supplied byte index
		 * 
		 * @param	cp				The codepoint to look for
		 * @param	start_codepoint	The byte index of the first codepoint to start looking from (backwards)
		 * @return	The codepoint index where and if the codepoint was found or basic_string::npos
		 */
		size_type raw_rfind( value_type cp , size_type start_byte = basic_string::npos ) const noexcept ;
		
		//! Find characters in string
		size_type find_first_of( const value_type* str , size_type start_codepoint = 0 ) const noexcept ;
		size_type raw_find_first_of( const value_type* str , size_type start_byte = 0 ) const noexcept ;
		size_type find_last_of( const value_type* str , size_type start_codepoint = basic_string::npos ) const noexcept ;
		size_type raw_find_last_of( const value_type* str , size_type start_byte = basic_string::npos ) const noexcept ;
		
		//! Find absence of characters in string
		size_type find_first_not_of( const value_type* str , size_type start_codepoint = 0 ) const noexcept ;
		size_type raw_find_first_not_of( const value_type* str , size_type start_byte = 0 ) const noexcept ;
		size_type find_last_not_of( const value_type* str , size_type start_codepoint = basic_string::npos ) const noexcept ;
		size_type raw_find_last_not_of( const value_type* str , size_type start_byte = basic_string::npos ) const noexcept ;
		
		
		/**
		 * Check, whether this string ends with the supplied character sequence
		 *
		 * @param	str		The string to compare the end of this string with
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		inline bool starts_with( const basic_string& str ) const noexcept {
			size_type my_size = size(), str_size = str.size();
			return my_size >= str_size && std::memcmp( data() , str.data() , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied character sequence
		 *
		 * @param	str		The string to compare the end of this string with
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		inline bool starts_with( const std::string& str ) const noexcept {
			size_type my_size = size(), str_size = str.size();
			return my_size >= str_size && std::memcmp( data() , str.data() , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied codepoint
		 *
		 * @param	str		The codepoint to compare the end of this string with
		 * @return	true, if this string ends with the codepoint 'cp', false otherwise.
		 */
		inline bool starts_with( value_type cp ) const noexcept {
			return !empty() && front() == cp;
		}
		/**
		 * Check, whether this string ends with the supplied UTF-8 sequence.
		 *
		 * @param	str		Null-terminated string literal, interpreted as UTF-8. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<typename T>
		bool starts_with( T str , enable_if_ptr<T, data_type>* = {} ) const noexcept {
			size_type my_size = size(), str_size = std::strlen( str );
			if( my_size < str_size )
				return false;
			for( const data_type* my_data = data() ; *str && *str == *my_data ; ++str, ++my_data );
			return !*str;
		}
		/**
		 * Check, whether this string ends with the supplied UTF-8 sequence.
		 *
		 * @param	str		Pointer to a string literal with possibly embedded zeros, interpreted as UTF-8. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<size_type LITLEN>
		bool starts_with( const data_type (&str)[LITLEN] ) const noexcept {
			size_type my_size = size(), str_size = str[LITLEN-1] ? LITLEN : LITLEN-1;
			return my_size >= str_size && std::memcmp( data() , str , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied codepoint sequence.
		 *
		 * @param	str		Pointer to a null-terminated string literal, interpreted as UTF-32. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<typename T>
		bool starts_with( T str , enable_if_ptr<T, value_type>* = {} ) const noexcept {
			for( const_iterator it = cbegin(), end = cend() ; *str && it != end && *str == *it ; ++str, ++it );
			return !*str;
		}
		/**
		 * Check, whether this string ends with the supplied codepoint sequence.
		 *
		 * @param	str		Pointer to a string literal with possibly embedded zeros, interpreted as UTF-32. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<size_type LITLEN>
		bool starts_with( const value_type (&str)[LITLEN] ) const noexcept {
			size_type		str_len = str[LITLEN-1] ? LITLEN : LITLEN-1;
			const_iterator	it = cbegin(), end = cend();
			while( it != end && str_len ){
				if( *it != *str )
					return false;
				++it, ++str, --str_len;
			}
			return !str_len;
		}


		/**
		 * Check, whether this string ends with the supplied character sequence
		 *
		 * @param	str		The string to compare the end of this string with
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		inline bool ends_with( const basic_string& str ) const noexcept {
			size_type my_size = size(), str_size = str.size();
			return my_size >= str_size && std::memcmp( data() + my_size - str_size , str.data() , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied character sequence
		 *
		 * @param	str		The string to compare the end of this string with
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		inline bool ends_with( const std::string& str ) const noexcept {
			size_type my_size = size(), str_size = str.size();
			return my_size >= str_size && std::memcmp( data() + my_size - str_size , str.data() , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied codepoint
		 *
		 * @param	str		The codepoint to compare the end of this string with
		 * @return	true, if this string ends with the codepoint 'cp', false otherwise.
		 */
		inline bool ends_with( value_type cp ) const noexcept {
			return !empty() && back() == cp;
		}
		/**
		 * Check, whether this string ends with the supplied UTF-8 sequence.
		 *
		 * @param	str		Null-terminated string literal, interpreted as UTF-8. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<typename T>
		bool ends_with( T str , enable_if_ptr<T, data_type>* = {} ) const noexcept {
			size_type my_size = size(), str_size = std::strlen(str);
			return my_size >= str_size && std::memcmp( data() + my_size - str_size , str , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied UTF-8 sequence.
		 *
		 * @param	str		Pointer to a string literal with possibly embedded zeros, interpreted as UTF-8. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<size_type LITLEN>
		bool ends_with( const data_type (&str)[LITLEN] ) const noexcept {
			size_type my_size = size(), str_size = str[LITLEN-1] ? LITLEN : LITLEN-1;
			return my_size >= str_size && std::memcmp( data() + my_size - str_size , str , str_size ) == 0;
		}
		/**
		 * Check, whether this string ends with the supplied codepoint sequence.
		 *
		 * @param	str		Pointer to a null-terminated string literal, interpreted as UTF-32. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<typename T>
		bool ends_with( T str , enable_if_ptr<T, value_type>* = {} ) const noexcept {
			size_type				str_len = tiny_utf8_detail::strlen( str );
			const_reverse_iterator	it = crbegin(), end = crend();
			while( it != end && str_len ){
				if( *it != str[--str_len] )
					return false;
				++it;
			}
			return !str_len;
		}
		/**
		 * Check, whether this string ends with the supplied codepoint sequence.
		 *
		 * @param	str		Pointer to a string literal with possibly embedded zeros, interpreted as UTF-32. The pointer is expected to be valid
		 * @return	true, if this string ends with the sequence 'str', false otherwise.
		 */
		template<size_type LITLEN>
		bool ends_with( const value_type (&str)[LITLEN] ) const noexcept {
			size_type				str_len = str[LITLEN-1] ? LITLEN : LITLEN-1;
			const_reverse_iterator	it = crbegin(), end = crend();
			while( it != end && str_len ){
				if( *it != str[--str_len] )
					return false;
				++it;
			}
			return !str_len;
		}


		/**
		 * Compare this string with the supplied one.
		 *
		 * @param	str		The string to compare this one with
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		inline int compare( const basic_string& str ) const noexcept {
			size_type	my_size = size(), str_size = str.size();
			int			result = std::memcmp( data() , str.data() , my_size < str_size ? my_size : str_size );
			if( !result && my_size != str_size )
				result = my_size < str_size ? -1 : 1;
			return result;
		}
		/**
		 * Compare this string with the supplied one.
		 *
		 * @param	str		The string to compare this one with, interpreted as UTF-8.
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		inline int compare( const std::string& str ) const noexcept {
			size_type	my_size = size(), str_size = str.size();
			int			result = std::memcmp( data() , str.data() , my_size < str_size ? my_size : str_size );
			if( !result && my_size != str_size )
				result = my_size < str_size ? -1 : 1;
			return result;
		}
		/**
		 * Compares this string with the supplied one.
		 * The supplied string literal is assumed to end at the (possibly trailling) '\0'.
		 * This is especially important, if this utf8 string contains embedded zeros.
		 *
		 * @param	str		Null-terminated string literal, interpreted as UTF-8. The pointer is expected to be valid
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		template<typename T>
		int compare( T str , enable_if_ptr<T, data_type>* = {} ) const noexcept {
			const data_type* it = data(), *end = it + size();
			while( it != end && *str ){
				if( *it != *str )
					return *it < *str ? -1 : 1;
				++it, ++str;
			}
			return *str ? -1 : it == end ? 0 : 1;
		}
		/**
		 * Compares this string with the supplied one.
		 *
		 * @param	str		Pointer to a string literal with possibly embedded zeros, interpreted as UTF-8. The pointer is expected to be valid
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		template<size_type LITLEN> 
		int compare( const data_type (&str)[LITLEN] ) const noexcept {
			const data_type* it = data(), *end = it + size();
			size_type index = 0, length = str[LITLEN-1] ? LITLEN : LITLEN-1;
			while( it != end && index < length ){
				if( *it != str[index] )
					return *it < str[index] ? -1 : 1;
				++it, ++index;
			}
			return index < length ? -1 : it == end ? 0 : 1;
		}
		/**
		 * Compares this string with the supplied one.
		 * Thes supplied string literal is assumed to end at the (possibly trailling) '\0'.
		 * This is especially important, if this utf8 string contains embedded zeros.
		 *
		 * @param	str		Pointer to a null-terminated string literal, interpreted as UTF-32. The pointer is expected to be valid
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		template<typename T>
		int compare( T str , enable_if_ptr<T, value_type>* = {} ) const noexcept {
			const_iterator	it = cbegin(), end = cend();
			while( it != end && *str ){
				if( *it != *str )
					return *it < *str ? -1 : 1;
				++it, ++str;
			}
			return *str ? -1 : it == end ? 0 : 1;
		}
		/**
		 * Compares this string with the supplied one.
		 *
		 * @param	str		Pointer to a string literal with possibly embedded zeros, interpreted as UTF-32. The pointer is expected to be valid
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		template<size_type LITLEN> 
		int compare( const value_type (&str)[LITLEN] ) const noexcept {
			const_iterator	it = cbegin(), end = cend();
			size_type		index = 0, length = str[LITLEN-1] ? LITLEN : LITLEN-1;
			while( it != end && index < length ){
				if( *it != str[index] )
					return *it < str[index] ? -1 : 1;
				++it, ++index;
			}
			return index < length ? -1 : it == end ? 0 : 1;
		}
		
		//! Equality Comparison Operators
		inline bool operator==( const basic_string& str ) const noexcept { return compare( str ) == 0; }
		inline bool operator!=( const basic_string& str ) const noexcept { return compare( str ) != 0; }
		inline bool operator==( const std::string& str ) const noexcept { return compare( str ) == 0; }
		inline bool operator!=( const std::string& str ) const noexcept { return compare( str ) != 0; }
		template<typename T> inline enable_if_ptr<T, data_type> operator==( T&& str ) const noexcept { return compare( str ) == 0; }
		template<typename T> inline enable_if_ptr<T, data_type> operator!=( T&& str ) const noexcept { return compare( str ) != 0; }
		template<typename T> inline enable_if_ptr<T, value_type> operator==( T&& str ) const noexcept { return compare( str ) == 0; }
		template<typename T> inline enable_if_ptr<T, value_type> operator!=( T&& str ) const noexcept { return compare( str ) != 0; }
		template<size_type LITLEN> inline bool operator==( const data_type (&str)[LITLEN] ) const noexcept { return compare( str ) == 0; }
		template<size_type LITLEN> inline bool operator!=( const data_type (&str)[LITLEN] ) const noexcept { return compare( str ) != 0; }
		template<size_type LITLEN> inline bool operator==( const value_type (&str)[LITLEN] ) const noexcept { return compare( str ) == 0; }
		template<size_type LITLEN> inline bool operator!=( const value_type (&str)[LITLEN] ) const noexcept { return compare( str ) != 0; }
		
		//! Lexicographical comparison Operators
		inline bool operator>( const basic_string& str ) const noexcept { return compare( str ) > 0; }
		inline bool operator>=( const basic_string& str ) const noexcept { return compare( str ) >= 0; }
		inline bool operator<( const basic_string& str ) const noexcept { return compare( str ) < 0; }
		inline bool operator<=( const basic_string& str ) const noexcept { return compare( str ) <= 0; }
		inline bool operator>( const std::string& str ) const noexcept { return compare( str ) > 0; }
		inline bool operator>=( const std::string& str ) const noexcept { return compare( str ) >= 0; }
		inline bool operator<( const std::string& str ) const noexcept { return compare( str ) < 0; }
		inline bool operator<=( const std::string& str ) const noexcept { return compare( str ) <= 0; }
		template<typename T> inline enable_if_ptr<T, data_type> operator>( T&& str ) const noexcept { return compare( str ) > 0; }
		template<typename T> inline enable_if_ptr<T, data_type> operator>=( T&& str ) const noexcept { return compare( str ) >= 0; }
		template<typename T> inline enable_if_ptr<T, data_type> operator<( T&& str ) const noexcept { return compare( str ) < 0; }
		template<typename T> inline enable_if_ptr<T, data_type> operator<=( T&& str ) const noexcept { return compare( str ) <= 0; }
		template<typename T> inline enable_if_ptr<T, value_type> operator>( T&& str ) const noexcept { return compare( str ) > 0; }
		template<typename T> inline enable_if_ptr<T, value_type> operator>=( T&& str ) const noexcept { return compare( str ) >= 0; }
		template<typename T> inline enable_if_ptr<T, value_type> operator<( T&& str ) const noexcept { return compare( str ) < 0; }
		template<typename T> inline enable_if_ptr<T, value_type> operator<=( T&& str ) const noexcept { return compare( str ) <= 0; }
		template<size_type LITLEN> inline bool operator>( const data_type (&str)[LITLEN] ) const noexcept { return compare( str ) > 0; }
		template<size_type LITLEN> inline bool operator>=( const data_type (&str)[LITLEN] ) const noexcept { return compare( str ) >= 0; }
		template<size_type LITLEN> inline bool operator<( const data_type (&str)[LITLEN] ) const noexcept { return compare( str ) < 0; }
		template<size_type LITLEN> inline bool operator<=( const data_type (&str)[LITLEN] ) const noexcept { return compare( str ) <= 0; }
		template<size_type LITLEN> inline bool operator>( const value_type (&str)[LITLEN] ) const noexcept { return compare( str ) > 0; }
		template<size_type LITLEN> inline bool operator>=( const value_type (&str)[LITLEN] ) const noexcept { return compare( str ) >= 0; }
		template<size_type LITLEN> inline bool operator<( const value_type (&str)[LITLEN] ) const noexcept { return compare( str ) < 0; }
		template<size_type LITLEN> inline bool operator<=( const value_type (&str)[LITLEN] ) const noexcept { return compare( str ) <= 0; }
		
		
		//! Get the number of bytes of codepoint in basic_string
		inline width_type get_index_bytes( size_type byte_index ) const noexcept {
			return get_codepoint_bytes( get_buffer()[byte_index] , size() - byte_index );
		}
		inline width_type get_codepoint_bytes( size_type codepoint_index ) const noexcept {
			return get_index_bytes( get_num_bytes_from_start( codepoint_index ) );
		}
		
		
		//! Get the number of bytes before a codepoint, that build up a new codepoint
		inline width_type get_index_pre_bytes( size_type byte_index ) const noexcept {
			const data_type* buffer = get_buffer();
			return get_num_bytes_of_utf8_char_before( buffer , byte_index );
		}
		inline width_type get_codepoint_pre_bytes( size_type codepoint_index ) const noexcept {
			return get_index_pre_bytes( get_num_bytes_from_start( codepoint_index ) );
		}
		
		
		//! Get the byte index of the last codepoint
		inline size_type	raw_back_index() const noexcept { size_type s = size(); return s - get_index_pre_bytes( s ); }
		
		/**
		 * Counts the number of codepoints
		 * that are contained within the supplied range of bytes
		 */
		size_type			get_num_codepoints( size_type byte_start , size_type byte_count ) const noexcept ;
		
		/**
		 * Counts the number of bytes required to hold the supplied amount of codepoints
		 * starting at the supplied byte index (or '0' for the '_from_start' version)
		 */
		size_type			get_num_bytes( size_type byte_start , size_type cp_count ) const noexcept ;
		size_type			get_num_bytes_from_start( size_type cp_count ) const noexcept ;
		
		
	public: //! tinyutf8-specific features
		
		
		/**
		 * Check whether the data inside this basic_string cannot be iterated by an std::string
		 * 
		 * @note	Returns true, if the basic_string has codepoints that exceed 7 bits to be stored
		 * @return	True, if there are UTF-8 formatted byte sequences,
		 *			false, if there are only ascii characters (<128) inside
		 */
		inline bool requires_unicode() const noexcept {
			return sso_inactive() ? t_non_sso.data_len != get_non_sso_string_len() : requires_unicode_sso();
		}
		
		
		/**
		 * Determine, if small string optimization is active
		 * @return	True, if the utf8 data is stored within the basic_string object itself
		 *			false, otherwise
		 */
		inline bool sso_active() const noexcept { return sso_inactive() == false; }
		
		
		/**
		 * Determine, if small string optimization is active
		 * @return	True, if the utf8 data is stored within the basic_string object itself
		 *			false, otherwise
		 */
		inline bool lut_active() const noexcept { return sso_inactive() && basic_string::is_lut_active( basic_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size ) ); }
		
		
		/**
		 * Receive a (null-terminated) wide string literal from this UTF-8 string
		 * 
		 * @param	dest	A buffer capable of holding at least 'length()+1' elements of type 'value_type'
		 * @return	void
		 */
		void to_wide_literal( value_type* dest ) const noexcept {
			for( const data_type* data = get_buffer(), * data_end = data + size() ; data < data_end ; )
				data += decode_utf8_and_len( data , *dest++ , data_end - data );
			*dest = 0;
		}
		
		
		/**
		 * Get the raw data contained in this basic_string wrapped by an std::string
		 * 
		 * @note	Returns the UTF-8 formatted content of this basic_string
		 * @return	UTF-8 formatted data, wrapped inside an std::string
		 */
		inline std::basic_string<data_type> cpp_str( bool prepend_bom = false ) const noexcept(TINY_UTF8_NOEXCEPT) { return prepend_bom ? cpp_str_bom() : std::basic_string<DataType>( c_str() , size() ); }
	};
} // Namespace 'tiny_utf8'


//! std::hash specialization
namespace std
{
	template<typename V, typename D, typename A>
	struct hash<tiny_utf8::basic_string<V, D, A> >
	{
		std::size_t operator()( const tiny_utf8::basic_string<V, D, A>& string ) const noexcept {
			using data_type = typename tiny_utf8::basic_string<V, D, A>::data_type;
			std::hash<data_type>	hasher;
			std::size_t				size = string.size();
			std::size_t				result = 0;
			const data_type*		buffer = string.data();
			for( std::size_t iterator = 0 ; iterator < size ; ++iterator )
				result = result * 31u + hasher( buffer[iterator] );
			return result;
		}
	};
}

//! Stream Operations
template<typename V, typename D, typename A>
std::ostream& operator<<( std::ostream& stream , const tiny_utf8::basic_string<V, D, A>& str ) noexcept(TINY_UTF8_NOEXCEPT) {
	return stream << str.cpp_str();
}
template<typename V, typename D, typename A>
std::istream& operator>>( std::istream& stream , tiny_utf8::basic_string<V, D, A>& str ) noexcept(TINY_UTF8_NOEXCEPT) {
	std::string tmp;
	stream >> tmp;
	str = move(tmp);
	return stream;
}


// Implementation
namespace tiny_utf8
{
	template<typename V, typename D, typename A>
	basic_string<V, D, A>::basic_string( basic_string<V, D, A>::size_type count , basic_string<V, D, A>::value_type cp , const typename basic_string<V, D, A>::allocator_type& alloc )
		noexcept(TINY_UTF8_NOEXCEPT)
		: A( alloc )
		, t_sso()
	{
		if( !count )
			return;
		
		width_type	num_bytes_per_cp = get_codepoint_bytes( cp );
		size_type	data_len = num_bytes_per_cp * count;
		data_type*	buffer;
		
		// Need to allocate a buffer?
		if( data_len > basic_string::get_sso_capacity() )
		{
			// Determine the buffer size
			size_type buffer_size	= determine_main_buffer_size( data_len );
			buffer = this->allocate( determine_total_buffer_size( buffer_size ) );
		#if defined(TINY_UTF8_NOEXCEPT)
			if( !buffer )
				return;
		#endif
			t_non_sso.data = buffer;
			
			// Set Attributes
			set_lut_indiciator( buffer + buffer_size , num_bytes_per_cp == 1 , 0 ); // Set lut indicator
			t_non_sso.buffer_size = buffer_size;
			t_non_sso.data_len = data_len;
			set_non_sso_string_len( count ); // This also disables SSO
		}
		else{
			buffer = t_sso.data;
			
			// Set Attributes
			set_sso_data_len( (unsigned char)data_len );
		}
		
		// Fill the buffer
		if( num_bytes_per_cp > 1 ){
			basic_string::encode_utf8( cp , buffer , num_bytes_per_cp );
			for( data_type* buffer_iter = buffer ; --count > 0 ; )
				std::memcpy( buffer_iter += num_bytes_per_cp , buffer , num_bytes_per_cp );
		}
		else
			std::memset( buffer , cp , count );
		
		// Set trailling zero (in case sso is inactive, this trailing zero also indicates, that no multibyte-tracking is enabled)
		buffer[data_len] = 0;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>::basic_string( basic_string<V, D, A>::size_type count , data_type cp , const typename basic_string<V, D, A>::allocator_type& alloc )
		noexcept(TINY_UTF8_NOEXCEPT)
		: A( alloc )
		, t_sso()
	{
		if( !count )
			return;
		
		data_type*		buffer;
		
		// Need to allocate a buffer?
		if( count > basic_string::get_sso_capacity() )
		{
			size_type buffer_size = determine_main_buffer_size( count );
			buffer = this->allocate( determine_total_buffer_size( buffer_size ) );
		#if defined(TINY_UTF8_NOEXCEPT)
			if( !buffer )
				return;
		#endif
			t_non_sso.data = buffer;
			
			// Set Attributes
			set_lut_indiciator( buffer + buffer_size , true , 0 ); // Set lut indicator
			t_non_sso.buffer_size = buffer_size;
			t_non_sso.data_len = count;
			set_non_sso_string_len( count ); // This also disables SSO
		}
		else{
			buffer = t_sso.data;
			set_sso_data_len( (unsigned char)count );
		}
		
		// Fill the buffer
		std::memset( buffer , cp , count );
		
		// Set trailling zero (in case sso is inactive, this trailing zero also indicates, that no multibyte-tracking is enabled)
		buffer[count] = 0;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>::basic_string( const data_type* str , size_type pos , size_type count , size_type data_left , const typename basic_string<V, D, A>::allocator_type& alloc , tiny_utf8_detail::read_codepoints_tag )
		noexcept(TINY_UTF8_NOEXCEPT)
		: basic_string( alloc )
	{
		if( !count )
			return;
		
		size_type	num_multibytes = 0;
		size_type	data_len = 0;
		size_type	string_len = 0;
		data_type*	buffer;
		
		// Iterate to the nth codepoint marking the start of the string to copy
		while( *str && string_len < pos && data_left != 0u ){
			width_type bytes = get_codepoint_bytes( str[data_len] , data_left ); // Read number of bytes of current codepoint
			data_left		-= bytes;
			str				+= bytes;
			++string_len;
		}
		string_len = 0;
		
		// Count bytes, multibytes and string length
		while( str[data_len] && string_len < count )
		{
			// Read number of bytes of current codepoint
			width_type bytes = get_codepoint_bytes( str[data_len] , data_left );
			data_len		+= bytes;				// Increase number of bytes
			data_left		-= bytes;				// Decrease amount of bytes left
			string_len		+= 1;					// Increase number of codepoints
			num_multibytes	+= bytes > 1 ? 1 : 0;	// Increase number of occoured multibytes?
		}
		
		// Need heap memory?
		if( data_len > basic_string::get_sso_capacity() )
		{
			if( basic_string::is_lut_worth( num_multibytes , string_len , false , false ) )
			{
				// Determine the buffer size (excluding the lut indicator) and the lut width
				width_type	lut_width;
				size_type	buffer_size	= determine_main_buffer_size( data_len , num_multibytes , &lut_width );
				buffer = this->allocate( determine_total_buffer_size( buffer_size ) );
			#if defined(TINY_UTF8_NOEXCEPT)
				if( !buffer )
					return;
			#endif
				t_non_sso.data = buffer;
				
				// Set up LUT
				data_type*	lut_iter = basic_string::get_lut_base_ptr( buffer , buffer_size );
				basic_string::set_lut_indiciator( lut_iter , true , num_multibytes ); // Set the LUT indicator
				
				// Fill the lut and copy bytes
				data_type* buffer_iter = buffer;
				const data_type* str_iter = str;
				const data_type* str_end = str + data_len; // Compute End of the string
				while( str_iter < str_end )
				{
					width_type bytes = get_codepoint_bytes( *str_iter , str_end - str_iter );
					switch( bytes )
					{
						case 7:	buffer_iter[6] = str_iter[6]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 6:	buffer_iter[5] = str_iter[5]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 5:	buffer_iter[4] = str_iter[4]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 4:	buffer_iter[3] = str_iter[3]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 3:	buffer_iter[2] = str_iter[2]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 2:	buffer_iter[1] = str_iter[1]; // Copy data byte
							// Set next entry in the LUT!
							basic_string::set_lut( lut_iter -= lut_width , lut_width , str_iter - str );
							TINY_UTF8_FALLTHROUGH
						case 1: buffer_iter[0] = str_iter[0]; break; // Copy data byte
					}
					buffer_iter	+= bytes;
					str_iter	+= bytes;
				}
				*buffer_iter = '\0'; // Set trailing '\0'
				
				// Set Attributes
				t_non_sso.buffer_size = buffer_size;
				t_non_sso.data_len = data_len;
				set_non_sso_string_len( string_len ); // This also disables SSO
				
				return; // We have already done everything!
			}
			
			size_type buffer_size = determine_main_buffer_size( data_len );
			buffer = t_non_sso.data = this->allocate( determine_total_buffer_size( buffer_size ) );
			
			// Set up LUT
			data_type*	lut_iter = basic_string::get_lut_base_ptr( buffer , buffer_size );
			basic_string::set_lut_indiciator( lut_iter , num_multibytes == 0 , 0 ); // Set the LUT indicator
			
			// Set Attributes
			t_non_sso.buffer_size = buffer_size;
			t_non_sso.data_len = data_len;
			set_non_sso_string_len( string_len );
		}
		else{
			buffer = t_sso.data;
			
			// Set Attrbutes
			set_sso_data_len( (unsigned char)data_len );
			
			// Set up LUT: Not necessary, since the LUT is automatically inactive,
			// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
		}
		
		//! Fill the buffer
		std::memcpy( buffer , str , data_len );
		buffer[data_len] = '\0';
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>::basic_string( const data_type* str , size_type data_len , const typename basic_string<V, D, A>::allocator_type& alloc , tiny_utf8_detail::read_bytes_tag )
		noexcept(TINY_UTF8_NOEXCEPT)
		: basic_string( alloc )
	{
		if( !data_len )
			return;
		
		size_type		num_multibytes = 0;
		size_type		index = 0;
		size_type		string_len = 0;
		
		// Count bytes, multibytes and string length
		while( index < data_len )
		{
			// Read number of bytes of current codepoint
			width_type bytes = get_codepoint_bytes( str[index] , basic_string::npos );
			index			+= bytes;				// Increase number of bytes
			string_len		+= 1;					// Increase number of codepoints
			num_multibytes	+= bytes > 1 ? 1 : 0;	// Increase number of occoured multibytes?
		}
		
		data_type*	buffer;
		
		// Need heap memory?
		if( data_len > basic_string::get_sso_capacity() )
		{
			if( basic_string::is_lut_worth( num_multibytes , string_len , false , false ) )
			{
				// Determine the buffer size (excluding the lut indicator) and the lut width
				width_type	lut_width;
				size_type	buffer_size	= determine_main_buffer_size( data_len , num_multibytes , &lut_width );
				buffer = this->allocate( determine_total_buffer_size( buffer_size ) );
			#if defined(TINY_UTF8_NOEXCEPT)
				if( !buffer )
					return;
			#endif
				t_non_sso.data = buffer;
				
				// Set up LUT
				data_type*	lut_iter = basic_string::get_lut_base_ptr( buffer , buffer_size );
				basic_string::set_lut_indiciator( lut_iter , true , num_multibytes ); // Set the LUT indicator
				
				// Fill the lut and copy bytes
				data_type* buffer_iter = buffer;
				const data_type* str_iter = str;
				const data_type* str_end = str + data_len; // Compute End of the string
				while( str_iter < str_end )
				{
					width_type bytes = get_codepoint_bytes( *str_iter , str_end - str_iter );
					switch( bytes )
					{
						case 7:	buffer_iter[6] = str_iter[6]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 6:	buffer_iter[5] = str_iter[5]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 5:	buffer_iter[4] = str_iter[4]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 4:	buffer_iter[3] = str_iter[3]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 3:	buffer_iter[2] = str_iter[2]; TINY_UTF8_FALLTHROUGH // Copy data byte
						case 2:	buffer_iter[1] = str_iter[1]; // Copy data byte
							// Set next entry in the LUT!
							basic_string::set_lut( lut_iter -= lut_width , lut_width , str_iter - str );
							TINY_UTF8_FALLTHROUGH
						case 1: buffer_iter[0] = str_iter[0]; break; // Copy data byte
					}
					buffer_iter	+= bytes;
					str_iter	+= bytes;
				}
				*buffer_iter = '\0'; // Set trailing '\0'
				
				// Set Attributes
				t_non_sso.buffer_size = buffer_size;
				t_non_sso.data_len = data_len;
				set_non_sso_string_len( string_len ); // This also disables SSO
				
				return; // We have already done everything!
			}
			
			size_type buffer_size = determine_main_buffer_size( data_len );
			buffer = t_non_sso.data = this->allocate(  determine_total_buffer_size( buffer_size ) );
			
			// Set up LUT
			data_type*	lut_iter = basic_string::get_lut_base_ptr( buffer , buffer_size );
			basic_string::set_lut_indiciator( lut_iter , num_multibytes == 0 , 0 ); // Set the LUT indicator
			
			// Set Attributes
			t_non_sso.buffer_size = buffer_size;
			t_non_sso.data_len = data_len;
			set_non_sso_string_len( string_len );
		}
		else{
			buffer = t_sso.data;
			
			// Set Attrbutes
			set_sso_data_len( (unsigned char)data_len );
			
			// Set up LUT: Not necessary, since the LUT is automatically inactive,
			// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
		}
		
		//! Fill the buffer
		std::memcpy( buffer , str , data_len );
		buffer[data_len] = '\0';
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>::basic_string( const value_type* str , size_type len , const typename basic_string<V, D, A>::allocator_type& alloc )
		noexcept(TINY_UTF8_NOEXCEPT)
		: basic_string( alloc )
	{
		if( !len )
			return;
		
		size_type		num_multibytes = 0;
		size_type		data_len = 0;
		size_type		string_len = 0;
		
		// Count bytes, mutlibytes and string length
		while( string_len < len && ( str[string_len] || len != basic_string::npos ) )
		{
			// Read number of bytes of current codepoint
			width_type bytes = get_codepoint_bytes( str[string_len] );
			
			data_len		+= bytes;		// Increase number of bytes
			string_len		+= 1;			// Increase number of codepoints
			num_multibytes	+= bytes > 1 ;	// Increase number of occoured multibytes?
		}
		
		data_type*	buffer;
		
		// Need heap memory?
		if( data_len > basic_string::get_sso_capacity() )
		{
			if( basic_string::is_lut_worth( num_multibytes , string_len , false , false ) )
			{	
				// Determine the buffer size (excluding the lut indicator) and the lut width
				width_type lut_width;
				size_type buffer_size	= determine_main_buffer_size( data_len , num_multibytes , &lut_width );
				buffer = this->allocate( determine_total_buffer_size( buffer_size ) );
			#if defined(TINY_UTF8_NOEXCEPT)
				if( !buffer )
					return;
			#endif
				t_non_sso.data = buffer;
				
				//! Fill LUT
				data_type* lut_iter		= basic_string::get_lut_base_ptr( buffer , buffer_size );
				data_type* buffer_iter	= buffer;
				basic_string::set_lut_indiciator( lut_iter , true , num_multibytes ); // Set the LUT indicator
				
				// Iterate through wide char literal
				for( size_type i = 0 ; i < string_len ; i++ )
				{
					// Encode wide char to utf8
					width_type codepoint_bytes = basic_string::encode_utf8( str[i] , buffer_iter );
					
					// Push position of character to 'indices'
					if( codepoint_bytes > 1 )
						basic_string::set_lut( lut_iter -= lut_width , lut_width , buffer_iter - buffer );
					
					// Step forward with copying to internal utf8 buffer
					buffer_iter += codepoint_bytes;
				}
				*buffer_iter = '\0'; // Set trailing '\0'
				
				// Set Attributes
				t_non_sso.buffer_size = buffer_size;
				t_non_sso.data_len = data_len;
				set_non_sso_string_len( string_len );
				
				return; // We have already done everything!
			}
			
			size_type buffer_size = determine_main_buffer_size( data_len );
			buffer = t_non_sso.data = this->allocate(  determine_total_buffer_size( buffer_size ) );
			
			// Set up LUT
			data_type*	lut_iter = basic_string::get_lut_base_ptr( buffer , buffer_size );
			basic_string::set_lut_indiciator( lut_iter , num_multibytes == 0 , 0 ); // Set the LUT indicator
			
			// Set Attributes
			t_non_sso.buffer_size = buffer_size;
			t_non_sso.data_len = data_len;
			set_non_sso_string_len( string_len );
		}
		else{
			buffer = t_sso.data;
			
			// Set Attrbutes
			set_sso_data_len( (unsigned char)data_len );
			
			// Set up LUT: Not necessary, since the LUT is automatically inactive,
			// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
		}
		
		data_type* buffer_iter = buffer;
		
		// Iterate through wide char literal
		for( size_type i = 0 ; i < string_len ; i++ )
			// Encode wide char to utf8 and step forward the number of bytes it took
			buffer_iter += basic_string::encode_utf8( str[i] , buffer_iter );
		
		*buffer_iter = '\0'; // Set trailing '\0'
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::width_type basic_string<V, D, A>::get_num_bytes_of_utf8_char_before( const data_type* data_start , size_type index ) noexcept
	{
		data_start += index;
		// Only Check the possibilities, that could appear
		switch( index )
		{
			default:
				if( ((unsigned char)data_start[-7] & 0xFE ) == 0xFC )	// 11111110 seven bytes
					return 7;
				TINY_UTF8_FALLTHROUGH
			case 6:
				if( ((unsigned char)data_start[-6] & 0xFE ) == 0xFC )	// 1111110X six bytes
					return 6;
				TINY_UTF8_FALLTHROUGH
			case 5:
				if( ((unsigned char)data_start[-5] & 0xFC ) == 0xF8 )	// 111110XX five bytes
					return 5;
				TINY_UTF8_FALLTHROUGH
			case 4:
				if( ((unsigned char)data_start[-4] & 0xF8 ) == 0xF0 )	// 11110XXX four bytes
					return 4;
				TINY_UTF8_FALLTHROUGH
			case 3:
				if( ((unsigned char)data_start[-3] & 0xF0 ) == 0xE0 )	// 1110XXXX three bytes
					return 3;
				TINY_UTF8_FALLTHROUGH
			case 2:
				if( ((unsigned char)data_start[-2] & 0xE0 ) == 0xC0 )	// 110XXXXX two bytes
					return 2;
				TINY_UTF8_FALLTHROUGH
			case 1:
			case 0:
				return 1;
		}
	}

	#if !TINY_UTF8_HAS_CLZ
	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::width_type basic_string<V, D, A>::get_codepoint_bytes( typename basic_string<V, D, A>::data_type first_byte , typename basic_string<V, D, A>::size_type data_left ) noexcept
	{
		// Only Check the possibilities, that could appear
		switch( data_left )
		{
			default:
				if( ( (unsigned char)first_byte & 0xFFu ) == 0xFEu ) // 11111110 -> seven bytes
					return 7;
			case 6:
				if( ( (unsigned char)first_byte & 0xFEu ) == 0xFCu ) // 1111110X -> six bytes
					return 6;
			case 5:
				if( ( (unsigned char)first_byte & 0xFCu ) == 0xF8u ) // 111110XX -> five bytes
					return 5;
			case 4:
				if( ( (unsigned char)first_byte & 0xF8u ) == 0xF0u ) // 11110XXX -> four bytes
					return 4;
			case 3:
				if( ( (unsigned char)first_byte & 0xF0u ) == 0xE0u ) // 1110XXXX -> three bytes
					return 3;
			case 2:
				if( ( (unsigned char)first_byte & 0xE0u ) == 0xC0u ) // 110XXXXX -> two bytes
					return 2;
			case 1:
			case 0:
				return 1; // one byte
		}
	}
	#endif // !TINY_UTF8_HAS_CLZ

	template<typename V, typename D, typename A>
	basic_string<V, D, A>& basic_string<V, D, A>::operator=( const basic_string<V, D, A>& str ) noexcept(TINY_UTF8_NOEXCEPT)
	{
		// Note: Self assignment is expected to be very rare. We tolerate overhead in this situation.
		// Therefore, we right away check for sso states in 'this' and 'str'.
		// If they are equal, perform the check then.
		
		switch( sso_inactive() + str.sso_inactive() * 2 )
		{
			case 3: // [sso-inactive] = [sso-inactive]
			{
				if( &str == this )
					return *this;
				const data_type* str_lut_base_ptr = basic_string::get_lut_base_ptr( str.t_non_sso.data , str.t_non_sso.buffer_size );
				if( basic_string::is_lut_active( str_lut_base_ptr ) )
				{
					width_type	lut_width = get_lut_width( t_non_sso.buffer_size ); // Lut width, if the current buffer is used
					size_type	str_lut_len = basic_string::get_lut_len( str_lut_base_ptr );
					
					// Can the current buffer hold the data and the lut?
					if( basic_string::determine_main_buffer_size( str.t_non_sso.data_len , str_lut_len , lut_width ) < t_non_sso.buffer_size )
					{
						width_type	str_lut_width = get_lut_width( str.t_non_sso.buffer_size );
						
						// How to copy indices?
						if( lut_width == str_lut_width ){
							str_lut_len *= str_lut_width; // Compute the size in bytes of the lut
							std::memcpy(
								basic_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size ) - str_lut_len
								, str_lut_base_ptr - str_lut_len
								, str_lut_len + sizeof(indicator_type) // Also copy lut indicator
							);
						}
						else{
							data_type* lut_iter = basic_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size );
							for( ; str_lut_len > 0 ; --str_lut_len )
								basic_string::set_lut(
									lut_iter -= lut_width
									, lut_width
									, basic_string::get_lut( str_lut_base_ptr -= str_lut_width , str_lut_width )
								);
						}
					}
					else
						goto lbl_replicate_whole_buffer;
				}
				// Is the current buffer too small to hold just the data?
				else if( basic_string::determine_main_buffer_size( str.t_non_sso.data_len ) > t_non_sso.buffer_size )
					goto lbl_replicate_whole_buffer;
				
				// Copy data and lut indicator only
				std::memcpy( t_non_sso.data , str.t_non_sso.data , str.t_non_sso.data_len + 1 );
				basic_string::copy_lut_indicator(
					basic_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size )
					, str_lut_base_ptr
				);
				t_non_sso.data_len = str.t_non_sso.data_len;
				t_non_sso.string_len = str.t_non_sso.string_len; // Copy the string_len bit pattern
				(allocator_type&)*this = (const allocator_type&)str; // Copy allocator
				return *this;
				
			lbl_replicate_whole_buffer: // Replicate the whole buffer
				this->deallocate( t_non_sso.data , t_non_sso.buffer_size );
			}
				TINY_UTF8_FALLTHROUGH
			case 2: // [sso-active] = [sso-inactive]
				(allocator_type&)*this = (const allocator_type&)str; // Copy allocator
				t_non_sso.data = this->allocate(  basic_string::determine_total_buffer_size( str.t_non_sso.buffer_size ) );
				std::memcpy( t_non_sso.data , str.t_non_sso.data , str.t_non_sso.buffer_size + sizeof(indicator_type) ); // Copy data
				t_non_sso.buffer_size = str.t_non_sso.buffer_size;
				t_non_sso.data_len = str.t_non_sso.data_len;
				t_non_sso.string_len = str.t_non_sso.string_len; // This also disables SSO
				return *this;
			case 1: // [sso-inactive] = [sso-active]
				this->deallocate( t_non_sso.data , t_non_sso.buffer_size );
				TINY_UTF8_FALLTHROUGH
			case 0: // [sso-active] = [sso-active]
				if( &str != this ){
					(allocator_type&)*this = (const allocator_type&)str; // Copy allocator
					std::memcpy( (void*)&this->t_sso , &str.t_sso , sizeof(basic_string::SSO) ); // Copy data
				}
				return *this;
		}
		return *this;
	}

	template<typename V, typename D, typename A>
	void basic_string<V, D, A>::shrink_to_fit() noexcept(TINY_UTF8_NOEXCEPT)
	{
		if( sso_active() )
			return;
		
		size_type	data_len = size();
		
		if( !data_len )
			return;
		
		size_type	buffer_size = get_buffer_size();
		data_type*	buffer = get_buffer();
		data_type*	lut_base_ptr = basic_string::get_lut_base_ptr( buffer , buffer_size );
		size_type	required_buffer_size;
		
		if( is_lut_active( lut_base_ptr ) )
		{
			size_type	lut_len				= get_lut_len( lut_base_ptr );
			width_type	new_lut_width;
			required_buffer_size			= determine_main_buffer_size( data_len , lut_len , &new_lut_width );
			
			//! Determine the threshold above which it's profitable to reallocate (at least 10 bytes and at least a quarter of the memory)
			if( buffer_size < std::max<size_type>( required_buffer_size + 10 , required_buffer_size >> 2 ) )
				return;
			
			// Allocate new buffer
			t_non_sso.data					= this->allocate(  determine_total_buffer_size( required_buffer_size ) );
			width_type	old_lut_width		= basic_string::get_lut_width( buffer_size );
			data_type*	new_lut_base_ptr	= basic_string::get_lut_base_ptr( t_non_sso.data , required_buffer_size );
			
			// Does the data type width change?
			if( old_lut_width != new_lut_width ){ // Copy indices one at a time
				basic_string::set_lut_indiciator( new_lut_base_ptr , true , lut_len );
				for( size_type i = 0 ; i < lut_len ; i++ )
					set_lut(
						new_lut_base_ptr -= new_lut_width
						, new_lut_width
						, get_lut( lut_base_ptr -= old_lut_width , old_lut_width )
					);
			}
			else{ // Copy the lut as well as the lut indicator in one action
				size_type lut_size = lut_len * old_lut_width;
				std::memcpy( new_lut_base_ptr - lut_size , lut_base_ptr - lut_size , lut_size + sizeof(indicator_type) );
			}
		}
		else
		{
			required_buffer_size = determine_main_buffer_size( data_len );
			
			//! Determine the threshold above which it's profitable to reallocate (at least 10 bytes and at least a quarter of the memory)
			if( buffer_size < std::max<size_type>( required_buffer_size + 10 , required_buffer_size >> 2 ) )
				return;
			
			t_non_sso.data = this->allocate(  determine_total_buffer_size( required_buffer_size ) ); // Allocate new buffer
		}
		
		// Copy BUFFER
		std::memcpy( t_non_sso.data , buffer , data_len + 1 );
		t_non_sso.buffer_size = required_buffer_size; // Set new buffer size
		
		// Delete old buffer
		this->deallocate( buffer , buffer_size );
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::get_non_sso_capacity() const noexcept
	{
		size_type	data_len		= t_non_sso.data_len;
		size_type	buffer_size		= t_non_sso.buffer_size;
		
		// If empty, assume an average number of bytes per codepoint of '1' (and an empty lut)
		if( !data_len )
			return buffer_size - 1;
		
		const data_type*	buffer			= t_non_sso.data;
		size_type	string_len				= get_non_sso_string_len();
		const data_type*	lut_base_ptr	= basic_string::get_lut_base_ptr( buffer , buffer_size );
		
		// If the lut is active, add the number of additional bytes to the current data length
		if( basic_string::is_lut_active( lut_base_ptr ) )
			data_len += basic_string::get_lut_width( buffer_size ) * basic_string::get_lut_len( lut_base_ptr );
		
		// Return the buffer size (excluding the potential trailing '\0') divided by the average number of bytes per codepoint
		return ( buffer_size - 1 ) * string_len / data_len;
	}

	template<typename V, typename D, typename A>
	bool basic_string<V, D, A>::requires_unicode_sso() const noexcept
	{
		constexpr size_type mask = get_msb_mask<size_type>();
		size_type			data_len = get_sso_data_len();
		size_type			i = 0;
		
		// Search sizeof(size_type) bytes at once
		for( ; i < data_len / sizeof(size_type) ; i++ )
			if( ((size_type*)t_sso.data)[i] & mask )
				return true;
		
		// Search byte-wise
		i *= sizeof(size_type);
		for( ; i < data_len ; i++ )
			if( t_sso.data[i] & 0x80 )
				return true;
		
		return false;
	}

	template<typename V, typename D, typename A>
	std::basic_string<typename basic_string<V, D, A>::data_type> basic_string<V, D, A>::cpp_str_bom() const noexcept
	{
		// Create std::string
		std::basic_string<data_type>	result = std::basic_string<data_type>( size() + 3 , ' ' );
		data_type*						tmp_buffer = const_cast<data_type*>( result.data() );
		
		// Write BOM
		tmp_buffer[0] = static_cast<data_type>(0xEF);
		tmp_buffer[1] = static_cast<data_type>(0xBB);
		tmp_buffer[2] = static_cast<data_type>(0xBF);
		
		// Copy my data into it
		std::memcpy( tmp_buffer + 3 , get_buffer() , size() + 1 );
		
		return result;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::get_num_codepoints( typename basic_string<V, D, A>::size_type index , typename basic_string<V, D, A>::size_type byte_count ) const noexcept
	{
		const data_type*	buffer;
		size_type			data_len;
		
		if( sso_inactive() )
		{
			buffer							= t_non_sso.data;
			data_len						= t_non_sso.data_len;
			size_type buffer_size			= t_non_sso.buffer_size;
			const data_type*	lut_iter	= basic_string::get_lut_base_ptr( buffer , buffer_size );
			
			// Is the LUT active?
			if( basic_string::is_lut_active( lut_iter ) )
			{
				size_type lut_len = basic_string::get_lut_len( lut_iter );
				
				if( !lut_len )
					return byte_count;
				
				width_type			lut_width	= basic_string::get_lut_width( buffer_size );
				const data_type*	lut_begin	= lut_iter - lut_len * lut_width;
				size_type			end_index	= index + byte_count;
				
				// Iterate to the start of the relevant part of the multibyte table
				while( lut_iter >= lut_begin ){
					lut_iter -= lut_width; // Move cursor to the next lut entry
					if( basic_string::get_lut( lut_iter , lut_width ) >= index )
						break;
				}
				
				// Iterate over relevant multibyte indices
				while( lut_iter >= lut_begin ){
					size_type multibyte_index = basic_string::get_lut( lut_iter , lut_width );
					if( multibyte_index >= end_index )
						break;
					byte_count -= basic_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
					lut_iter -= lut_width; // Move cursor to the next lut entry
				}
				
				// Now byte_count is the number of codepoints
				return byte_count;
			}
		}
		else{
			buffer = t_sso.data;
			data_len = get_sso_data_len();
		}
		
		// Procedure: Reduce the byte count by the number of data bytes within multibytes
		const data_type*	buffer_iter = buffer + index;
		const data_type*	fragment_end = buffer_iter + byte_count;
		
		// Iterate the data byte by byte...
		while( buffer_iter < fragment_end ){
			width_type bytes = basic_string::get_codepoint_bytes( *buffer_iter , fragment_end - buffer_iter );
			buffer_iter += bytes;
			byte_count -= bytes - 1;
		}
		
		// Now byte_count is the number of codepoints
		return byte_count;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::get_num_bytes_from_start( typename basic_string<V, D, A>::size_type cp_count ) const noexcept
	{
		const data_type*	buffer;
		size_type			data_len;
		
		if( sso_inactive() )
		{
			buffer = t_non_sso.data;
			data_len = t_non_sso.data_len;
			size_type			buffer_size	= t_non_sso.buffer_size;
			const data_type*	lut_iter	= basic_string::get_lut_base_ptr( buffer , buffer_size );
			
			// Is the lut active?
			if( basic_string::is_lut_active( lut_iter ) )
			{
				// Reduce the byte count by the number of data bytes within multibytes
				width_type lut_width = basic_string::get_lut_width( buffer_size );
				
				// Iterate over relevant multibyte indices
				for( size_type lut_len = basic_string::get_lut_len( lut_iter ) ; lut_len-- > 0 ; )
				{
					size_type multibyte_index = basic_string::get_lut( lut_iter -= lut_width , lut_width );
					if( multibyte_index >= cp_count )
						break;
					cp_count += basic_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
				}
				
				return cp_count;
			}
		}
		else{
			buffer = t_sso.data;
			data_len = get_sso_data_len();
		}
		
		size_type num_bytes = 0;
		while( cp_count-- > 0 && num_bytes <= data_len )
			num_bytes += get_codepoint_bytes( buffer[num_bytes] , data_len - num_bytes );
		
		return num_bytes;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::get_num_bytes( typename basic_string<V, D, A>::size_type index , typename basic_string<V, D, A>::size_type cp_count ) const noexcept
	{
		size_type			potential_end_index = index + cp_count;
		const data_type*	buffer;
		size_type			data_len;
		
		// Procedure: Reduce the byte count by the number of utf8 data bytes
		if( sso_inactive() )
		{
			buffer = t_non_sso.data;
			data_len = t_non_sso.data_len;
			size_type			buffer_size	= t_non_sso.buffer_size;
			const data_type*	lut_iter	= basic_string::get_lut_base_ptr( buffer , buffer_size );
			
			// 'potential_end_index < index' is needed because of potential integer overflow in sum
			if( potential_end_index > data_len || potential_end_index < index )
				return data_len - index;
			
			// Is the lut active?
			if( basic_string::is_lut_active( lut_iter ) )
			{
				size_type orig_index = index;
				size_type lut_len = basic_string::get_lut_len( lut_iter );
				
				if( !lut_len )
					return cp_count;
				
				// Reduce the byte count by the number of data bytes within multibytes
				width_type			lut_width = basic_string::get_lut_width( buffer_size );
				const data_type*	lut_begin = lut_iter - lut_len * lut_width;
				
				// Iterate to the start of the relevant part of the multibyte table
				for( lut_iter -= lut_width /* Move to first entry */ ; lut_iter >= lut_begin ; lut_iter -= lut_width )
					if( basic_string::get_lut( lut_iter , lut_width ) >= index )
						break;
				
				// Add at least as many bytes as codepoints
				index += cp_count;
				
				// Iterate over relevant multibyte indices
				while( lut_iter >= lut_begin ){
					size_type multibyte_index = basic_string::get_lut( lut_iter , lut_width );
					if( multibyte_index >= index )
						break;
					index += basic_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
					lut_iter -= lut_width; // Move cursor to the next lut entry
				}
				
				return index - orig_index;
			}
		}
		else{
			buffer = t_sso.data;
			data_len = get_sso_data_len();
			
			// 'potential_end_index < index' is needed because of potential integer overflow in sum
			if( potential_end_index > data_len || potential_end_index < index )
				return data_len - index;
		}
		
		size_type orig_index = index;
		
		// Procedure: Reduce the byte count by the number of utf8 data bytes
		while( cp_count-- > 0 && index <= data_len )
			index += get_codepoint_bytes( buffer[index] , data_len - index );
		
		return index - orig_index;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A> basic_string<V, D, A>::raw_substr( typename basic_string<V, D, A>::size_type index , typename basic_string<V, D, A>::size_type byte_count ) const noexcept(TINY_UTF8_NOEXCEPT)
	{
		// Bound checks...
		size_type data_len = size();
		if( index > data_len ){
			TINY_UTF8_THROW( "tiny_utf8::basic_string::(raw_)substr" , index > data_len );
			return {};
		}
		size_type		end_index = index + byte_count;
		if( end_index > data_len || end_index < index ){ // 'end_index < index' is needed because of potential integer overflow in sum
			end_index = data_len;
			byte_count = end_index - index;
		}
		
		// If the substring is not a substring
		if( byte_count == data_len )
			return *this;
		
		// If the new string is a small string
		if( byte_count <= basic_string::get_sso_capacity() )
		{
			basic_string	result;
			
			// Copy data
			std::memcpy( result.t_sso.data , get_buffer() + index , byte_count );
			result.t_sso.data[byte_count] = '\0';
			
			// Set length
			result.set_sso_data_len( (unsigned char)byte_count );
			
			return result;
		}
		
		// At this point, sso must be inactive, because a substring can only be smaller than this string
		
		size_type			mb_index;
		size_type			substr_cps;
		size_type			substr_mbs		= 0;
		size_type			buffer_size		= t_non_sso.buffer_size;
		const data_type*	buffer			= t_non_sso.data;
		const data_type*	lut_base_ptr	= basic_string::get_lut_base_ptr( buffer , buffer_size );
		bool				lut_active		= basic_string::is_lut_active( lut_base_ptr );
		width_type			lut_width; // Ignore uninitialized warning, see [5]
		
		// Count the number of SUBSTRING Multibytes and codepoints
		if( lut_active )
		{
			lut_width	= basic_string::get_lut_width( buffer_size );
			mb_index	= 0;
			const data_type* lut_begin	= lut_base_ptr - lut_width * basic_string::get_lut_len( lut_base_ptr );
			const data_type* lut_iter	= lut_base_ptr;
			for( lut_iter -= lut_width; lut_iter >= lut_begin ; lut_iter -= lut_width ){
				if( basic_string::get_lut( lut_iter , lut_width ) >= index )
					break;
				mb_index++;
			}
			substr_cps = byte_count; // Add at least as many bytes as codepoints
			for( ; lut_iter >= lut_begin ; lut_iter -= lut_width ){ // Iterate over relevant multibyte indices
				size_type multibyte_index = basic_string::get_lut( lut_iter , lut_width );
				if( multibyte_index >= end_index )
					break;
				substr_cps -= basic_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ); // Actually '- 1', but see[4]
				++substr_mbs;
			}
			substr_cps += substr_mbs; // [4]: We subtracted all bytes of the relevant multibytes. We therefore need to re-add substr_mbs codepoints.
		}
		else
		{
			substr_cps		= 0;
			size_type iter	= index;
			while( iter < end_index ){ // Count REPLACED multibytes and codepoints
				width_type bytes = get_codepoint_bytes( buffer[iter] , data_len - iter );
				substr_mbs += bytes > 1; iter += bytes; ++substr_cps;
			}
		}
		
		size_type	substr_buffer_size;
		width_type	substr_lut_width;
		
		// Indices Table worth the memory loss?
		if( size_type( substr_mbs - 1 ) < size_type( substr_cps / 2 ) ) // Note: substr_mbs is intended to underflow at '0'
			substr_buffer_size	= determine_main_buffer_size( byte_count , substr_mbs , &substr_lut_width );
		else{
			substr_lut_width	= 0;
			substr_buffer_size	= determine_main_buffer_size( byte_count );
		}
		
		data_type* substr_buffer		= this->allocate( determine_total_buffer_size( substr_buffer_size ) );
		data_type* substr_lut_base_ptr	= basic_string::get_lut_base_ptr( substr_buffer , substr_buffer_size );
		
		// Copy requested BUFFER part
		std::memcpy( substr_buffer , buffer + index , byte_count );
		substr_buffer[byte_count] = '\0'; // Add trailing '\0'
		
		if( substr_lut_width )
		{
			// Set new lut size and mode
			basic_string::set_lut_indiciator( substr_lut_base_ptr , true , substr_mbs );
			
			// Reuse old indices?
			if( lut_active )
			{
				// Can we do a plain copy of indices?
				if( index == 0 && substr_lut_width == lut_width ) // [5]: lut_width is initialized, as soon as 'lut_active' is true
					std::memcpy(
						substr_lut_base_ptr - substr_mbs * lut_width
						, lut_base_ptr - ( mb_index + substr_mbs ) * lut_width // mb_index is initialized, as soon as 'lut_active' is true
						, substr_mbs * lut_width
					);
				else
					for( const data_type* lut_iter = lut_base_ptr - mb_index * lut_width; substr_mbs-- > 0 ; )
						basic_string::set_lut(
							substr_lut_base_ptr -= substr_lut_width
							, substr_lut_width
							, basic_string::get_lut( lut_iter -= lut_width , lut_width ) - index
						);
			}
			else // Fill the lut by iterating over the substrings data
				for( size_type	substr_iter = 0 ; substr_iter < byte_count ; ){
					width_type bytes = get_codepoint_bytes( substr_buffer[substr_iter] , byte_count - substr_iter );
					if( bytes > 1 )
						basic_string::set_lut( substr_lut_base_ptr -= substr_lut_width , substr_lut_width , substr_iter );
					substr_iter += bytes;
				}
		}
		else // Set substring lut mode
			basic_string::set_lut_indiciator( substr_lut_base_ptr , substr_mbs == 0 , 0 );
		
		// Prepare result
		basic_string result;
		result.t_non_sso.data = substr_buffer;
		result.t_non_sso.data_len = byte_count;
		result.t_non_sso.buffer_size = substr_buffer_size;
		result.set_non_sso_string_len( substr_cps );
		
		return result;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>& basic_string<V, D, A>::append( const basic_string<V, D, A>& app ) noexcept(TINY_UTF8_NOEXCEPT)
	{
		// Will add nothing?
		bool app_sso_inactive = app.sso_inactive();
		size_type app_data_len = app_sso_inactive ? app.t_non_sso.data_len : app.get_sso_data_len();
		if( app_data_len == 0 )
			return *this;
		
		// Compute some metrics
		size_type old_data_len	= size();
		size_type new_data_len	= old_data_len + app_data_len;
		
		// Will be sso string?
		if( new_data_len <= basic_string::get_sso_capacity() ){
			std::memcpy( t_sso.data + old_data_len , app.t_sso.data , app_data_len ); // Copy APPENDIX (must have sso active as well)
			t_sso.data[new_data_len] = '\0'; // Trailing '\0'
			set_sso_data_len( (unsigned char)new_data_len ); // Adjust size
			return *this;
		}
		
		//! Ok, obviously no small string, we have to update the data, the lut and the number of codepoints
		
		
		// Count codepoints and multibytes of insertion
		bool				app_lut_active;
		const data_type*	app_buffer;
		const data_type*	app_lut_base_ptr;
		size_type			app_buffer_size;
		size_type			app_string_len;
		size_type			app_lut_len;
		if( app.sso_inactive() )
		{
			app_buffer_size	= app.t_non_sso.buffer_size;
			app_buffer		= app.t_non_sso.data;
			app_string_len	= app.get_non_sso_string_len();
			
			// Compute the number of multibytes
			app_lut_base_ptr = basic_string::get_lut_base_ptr( app_buffer , app_buffer_size );
			app_lut_active = basic_string::is_lut_active( app_lut_base_ptr );
			if( app_lut_active )
				app_lut_len = basic_string::get_lut_len( app_lut_base_ptr );
			else{
				app_lut_len = 0;
				for( size_type iter = 0 ; iter < app_data_len ; ){
					width_type bytes = get_codepoint_bytes( app_buffer[iter] , app_data_len - iter );
					app_lut_len += bytes > 1; iter += bytes;
				}
			}
		}
		else
		{
			app_lut_active	= false;
			app_string_len	= 0;
			app_buffer		= app.t_sso.data;
			app_buffer_size	= basic_string::get_sso_capacity();
			app_lut_len		= 0;
			for( size_type iter = 0 ; iter < app_data_len ; ){
				width_type bytes = get_codepoint_bytes( app_buffer[iter] , app_data_len - iter );
				app_lut_len += bytes > 1; iter += bytes; ++app_string_len;
			}
		}
		
		// Count codepoints and multibytes of this string
		data_type*	old_buffer;
		data_type*	old_lut_base_ptr; // Ignore uninitialized warning, see [3]
		size_type	old_buffer_size;
		size_type	old_string_len;
		bool		old_lut_active;
		size_type	old_lut_len;
		bool		old_sso_inactive = sso_inactive();
		if( old_sso_inactive )
		{
			old_buffer_size	= t_non_sso.buffer_size;
			old_buffer		= t_non_sso.data;
			old_string_len	= get_non_sso_string_len();
			
			// Count TOTAL multibytes
			old_lut_base_ptr = basic_string::get_lut_base_ptr( old_buffer , old_buffer_size );
			old_lut_active = basic_string::is_lut_active( old_lut_base_ptr );
			if( old_lut_active )
				old_lut_len = basic_string::get_lut_len( old_lut_base_ptr );
			else{
				old_lut_len = 0;
				for( size_type iter	= 0 ; iter < old_data_len ; ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					old_lut_len += bytes > 1; iter += bytes;
				}
			}
		}
		else
		{
			old_buffer		= t_sso.data;
			old_buffer_size	= basic_string::get_sso_capacity();
			old_string_len	= 0;
			old_lut_len		= 0;
			size_type iter	= 0;
			old_lut_active	= false;
			while( iter < old_data_len ){ // Count multibytes and codepoints
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				old_lut_len += bytes > 1; iter += bytes; ++old_string_len;
			}
		}
		
		
		// Compute updated metrics
		size_type	new_lut_len = old_lut_len + app_lut_len;
		size_type	new_string_len = old_string_len + app_string_len;
		size_type	new_buffer_size;
		width_type	new_lut_width; // [2] ; 0 signalizes, that we don't need a lut
		
		// Indices Table worth the memory loss?
		// If the ratio of indices/codepoints is lower 5/8 and we have a LUT -> keep it
		// If we don't have a LUT, it has to drop below 3/8 for us to start one
		if( basic_string::is_lut_worth( new_lut_len , new_string_len , old_lut_active , old_sso_inactive ) )
			new_buffer_size	= determine_main_buffer_size( new_data_len , new_lut_len , &new_lut_width );
		else{
			new_lut_width = 0;
			new_buffer_size = determine_main_buffer_size( new_data_len );
		}
		
		// Can we reuse the old buffer?
		if( new_buffer_size <= old_buffer_size )
		{
			// [3] At this point, 'old_sso_inactive' MUST be true, because else,
			// the resulting string would have sso active (which is handled way above)
			
			// Need to fill the lut? (see [2])
			if( new_lut_width )
			{
				// Make sure, the lut width stays the same, because we still have the same buffer size
				new_lut_width = basic_string::get_lut_width( old_buffer_size );
				
				// Append new INDICES
				data_type*		lut_dest_iter = old_lut_base_ptr - old_lut_len * new_lut_width; // 'old_lut_base_ptr' is initialized as 'old_sso_inactive' is true (see [3])
				if( app_lut_active )
				{
					width_type	app_lut_width = basic_string::get_lut_width( app_buffer_size );
					const data_type*	app_lut_iter = app_lut_base_ptr; // 'app_lut_base_ptr' is initialized as soon as 'app_lut_active' is set to true
					while( app_lut_len-- > 0 )
						basic_string::set_lut(
							lut_dest_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( app_lut_iter -= app_lut_width , app_lut_width ) + old_data_len
						);
				}
				else{
					size_type iter = 0;
					while( iter < app_data_len ){
						width_type bytes = get_codepoint_bytes( app_buffer[iter] , app_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , iter + old_data_len );
						iter += bytes;
					}
				}
				
				// Set new lut mode
				basic_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len );
			}
			else // Set new lut mode
				basic_string::set_lut_indiciator( old_lut_base_ptr , new_lut_len == 0 , 0 );
			
			// Update buffer and data_len
			std::memcpy( old_buffer + old_data_len , app_buffer , app_data_len ); // Copy BUFFER of the insertion
			old_buffer[new_data_len] = '\0'; // Trailing '\0'
		}
		else // No, apparently we have to allocate a new buffer...
		{
			new_buffer_size <<= 1; // Allocate twice as much, in order to amortize allocations (keeping in mind alignment)
			data_type*	new_buffer			= this->allocate(  determine_total_buffer_size( new_buffer_size ) );
			data_type*	new_lut_base_ptr	= basic_string::get_lut_base_ptr( new_buffer , new_buffer_size );
			
			// Write NEW BUFFER
			std::memcpy( new_buffer , old_buffer , old_data_len ); // Copy current BUFFER
			std::memcpy( new_buffer + old_data_len , app_buffer , app_data_len ); // Copy BUFFER of appendix
			new_buffer[new_data_len] = '\0'; // Trailing '\0'
			
			// Need to fill the lut? (see [2])
			if( new_lut_width )
			{
				// Update the lut width, since we doubled the buffer size a couple of lines above
				new_lut_width = basic_string::get_lut_width( new_buffer_size );
				
				// Reuse indices from old lut?
				if( old_lut_active )
				{
					width_type	old_lut_width = basic_string::get_lut_width( old_buffer_size );
				
					// Copy all old INDICES
					if( new_lut_width != old_lut_width )
					{
						data_type*	lut_iter = old_lut_base_ptr;
						data_type*	new_lut_iter = new_lut_base_ptr;
						size_type	num_indices = old_lut_len;
						while( num_indices-- > 0 )
							basic_string::set_lut(
								new_lut_iter -= new_lut_width
								, new_lut_width
								, basic_string::get_lut( lut_iter -= old_lut_width , old_lut_width )
							);
					}
					else // Plain copy of them
						std::memcpy(
							new_lut_base_ptr - old_lut_len * new_lut_width
							, old_lut_base_ptr - old_lut_len * old_lut_width
							, old_lut_len * old_lut_width
						);
				}
				else // We need to fill these indices manually...
				{
					data_type*	new_lut_iter	= new_lut_base_ptr;
					size_type	iter			= 0;
					while( iter < old_data_len ){ // Fill lut with indices BEFORE insertion
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( new_lut_iter -= new_lut_width , new_lut_width , iter );
						iter += bytes;
					}
				}
				
				// Copy INDICES of the insertion
				data_type*		lut_dest_iter = new_lut_base_ptr - old_lut_len * new_lut_width;
				if( app_lut_active )
				{
					width_type	app_lut_width = basic_string::get_lut_width( app_buffer_size );
					const data_type*	app_lut_iter = app_lut_base_ptr;
					while( app_lut_len-- > 0 )
						basic_string::set_lut(
							lut_dest_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( app_lut_iter -= app_lut_width , app_lut_width ) + old_data_len
						);
				}
				else{
					size_type app_iter = 0;
					while( app_iter < app_data_len ){
						width_type bytes = get_codepoint_bytes( app_buffer[app_iter] , app_data_len - app_iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , app_iter + old_data_len );
						app_iter += bytes;
					}
				}
				
				basic_string::set_lut_indiciator( new_lut_base_ptr , true , new_lut_len ); // Set new lut mode and len
			}
			else // Set new lut mode
				basic_string::set_lut_indiciator( new_lut_base_ptr , new_lut_len == 0 , 0 );
		
			// Delete the old buffer?
			if( old_sso_inactive )
				this->deallocate( old_buffer , old_buffer_size );
			
			// Set new Attributes
			t_non_sso.data			= new_buffer;
			t_non_sso.buffer_size	= new_buffer_size;
		}
		
		// Adjust Attributes
		t_non_sso.data_len = new_data_len;
		set_non_sso_string_len( new_string_len );
		
		return *this;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>& basic_string<V, D, A>::raw_insert( typename basic_string<V, D, A>::size_type index , const basic_string<V, D, A>& str ) noexcept(TINY_UTF8_NOEXCEPT)
	{
		// Bound checks...
		size_type old_data_len = size();
		if( index > old_data_len ){
			TINY_UTF8_THROW( "tiny_utf8::basic_string::(raw_)insert" , index > old_data_len );
			return *this;
		}
		
		// Compute the updated metrics
		size_type str_data_len	= str.size();
		size_type new_data_len	= old_data_len + str_data_len;
		
		// Will be empty?
		if( str_data_len == 0 )
			return *this;
		
		// Will be sso string?
		if( new_data_len <= basic_string::get_sso_capacity() )
		{
			// Copy AFTER inserted part, if it has moved in position
			std::memmove( t_sso.data + index + str_data_len , t_sso.data + index , old_data_len - index );
			
			// Copy INSERTION (Note: Since the resulting string is small, the insertion must be small as well!)
			std::memcpy( t_sso.data + index , str.t_sso.data , str_data_len );
			
			// Finish the new string object
			t_sso.data[new_data_len] = '\0'; // Trailing '\0'
			set_sso_data_len( (unsigned char)new_data_len );
			
			return *this;
		}
		
		//! Ok, obviously no small string, we have to update the data, the lut and the number of codepoints
		
		
		// Count codepoints and multibytes of insertion
		bool				str_lut_active;
		const data_type*	str_buffer;
		const data_type*	str_lut_base_ptr;
		size_type			str_buffer_size;
		size_type			str_string_len;
		size_type			str_lut_len;
		if( str.sso_inactive() )
		{
			str_buffer_size	= str.t_non_sso.buffer_size;
			str_buffer			= str.t_non_sso.data;
			str_string_len		= str.get_non_sso_string_len();
			
			// Compute the number of multibytes
			str_lut_base_ptr = basic_string::get_lut_base_ptr( str_buffer , str_buffer_size );
			str_lut_active = basic_string::is_lut_active( str_lut_base_ptr );
			if( str_lut_active )
				str_lut_len = basic_string::get_lut_len( str_lut_base_ptr );
			else{
				str_lut_len = 0;
				for( size_type iter = 0 ; iter < str_data_len ; ){
					width_type bytes = get_codepoint_bytes( str_buffer[iter] , str_data_len - iter );
					str_lut_len += bytes > 1; iter += bytes;
				}
			}
		}
		else
		{
			str_lut_active		= false;
			str_string_len		= 0;
			str_buffer			= str.t_sso.data;
			str_buffer_size	= basic_string::get_sso_capacity();
			str_lut_len		= 0;
			for( size_type iter = 0 ; iter < str_data_len ; ){
				width_type bytes = get_codepoint_bytes( str_buffer[iter] , str_data_len - iter );
				str_lut_len += bytes > 1; iter += bytes; ++str_string_len;
			}
		}
		
		// Count codepoints and multibytes of this string
		data_type*	old_buffer;
		data_type*	old_lut_base_ptr; // Ignore uninitialized warning, see [3]
		size_type	old_buffer_size;
		size_type	old_string_len;
		bool		old_lut_active;
		size_type	mb_index = 0;
		size_type	old_lut_len;
		bool		old_sso_inactive = sso_inactive();
		if( old_sso_inactive )
		{
			old_buffer_size	= t_non_sso.buffer_size;
			old_buffer		= t_non_sso.data;
			old_string_len	= get_non_sso_string_len();
			size_type iter	= 0;
			while( iter < index ){ // Count multibytes and codepoints BEFORE insertion
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				mb_index += bytes > 1; iter += bytes;
			}
			// Count TOTAL multibytes
			old_lut_base_ptr = basic_string::get_lut_base_ptr( old_buffer , old_buffer_size );
			old_lut_active = basic_string::is_lut_active( old_lut_base_ptr );
			if( old_lut_active )
				old_lut_len = basic_string::get_lut_len( old_lut_base_ptr );
			else{
				old_lut_len = mb_index;
				while( iter < old_data_len ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					old_lut_len += bytes > 1; iter += bytes;
				}
			}
		}
		else
		{
			old_buffer		= t_sso.data;
			old_buffer_size	= basic_string::get_sso_capacity();
			old_string_len	= 0;
			old_lut_len		= 0;
			size_type iter	= 0;
			old_lut_active	= false;
			while( iter < index ){ // Count multibytes and codepoints BEFORE insertion
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				mb_index += bytes > 1; iter += bytes; ++old_string_len;
			}
			old_lut_len = mb_index;
			while( iter < old_data_len ){ // Count multibytes and codepoints AFTER insertion
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				old_lut_len += bytes > 1; iter += bytes; ++old_string_len;
			}
		}
		
		
		// Compute updated metrics
		size_type	new_lut_len = old_lut_len + str_lut_len;
		size_type	new_string_len = old_string_len + str_string_len;
		size_type	new_buffer_size;
		width_type	new_lut_width; // [2] ; 0 signalizes, that we don't need a lut
		
		// Indices Table worth the memory loss?
		if( basic_string::is_lut_worth( new_lut_len , new_string_len , old_lut_active , old_sso_inactive ) )
			new_buffer_size	= determine_main_buffer_size( new_data_len , new_lut_len , &new_lut_width );
		else{
			new_lut_width = 0;
			new_buffer_size = determine_main_buffer_size( new_data_len );
		}
		
		// Can we reuse the old buffer?
		if( new_buffer_size <= old_buffer_size )
		{
			// [3] At this point, 'old_sso_inactive' MUST be true, because else,
			// the resulting string would have sso active (which is handled way above)
			
			// Need to fill the lut? (see [2])
			if( new_lut_width )
			{
				// Make sure, the lut width stays the same, because we still have the same buffer size
				new_lut_width = basic_string::get_lut_width( old_buffer_size );
				
				// Reuse indices from old lut?
				if( old_lut_active )
				{
					// Offset all indices
					data_type*	lut_iter = old_lut_base_ptr - mb_index * new_lut_width; // 'old_lut_base_ptr' is initialized as soon as 'old_lut_active' is set to true
					size_type	num_indices = old_lut_len - mb_index;
					while( num_indices-- > 0 ){
						lut_iter -= new_lut_width;
						basic_string::set_lut( lut_iter , new_lut_width , basic_string::get_lut( lut_iter , new_lut_width ) + str_data_len );
					}
					
					// Copy INDICES from AFTER insertion
					// We only need to copy them, if the number of multibytes in the inserted part has changed
					if( str_lut_len )
					{
						// Move the indices!
						std::memmove(
							old_lut_base_ptr - new_lut_len * new_lut_width
							, old_lut_base_ptr - old_lut_len * new_lut_width
							, ( old_lut_len - mb_index ) * new_lut_width
						);
						basic_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set new lut size
					}
				}
				else // We need to fill the lut manually...
				{
					// Fill INDICES BEFORE insertion
					size_type	iter		= 0;
					data_type*	lut_iter	= old_lut_base_ptr;
					while( iter < index ){ // Fill lut with indices BEFORE insertion
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
						iter += bytes;
					}
					
					// Fill INDICES AFTER insertion
					lut_iter -= str_lut_len * new_lut_width;
					while( iter < old_data_len ){
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + str_data_len );
						iter += bytes;
					}
					
					basic_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set lut size
				}
				
				// Copy INDICES of the insertion
				data_type*		lut_dest_iter = old_lut_base_ptr - mb_index * new_lut_width;
				if( str_lut_active )
				{
					width_type			str_lut_width = basic_string::get_lut_width( str_buffer_size );
					const data_type*	str_lut_iter = str_lut_base_ptr; // 'str_lut_base_ptr' is initialized as soon as 'str_lut_active' is set to true
					while( str_lut_len-- > 0 )
						basic_string::set_lut(
							lut_dest_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( str_lut_iter -= str_lut_width , str_lut_width ) + index
						);
				}
				else{
					size_type iter = 0;
					while( iter < str_data_len ){
						width_type bytes = get_codepoint_bytes( str_buffer[iter] , str_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , iter + index );
						iter += bytes;
					}
				}
			}
			else // Set new lut mode
				basic_string::set_lut_indiciator( old_lut_base_ptr , new_lut_len == 0 , 0 );
			
			// Move BUFFER from AFTER the insertion (Note: We don't need to copy before it, because the buffer hasn't changed)
			std::memmove( old_buffer + index + str_data_len , old_buffer + index , old_data_len - index );
			old_buffer[new_data_len] = '\0'; // Trailing '\0'
			
			// Copy BUFFER of the insertion
			std::memcpy( old_buffer + index , str_buffer , str_data_len );
		}
		else // No, apparently we have to allocate a new buffer...
		{
			new_buffer_size <<= 1; // Allocate twice as much, in order to amortize allocations (keeping in mind alignment)
			data_type*	new_buffer			= this->allocate(  determine_total_buffer_size( new_buffer_size ) );
			data_type*	new_lut_base_ptr	= basic_string::get_lut_base_ptr( new_buffer , new_buffer_size );
			
			// Copy BUFFER from BEFORE insertion
			std::memcpy( new_buffer , old_buffer , index );
			
			// Copy BUFFER of insertion
			std::memcpy( new_buffer + index , str_buffer , str_data_len );
			
			// Copy BUFFER from AFTER the insertion
			std::memcpy( new_buffer + index + str_data_len , old_buffer + index , old_data_len - index );
			new_buffer[new_data_len] = '\0'; // Trailing '\0'
			
			// Need to fill the lut? (see [2])
			if( new_lut_width )
			{
				// Update the lut width, since we doubled the buffer size a couple of lines above
				new_lut_width = basic_string::get_lut_width( new_buffer_size );
				
				// Reuse indices from old lut?
				if( old_lut_active )
				{
					width_type	old_lut_width = basic_string::get_lut_width( old_buffer_size );
					
					// Copy all INDICES BEFORE the insertion
					if( new_lut_width != old_lut_width )
					{
						data_type*		lut_iter = old_lut_base_ptr;
						data_type*		new_lut_iter = new_lut_base_ptr;
						size_type	num_indices = mb_index;
						while( num_indices-- > 0 )
							basic_string::set_lut(
								new_lut_iter -= new_lut_width
								, new_lut_width
								, basic_string::get_lut( lut_iter -= old_lut_width , old_lut_width )
							);
					}
					else // Plain copy of them
						std::memcpy(
							new_lut_base_ptr - mb_index * new_lut_width
							, old_lut_base_ptr - mb_index * old_lut_width
							, mb_index * old_lut_width
						);
					
					// Copy all INDICES AFTER the insertion
					// Need to offset all indices or translate the lut width? (This can be, if the insertion data has different size as the inserted data)
					data_type*	lut_iter = old_lut_base_ptr - mb_index * old_lut_width;
					data_type*	new_lut_iter = new_lut_base_ptr - ( mb_index + str_lut_len ) * new_lut_width;
					size_type	num_indices = old_lut_len - mb_index;
					while( num_indices-- > 0 )
						basic_string::set_lut(
							new_lut_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( lut_iter -= old_lut_width , old_lut_width ) + str_data_len
						);
				}
				else // We need to fill the lut manually...
				{
					// Fill INDICES BEFORE insertion
					size_type	iter		= 0;
					data_type*		lut_iter	= new_lut_base_ptr;
					while( iter < index ){ // Fill lut with indices BEFORE insertion
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
						iter += bytes;
					}
					
					// Fill INDICES AFTER insertion
					lut_iter -= str_lut_len * new_lut_width;
					while( iter < old_data_len ){
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + str_data_len );
						iter += bytes;
					}
				}
				
				// Copy INDICES of the insertion
				data_type*		lut_dest_iter = new_lut_base_ptr - mb_index * new_lut_width;
				if( str_lut_active )
				{
					width_type			str_lut_width = basic_string::get_lut_width( str_buffer_size );
					const data_type*	str_lut_iter = str_lut_base_ptr;
					while( str_lut_len-- > 0 )
						basic_string::set_lut(
							lut_dest_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( str_lut_iter -= str_lut_width , str_lut_width ) + index
						);
				}
				else{
					size_type str_iter = 0;
					while( str_iter < str_data_len ){
						width_type bytes = get_codepoint_bytes( str_buffer[str_iter] , str_data_len - str_iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , str_iter + index );
						str_iter += bytes;
					}
				}
				
				basic_string::set_lut_indiciator( new_lut_base_ptr , true , new_lut_len ); // Set new lut mode and len
			}
			else // Set new lut mode
				basic_string::set_lut_indiciator( new_lut_base_ptr , new_lut_len == 0 , 0 );
		
			// Delete the old buffer?
			if( old_sso_inactive )
				this->deallocate( old_buffer , old_buffer_size );
			
			// Set new Attributes
			t_non_sso.data		= new_buffer;
			t_non_sso.buffer_size = new_buffer_size;
		}
		
		// Adjust Attributes
		t_non_sso.data_len	= new_data_len;
		set_non_sso_string_len( new_string_len );
		
		return *this;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>& basic_string<V, D, A>::raw_replace( typename basic_string<V, D, A>::size_type index , typename basic_string<V, D, A>::size_type replaced_len , const basic_string<V, D, A>& repl ) noexcept(TINY_UTF8_NOEXCEPT)
	{
		// Bound checks...
		size_type old_data_len = size();
		if( index > old_data_len ){
			TINY_UTF8_THROW( "tiny_utf8::basic_string::(raw_)replace" , index > old_data_len );
			return *this;
		}
		size_type end_index = index + replaced_len;
		if( end_index > old_data_len || end_index < index ){ // 'end_index < index' is needed because of potential integer overflow in sum
			end_index = old_data_len;
			replaced_len = end_index - index;
		}
		
		// Compute the updated metrics
		size_type		repl_data_len	= repl.size();
		difference_type delta_len		= repl_data_len - replaced_len;
		size_type		new_data_len	= old_data_len + delta_len;
		
		// Will be empty?
		if( !new_data_len ){
			clear();
			return *this;
		}
		else if( replaced_len == 0 && repl_data_len == 0 )
			return *this;
		
		// Will be sso string?
		bool old_sso_inactive = sso_inactive();
		if( new_data_len <= basic_string::get_sso_capacity() )
		{
			// Was the buffer on the heap and has to be moved now?
			if( old_sso_inactive )
			{
				data_type*	old_buffer = t_non_sso.data; // Backup old buffer, since we override the pointer to it (see [1])
				size_type	old_buffer_size = t_non_sso.buffer_size; // Backup old buffer size, since we override the pointer to it (see [1])
				
				// Copy BEFORE replaced part
				std::memcpy( t_sso.data , old_buffer , index ); // [1]
				
				// Copy AFTER replaced part
				std::memcpy( t_sso.data + index + repl_data_len , old_buffer + end_index , old_data_len - end_index );
				
				this->deallocate( old_buffer , old_buffer_size ); // Delete the old buffer
			}
			// Copy AFTER replaced part, if it has moved in position
			else if( new_data_len != old_data_len )
				std::memmove( t_sso.data + index + repl_data_len , t_sso.data + index + replaced_len , old_data_len - index );
			
			// Copy REPLACEMENT (Note: Since the resulting string is small, the replacement must be small as well!)
			std::memcpy( t_sso.data + index , repl.t_sso.data , repl_data_len );
			
			// Finish the new string object
			t_sso.data[new_data_len] = '\0'; // Trailing '\0'
			set_sso_data_len( (unsigned char)new_data_len );
			
			return *this;
		}
		
		//! Ok, obviously no small string, we have to update the data, the lut and the number of codepoints
		
		// Count codepoints and multibytes of replacement
		bool				repl_lut_active;
		const data_type*	repl_buffer;
		const data_type*	repl_lut_base_ptr;
		size_type			repl_buffer_size;
		size_type			repl_string_len;
		size_type			repl_lut_len;
		if( repl.sso_inactive() )
		{
			repl_buffer_size	= repl.t_non_sso.buffer_size;
			repl_buffer			= repl.t_non_sso.data;
			repl_string_len		= repl.get_non_sso_string_len();
			
			// Compute the number of multibytes
			repl_lut_base_ptr = basic_string::get_lut_base_ptr( repl_buffer , repl_buffer_size );
			repl_lut_active = basic_string::is_lut_active( repl_lut_base_ptr );
			if( repl_lut_active )
				repl_lut_len = basic_string::get_lut_len( repl_lut_base_ptr );
			else{
				repl_lut_len = 0;
				for( size_type iter = 0 ; iter < repl_data_len ; ){
					width_type bytes = get_codepoint_bytes( repl_buffer[iter] , repl_data_len - iter );
					repl_lut_len += bytes > 1; iter += bytes;
				}
			}
		}
		else
		{
			repl_lut_active		= false;
			repl_string_len		= 0;
			repl_buffer			= repl.t_sso.data;
			repl_buffer_size	= basic_string::get_sso_capacity();
			repl_lut_len		= 0;
			for( size_type iter = 0 ; iter < repl_data_len ; ){
				width_type bytes = get_codepoint_bytes( repl_buffer[iter] , repl_data_len - iter );
				repl_lut_len += bytes > 1; iter += bytes; ++repl_string_len;
			}
		}
		
		// Count codepoints and multibytes of this string
		data_type*	old_buffer;
		data_type*	old_lut_base_ptr; // Ignore uninitialized warning, see [3]
		size_type	old_buffer_size;
		size_type	old_string_len;
		bool		old_lut_active;
		size_type	mb_index = 0;
		size_type	replaced_mbs = 0;
		size_type	replaced_cps = 0;
		size_type	old_lut_len;
		if( old_sso_inactive )
		{
			old_buffer_size	= t_non_sso.buffer_size;
			old_buffer		= t_non_sso.data;
			old_string_len	= get_non_sso_string_len();
			size_type iter	= 0;
			while( iter < index ){ // Count multibytes and codepoints BEFORE replacement
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				mb_index += bytes > 1; iter += bytes;
			}
			while( iter < end_index ){ // Count REPLACED multibytes and codepoints
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				replaced_mbs += bytes > 1; iter += bytes; ++replaced_cps;
			}
			// Count TOTAL multibytes
			old_lut_base_ptr = basic_string::get_lut_base_ptr( old_buffer , old_buffer_size );
			old_lut_active = basic_string::is_lut_active( old_lut_base_ptr );
			if( old_lut_active )
				old_lut_len = basic_string::get_lut_len( old_lut_base_ptr );
			else{
				old_lut_len = mb_index + replaced_mbs;
				while( iter < old_data_len ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					old_lut_len += bytes > 1; iter += bytes;
				}
			}
		}
		else
		{
			old_buffer		= t_sso.data;
			old_buffer_size	= basic_string::get_sso_capacity();
			old_string_len	= 0;
			old_lut_len		= 0;
			size_type iter	= 0;
			old_lut_active	= false;
			while( iter < index ){ // Count multibytes and codepoints BEFORE replacement
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				mb_index += bytes > 1; iter += bytes; ++old_string_len;
			}
			while( iter < end_index ){ // Count REPLACED multibytes and codepoints
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				replaced_mbs += bytes > 1; iter += bytes; ++replaced_cps;
			}
			old_lut_len = mb_index + replaced_mbs;
			while( iter < old_data_len ){ // Count multibytes and codepoints AFTER replacement
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				old_lut_len += bytes > 1; iter += bytes; ++old_string_len;
			}
			old_string_len	+= replaced_cps;
		}
		
		
		// Compute updated metrics
		size_type	new_lut_len = old_lut_len - replaced_mbs + repl_lut_len;
		size_type	new_string_len = old_string_len - replaced_cps + repl_string_len;
		size_type	new_buffer_size;
		width_type	new_lut_width; // [2] ; 0 signalizes, that we don't need a lut
		
		
		// Indices Table worth the memory loss?
		if( basic_string::is_lut_worth( new_lut_len , new_string_len , old_lut_active , old_sso_inactive ) )
			new_buffer_size	= determine_main_buffer_size( new_data_len , new_lut_len , &new_lut_width );
		else{
			new_lut_width = 0;
			new_buffer_size = determine_main_buffer_size( new_data_len );
		}
		
		// Can we reuse the old buffer?
		if( new_buffer_size <= old_buffer_size )
		{
			// [3] At this point, 'old_sso_inactive' MUST be true, because else,
			// the resulting string would have sso active (which is handled way above)
			
			// Need to fill the lut? (see [2])
			if( new_lut_width )
			{
				// Make sure, the lut width stays the same, because we still have the same buffer size
				new_lut_width = basic_string::get_lut_width( old_buffer_size );
				
				// Reuse indices from old lut?
				if( old_lut_active )
				{
					size_type	mb_end_index = mb_index + replaced_mbs;
					
					// Need to offset all indices? (This can be, if the replacement data has different size as the replaced data)
					if( delta_len ){
						data_type*	lut_iter = old_lut_base_ptr - mb_end_index * new_lut_width; // 'old_lut_base_ptr' is initialized as soon as 'old_lut_active' is set to true
						size_type	num_indices = old_lut_len - mb_end_index;
						while( num_indices-- > 0 ){
							lut_iter -= new_lut_width;
							basic_string::set_lut( lut_iter , new_lut_width , basic_string::get_lut( lut_iter , new_lut_width ) + delta_len );
						}
					}
					
					// Copy INDICES from AFTER replacement
					// We only need to copy them, if the number of multibytes in the replaced part has changed
					if( replaced_mbs != repl_lut_len )
					{
						// Move the indices!
						std::memmove(
							old_lut_base_ptr - new_lut_len * new_lut_width
							, old_lut_base_ptr - old_lut_len * new_lut_width
							, ( old_lut_len - mb_end_index ) * new_lut_width
						);
						
						basic_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set new lut size
					}
				}
				else // We need to fill the lut manually...
				{
					// Fill INDICES BEFORE replacement
					size_type	iter		= 0;
					data_type*	lut_iter	= old_lut_base_ptr;
					while( iter < index ){ // Fill lut with indices BEFORE replacement
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
						iter += bytes;
					}
					
					// Fill INDICES AFTER replacement
					iter += replaced_len;
					lut_iter -= repl_lut_len * new_lut_width;
					while( iter < old_data_len ){
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + delta_len );
						iter += bytes;
					}
					
					basic_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set lut size
				}
				
				// Copy INDICES of the replacement
				data_type*		lut_dest_iter = old_lut_base_ptr - mb_index * new_lut_width;
				if( repl_lut_active )
				{
					width_type	repl_lut_width = basic_string::get_lut_width( repl_buffer_size );
					const data_type*	repl_lut_iter = repl_lut_base_ptr; // 'repl_lut_base_ptr' is initialized as soon as repl_lut_active' is set to true
					while( repl_lut_len-- > 0 )
						basic_string::set_lut(
							lut_dest_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( repl_lut_iter -= repl_lut_width , repl_lut_width ) + index
						);
				}
				else{
					size_type iter = 0;
					while( iter < repl_data_len ){
						width_type bytes = get_codepoint_bytes( repl_buffer[iter] , repl_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , iter + index );
						iter += bytes;
					}
				}
			}
			else // Set new lut mode
				basic_string::set_lut_indiciator( old_lut_base_ptr , new_lut_len == 0 , 0 );
			
			// Move BUFFER from AFTER the replacement (Note: We don't need to copy before it, because the buffer hasn't changed)
			if( new_data_len != old_data_len ){
				std::memmove( old_buffer + index + repl_data_len , old_buffer + end_index , old_data_len - end_index );
				t_non_sso.data_len = new_data_len;
				old_buffer[new_data_len] = '\0'; // Trailing '\0'
			}
			
			// Copy BUFFER of the replacement
			std::memcpy( old_buffer + index , repl_buffer , repl_data_len );
		}
		else // No, apparently we have to allocate a new buffer...
		{
			new_buffer_size <<= 1; // Allocate twice as much, in order to amortize allocations (keeping in mind alignment)
			data_type*	new_buffer			= this->allocate( determine_total_buffer_size( new_buffer_size ) );
			data_type*	new_lut_base_ptr	= basic_string::get_lut_base_ptr( new_buffer , new_buffer_size );
			
			// Copy BUFFER from BEFORE replacement
			std::memcpy( new_buffer , old_buffer , index );
			
			// Copy BUFFER of replacement
			std::memcpy( new_buffer + index , repl_buffer , repl_data_len );
			
			// Copy BUFFER from AFTER the replacement
			std::memcpy( new_buffer + index + repl_data_len , old_buffer + end_index , old_data_len - end_index );
			new_buffer[new_data_len] = '\0'; // Trailing '\0'
			
			// Need to fill the lut? (see [2])
			if( new_lut_width )
			{
				// Update the lut width, since we doubled the buffer size a couple of lines above
				new_lut_width = basic_string::get_lut_width( new_buffer_size );
				
				// Reuse indices from old lut?
				if( old_lut_active )
				{
					size_type	mb_end_index = mb_index + replaced_mbs;
					width_type	old_lut_width = basic_string::get_lut_width( old_buffer_size );
					
					// Copy all INDICES BEFORE the replacement
					if( new_lut_width != old_lut_width )
					{
						data_type*	lut_iter = old_lut_base_ptr;
						data_type*	new_lut_iter = new_lut_base_ptr;
						size_type	num_indices = mb_index;
						while( num_indices-- > 0 )
							basic_string::set_lut(
								new_lut_iter -= new_lut_width
								, new_lut_width
								, basic_string::get_lut( lut_iter -= old_lut_width , old_lut_width )
							);
					}
					else // Plain copy of them
						std::memcpy(
							new_lut_base_ptr - mb_index * new_lut_width
							, old_lut_base_ptr - mb_index * old_lut_width
							, mb_index * old_lut_width
						);
					
					// Copy all INDICES AFTER the replacement
					// Need to offset all indices or translate the lut width? (This can be, if the replacement data has different size as the replaced data)
					if( delta_len || new_lut_width != old_lut_width ){ // [Optimization possible here]
						data_type*	lut_iter = old_lut_base_ptr - mb_end_index * old_lut_width;
						data_type*	new_lut_iter = new_lut_base_ptr - ( mb_index + repl_lut_len ) * new_lut_width;
						size_type	num_indices = old_lut_len - mb_end_index;
						while( num_indices-- > 0 )
							basic_string::set_lut(
								new_lut_iter -= new_lut_width
								, new_lut_width
								, basic_string::get_lut( lut_iter -= old_lut_width , old_lut_width ) + delta_len
							);
					}
					else // Plain copy of them
						std::memcpy(
							new_lut_base_ptr - new_lut_len * new_lut_width
							, old_lut_base_ptr - old_lut_len * old_lut_width
							, ( old_lut_len - mb_end_index ) * old_lut_width
						);
				}
				else // We need to fill the lut manually...
				{
					// Fill INDICES BEFORE replacement
					size_type	iter		= 0;
					data_type*	lut_iter	= new_lut_base_ptr;
					while( iter < index ){ // Fill lut with indices BEFORE replacement
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
						iter += bytes;
					}
					
					// Fill INDICES AFTER replacement
					iter += replaced_len;
					lut_iter -= repl_lut_len * new_lut_width;
					while( iter < old_data_len ){
						width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + delta_len );
						iter += bytes;
					}
				}
				
				// Copy INDICES of the replacement
				data_type*		lut_dest_iter = new_lut_base_ptr - mb_index * new_lut_width;
				if( repl_lut_active )
				{
					width_type			repl_lut_width = basic_string::get_lut_width( repl_buffer_size );
					const data_type*	repl_lut_iter = repl_lut_base_ptr;
					while( repl_lut_len-- > 0 )
						basic_string::set_lut(
							lut_dest_iter -= new_lut_width
							, new_lut_width
							, basic_string::get_lut( repl_lut_iter -= repl_lut_width , repl_lut_width ) + index
						);
				}
				else{
					size_type repl_iter = 0; // Todo
					while( repl_iter < repl_data_len ){
						width_type bytes = get_codepoint_bytes( repl_buffer[repl_iter] , repl_data_len - repl_iter );
						if( bytes > 1 )
							basic_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , repl_iter + index );
						repl_iter += bytes;
					}
				}
				
				basic_string::set_lut_indiciator( new_lut_base_ptr , true , new_lut_len ); // Set new lut mode and len
			}
			else // Set new lut mode
				basic_string::set_lut_indiciator( new_lut_base_ptr , new_lut_len == 0 , 0 );
		
			// Delete the old buffer?
			if( old_sso_inactive )
				this->deallocate( old_buffer , old_buffer_size );
			
			// Set new Attributes
			t_non_sso.data		= new_buffer;
			t_non_sso.data_len	= new_data_len;
			t_non_sso.buffer_size = new_buffer_size;
		}
		
		// Adjust string length
		set_non_sso_string_len( new_string_len );
		
		return *this;
	}

	template<typename V, typename D, typename A>
	basic_string<V, D, A>& basic_string<V, D, A>::raw_erase( typename basic_string<V, D, A>::size_type index , typename basic_string<V, D, A>::size_type len ) noexcept(TINY_UTF8_NOEXCEPT)
	{
		// Bound checks...
		size_type old_data_len = size();
		if( index > old_data_len ){
			TINY_UTF8_THROW( "tiny_utf8::basic_string::(raw_)erase" , index > old_data_len );
			return *this;
		}
		if( !len )
			return *this;
		size_type		end_index = index + len;
		if( end_index > old_data_len || end_index < index ){ // 'end_index < index' is needed because of potential integer overflow in sum
			end_index = old_data_len;
			len = end_index - index;
		}
		
		// Compute the updated metrics
		size_type		new_data_len = old_data_len - len;
		
		// Will be empty?
		if( !new_data_len ){
			clear();
			return *this;
		}
		
		// Will be sso string?
		bool old_sso_inactive = sso_inactive();
		if( new_data_len <= basic_string::get_sso_capacity() )
		{
			// Was the buffer on the heap and has to be moved now?
			if( old_sso_inactive )
			{
				data_type*	old_buffer = t_non_sso.data; // Backup old buffer, since we override the pointer to it (see [1])
				size_type	old_buffer_size = t_non_sso.buffer_size; // Backup old buffer size, since we override the pointer to it (see [1])
				
				// Copy BEFORE replaced part
				std::memcpy( t_sso.data , old_buffer , index ); // [1]
				
				// Copy AFTER replaced part
				std::memcpy( t_sso.data + index , old_buffer + end_index , old_data_len - end_index );
				
				this->deallocate( old_buffer , old_buffer_size ); // Delete the old buffer
			}
			// Move the part AFTER the removal
			else
				std::memmove( t_sso.data + index , t_sso.data + index + len , old_data_len - index );
			
			// Finish the new string object
			t_sso.data[new_data_len] = '\0'; // Trailing '\0'
			set_sso_data_len( (unsigned char)new_data_len );
			
			return *this;
		}
		
		//! Ok, obviously no small string, we have to update the data, the lut and the number of codepoints.
		//! BUT: We will keep the lut in the mode it is: inactive stay inactive, active stays active
		
		// Count codepoints and multibytes of this string
		data_type*	old_buffer = t_non_sso.data;
		size_type	old_buffer_size = t_non_sso.buffer_size;
		data_type*	old_lut_base_ptr = basic_string::get_lut_base_ptr( old_buffer , old_buffer_size );
		bool		old_lut_active = basic_string::is_lut_active( old_lut_base_ptr );
		size_type	replaced_cps = 0;
		
		// Adjust data length
		t_non_sso.data_len -= len;
		
		// Was the lut active? => Keep it active and therefore update the data, the string length and the lut
		if( old_lut_active )
		{
			size_type	old_lut_len = basic_string::get_lut_len( old_lut_base_ptr );
			width_type	old_lut_width = basic_string::get_lut_width( old_buffer_size );
			size_type	mb_end_index = 0;
			size_type	replaced_mbs = 0;
			size_type	iter	= 0;
			while( iter < index ){ // Count multibytes and codepoints BEFORE erased part
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				mb_end_index += bytes > 1; iter += bytes;
			}
			while( iter < end_index ){ // Count REPLACED multibytes and codepoints
				width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				replaced_mbs += bytes > 1; iter += bytes; ++replaced_cps;
			}
			mb_end_index += replaced_mbs;
			
			// Offset all indices
			data_type*	lut_iter = old_lut_base_ptr - mb_end_index * old_lut_width;
			size_type	num_indices = old_lut_len - mb_end_index;
			while( num_indices-- > 0 ){
				lut_iter -= old_lut_width;
				basic_string::set_lut( lut_iter , old_lut_width , basic_string::get_lut( lut_iter , old_lut_width ) - len );
			}
			
			// Copy INDICES AFTER erased part
			// We only need to move them, if the number of multibytes in the replaced part has changed
			if( replaced_mbs )
			{
				size_type	new_lut_len = old_lut_len - replaced_mbs;
				// Move the indices!
				std::memmove(
					old_lut_base_ptr - new_lut_len * old_lut_width
					, old_lut_base_ptr - old_lut_len * old_lut_width
					, ( old_lut_len - mb_end_index ) * old_lut_width
				);
				
				basic_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set new lut size
			}
		}
		// The lut was inactive => only update the string length
		else{
			size_type iter = 0;
			while( iter < index )
				iter += get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			while( iter < end_index ){
				iter += get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
				++replaced_cps;
			}
		}
		
		// Move BUFFER AFTER the erased part forward
		std::memmove( old_buffer + index , old_buffer + end_index , old_data_len - end_index + 1 ); // +1 for the trailing '\0'
		
		// Adjust string length
		set_non_sso_string_len( get_non_sso_string_len() - replaced_cps );
		
		return *this;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::raw_rfind( typename basic_string<V, D, A>::value_type cp , typename basic_string<V, D, A>::size_type index ) const noexcept {
		if( index >= size() )
			index = raw_back_index();
		for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) )
			if( raw_at(it) == cp )
				return it;
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::find_first_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type start_pos ) const noexcept
	{
		if( start_pos >= length() )
			return basic_string::npos;
		
		for( const_iterator it = get( start_pos ), end = cend() ; it < end ; ++it, ++start_pos )
		{
			const value_type*	tmp = str;
			value_type			cur = *it;
			do{
				if( cur == *tmp )
					return start_pos;
			}while( *++tmp );
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::raw_find_first_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type index ) const noexcept
	{
		if( index >= size() )
			return basic_string::npos;
		
		for( const_iterator it = raw_get(index), end = cend() ; it < end ; ++it )
		{
			const value_type*	tmp = str;
			value_type			cur = *it;
			do{
				if( cur == *tmp )
					return it.get_index();
			}while( *++tmp );
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::find_last_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type start_pos ) const noexcept
	{
		const_reverse_iterator  it;
		size_type               string_len = length();
		if( start_pos >= string_len ){
			it = crbegin();
			start_pos = string_len - 1;
		}
		else
			it = rget( start_pos );
		
		for( const_reverse_iterator rend = crend() ; it != rend ; ++it, --start_pos ){
			const value_type*	tmp = str;
			value_type			cur = *it;
			do{
				if( cur == *tmp )
					return start_pos;
			}while( *++tmp );
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::raw_find_last_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type index ) const noexcept
	{
		if( empty() )
			return basic_string::npos;
		
		if( index >= size() )
			index = raw_back_index();
		
		for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) ){
			const value_type*	tmp = str;
			value_type			cur = raw_at(it);
			do{
				if( cur == *tmp )
					return it;
			}while( *++tmp );
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::find_first_not_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type start_pos ) const noexcept
	{
		if( start_pos >= length() )
			return basic_string::npos;
		
		for( const_iterator it = get(start_pos) , end = cend() ; it != end ; ++it, ++start_pos ){
			const value_type*	tmp = str;
			value_type			cur = *it;
			do{
				if( cur == *tmp )
					goto continue2;
			}while( *++tmp );
			return start_pos;
			continue2:;
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::raw_find_first_not_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type index ) const noexcept
	{
		if( index >= size() )
			return basic_string::npos;
		
		for( const_iterator it = raw_get(index), end = cend() ; it < end ; ++it )
		{
			const value_type*	tmp = str;
			value_type			cur = *it;
			do{
				if( cur == *tmp )
					goto continue2;
			}while( *++tmp );
			return it.get_index();
			continue2:;
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::find_last_not_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type start_pos ) const noexcept
	{
		if( empty() )
			return basic_string::npos;
		
		const_reverse_iterator  end = rend(), it;
		size_type               string_len = length();
		if( start_pos >= string_len ){
			it = crbegin();
			start_pos = string_len - 1;
		}
		else
			it = rget( start_pos );
		
		for( ; it < end ; ++it, --start_pos ){
			const value_type*	tmp = str;
			value_type			cur = *it;
			do{
				if( cur == *tmp )
					goto continue2;
			}while( *++tmp );
			return start_pos;
			continue2:;
		}
		
		return basic_string::npos;
	}

	template<typename V, typename D, typename A>
	typename basic_string<V, D, A>::size_type basic_string<V, D, A>::raw_find_last_not_of( const typename basic_string<V, D, A>::value_type* str , typename basic_string<V, D, A>::size_type index ) const noexcept
	{
		if( empty() )
			return basic_string::npos;
		
		if( index >= size() )
			index = raw_back_index();
		
		for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) )
		{
			const value_type*	tmp = str;
			value_type			cur = raw_at(it);
			
			do{
				if( cur == *tmp )
					goto continue2;
			}while( *++tmp );
			
			return it; // It will be either non-negative or -1, which equals basic_string::npos!
			
			continue2:;
		}
		
		return basic_string::npos;
	}

} // Namespace 'tiny_utf8'

#if defined (__clang__)
#pragma clang diagnostic pop
#elif defined (__GNUC__)
#pragma GCC diagnostic pop
#elif defined (_MSC_VER)
#pragma warning(pop)
#endif

#endif // _TINY_UTF8_H_
