// Copyright (c) 2018 Jakob Riedle (DuffsDevice)
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products
//    derived from this software without specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE AUTHOR 'AS IS' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
// THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef _TINY_UTF8_H_
#define _TINY_UTF8_H_

// Includes
#include <memory> // for std::unique_ptr
#include <cstring> // for std::memcpy, std::memmove
#include <string> // for std::string
#include <limits> // for numeric_limits
#include <cstddef> // for ptrdiff_t, size_t and offsetof
#include <cstdint> // for uint8_t, uint16_t, uint32_t, std::uint_least16_t, std::uint_fast32_t
#include <stdexcept> // for std::out_of_range
#include <initializer_list> // for std::initializer_list
#include <iosfwd> // for ostream/istream forward declarations
#ifdef _MSC_VER
#include <intrin.h> // for __lzcnt
#endif


namespace detail
{
	// Used for tag dispatching in constructor
	struct read_codepoints_tag{};
	struct read_bytes_tag{};
	
	//! Count leading zeros utility
	#if defined(__GNUC__)
	#define _TINY_UTF8_H_HAS_CLZ_ true
	static inline unsigned int clz( unsigned int val ){ return __builtin_clz( val ); }
	static inline unsigned int clz( unsigned long int val ){ return __builtin_clzl( val ); }
	static inline unsigned int clz( char32_t val ){
		return sizeof(char32_t) == sizeof(unsigned long int) ? __builtin_clzl( val ) : __builtin_clz( val );
	}
	#elif defined(_MSC_VER)
	#define _TINY_UTF8_H_HAS_CLZ_ true
	static inline unsigned int clz( uint16_t val ){ return __lzcnt16( val ); }
	static inline unsigned int clz( uint32_t val ){ return __lzcnt( val ); }
	#ifndef WIN32
	static inline unsigned int clz( uint64_t val ){ return __lzcnt64(val); }
	#endif // WIN32
	static inline unsigned int clz( char32_t val ){ return __lzcnt( val ); }
	#endif
	
	//! Helper to detect little endian
	class is_little_endian
	{
		constexpr static uint32_t	u4 = 1;
		constexpr static uint8_t	u1 = (const uint8_t &) u4;
	public:
		constexpr static bool value = u1;
	};
	
	//! Helper to modify the last (address-wise) byte of a little endian value of type 'T'
	template<typename T>
	union last_byte
	{
		T			number;
		struct{
			char	dummy[sizeof(T)-1];
			char	last;
		} bytes;
	};
}

class utf8_string
{
public:
	
	typedef std::size_t					size_type;
	typedef std::ptrdiff_t				difference_type;
	typedef char32_t					value_type;
	typedef value_type					const_reference;
	typedef std::uint_fast8_t			width_type; // Data type capable of holding the number of code units in a code point
	constexpr const static size_type	npos = -1;
	
private: //! Layout specifications
	
	/*
	 * To determine, which layout is active, read either t_sso.data_len, or the last byte of t_non_sso.buffer_size:
	 * LSB == 0  =>  SSO
	 * LSB == 1  =>  NON-SSO
	 */
	
	// Layout used, if sso is inactive
	struct NON_SSO
	{
		char*			data; // Points to [ <data::char>... | '0'::char | <index::rle>... | <lut_indicator::size_type> ]
		size_type		data_len; // In bytes, excluding the trailing '\0'
		size_type		buffer_size; // Indicates the size of '::data' minus 'lut_width'
		size_type		string_len; // Shadows data_len on the last byte
	};
	
	// Layout used, if sso is active
	struct SSO
	{
		char			data[sizeof(NON_SSO)-1];
		unsigned char	data_len; // This field holds ( sizeof(SSO::data) - num_characters ) << 1
		SSO( unsigned char data_len , char value ) :
			data{ value }
			, data_len( ( sizeof(SSO::data) - data_len ) << 1 )
		{}
		SSO( unsigned char data_len ) :
			data_len( ( sizeof(SSO::data) - data_len ) << 1 )
		{}
	};
	
	// Typedef for the lut indicator. Note: Don't change this, because else the buffer will not be a multiple of sizeof(size_type)
	typedef size_type					indicator_type;
	
private:
	
	template<bool RangeCheck>
	class utf8_codepoint_reference
	{
	private:
		
		size_type		t_index;
		utf8_string*	t_instance;
		
	public:
		
		//! Ctor
		utf8_codepoint_reference( size_type index , utf8_string* instance ) :
			t_index( index )
			, t_instance( instance )
		{}
		
		//! Cast to wide char
		operator value_type() const {
			if( RangeCheck )
				return static_cast<const utf8_string*>(t_instance)->at( t_index );
			return static_cast<const utf8_string*>(t_instance)->at( t_index , std::nothrow );
		}
		
		//! Assignment operator
		utf8_codepoint_reference& operator=( value_type cp ){
			t_instance->replace( t_index , cp );
			return *this;
		}
	};
	
	template<bool RangeCheck>
	class utf8_codepoint_raw_reference
	{
	private:
		
		size_type		t_raw_index;
		utf8_string*	t_instance;
		
	public:
		
		//! Ctor
		utf8_codepoint_raw_reference( size_type raw_index , utf8_string* instance ) :
			t_raw_index( raw_index )
			, t_instance( instance )
		{}
		
		//! Cast to wide char
		operator value_type() const {
			if( RangeCheck )
				return static_cast<const utf8_string*>(t_instance)->raw_at( t_raw_index );
			return static_cast<const utf8_string*>(t_instance)->raw_at( t_raw_index , std::nothrow );
		}
		
		//! Assignment operator
		utf8_codepoint_raw_reference& operator=( value_type cp ){
			t_instance->raw_replace( t_raw_index , t_instance->get_index_bytes( t_raw_index ) , utf8_string( cp ) );
			return *this;
		}
	};
	
	class iterator_base
	{
		friend class utf8_string;
	
	public:
		
		typedef utf8_string::value_type				value_type;
		typedef utf8_string::difference_type		difference_type;
		typedef utf8_codepoint_raw_reference<false>	reference;
		typedef void*								pointer;
		typedef std::bidirectional_iterator_tag		iterator_category;
		
		bool operator==( const iterator_base& it ) const { return t_raw_index == it.t_raw_index; }
		bool operator!=( const iterator_base& it ) const { return t_raw_index != it.t_raw_index; }
		
		//! Ctor
		iterator_base( difference_type raw_index , utf8_string* instance ) :
			t_raw_index( raw_index )
			, t_instance( instance )
		{}
		
		//! Default function
		iterator_base() = default;
		iterator_base( const iterator_base& ) = default;
		iterator_base& operator=( const iterator_base& ) = default;
		
		// Getter for the iterator index
		difference_type get_index() const { return t_raw_index; }
		utf8_string* get_instance() const { return t_instance; }
		
	protected:
		
		difference_type	t_raw_index;
		utf8_string*	t_instance;
		
		void advance( difference_type n ){
			if( n > 0 )
				do
					increment();
				while( --n > 0 );
			else
				while( n++ < 0 )
					decrement();
		}
		
		void increment(){
			t_raw_index += t_instance->get_index_bytes( t_raw_index );
		}
		
		void decrement(){
			t_raw_index -= t_instance->get_index_pre_bytes( t_raw_index );
		}
		
		utf8_codepoint_raw_reference<false> get_reference() const {
			return t_instance->raw_at( t_raw_index , std::nothrow );
		}
		value_type get_value() const {
			return static_cast<const utf8_string*>(t_instance)->raw_at( t_raw_index );
		}
	};
	
public:
	
	typedef utf8_codepoint_reference<false> reference;
	
	struct iterator : iterator_base
	{
		//! Ctor
		iterator( difference_type raw_index , utf8_string* instance ) :
			iterator_base( raw_index , instance )
		{}
		
		//! Default Functions
		iterator() = default;
		iterator( const iterator& ) = default;
		iterator& operator=( const iterator& ) = default;
		
		//! Delete ctor from const iterator types
		iterator( const struct const_iterator& ) = delete;
		iterator( const struct const_reverse_iterator& ) = delete;
		
		//! Increase the Iterator by one
		iterator& operator++(){ // prefix ++iter
			increment();
			return *this;
		}
		iterator operator++( int ){ // postfix iter++
			iterator tmp{ t_raw_index , t_instance };
			increment();
			return tmp;
		}
		
		//! Decrease the iterator by one
		iterator& operator--(){ // prefix --iter
			decrement();
			return *this;
		}
		iterator operator--( int ){ // postfix iter--
			iterator tmp{ t_raw_index , t_instance };
			decrement();
			return tmp;
		}
		
		//! Increase the Iterator n times
		iterator operator+( difference_type n ) const {
			iterator it{*this};
			it.advance( n );
			return it;
		}
		iterator& operator+=( difference_type n ){
			advance( n );
			return *this;
		}
		
		//! Decrease the Iterator n times
		iterator operator-( difference_type n ) const {
			iterator it{*this};
			it.advance( -n );
			return it;
		}
		iterator& operator-=( difference_type n ){
			advance( -n );
			return *this;
		}
		
		//! Returns the value of the code point behind the iterator
		utf8_codepoint_raw_reference<false> operator*() const { return get_reference(); }
	};
	
	struct const_iterator : iterator
	{
		//! Ctor
		const_iterator( difference_type raw_index , const utf8_string* instance ) :
			iterator( raw_index , const_cast<utf8_string*>(instance) )
		{}
		
		//! Ctor from non const
		const_iterator( const iterator& it ) :
			iterator( it.get_index() , it.get_instance() )
		{}
		
		//! Default Functions
		const_iterator() = default;
		const_iterator( const const_iterator& ) = default;
		const_iterator& operator=( const const_iterator& ) = default;
		
		//! Returns the (raw) value behind the iterator
		value_type operator*() const { return get_value(); }
	};
	
	struct reverse_iterator : iterator_base
	{
		//! Ctor
		reverse_iterator( difference_type raw_index , utf8_string* instance ) :
			iterator_base( raw_index , instance )
		{}
		
		//! Ctor from normal iterator
		reverse_iterator( const iterator& it ) :
			iterator_base( it.get_index() , it.get_instance() )
		{}
		
		//! Default Functions
		reverse_iterator() = default;
		reverse_iterator( const reverse_iterator& ) = default;
		reverse_iterator& operator=( const reverse_iterator& ) = default;
		
		//! Delete ctor from const iterator types
		reverse_iterator( const const_iterator& ) = delete;
		reverse_iterator( const struct const_reverse_iterator& ) = delete;
		
		//! Increase the iterator by one
		reverse_iterator& operator++(){ // prefix ++iter
			decrement();
			return *this;
		}
		reverse_iterator operator++( int ){ // postfix iter++
			reverse_iterator tmp{ t_raw_index , t_instance };
			decrement();
			return tmp;
		}
		
		//! Decrease the Iterator by one
		reverse_iterator& operator--(){ // prefix --iter
			increment();
			return *this;
		}
		reverse_iterator operator--( int ){ // postfix iter--
			reverse_iterator tmp{ t_raw_index , t_instance };
			increment();
			return tmp;
		}
		
		//! Increase the Iterator n times
		reverse_iterator operator+( difference_type n ) const {
			reverse_iterator it{*this};
			it.advance( -n );
			return it;
		}
		reverse_iterator& operator+=( difference_type n ){
			advance( -n );
			return *this;
		}
		
		//! Decrease the Iterator n times
		reverse_iterator operator-( difference_type n ) const {
			reverse_iterator it{*this};
			it.advance( n );
			return it;
		}
		reverse_iterator& operator-=( difference_type n ){
			advance( n );
			return *this;
		}
		
		//! Returns the value of the code point behind the iterator
		utf8_codepoint_raw_reference<false> operator*() const { return get_reference(); }
		
		//! Get the underlying iterator instance
		iterator base() const {
			return iterator( t_raw_index , t_instance );
		}
	};
	
	struct const_reverse_iterator : reverse_iterator
	{
		//! Ctor
		const_reverse_iterator( difference_type raw_index , const utf8_string* instance ) :
			reverse_iterator( raw_index , const_cast<utf8_string*>(instance) )
		{}
		
		//! Ctor from non const
		const_reverse_iterator( const reverse_iterator& it ) :
			reverse_iterator( it.get_index() , it.get_instance() )
		{}
		
		//! Ctor from normal iterator
		const_reverse_iterator( const const_iterator& it ) :
			reverse_iterator( it.get_index() , it.get_instance() )
		{}
		
		//! Default Functions
		const_reverse_iterator() = default;
		const_reverse_iterator( const const_reverse_iterator& ) = default;
		const_reverse_iterator& operator=( const const_reverse_iterator& ) = default;
		
		//! Returns the (raw) value behind the iterator
		value_type operator*() const { return get_value(); }
		
		//! Get the underlying iterator instance
		const_iterator base() const {
			return const_iterator( t_raw_index , t_instance );
		}
	};
	
private: //! Attributes
	
	union{
		SSO		t_sso;
		NON_SSO	t_non_sso;
	};
	
private: //! Static helper methods
	
	//! Get the maximum number of bytes (excluding the trailing '\0') that can be stored within a utf8_string object
	static constexpr inline size_type	get_sso_capacity(){ return sizeof(SSO::data); }
	
	//! Helpers for the constructors
	template<size_type L>
	using enable_if_small_string = typename std::enable_if<L<=get_sso_capacity(),int*>::type;
	template<size_type L>
	using enable_if_not_small_string = typename std::enable_if<get_sso_capacity()<L,int*>::type;
	// Template to enable overloads, if the supplied type T is a character array without known bounds
	template<typename T, typename CharType>
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
		, int*
	>::type;
	
	//! Check, if the lut is active using the lut base ptr
	static inline bool					lut_active( const char* lut_base_ptr ){ return *((const unsigned char*)lut_base_ptr) & 0x1; }
	
	//! Rounds the supplied value to a multiple of sizeof(size_type)
	static inline size_type				round_up_to_align( size_type val ){
		return ( val + sizeof(size_type) - 1 ) & ~( sizeof(size_type) - 1 );
	}
	
	//! Get the LUT base pointer from buffer and buffer size
	static inline char*					get_lut_base_ptr( char* buffer , size_type buffer_size ){ return buffer + buffer_size; }
	static inline const char*			get_lut_base_ptr( const char* buffer , size_type buffer_size ){ return buffer + buffer_size; }
	
	//! Construct the lut mode indicator
	static inline void					set_lut_indiciator( char* lut_base_ptr , bool active , size_type lut_len = 0 ){
		*(indicator_type*)lut_base_ptr = active ? ( lut_len << 1 ) | 0x1 : 0;
	}
	//! Copy lut indicator
	static inline void					copy_lut_indicator( char* dest , const char* source ){
		*(indicator_type*)dest = *(indicator_type*)source;
	}
	
	//! Determine, whether we will use a 'uint8_t', 'uint16_t', 'uint32_t' or 'uint64_t'-based index table.
	//! Returns the number of bytes of the destination data type
	static inline width_type			get_lut_width( size_type buffer_size );
	
	//! Determine, whether or not a LUT is worth to set up
	static inline bool					is_lut_worth( size_type pot_lut_len , size_type string_len , bool lut_present , bool biased = true ){
		size_type threshold = ( biased ? ( lut_present ? 5 : 3 ) : 4 ) * string_len / 8u;
		// Note pot_lut_len is supposed to underflow at '0'
		return size_type( pot_lut_len - 1 ) < threshold;
	}
	
	//! Determine the needed buffer size and the needed lut width (excluding the trailling LUT indicator)
	static inline size_type				determine_main_buffer_size( size_type data_len , size_type lut_len , width_type* lut_width );
	//! Determine the needed buffer size if the lut width is known (excluding the trailling LUT indicator)
	static inline size_type				determine_main_buffer_size( size_type data_len , size_type lut_len , width_type lut_width );
	//! Determine the needed buffer size if the lut is empty (excluding the trailling LUT indicator)
	static inline size_type				determine_main_buffer_size( size_type data_len );
	
	//! Same as above but this time including the LUT indicator
	static inline size_type				determine_total_buffer_size( size_type main_buffer_size ){
		return main_buffer_size + sizeof(indicator_type); // Add the lut indicator
	}
	
	//! Get the nth index within a multibyte index table
	static inline size_type				get_lut( const char* iter , width_type lut_width ){
		switch( lut_width ){
		case sizeof(std::uint8_t):	return *(const std::uint8_t*)iter;
		case sizeof(std::uint16_t):	return *(const std::uint16_t*)iter;
		case sizeof(std::uint32_t):	return *(const std::uint32_t*)iter;
		}
		return *(const std::uint64_t*)iter;
	}
	static inline void					set_lut( char* iter , width_type lut_width , size_type value ){
		switch( lut_width ){
		case sizeof(std::uint8_t):	*(std::uint8_t*)iter = value; break;
		case sizeof(std::uint16_t):	*(std::uint16_t*)iter = value; break;
		case sizeof(std::uint32_t):	*(std::uint32_t*)iter = value; break;
		case sizeof(std::uint64_t):	*(std::uint64_t*)iter = value; break;
		}
	}
	
	//! Get the LUT size (given the lut is active!)
	static inline size_type				get_lut_len( const char* lut_base_ptr ){
		return *(indicator_type*)lut_base_ptr >> 1;
	}
	
	/**
	 * Returns the number of code units (bytes) using the supplied first byte of a utf8 code point
	 */
	// Data left is the number of bytes left in the buffer INCLUDING this one
	#if defined(_TINY_UTF8_H_HAS_CLZ_) && _TINY_UTF8_H_HAS_CLZ_ == true
	static inline width_type			get_codepoint_bytes( char first_byte , size_type data_left )
	{
		if( first_byte ){
			// Before counting the leading one's we need to shift the byte into the most significant part of the integer
			size_type codepoint_bytes = detail::clz( ~((unsigned int)first_byte << (sizeof(unsigned int)-1)*8 ) );
			
			// The test below would actually be ( codepoint_bytes <= data_left && codepoint_bytes ),
			// but codepoint_bytes is unsigned and thus wraps around zero, which makes the following faster:
			if( size_type( codepoint_bytes - 1 ) < size_type(data_left) )
				return codepoint_bytes;
		}
		return 1;
	}
	#else
	static width_type					get_codepoint_bytes( char first_byte , size_type data_left ); // Defined in source file
	#endif
	
	/**
	 * Returns the number of code units (bytes) a code point will translate to in utf8
	 */
	static inline width_type			get_codepoint_bytes( value_type cp )
	{
		#if defined(_TINY_UTF8_H_HAS_CLZ_) && _TINY_UTF8_H_HAS_CLZ_ == true
			if( !cp )
				return 1;
			static const width_type lut[32] = {
				1 , 1 , 1 , 1 , 1 , 1 , 1 , 2 , 2 , 2 , 2 , 3 , 3 , 3 , 3 , 3
				, 4 , 4 , 4 , 4 , 4 , 5 , 5 , 5 , 5 , 5 , 6 , 6 , 6 , 6 , 6 , 7
			};
			return lut[ 31 - detail::clz( cp ) ];
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
			return 1;
		#endif
	}
	
	//! Returns the number of bytes to expect before this one (including this one) that belong to this utf8 char
	static width_type					get_num_bytes_of_utf8_char_before( const char* data_start , size_type index );
	
	//! Decodes a given input of rle utf8 data to a unicode code point, given the number of bytes it's made of
	static inline value_type			decode_utf8( const char* data , width_type num_bytes ){
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
	 * unicode code point and returns the number of bytes it used
	 */
	static inline width_type			decode_utf8_and_len( const char* data , value_type& dest , size_type data_left ){
		// See 'get_codepoint_bytes' for 'data_left'
		width_type num_bytes = utf8_string::get_codepoint_bytes( *data , data_left );
		dest = decode_utf8( data , num_bytes );
		return num_bytes;
	}
	
	/**
	 * Encodes a given code point (expected to use 'cp_bytes') to a character
	 * buffer capable of holding that many bytes.
	 */
	inline static void					encode_utf8( value_type cp , char* dest , width_type cp_bytes ){
		switch( cp_bytes ){
			case 7: dest[cp_bytes-6] = 0x80 | ((cp >> 30) & 0x3F);
			case 6: dest[cp_bytes-5] = 0x80 | ((cp >> 24) & 0x3F);
			case 5: dest[cp_bytes-4] = 0x80 | ((cp >> 18) & 0x3F);
			case 4: dest[cp_bytes-3] = 0x80 | ((cp >> 12) & 0x3F);
			case 3: dest[cp_bytes-2] = 0x80 | ((cp >>  6) & 0x3F);
			case 2: dest[cp_bytes-1] = 0x80 | ((cp >>  0) & 0x3F);
				dest[0] = ( std::uint_least16_t(0xFF00uL) >> cp_bytes ) | ( cp >> ( 6 * cp_bytes - 6 ) );
				break;
			case 1: dest[0]	= char(cp);
		}
	}
	
	/**
	 * Encodes a given code point to a character buffer of at least 7 bytes
	 * and returns the number of bytes it used
	 */
	inline static width_type			encode_utf8( value_type cp , char* dest ){
		width_type width = get_codepoint_bytes( cp );
		utf8_string::encode_utf8( cp , dest , width );
		return width;
	}
	
private: //! Non-static helper methods
	
	//! Set the main buffer size (also disables SSO)
	inline void			set_non_sso_string_len( size_type string_len )
	{
		// Check, if NON_SSO is larger than its members, in which case it's not ambiguated by SSO::data_len
		if( offsetof(SSO, data_len) > offsetof(NON_SSO, string_len) + sizeof(NON_SSO::string_len) - 1 ){
			t_non_sso.string_len = string_len;
			t_sso.data_len = 0x1; // Manually set flag to deactivate SSO
		}
		else if( detail::is_little_endian::value ){
			detail::last_byte<size_type> lb;
			lb.number = string_len;
			lb.bytes.last <<= 1;
			lb.bytes.last |= 0x1;
			t_non_sso.string_len = lb.number;
		}
		else
			t_non_sso.string_len = ( string_len << 1 ) | size_type(0x1);
	}
	
	//! Get buffer size, if SSO is disabled
	inline size_type	get_non_sso_string_len() const {
		// Check, if NON_SSO is larger than its members, in which case it's not ambiguated by SSO::data_len
		if( offsetof(SSO, data_len) > offsetof(NON_SSO, string_len) + sizeof(NON_SSO::string_len) - 1 )
			return t_non_sso.string_len;
		else if( detail::is_little_endian::value ){
			detail::last_byte<size_type> lb;
			lb.number = t_non_sso.string_len;
			lb.bytes.last >>= 1;
			return lb.number;
		}
		else
			return t_non_sso.string_len >> 1;
	}
	
	//! Set the data length (also enables SSO)
	inline void			set_sso_data_len( unsigned char data_len = 0 ){
		t_sso.data_len = ( sizeof(SSO::data) - data_len ) << 1;
	}
	
	//! Get the data length (when SSO is active)
	inline size_type	get_sso_data_len() const { return get_sso_capacity() - ( t_sso.data_len >> 1 ); }
	
	//! Return a good guess of how many codepoints the currently allocated buffer can hold
	size_type			get_non_sso_capacity() const ;
	
	//! Check, if sso is inactive (this operation doesn't require a negation and is faster)
	inline bool			sso_inactive() const { return t_sso.data_len & 0x1; }
	
	// Helper for requires_unicode_sso that generates masks of the form 10000000 10000000...
	template<typename T>
	static constexpr T get_msb_mask( width_type bytes = sizeof(T) ){ return bytes ? ( T(1) << ( 8 * bytes - 1 ) ) | get_msb_mask<T>( bytes - 1 ) : T(0); }
	
	//! Check, whether the string contains code points > 127
	bool				requires_unicode_sso() const ;
	
	//! Get buffer
	inline const char*	get_buffer() const { return sso_inactive() ? t_non_sso.data : t_sso.data; }
	inline char*		get_buffer(){ return sso_inactive() ? t_non_sso.data : t_sso.data; }
	
	//! Get buffer size (excluding the trailing LUT indicator)
	inline size_type	get_buffer_size() const {
		return sso_inactive() ? t_non_sso.buffer_size : get_sso_capacity();
	}
	
	//! Returns an std::string with the UTF-8 BOM prepended
	std::string			cpp_str_bom() const ;
	
	//! Constructs an utf8_string from a character literal
	utf8_string( const char* str , size_type len , detail::read_codepoints_tag );
	utf8_string( const char* str , size_type len , detail::read_bytes_tag );
	
public:
	
	/**
	 * Default Ctor
	 * 
	 * @note Creates an Instance of type utf8_string that is empty
	 */
	inline utf8_string() : t_sso( 0 , '\0' ) {}
	/**
	 * Constructor taking an utf8 sequence and the maximum length to read from it (in number of codepoints)
	 * 
	 * @note	Creates an Instance of type utf8_string that holds the given utf8 sequence
	 * @param	str		The UTF-8 sequence to fill the utf8_string with
	 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
	 */
	template<typename T>
	inline utf8_string( T&& str , enable_if_ptr<T,char> = nullptr ) :
		utf8_string( str , utf8_string::npos , detail::read_codepoints_tag() )
	{}
	inline utf8_string( const char* str , size_type len ) :
		utf8_string( str , len , detail::read_codepoints_tag() )
	{}
	/**
	 * Constructor taking an utf8 char literal
	 * 
	 * @note	Creates an Instance of type utf8_string that holds the given utf8 sequence
	 * @param	str		The UTF-8 literal to fill the utf8_string with
	 */
	template<size_type LITLEN>
	inline utf8_string( const char (&str)[LITLEN] , enable_if_small_string<LITLEN> = nullptr ){
		std::memcpy( t_sso.data , str , LITLEN );
		if( str[LITLEN-1] ){
			t_sso.data[LITLEN] = '\0';
			set_sso_data_len( LITLEN );
		}
		else{
			set_sso_data_len( LITLEN - 1 );
		}
	}
	template<size_type LITLEN>
	inline utf8_string( const char (&str)[LITLEN] , enable_if_not_small_string<LITLEN> = nullptr ) :
		utf8_string( str , utf8_string::npos , detail::read_codepoints_tag() ) // Call the one that works on arbitrary character arrays
	{}
	/**
	 * Constructor taking an std::string
	 * 
	 * @note	Creates an Instance of type utf8_string that holds the given data sequence
	 * @param	str		The byte data, that will be interpreted as UTF-8
	 */
	inline utf8_string( std::string str ) :
		utf8_string( str.c_str() , str.length() , detail::read_bytes_tag() )
	{}
	/**
	 * Constructor that fills the string with a certain amount of codepoints
	 * 
	 * @note	Creates an Instance of type utf8_string that gets filled with 'n' codepoints
	 * @param	n		The number of codepoints generated
	 * @param	cp		The code point that the whole buffer will be set to
	 */
	utf8_string( size_type n , value_type cp );
	/**
	 * Constructor that fills the string with the supplied codepoint
	 * 
	 * @note	Creates an Instance of type utf8_string that gets filled with 'n' codepoints
	 * @param	n		The number of codepoints generated
	 * @param	cp		The code point that the whole buffer will be set to
	 */
	inline utf8_string( value_type cp ) : t_sso( cp = encode_utf8( cp , t_sso.data ) ) {
		t_sso.data[cp] = '\0';
	}
	/**
	 * Constructor that fills the string with a certain amount of characters
	 * 
	 * @note	Creates an Instance of type utf8_string that gets filled with 'n' characters
	 * @param	n		The number of characters generated
	 * @param	cp		The characters that the whole buffer will be set to
	 */
	utf8_string( size_type n , char cp );
	/**
	 * Constructs the string with a portion of the supplied string
	 * 
	 * @param	str		The string that the constructed string shall be a substring of
	 * @param	pos		The code point position indicating the start of the string to be used for construction
	 * @param	cp		The number of code points to be taken from 'str'
	 */
	utf8_string( const utf8_string& str , size_type pos , size_type count = utf8_string::npos ) :
		utf8_string( str.substr( pos , count ) )
	{}
	/**
	 * Constructs the string from the range of code points supplied. The resulting string will equal [first,last)
	 * 
	 * @note	The Range is expected to contain code points (rather than code units, i.e. bytes)
	 * @param	first	The start of the range to construct from
	 * @param	last	The end of the range
	 */
	template<typename InputIt>
	utf8_string( InputIt first , InputIt last ) : t_sso( 0 ) {
		while( first != last ) push_back( *first++ );
	}
	/**
	 * Copy Constructor that copies the supplied utf8_string to construct the string
	 * 
	 * @note	Creates an Instance of type utf8_string that has the exact same data as 'str'
	 * @param	str		The utf8_string to copy from
	 */
	utf8_string( const utf8_string& str )
	{
		// Copy data
		std::memcpy( this , &str , sizeof(utf8_string) );
		
		// Create a new buffer, if sso is not active
		if( str.sso_inactive() ){
			size_type total_buffer_size = utf8_string::determine_total_buffer_size( t_non_sso.buffer_size );
			t_non_sso.data = new char[ total_buffer_size ];
			std::memcpy( t_non_sso.data , str.t_non_sso.data , total_buffer_size );
		}
	}
	/**
	 * Move Constructor that moves the supplied utf8_string content into the new utf8_string
	 * 
	 * @note	Creates an Instance of type utf8_string that takes all data from 'str'
	 * 			The supplied utf8_string is invalid afterwards and may not be used anymore
	 * @param	str		The utf8_string to move from
	 */
	inline utf8_string( utf8_string&& str ){
		std::memcpy( this , &str , sizeof(utf8_string) ); // Copy data
		str.set_sso_data_len(0); // Reset old string
	}
	/**
	 * Constructor taking a wide code point literal that will be copied to construct this utf8_string
	 * 
	 * @note	Creates an Instance of type utf8_string that holds the given codepoints
	 *			The data itself will be first converted to UTF-8
	 * @param	str		The code point sequence to fill the utf8_string with
	 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
	 */
	utf8_string( const value_type* str , size_type len = utf8_string::npos );
	/**
	 * Constructor taking an initializer list of codepoints.
	 * 
	 * @note	The initializer list is expected to contain code points (rather than code units, i.e. bytes)
	 * @param	ilist	The initializer list with the contents to be applied to this string
	 */
	inline utf8_string( std::initializer_list<value_type> ilist ) :
		utf8_string( ilist.begin() , ilist.end() )
	{}
	
	/**
	 * Destructor
	 * 
	 * @note	Destructs a utf8_string at the end of its lifetime releasing all held memory
	 */
	inline ~utf8_string(){
		clear();
	}
	
	
	/**
	 * Copy Assignment operator that sets the utf8_string to a copy of the supplied one
	 * 
	 * @note	Assigns a copy of 'str' to this utf8_string deleting all data that previously was in there
	 * @param	str		The utf8_string to copy from
	 * @return	A reference to the string now holding the data (*this)
	 */
	utf8_string& operator=( const utf8_string& str );
	/**
	 * Move Assignment operator that moves all data out of the supplied and into this utf8_string
	 * 
	 * @note	Moves all data from 'str' into this utf8_string deleting all data that previously was in there
	 *			The supplied utf8_string is invalid afterwards and may not be used anymore
	 * @param	str		The utf8_string to move from
	 * @return	A reference to the string now holding the data (*this)
	 */
	inline utf8_string& operator=( utf8_string&& str ){
		if( &str != this ){
			clear(); // Reset old data
			std::memcpy( this , &str , sizeof(utf8_string) ); // Copy data
			str.set_sso_data_len(0); // Reset old string
		}
		return *this;
	}
	
	
	/**
	 * Swaps the contents of this utf8_string with the supplied one
	 * 
	 * @note	Swaps all data with the supplied utf8_string
	 * @param	str		The utf8_string to swap contents with
	 */
	inline void swap( utf8_string& str ){
		if( &str != this ){
			char tmp[sizeof(utf8_string)];
			std::memcpy( &tmp , &str , sizeof(utf8_string) );
			std::memcpy( &str , this , sizeof(utf8_string) );
			std::memcpy( this , &tmp , sizeof(utf8_string) );
		}
	}
	
	
	/**
	 * Clears the content of this utf8_string
	 * 
	 * @note	Resets the data to an empty string ("")
	 */
	inline void clear(){
		if( sso_inactive() )
			delete[] t_non_sso.data;
		set_sso_data_len( 0 );
		t_sso.data[0] = 0;
	}
	
	
	/**
	 * Returns the current capacity of this string. That is, the number of codepoints it can hold without reallocation
	 * 
	 * @return	The number of bytes currently allocated
	 */
	inline size_type capacity() const {
		return sso_inactive() ? get_non_sso_capacity() : get_sso_capacity();
	}
	
	
	/**
	 * Requests the removal of unused capacity.
	 */
	void shrink_to_fit();
	
	
	/**
	 * Returns the code point at the supplied index
	 * 
	 * @param	n	The code point index of the code point to receive
	 * @return	The code point at position 'n'
	 */
	inline value_type at( size_type n ) const {
		return raw_at( get_num_bytes_from_start( n ) );
	}
	inline value_type at( size_type n , std::nothrow_t ) const {
		return raw_at( get_num_bytes_from_start( n ) , std::nothrow );
	}
	inline utf8_codepoint_reference<true> at( size_type n ){
		return utf8_codepoint_reference<true>( n , this );
	}
	inline utf8_codepoint_reference<false> at( size_type n , std::nothrow_t ){
		return utf8_codepoint_reference<false>( n , this );
	}
	/**
	 * Returns the code point at the supplied byte position
	 * 
	 * @note	As this access is raw, that is not looking up for the actual byte position,
	 *			it is very fast.
	 * @param	byte_index	The byte position of the code point to receive
	 * @return	The code point at the supplied position
	 */
	inline utf8_codepoint_raw_reference<true> raw_at( size_type byte_index ){
		return utf8_codepoint_raw_reference<true>( byte_index , this );
	}
	inline utf8_codepoint_raw_reference<false> raw_at( size_type byte_index , std::nothrow_t ){
		return utf8_codepoint_raw_reference<false>( byte_index , this );
	}
	value_type raw_at( size_type byte_index ) const {
		size_type size = this->size();
		if( byte_index >= size )
			throw std::out_of_range( "utf8_string::(raw_)at" );
		const char* pos = get_buffer() + byte_index;
		return *pos ? decode_utf8( pos , utf8_string::get_codepoint_bytes( *pos , size - byte_index ) ) : 0;
	}
	value_type raw_at( size_type byte_index , std::nothrow_t ) const {
		const char* pos = get_buffer() + byte_index;
		return *pos ? decode_utf8( pos , utf8_string::get_codepoint_bytes( *pos , size() - byte_index ) ) : 0;
	}
	
	
	/**
	 * Returns an iterator pointing to the supplied code point index
	 * 
	 * @param	n	The index of the code point to get the iterator to
	 * @return	An iterator pointing to the specified code point index
	 */
	inline iterator get( size_type n ){ return iterator( get_num_bytes_from_start( n ) , this ); }
	inline const_iterator get( size_type n ) const { return const_iterator( get_num_bytes_from_start( n ) , this ); }
	/**
	 * Returns an iterator pointing to the code point at the supplied byte position
	 * 
	 * @note	As this access is raw, that is not looking up for the actual byte position,
	 *			it is very fast
	 * @param	n	The byte position of the code point to get the iterator to
	 * @return	An iterator pointing to the specified byte position
	 */
	inline iterator raw_get( size_type n ){ return iterator( n , this ); }
	inline const_iterator raw_get( size_type n ) const { return const_iterator( n , this ); }
	
	
	/**
	 * Returns a reverse iterator pointing to the supplied code point index
	 * 
	 * @param	n	The index of the code point to get the reverse iterator to
	 * @return	A reverse iterator pointing to the specified code point index
	 */
	inline reverse_iterator rget( size_type n ){ return reverse_iterator( get_num_bytes_from_start( n ) , this ); }
	inline const_reverse_iterator rget( size_type n ) const { return const_reverse_iterator( get_num_bytes_from_start( n ) , this ); }
	/**
	 * Returns a reverse iterator pointing to the code point at the supplied byte position
	 * 
	 * @note	As this access is raw, that is not looking up for the actual byte position,
	 *			it is very fast
	 * @param	n	The byte position of the code point to get the reverse iterator to
	 * @return	A reverse iterator pointing to the specified byte position
	 */
	inline reverse_iterator raw_rget( size_type n ){ return reverse_iterator( n , this ); }
	inline const_reverse_iterator raw_rget( size_type n ) const { return const_reverse_iterator( n , this ); }
	
	
	/**
	 * Returns a reference to the code point at the supplied index
	 * 
	 * @param	n	The code point index of the code point to receive
	 * @return	A reference wrapper to the code point at position 'n'
	 */
	inline utf8_codepoint_reference<false> operator[]( size_type n ){ return utf8_codepoint_reference<false>( n , this ); }
	inline value_type operator[]( size_type n ) const { return at( n , std::nothrow ); }
	/**
	 * Returns a reference to the code point at the supplied byte position
	 * 
	 * @note	As this access is raw, that is not looking up for the actual byte position,
	 *			it is very fast
	 * @param	n	The byte position of the code point to receive
	 * @return	A reference wrapper to the code point at byte position 'n'
	 */
	inline utf8_codepoint_raw_reference<false> operator()( size_type n ){ return utf8_codepoint_raw_reference<false>( n , this ); }
	inline value_type operator()( size_type n ) const { return raw_at( n , std::nothrow ); }
	
	
	/**
	 * Get the raw data contained in this utf8_string
	 * 
	 * @note	Returns the UTF-8 formatted content of this utf8_string
	 * @return	UTF-8 formatted data, trailled by a '\0'
	 */
	inline const char* c_str() const { return get_buffer(); }
	inline const char* data() const { return get_buffer(); }
	inline char* data(){ return get_buffer(); }
	
	
	/**
	 * Get the raw data contained in this utf8_string wrapped by an std::string
	 * 
	 * @note	Returns the UTF-8 formatted content of this utf8_string
	 * @return	UTF-8 formatted data, wrapped inside an std::string
	 */
	inline std::string cpp_str( bool prepend_bom = false ) const { return prepend_bom ? cpp_str_bom() : std::string( c_str() , size() ); }
	
	
	/**
	 * Get the number of codepoints in this utf8_string
	 * 
	 * @note	Returns the number of codepoints that are taken care of
	 *			For the number of bytes, @see size()
	 * @return	Number of codepoints (not bytes!)
	 */
	inline size_type length() const { return sso_inactive() ? get_non_sso_string_len() : cend() - cbegin(); }
	
	
	/**
	 * Get the number of bytes that are used by this utf8_string
	 * 
	 * @note	Returns the number of bytes required to hold the contained wide string,
	 *			That is, without counting the trailling '\0'
	 * @return	Number of bytes (not codepoints!)
	 */
	inline size_type size() const { return sso_inactive() ? t_non_sso.data_len : get_sso_data_len(); }
	
	
	/**
	 * Check, whether this utf8_string is empty
	 * 
	 * @note	Returns True, if this utf8_string is empty, that also is comparing true with ""
	 * @return	True, if this utf8_string is empty, false if its length is >0
	 */
	inline bool empty() const { return sso_inactive() ? !t_non_sso.data_len : t_sso.data_len == (get_sso_capacity() << 1); }
	
	
	/**
	 * Check whether the data inside this utf8_string cannot be iterated by an std::string
	 * 
	 * @note	Returns true, if the utf8_string has codepoints that exceed 7 bits to be stored
	 * @return	True, if there are UTF-8 formatted byte sequences,
	 *			false, if there are only ascii characters (<128) inside
	 */
	inline bool requires_unicode() const {
		return sso_inactive() ? t_non_sso.data_len != get_non_sso_string_len() : requires_unicode_sso();
	}
	
	
	/**
	 * Determine, if small string optimization is active
	 * @return	True, if the utf8 data is stored within the utf8_string object itself
	 *			false, otherwise
	 */
	inline bool sso_active() const { return sso_inactive() == false; }
	
	
	/**
	 * Receive a (null-terminated) wide string literal from this UTF-8 string
	 * 
	 * @param	dest	A buffer capable of holding at least 'length()+1' elements of type 'value_type'
	 * @return	void
	 */
	void to_wide_literal( value_type* dest ) const {
		for( const char* data = get_buffer(), * data_end = data + size() ; data < data_end ; )
			data += decode_utf8_and_len( data , *dest++ , data_end - data );
		*dest = 0;
	}
	
	
	/**
	 * Get an iterator to the beginning of the utf8_string
	 * 
	 * @return	An iterator class pointing to the beginning of this utf8_string
	 */
	inline iterator begin(){ return iterator( 0 , this ); }
	inline const_iterator begin() const { return const_iterator( 0 , this ); }
	/**
	 * Get an iterator to the end of the utf8_string
	 * 
	 * @return	An iterator class pointing to the end of this utf8_string, that is pointing behind the last code point
	 */
	inline iterator end(){ return iterator( size() , this ); }
	inline const_iterator end() const { return const_iterator( size() , this ); }
	
	
	/**
	 * Get a reverse iterator to the end of this utf8_string
	 * 
	 * @return	A reverse iterator class pointing to the end of this utf8_string,
	 *			that is exactly to the last code point
	 */
	inline reverse_iterator rbegin(){ return reverse_iterator( back_index() , this ); }
	inline const_reverse_iterator rbegin() const { return const_reverse_iterator( back_index() , this ); }
	/**
	 * Get a reverse iterator to the beginning of this utf8_string
	 * 
	 * @return	A reverse iterator class pointing to the end of this utf8_string,
	 *			that is pointing before the first code point
	 */
	inline reverse_iterator rend(){ return reverse_iterator( -1 , this ); }
	inline const_reverse_iterator rend() const { return const_reverse_iterator( -1 , this ); }
	
	
	/**
	 * Get a const iterator to the beginning of the utf8_string
	 * 
	 * @return	A const iterator class pointing to the beginning of this utf8_string,
	 * 			which cannot alter things inside this utf8_string
	 */
	inline const_iterator cbegin() const { return const_iterator( 0 , this ); }
	/**
	 * Get an iterator to the end of the utf8_string
	 * 
	 * @return	A const iterator class, which cannot alter this utf8_string, pointing to
	 *			the end of this utf8_string, that is pointing behind the last code point
	 */
	inline const_iterator cend() const { return const_iterator( size() , this ); }
	
	
	/**
	 * Get a const reverse iterator to the end of this utf8_string
	 * 
	 * @return	A const reverse iterator class, which cannot alter this utf8_string, pointing to
	 *			the end of this utf8_string, that is exactly to the last code point
	 */
	inline const_reverse_iterator crbegin() const { return const_reverse_iterator( back_index() , this ); }
	/**
	 * Get a const reverse iterator to the beginning of this utf8_string
	 * 
	 * @return	A const reverse iterator class, which cannot alter this utf8_string, pointing to
	 *			the end of this utf8_string, that is pointing before the first code point
	 */
	inline const_reverse_iterator crend() const { return const_reverse_iterator( -1 , this ); }
	
	
	/**
	 * Returns a reference to the first code point in the utf8_string
	 * 
	 * @return	A reference wrapper to the first code point in the utf8_string
	 */
	inline utf8_codepoint_raw_reference<false> front(){ return utf8_codepoint_raw_reference<false>( 0 , this ); }
	inline value_type front() const { return raw_at( 0 , std::nothrow ); }
	/**
	 * Returns a reference to the last code point in the utf8_string
	 * 
	 * @return	A reference wrapper to the last code point in the utf8_string
	 */
	inline utf8_codepoint_raw_reference<false> back(){ return utf8_codepoint_raw_reference<false>( back_index() , this ); }
	inline value_type back() const {
		size_type	sz = size();
		const char*	buffer = get_buffer();
		width_type	bytes = get_num_bytes_of_utf8_char_before( buffer , sz );
		return decode_utf8( buffer + sz - bytes , bytes );
	}
	
	
	/**
	 * Replace a code point of this utf8_string by a number of codepoints
	 * 
	 * @param	index	The codpoint index to be replaced
	 * @param	repl	The wide character that will be used to replace the code point
	 * @param	n		The number of code point that will be inserted
	 *					instead of the one residing at position 'index'
	 * @return	A reference to this utf8_string, which now has the replaced part in it
	 */
	inline utf8_string& replace( size_type index , value_type repl , size_type n = 1 ){
		return replace( index , 1 , repl , n );
	}
	/**
	 * Replace a number of codepoints of this utf8_string by a number of other codepoints
	 * 
	 * @param	index	The codpoint index at which the replacement is being started
	 * @param	len		The number of codepoints that are being replaced
	 * @param	repl	The wide character that will be used to replace the codepoints
	 * @param	n		The number of code point that will replace the old ones
	 * @return	A reference to this utf8_string, which now has the replaced part in it
	 */
	inline utf8_string& replace( size_type index , size_type len , value_type repl , size_type n ){
		return replace( index , len , utf8_string( n , repl ) );
	}
	inline utf8_string& replace( size_type index , size_type len , value_type repl ){
		return replace( index , len , utf8_string( repl ) );
	}
	/**
	 * Replace a range of codepoints by a number of codepoints
	 * 
	 * @param	first	An iterator pointing to the first code point to be replaced
	 * @param	last	An iterator pointing to the code point behind the last code point to be replaced
	 * @param	repl	The wide character that will be used to replace the codepoints in the range
	 * @param	n		The number of code point that will replace the old ones
	 * @return	A reference to this utf8_string, which now has the replaced part in it
	 */
	inline utf8_string& replace( iterator first , iterator last , value_type repl , size_type n ){
		return raw_replace( first.t_raw_index , last.t_raw_index - first.t_raw_index , utf8_string( n , repl ) );
	}
	inline utf8_string& replace( iterator first , iterator last , value_type repl ){
		return raw_replace( first.t_raw_index , last.t_raw_index - first.t_raw_index , utf8_string( repl ) );
	}
	/**
	 * Replace a range of codepoints with the contents of the supplied utf8_string
	 * 
	 * @param	first	An iterator pointing to the first code point to be replaced
	 * @param	last	An iterator pointing to the code point behind the last code point to be replaced
	 * @param	repl	The utf8_string to replace all codepoints in the range
	 * @return	A reference to this utf8_string, which now has the replaced part in it
	 */
	inline utf8_string& replace( iterator first , iterator last , const utf8_string& repl ){
		return raw_replace( first.t_raw_index , last.t_raw_index - first.t_raw_index , repl );
	}
	/**
	 * Replace a number of codepoints of this utf8_string with the contents of the supplied utf8_string
	 * 
	 * @param	index	The codpoint index at which the replacement is being started
	 * @param	len		The number of codepoints that are being replaced
	 * @param	repl	The utf8_string to replace all codepoints in the range
	 * @return	A reference to this utf8_string, which now has the replaced part in it
	 */
	inline utf8_string& replace( size_type index , size_type count , const utf8_string& repl ){
		size_type start_byte = get_num_bytes_from_start( index );
		return raw_replace(
			start_byte
			, count == utf8_string::npos ? utf8_string::npos : get_num_bytes( start_byte , count )
			, repl
		);
	}
	/**
	 * Replace a number of bytes of this utf8_string with the contents of the supplied utf8_string
	 * 
	 * @note	As this function is raw, that is not having to compute byte indices,
	 *			it is much faster than the code point-base replace function
	 * @param	start_byte	The byte position at which the replacement is being started
	 * @param	byte_count	The number of bytes that are being replaced
	 * @param	repl		The utf8_string to replace all bytes inside the range
	 * @return	A reference to this utf8_string, which now has the replaced part in it
	 */
	utf8_string& raw_replace( size_type start_byte , size_type byte_count , const utf8_string& repl );
	
	
	/**
	 * Prepend the supplied utf8_string to this utf8_string
	 * 
	 * @param	prependix	The utf8_string to be prepended
	 * @return	A reference to this utf8_string, which now has the supplied string prepended
	 */
	inline utf8_string& prepend( const utf8_string& prependix ){
		return raw_insert( 0 , prependix );
	}
	
	/**
	 * Appends the supplied utf8_string to the end of this utf8_string
	 * 
	 * @param	appendix	The utf8_string to be appended
	 * @return	A reference to this utf8_string, which now has the supplied string appended
	 */
	utf8_string& append( const utf8_string& appendix );
	inline utf8_string& operator+=( const utf8_string& appendix ){
		return append( appendix );
	}
	
	
	/**
	 * Appends the supplied code point to the end of this utf8_string
	 * 
	 * @param	cp	The code point to be appended
	 * @return	A reference to this utf8_string, which now has the supplied code point appended
	 */
	inline utf8_string& push_back( value_type cp ){
		return append( utf8_string( cp ) );
	}
	
	/**
	 * Prepends the supplied code point to this utf8_string
	 * 
	 * @param	cp	The code point to be prepended
	 * @return	A reference to this utf8_string, which now has the supplied code point prepended
	 */
	inline utf8_string& push_front( value_type cp ){
		return raw_insert( 0 , cp );
	}
	
	
	/**
	 * Adds the supplied utf8_string to a copy of this utf8_string
	 * 
	 * @param	summand		The utf8_string to be added 
	 * @return	A reference to the newly created utf8_string, which now has the supplied string appended
	 */
	inline utf8_string operator+( const utf8_string& summand ) const {
		utf8_string str = *this;
		str.append( summand );
		return str;
	}
	
	
	/**
	 * Sets the contents of this string to 'count' times 'cp'
	 * 
	 * @param	count	The number of times the supplied code point is to be repeated
	 * @param	cp		The code point to repeat 'count' times
	 * @return	A reference to this utf8_string, updated to the new string
	 */
	inline utf8_string& assign( size_type count , value_type cp ){
		return *this = utf8_string( count , cp );
	}
	/**
	 * Sets the contents of this string to a copy of the supplied utf8_string
	 * 
	 * @param	str	The utf8_string this string shall be made a copy of
	 * @return	A reference to this utf8_string, updated to the new string
	 */
	inline utf8_string& assign( const utf8_string& str ){
		return *this = str;
	}
	/**
	 * Sets the contents of this string to a substring of the supplied utf8_string
	 * 
	 * @param	str	The utf8_string this string shall be constructed a substring of
	 * @param	pos	The code point position indicating the start of the string to be used for construction
	 * @param	cp	The number of code points to be taken from 'str'
	 * @return	A reference to this utf8_string, updated to the new string
	 */
	inline utf8_string& assign( const utf8_string& str , size_type pos , size_type count ){
		return *this = utf8_string( str , pos , count );
	}
	/**
	 * Moves the contents out of the supplied string into this one
	 * 
	 * @param	str	The utf8_string this string shall move from
	 * @return	A reference to this utf8_string, updated to the new string
	 */
	inline utf8_string& assign( utf8_string&& str ){
		return *this = std::move(str);
	}
	/**
	 * Assigns an utf8 sequence and the maximum length to read from it (in number of codepoints) to this string
	 * 
	 * @param	str		The UTF-8 sequence to fill the this utf8_string with
	 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
	 */
	template<typename T>
	inline utf8_string& assign( T&& str , enable_if_ptr<T,char> = nullptr ){
		return *this = utf8_string( str );
	}
	inline utf8_string& assign( const char* str , size_type len ){
		return *this = utf8_string( str , len );
	}
	/**
	 * Assigns an utf8 char literal to this string
	 * 
	 * @param	str		The UTF-8 literal to fill the utf8_string with
	 */
	template<size_type LITLEN>
	inline utf8_string& assign( const char (&str)[LITLEN] ){
		return *this = utf8_string( str );
	}
	/**
	 * Assigns the range of code points supplied to this string. The resulting string will equal [first,last)
	 * 
	 * @note	The Range is expected to contain code points (rather than code units, i.e. bytes)
	 * @param	first	The start of the range to read from
	 * @param	last	The end of the range
	 */
	template<typename InputIt>
	inline utf8_string& assign( InputIt first , InputIt last ){
		return *this = utf8_string( first , last );
	}
	/**
	 * Assigns the supplied initializer list of codepoints to this string.
	 * 
	 * @note	The initializer list is expected to contain code points (rather than code units, i.e. bytes)
	 * @param	ilist	The initializer list with the contents to be applied to this string
	 */
	inline utf8_string& assign( std::initializer_list<value_type> ilist ){
		return *this = utf8_string( std::move(ilist) );
	}
	
	
	/**
	 * Inserts a given code point into this utf8_string at the supplied code point index
	 * 
	 * @param	pos		The code point index to insert at
	 * @param	cp		The code point to be inserted
	 * @return	A reference to this utf8_string, with the supplied code point inserted
	 */
	inline utf8_string& insert( size_type pos , value_type cp ){
		return raw_insert( get_num_bytes_from_start( pos ) , cp );
	}
	/**
	 * Inserts a given utf8_string into this utf8_string at the supplied code point index
	 * 
	 * @param	pos		The code point index to insert at
	 * @param	str		The utf8_string to be inserted
	 * @return	A reference to this utf8_string, with the supplied utf8_string inserted
	 */
	inline utf8_string& insert( size_type pos , const utf8_string& str ){
		return raw_insert( get_num_bytes_from_start( pos ) , str );
	}
	/**
	 * Inserts a given code point into this utf8_string at the supplied iterator position
	 * 
	 * @param	it	The iterator position to insert at
	 * @param	cp	The code point to be inserted
	 * @return	A reference to this utf8_string, with the supplied code point inserted
	 */
	inline utf8_string& insert( iterator it , value_type cp ){
		return raw_insert( it.t_raw_index , utf8_string( cp ) );
	}
	/**
	 * Inserts a given utf8_string into this utf8_string at the supplied iterator position
	 * 
	 * @param	it	The iterator position to insert at
	 * @param	cp	The utf8_string to be inserted
	 * @return	A reference to this utf8_string, with the supplied code point inserted
	 */
	inline utf8_string& insert( iterator it , const utf8_string& str ){
		return raw_insert( it.t_raw_index , str );
	}
	/**
	 * Inserts a given utf8_string into this utf8_string at the supplied byte position
	 * 
	 * @note	As this function is raw, that is without having to compute
	 *			actual byte indices, it is much faster that insert()
	 * @param	pos		The byte position index to insert at
	 * @param	str		The utf8_string to be inserted
	 * @return	A reference to this utf8_string, with the supplied utf8_string inserted
	 */
	utf8_string& raw_insert( size_type pos , const utf8_string& str );
	/**
	 * Inserts a given code point into this utf8_string at the supplied byte position
	 * 
	 * @note	As this function is raw, that is without having to compute
	 *			actual byte indices, it is much faster that insert()
	 * @param	pos		The byte position index to insert at
	 * @param	cp		The code point to be inserted
	 * @return	A reference to this utf8_string, with the supplied utf8_string inserted
	 */
	inline utf8_string& raw_insert( size_type pos , value_type cp ){
		return raw_insert( pos , utf8_string( cp ) ); 
	}
	
	
	/**
	 * Erases the code point at the supplied iterator position
	 * 
	 * @param	pos		The iterator pointing to the position being erased
	 * @return	A reference to this utf8_string, which now has the code point erased
	 */
	inline utf8_string& erase( iterator pos ){
		return raw_erase( pos.t_raw_index , get_index_bytes( pos.t_raw_index ) );
	}
	/**
	 * Erases the codepoints inside the supplied range
	 * 
	 * @param	first	An iterator pointing to the first code point to be erased
	 * @param	last	An iterator pointing to the code point behind the last code point to be erased
	 * @return	A reference to this utf8_string, which now has the codepoints erased
	 */
	inline utf8_string& erase( iterator first , iterator last ){
		return raw_erase( first.t_raw_index , last.t_raw_index - first.t_raw_index );
	}
	/**
	 * Erases a portion of this string
	 * 
	 * @param	pos		The code point index to start eraseing from
	 * @param	len		The number of codepoints to be erased from this utf8_string
	 * @return	A reference to this utf8_string, with the supplied portion erased
	 */
	inline utf8_string& erase( size_type pos , size_type len = 1 ){
		size_type start_byte = get_num_bytes_from_start( pos );
		return raw_erase( start_byte , get_num_bytes( start_byte , len ) );
	}
	/**
	 * Erases a byte range of this string
	 * 
	 * @note	As this function is raw, that is without having to compute
	 *			actual byte indices, it is much faster that erase()
	 * @param	pos		The byte position index to start erasing from
	 * @param	len		The number of bytes to be erased from the utf8_string
	 * @return	A reference to this utf8_string, with the supplied bytes erased
	 */
	utf8_string& raw_erase( size_type pos , size_type len );
	
	
	/**
	 * Returns a portion of the utf8_string
	 * 
	 * @param	first	An iterator pointing to the first code point to be included in the substring
	 * @param	last	An iterator pointing to the code point behind the last code point in the substring
	 * @return	The utf8_string holding the specified range
	 */
	inline utf8_string substr( iterator first , iterator last ) const {
		size_type byte_count = last.t_raw_index - first.t_raw_index;
		return raw_substr( first.t_raw_index , byte_count );
	}
	/**
	 * Returns a portion of the utf8_string
	 * 
	 * @param	pos		The code point index that should mark the start of the substring
	 * @param	len		The number codepoints to be included within the substring
	 * @return	The utf8_string holding the specified codepoints
	 */
	utf8_string substr( size_type pos , size_type len = utf8_string::npos ) const
	{
		size_type byte_start = get_num_bytes_from_start( pos );
		
		if( len == utf8_string::npos )
			return raw_substr( byte_start , utf8_string::npos );
		
		size_type byte_count = get_num_bytes( byte_start , len );
		
		return raw_substr( byte_start , byte_count );
	}
	/**
	 * Returns a portion of the utf8_string (indexed on byte-base)
	 * 
	 * @note	As this function is raw, that is without having to compute
	 *			actual byte indices, it is much faster that substr()
	 * @param	start_byte		The byte position where the substring shall start
	 * @param	byte_count		The number of bytes that the substring shall have
	 * @return	The utf8_string holding the specified bytes
	 */
	utf8_string raw_substr( size_type start_byte , size_type byte_count ) const ;
	
	
	/**
	 * Finds a specific code point inside the utf8_string starting at the supplied code point index
	 * 
	 * @param	cp				The code point to look for
	 * @param	start_codepoint	The index of the first code point to start looking from
	 * @return	The code point index where and if the code point was found or utf8_string::npos
	 */
	size_type find( value_type cp , size_type start_codepoint = 0 ) const {
		if( sso_inactive() && start_codepoint >= length() ) // length() is only O(1), if sso is inactive
			return utf8_string::npos;
		for( const_iterator it = get(start_codepoint) , end = cend() ; it != end ; ++it,++start_codepoint )
			if( *it == cp )
				return start_codepoint;
		return utf8_string::npos;
	}
	/**
	 * Finds a specific pattern within the utf8_string starting at the supplied code point index
	 * 
	 * @param	cp				The code point to look for
	 * @param	start_codepoint	The index of the first code point to start looking from
	 * @return	The code point index where and if the pattern was found or utf8_string::npos
	 */
	size_type find( const utf8_string& pattern , size_type start_codepoint = 0 ) const {
		if( sso_inactive() && start_codepoint >= length() ) // length() is only O(1), if sso is inactive
			return utf8_string::npos;
		size_type actual_start = get_num_bytes_from_start( start_codepoint );
		const char* buffer = get_buffer();
		const char* result = std::strstr( buffer + actual_start , pattern.c_str() );
		if( !result )
			return utf8_string::npos;
		return start_codepoint + get_num_codepoints( actual_start , result - ( buffer + actual_start ) );
	}
	/**
	 * Finds a specific pattern within the utf8_string starting at the supplied code point index
	 * 
	 * @param	cp				The code point to look for
	 * @param	start_codepoint	The index of the first code point to start looking from
	 * @return	The code point index where and if the pattern was found or utf8_string::npos
	 */
	size_type find( const char* pattern , size_type start_codepoint = 0 ) const {
		if( sso_inactive() && start_codepoint >= length() ) // length() is only O(1), if sso is inactive
			return utf8_string::npos;
		size_type actual_start = get_num_bytes_from_start( start_codepoint );
		const char* buffer = get_buffer();
		const char* result = std::strstr( buffer + actual_start , pattern );
		if( !result )
			return utf8_string::npos;
		return start_codepoint + get_num_codepoints( actual_start , result - ( buffer + actual_start ) );
	}
	/**
	 * Finds a specific code point inside the utf8_string starting at the supplied byte position
	 * 
	 * @param	cp			The code point to look for
	 * @param	start_byte	The byte position of the first code point to start looking from
	 * @return	The byte position where and if the code point was found or utf8_string::npos
	 */
	size_type raw_find( value_type cp , size_type start_byte = 0 ) const {
		size_type my_size = size();
		if( start_byte >= my_size )
			return utf8_string::npos;
		for( const_iterator it = raw_get(start_byte) , end = raw_get(my_size) ; it != end ; ++it )
			if( *it == cp )
				return it - begin();
		return utf8_string::npos;
	}
	/**
	 * Finds a specific pattern within the utf8_string starting at the supplied byte position
	 * 
	 * @param	needle		The pattern to look for
	 * @param	start_byte	The byte position of the first code point to start looking from
	 * @return	The byte position where and if the pattern was found or utf8_string::npos
	 */
	size_type raw_find( const utf8_string& pattern , size_type start_byte = 0 ) const {
		if( start_byte >= size() )
			return utf8_string::npos;
		const char* buffer = get_buffer();
		const char* result = std::strstr( buffer + start_byte , pattern.c_str() );
		if( !result )
			return utf8_string::npos;
		return result - buffer;
	}
	/**
	 * Finds a specific pattern within the utf8_string starting at the supplied byte position
	 * 
	 * @param	needle		The pattern to look for
	 * @param	start_byte	The byte position of the first code point to start looking from
	 * @return	The byte position where and if the pattern was found or utf8_string::npos
	 */
	size_type raw_find( const char* pattern , size_type start_byte = 0 ) const {
		if( start_byte >= size() )
			return utf8_string::npos;
		const char* buffer = get_buffer();
		const char* result = std::strstr( buffer + start_byte , pattern );
		if( !result )
			return utf8_string::npos;
		return result - buffer;
	}
	
	/**
	 * Finds the last occourence of a specific code point inside the
	 * utf8_string starting backwards at the supplied code point index
	 * 
	 * @param	cp				The code point to look for
	 * @param	start_codepoint	The index of the first code point to start looking from (backwards)
	 * @return	The code point index where and if the code point was found or utf8_string::npos
	 */
	size_type rfind( value_type cp , size_type start_codepoint = utf8_string::npos ) const {
		const_reverse_iterator  end = rend(), it;
		size_type               string_len = length();
		if( start_codepoint >= string_len )
			it = crbegin(), start_codepoint = string_len - 1;
		else
			it = rget( start_codepoint );
		for( ; it != end ; ++it, --start_codepoint )
			if( *it == cp )
				return start_codepoint;
		return utf8_string::npos;
	}
	/**
	 * Finds the last occourence of a specific code point inside the
	 * utf8_string starting backwards at the supplied byte index
	 * 
	 * @param	cp				The code point to look for
	 * @param	start_codepoint	The byte index of the first code point to start looking from (backwards)
	 * @return	The code point index where and if the code point was found or utf8_string::npos
	 */
	size_type raw_rfind( value_type cp , size_type start_byte = utf8_string::npos ) const ;
	
	//! Find characters in string
	size_type find_first_of( const value_type* str , size_type start_codepoint = 0 ) const ;
	size_type raw_find_first_of( const value_type* str , size_type start_byte = 0 ) const ;
	size_type find_last_of( const value_type* str , size_type start_codepoint = utf8_string::npos ) const ;
	size_type raw_find_last_of( const value_type* str , size_type start_byte = utf8_string::npos ) const ;
	
	//! Find absence of characters in string
	size_type find_first_not_of( const value_type* str , size_type start_codepoint = 0 ) const ;
	size_type raw_find_first_not_of( const value_type* str , size_type start_byte = 0 ) const ;
	size_type find_last_not_of( const value_type* str , size_type start_codepoint = utf8_string::npos ) const ;
	size_type raw_find_last_not_of( const value_type* str , size_type start_byte = utf8_string::npos ) const ;
	
	
	//! Removes the last code point in the utf8_string
	inline utf8_string& pop_back(){
		size_type pos = back_index();
		return raw_erase( pos , get_index_bytes( pos ) );
	}
	
	/**
	 * Compares this utf8_string to the supplied one
	 *
	 * @param	str	The string to compare this one with
	 * @return	0	They compare equal
	 *			<0	Either the value of the first character that does not match is lower in
	 *			the compared string, or all compared characters match but the compared string is shorter.
	 *			>0	Either the value of the first character that does not match is greater in
	 *			the compared string, or all compared characters match but the compared string is longer.
	 */
	difference_type compare( const utf8_string& str ) const ;
	
	/**
	 * Returns true, if the supplied string compares unequal to this one.
	 *
	 * @param	str		The string to compare this one with
	 * @return	true	They compare unequal
	 *			false	They compare equal
	 */
	inline bool equals_not( const utf8_string& str ) const {
		size_type my_size = size();
		return my_size != str.size() || std::memcmp( str.data() , data() , my_size + 1 );
	}
	/**
	 * Returns true, if the supplied string compares unequal to this one.
	 *
	 * @param	str		The string to compare this one with, interpreted as UTF-8.
	 * @return	true	They compare equal
	 *			false	They compare different
	 */
	inline bool equals_not( const std::string& str ) const {
		size_type my_size = size();
		return my_size != str.size() || std::memcmp( str.data() , data() , my_size + 1 );
	}
	/**
	 * Returns true, if the supplied string compares unequal to the contents of this utf8 string.
	 * If this utf8 string contains embedded zeros, the result will always be false.
	 *
	 * @param	str		Null-terminated string literal, interpreted as UTF-8. The pointer is expected to be valid
	 * @return	true	They compare unequal
	 *			false	They compare equal
	 */
	inline bool equals_not( const char* str ) const {
		const char* it = data();
		for( const char* end = it + size() ; it != end && *it == *str ; ++it, ++str );
		return *it != *str; // Even if it == end, it points to the zero after the data, because its 0 terminated
	}
	/**
	 * Returns true, if the supplied string compares unequal to the contents of this utf8 string.
	 * If this utf8 string contains embedded zeros, the result will always be false.
	 *
	 * @param	str		Pointer to a null-terminated string literal, interpreted as UTF-32. The pointer is expected to be valid
	 * @return	true	They compare unequal
	 *			false	They compare equal
	 */
	inline bool equals_not( const value_type* str ) const {
		const_iterator it = cbegin();
		for( const_iterator end = cend() ; it != end && *str == *it ; ++it, ++str );
		return *it != *str; // Even if it == end, it points to the zero after the data, because its 0 terminated
	}
	
	//! Compare this utf8_string to another string
	inline bool operator==( const utf8_string& str ) const { return !equals_not( str ); }
	inline bool operator!=( const utf8_string& str ) const { return equals_not( str ); }
	inline bool operator==( const char* str ) const { return !equals_not( str ); }
	inline bool operator!=( const char* str ) const { return equals_not( str ); }
	inline bool operator==( const value_type* str ) const { return !equals_not( str ); }
	inline bool operator!=( const value_type* str ) const { return equals_not( str ); }
	inline bool operator==( const std::string& str ) const { return !equals_not( str ); }
	inline bool operator!=( const std::string& str ) const { return equals_not( str ); }
	
	inline bool operator>( const utf8_string& str ) const { return str.compare( *this ) > 0; }
	inline bool operator>=( const utf8_string& str ) const { return str.compare( *this ) >= 0; }
	inline bool operator<( const utf8_string& str ) const { return str.compare( *this ) < 0; }
	inline bool operator<=( const utf8_string& str ) const { return str.compare( *this ) <= 0; }
	
	
	//! Get the number of bytes of code point in utf8_string
	inline width_type get_index_bytes( size_type byte_index ) const {
		return get_codepoint_bytes( get_buffer()[byte_index] , size() - byte_index );
	}
	inline width_type get_codepoint_bytes( size_type codepoint_index ) const {
		return get_index_bytes( get_num_bytes_from_start( codepoint_index ) );
	}
	
	
	//! Get the number of bytes before a code point, that build up a new code point
	inline width_type get_index_pre_bytes( size_type byte_index ) const {
		const char* buffer = get_buffer();
		return get_num_bytes_of_utf8_char_before( buffer , byte_index );
	}
	inline width_type get_codepoint_pre_bytes( size_type codepoint_index ) const {
		return get_index_pre_bytes( get_num_bytes_from_start( codepoint_index ) );
	}
	
	
	//! Get the byte index of the last code point
	inline size_type	back_index() const { size_type s = size(); return s - get_index_pre_bytes( s ); }
	
	/**
	 * Counts the number of codepoints
	 * that are contained within the supplied range of bytes
	 */
	size_type			get_num_codepoints( size_type byte_start , size_type byte_count ) const ;
	
	/**
	 * Counts the number of bytes required to hold the supplied amount of codepoints
	 * starting at the supplied byte index (or '0' for the '_from_start' version)
	 */
	size_type			get_num_bytes( size_type byte_start , size_type cp_count ) const ;
	size_type			get_num_bytes_from_start( size_type cp_count ) const ;
	
	
	//! Friend iterator difference computation functions
	friend int operator-( const const_iterator& lhs , const const_iterator& rhs );
	friend int operator-( const const_reverse_iterator& lhs , const const_reverse_iterator& rhs );
};

//! Compare two iterators
static inline bool operator>( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs ){
	return lhs.get_index() > rhs.get_index();
}
static inline bool operator>( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs ){
	return lhs.get_index() < rhs.get_index();
}
static inline bool operator>=( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs ){
	return lhs.get_index() >= rhs.get_index();
}
static inline bool operator>=( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs ){
	return lhs.get_index() <= rhs.get_index();
}
static inline bool operator<( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs ){
	return lhs.get_index() < rhs.get_index();
}
static inline bool operator<( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs ){
	return lhs.get_index() > rhs.get_index();
}
static inline bool operator<=( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs ){
	return lhs.get_index() <= rhs.get_index();
}
static inline bool operator<=( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs ){
	return lhs.get_index() >= rhs.get_index();
}


//! Compute distance between iterators
extern int operator-( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs );
extern int operator-( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs );

//! Stream Operations
extern std::ostream& operator<<( std::ostream& stream , const utf8_string& str );
extern std::istream& operator>>( std::istream& stream , utf8_string& str );

#endif
