/**
 * Copyright (c) 2019 Jakob Riedle (DuffsDevice)
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
#include <limits> // for numeric_limits
#include <cstddef> // for ptrdiff_t, size_t and offsetof
#include <cstdint> // for uint8_t, uint16_t, uint32_t, std::uint_least16_t, std::uint_fast32_t
#include <stdexcept> // for std::out_of_range
#include <initializer_list> // for std::initializer_list
#include <iosfwd> // for ostream/istream forward declarations
#ifdef _MSC_VER
#include <intrin.h> // for __lzcnt
#endif

namespace tiny_utf8_detail
{
	// Used for tag dispatching in constructor
	struct read_codepoints_tag{};
	struct read_bytes_tag{};
	
	//! Count leading zeros utility
	#if defined(__GNUC__)
		#ifndef TINY_UTF8_HAS_CLZ
			#define TINY_UTF8_HAS_CLZ true
		#endif
		static inline unsigned int clz( unsigned int val ){ return __builtin_clz( val ); }
		static inline unsigned int clz( unsigned long int val ){ return __builtin_clzl( val ); }
		static inline unsigned int clz( char32_t val ){
			return sizeof(char32_t) == sizeof(unsigned long int) ? __builtin_clzl( val ) : __builtin_clz( val );
		}
	#elif defined(_MSC_VER)
		#ifndef TINY_UTF8_HAS_CLZ
			#define TINY_UTF8_HAS_CLZ true
		#endif
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
	enum : size_type{					npos = (size_type)-1 };
	
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
	using enable_if_small_string = typename std::enable_if<L<=get_sso_capacity(),bool>::type;
	template<size_type L>
	using enable_if_not_small_string = typename std::enable_if<get_sso_capacity()<L,bool>::type;
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
		, bool
	>::type;
	
	//! Check, if the lut is active using the lut base ptr
	static inline bool					is_lut_active( const char* lut_base_ptr ){ return *((const unsigned char*)lut_base_ptr) & 0x1; }
	
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
	
	//! Determine, whether or not a LUT is worth to set up. General case: worth below 25%. If LUT present <33,3%, otherwise <16,7%
	static inline bool					is_lut_worth( size_type pot_lut_len , size_type string_len , bool lut_present , bool biased = true ){
		size_type threshold = biased ? ( lut_present ? string_len / 3u : string_len / 6u ) : string_len / 4u;
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
	#if defined(TINY_UTF8_HAS_CLZ) && TINY_UTF8_HAS_CLZ == true
	static inline width_type			get_codepoint_bytes( char first_byte , size_type data_left )
	{
		if( first_byte ){
			// Before counting the leading one's we need to shift the byte into the most significant part of the integer
			size_type codepoint_bytes = tiny_utf8_detail::clz( ~((unsigned int)first_byte << (sizeof(unsigned int)-1)*8 ) );
			
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
		#if defined(TINY_UTF8_HAS_CLZ) && TINY_UTF8_HAS_CLZ == true
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
			case 7: dest[cp_bytes-6] = 0x80 | ((cp >> 30) & 0x3F); [[fallthrough]];
			case 6: dest[cp_bytes-5] = 0x80 | ((cp >> 24) & 0x3F); [[fallthrough]];
			case 5: dest[cp_bytes-4] = 0x80 | ((cp >> 18) & 0x3F); [[fallthrough]];
			case 4: dest[cp_bytes-3] = 0x80 | ((cp >> 12) & 0x3F); [[fallthrough]];
			case 3: dest[cp_bytes-2] = 0x80 | ((cp >>  6) & 0x3F); [[fallthrough]];
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
		else if( tiny_utf8_detail::is_little_endian::value ){
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
	inline size_type	get_non_sso_string_len() const {
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
	utf8_string( const char* str , size_type len , tiny_utf8_detail::read_codepoints_tag );
	utf8_string( const char* str , size_type len , tiny_utf8_detail::read_bytes_tag );
	
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
	inline utf8_string( T&& str , enable_if_ptr<T,char>* = {} ) :
		utf8_string( str , utf8_string::npos , tiny_utf8_detail::read_codepoints_tag() )
	{}
	inline utf8_string( const char* str , size_type len ) :
		utf8_string( str , len , tiny_utf8_detail::read_codepoints_tag() )
	{}
	/**
	 * Constructor taking an utf8 char literal
	 * 
	 * @note	Creates an Instance of type utf8_string that holds the given utf8 sequence
	 * @param	str		The UTF-8 literal to fill the utf8_string with
	 */
	template<size_type LITLEN>
	inline utf8_string( const char (&str)[LITLEN] , enable_if_small_string<LITLEN> = {} ){
		std::memcpy( t_sso.data , str , LITLEN );
		if( str[LITLEN-1] ){
			t_sso.data[LITLEN] = '\0';
			set_sso_data_len( LITLEN );
		}
		else
			set_sso_data_len( LITLEN - 1 );
	}
	template<size_type LITLEN>
	inline utf8_string( const char (&str)[LITLEN] , enable_if_not_small_string<LITLEN> = {} ) :
		utf8_string( str , LITLEN - ( str[LITLEN-1] ? 0 : 1 ) , tiny_utf8_detail::read_bytes_tag() )
	{}
	/**
	 * Constructor taking an std::string
	 * 
	 * @note	Creates an Instance of type utf8_string that holds the given data sequence
	 * @param	str		The byte data, that will be interpreted as UTF-8
	 */
	inline utf8_string( std::string str ) :
		utf8_string( str.c_str() , str.length() , tiny_utf8_detail::read_bytes_tag() )
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
	explicit inline utf8_string( value_type cp ) : t_sso( cp = encode_utf8( cp , t_sso.data ) ) {
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
	utf8_string( const value_type* str , size_type len );
	template<typename T>
	utf8_string( T&& str , enable_if_ptr<T,value_type>* = {} ) :
		utf8_string( str , utf8_string::npos )
	{}
	template<size_type LITLEN>
	inline utf8_string( const value_type (&str)[LITLEN] ) :
		utf8_string( str , LITLEN - ( str[LITLEN-1] ? 0 : 1 ) )
	{}
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
			std::memcpy( (void*)this , &str , sizeof(utf8_string) ); // Copy data
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
			std::memcpy( (void*)&str , this , sizeof(utf8_string) );
			std::memcpy( (void*)this , &tmp , sizeof(utf8_string) );
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
	inline utf8_string& assign( T&& str , enable_if_ptr<T,char>* = {} ){
		return *this = utf8_string( str );
	}
	inline utf8_string& assign( const char* str , size_type len ){
		return *this = utf8_string( str , len );
	}
	/**
	 * Assigns an utf8 char literal to this string (with possibly embedded '\0's)
	 * 
	 * @param	str		The UTF-8 literal to fill the utf8_string with
	 */
	template<size_type LITLEN>
	inline utf8_string& assign( const char (&str)[LITLEN] ){
		return *this = utf8_string( str );
	}
	/**
	 * Assigns an utf-32 sequence and the maximum length to read from it (in number of codepoints) to this string
	 * 
	 * @param	str		The UTF-8 sequence to fill the this utf8_string with
	 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
	 */
	template<typename T>
	inline utf8_string& assign( T&& str , enable_if_ptr<T,value_type>* = {} ){
		return *this = utf8_string( str );
	}
	inline utf8_string& assign( const value_type* str , size_type len ){
		return *this = utf8_string( str , len );
	}
	/**
	 * Assigns an utf-32 char literal to this string (with possibly embedded '\0's)
	 * 
	 * @param	str		The UTF-32 literal to fill the utf8_string with
	 */
	template<size_type LITLEN>
	inline utf8_string& assign( const value_type (&str)[LITLEN] ){
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
	utf8_string substr( size_type pos , size_type len = utf8_string::npos ) const {
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
	 * Compare this string with the supplied one.
	 *
	 * @param	str		The string to compare this one with
	 * @return	0	They compare equal
	 *			<0	Either the value of the first character that does not match is lower in
	 *			the compared string, or all compared characters match but the compared string is shorter.
	 *			>0	Either the value of the first character that does not match is greater in
	 *			the compared string, or all compared characters match but the compared string is longer.
	 */
	inline int compare( const utf8_string& str ) const {
		size_type my_size = size(), str_size = str.size();
		if( my_size != str_size )
			return my_size < str_size ? -1 : 1;
		return std::memcmp( data() , str.data() , my_size );
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
	inline int compare( const std::string& str ) const {
		size_type my_size = size(), str_size = str.size();
		if( my_size != str_size )
			return my_size < str_size ? -1 : 1;
		return std::memcmp( data() , str.data() , my_size );
	}
	/**
	 * Compares this string with the supplied one.
	 * Thes supplied string literal is considered to end with the trailling '\0'.
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
	int compare( T str , enable_if_ptr<T,char>* = {} ) const {
		const char* it = data(), *end = it + size();
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
	int compare( const char (&str)[LITLEN] ) const {
		const char* it = data(), *end = it + size();
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
	 * Thes supplied string literal is considered to end with the trailling '\0'.
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
	int compare( T str , enable_if_ptr<T,value_type>* = {} ) const {
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
	int compare( const value_type (&str)[LITLEN] ) const {
		const_iterator	it = cbegin(), end = cend();
		size_type index = 0, length = str[LITLEN-1] ? LITLEN : LITLEN-1;
		while( it != end && index < length ){
			if( *it != str[index] )
				return *it < str[index] ? -1 : 1;
			++it, ++index;
		}
		return index < length ? -1 : it == end ? 0 : 1;
	}
	
	//! Equality Comparison Operators
	inline bool operator==( const utf8_string& str ) const { return compare( str ) == 0; }
	inline bool operator!=( const utf8_string& str ) const { return compare( str ) != 0; }
	inline bool operator==( const std::string& str ) const { return compare( str ) == 0; }
	inline bool operator!=( const std::string& str ) const { return compare( str ) != 0; }
	template<typename T> inline enable_if_ptr<T,char> operator==( T&& str ) const { return compare( str ) == 0; }
	template<typename T> inline enable_if_ptr<T,char> operator!=( T&& str ) const { return compare( str ) != 0; }
	template<typename T> inline enable_if_ptr<T,value_type> operator==( T&& str ) const { return compare( str ) == 0; }
	template<typename T> inline enable_if_ptr<T,value_type> operator!=( T&& str ) const { return compare( str ) != 0; }
	template<size_type LITLEN> inline bool operator==( const char (&str)[LITLEN] ) const { return compare( str ) == 0; }
	template<size_type LITLEN> inline bool operator!=( const char (&str)[LITLEN] ) const { return compare( str ) != 0; }
	template<size_type LITLEN> inline bool operator==( const value_type (&str)[LITLEN] ) const { return compare( str ) == 0; }
	template<size_type LITLEN> inline bool operator!=( const value_type (&str)[LITLEN] ) const { return compare( str ) != 0; }
	
	//! Lexicographical comparison Operators
	inline bool operator>( const utf8_string& str ) const { return compare( str ) > 0; }
	inline bool operator>=( const utf8_string& str ) const { return compare( str ) >= 0; }
	inline bool operator<( const utf8_string& str ) const { return compare( str ) < 0; }
	inline bool operator<=( const utf8_string& str ) const { return compare( str ) <= 0; }
	inline bool operator>( const std::string& str ) const { return compare( str ) > 0; }
	inline bool operator>=( const std::string& str ) const { return compare( str ) >= 0; }
	inline bool operator<( const std::string& str ) const { return compare( str ) < 0; }
	inline bool operator<=( const std::string& str ) const { return compare( str ) <= 0; }
	template<typename T> inline enable_if_ptr<T,char> operator>( T&& str ) const { return compare( str ) > 0; }
	template<typename T> inline enable_if_ptr<T,char> operator>=( T&& str ) const { return compare( str ) >= 0; }
	template<typename T> inline enable_if_ptr<T,char> operator<( T&& str ) const { return compare( str ) < 0; }
	template<typename T> inline enable_if_ptr<T,char> operator<=( T&& str ) const { return compare( str ) <= 0; }
	template<typename T> inline enable_if_ptr<T,value_type> operator>( T&& str ) const { return compare( str ) > 0; }
	template<typename T> inline enable_if_ptr<T,value_type> operator>=( T&& str ) const { return compare( str ) >= 0; }
	template<typename T> inline enable_if_ptr<T,value_type> operator<( T&& str ) const { return compare( str ) < 0; }
	template<typename T> inline enable_if_ptr<T,value_type> operator<=( T&& str ) const { return compare( str ) <= 0; }
	template<size_type LITLEN> inline bool operator>( const char (&str)[LITLEN] ) const { return compare( str ) > 0; }
	template<size_type LITLEN> inline bool operator>=( const char (&str)[LITLEN] ) const { return compare( str ) >= 0; }
	template<size_type LITLEN> inline bool operator<( const char (&str)[LITLEN] ) const { return compare( str ) < 0; }
	template<size_type LITLEN> inline bool operator<=( const char (&str)[LITLEN] ) const { return compare( str ) <= 0; }
	template<size_type LITLEN> inline bool operator>( const value_type (&str)[LITLEN] ) const { return compare( str ) > 0; }
	template<size_type LITLEN> inline bool operator>=( const value_type (&str)[LITLEN] ) const { return compare( str ) >= 0; }
	template<size_type LITLEN> inline bool operator<( const value_type (&str)[LITLEN] ) const { return compare( str ) < 0; }
	template<size_type LITLEN> inline bool operator<=( const value_type (&str)[LITLEN] ) const { return compare( str ) <= 0; }
	
	
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
	
	
public: //! tinyutf8-specific features
	
	
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
	 * Determine, if small string optimization is active
	 * @return	True, if the utf8 data is stored within the utf8_string object itself
	 *			false, otherwise
	 */
	inline bool lut_active() const { return sso_inactive() && utf8_string::is_lut_active( utf8_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size ) ); }
	
	
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
	 * Get the raw data contained in this utf8_string wrapped by an std::string
	 * 
	 * @note	Returns the UTF-8 formatted content of this utf8_string
	 * @return	UTF-8 formatted data, wrapped inside an std::string
	 */
	inline std::string cpp_str( bool prepend_bom = false ) const { return prepend_bom ? cpp_str_bom() : std::string( c_str() , size() ); }
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

#if defined(TINY_UTF8_FORWARD_DECLARE_ONLY) && TINY_UTF8_FORWARD_DECLARE_ONLY == true
	//! Compute distance between iterators
	extern int operator-( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs );
	extern int operator-( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs );

	//! Stream Operations
	extern std::ostream& operator<<( std::ostream& stream , const utf8_string& str );
	extern std::istream& operator>>( std::istream& stream , utf8_string& str );
#else
#include <ostream>
#include <istream>
#include <algorithm>

inline utf8_string::width_type utf8_string::get_lut_width( size_type buffer_size ){
	return buffer_size <= std::numeric_limits<std::uint8_t>::max()
		? sizeof(std::uint8_t)
		: buffer_size <= std::numeric_limits<std::uint16_t>::max()
			? sizeof(std::uint16_t)
			: buffer_size <= std::numeric_limits<std::uint32_t>::max()
				? sizeof(std::uint32_t)
				: sizeof(std::uint64_t)
	;
}

inline utf8_string::size_type utf8_string::determine_main_buffer_size( size_type data_len , size_type lut_len , width_type* lut_width ){
	size_type width_guess	= get_lut_width( ++data_len ); // Don't forget, we need a terminating '\0', distinct from the lut indicator
	data_len += lut_len * width_guess; // Add the estimated number of bytes from the lut
	data_len += lut_len * ( ( *lut_width = get_lut_width( data_len ) ) - width_guess ); // Adjust the added bytes from the lut
	return round_up_to_align( data_len ); // Make the buffer size_type-aligned
}

inline utf8_string::size_type utf8_string::determine_main_buffer_size( size_type data_len , size_type lut_len , width_type lut_width ){
	return round_up_to_align( data_len + 1 + lut_len * lut_width ); // Compute the size_type-aligned buffer size
}

inline utf8_string::size_type utf8_string::determine_main_buffer_size( size_type data_len ){
	// Make the buffer size_type-aligned
	return round_up_to_align( data_len + 1 );
}

utf8_string::utf8_string( utf8_string::size_type count , utf8_string::value_type cp ) :
	t_sso( 0 )
{
	if( !count )
		return;
	
	width_type	num_bytes_per_cp = get_codepoint_bytes( cp );
	size_type	data_len = num_bytes_per_cp * count;
	char*		buffer;
	
	// Need to allocate a buffer?
	if( data_len > utf8_string::get_sso_capacity() )
	{
		// Determine the buffer size
		size_type buffer_size	= determine_main_buffer_size( data_len );
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		
		// Set Attributes
		set_lut_indiciator( buffer + buffer_size , num_bytes_per_cp == 1 , 0 ); // Set lut indicator
		t_non_sso.buffer_size = buffer_size;
		t_non_sso.data_len = data_len;
		set_non_sso_string_len( count ); // This also disables SSO
	}
	else{
		buffer = t_sso.data;
		
		// Set Attributes
		set_sso_data_len( data_len );
	}
	
	// Fill the buffer
	if( num_bytes_per_cp > 1 ){
		utf8_string::encode_utf8( cp , buffer , num_bytes_per_cp );
		for( char* buffer_iter = buffer ; --count > 0 ; )
			std::memcpy( buffer_iter += num_bytes_per_cp , buffer , num_bytes_per_cp );
	}
	else
		std::memset( buffer , cp , count );
	
	// Set trailling zero (in case sso is inactive, this trailing zero also indicates, that no multibyte-tracking is enabled)
	buffer[data_len] = 0;
}

utf8_string::utf8_string( utf8_string::size_type count , char cp ) :
	t_sso( 0 )
{
	if( !count )
		return;
	
	char*		buffer;
	
	// Need to allocate a buffer?
	if( count > utf8_string::get_sso_capacity() )
	{
		size_type buffer_size = determine_main_buffer_size( count );
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		
		// Set Attributes
		set_lut_indiciator( buffer + buffer_size , true , 0 ); // Set lut indicator
		t_non_sso.buffer_size = buffer_size;
		t_non_sso.data_len = count;
		set_non_sso_string_len( count ); // This also disables SSO
	}
	else{
		buffer = t_sso.data;
		set_sso_data_len( count );
	}
	
	// Fill the buffer
	std::memset( buffer , cp , count );
	
	// Set trailling zero (in case sso is inactive, this trailing zero also indicates, that no multibyte-tracking is enabled)
	buffer[count] = 0;
}

utf8_string::utf8_string( const char* str , size_type len , tiny_utf8_detail::read_codepoints_tag ) :
	utf8_string()
{
	if( !len )
		return;
	
	size_type		num_multibytes = 0;
	size_type		data_len = 0;
	size_type		string_len = 0;
	
	// Count bytes, multibytes and string length
	while( string_len < len && ( str[data_len] || len != utf8_string::npos ) )
	{
		// Read number of bytes of current code point
		width_type bytes = get_codepoint_bytes( str[data_len] , utf8_string::npos );
		data_len		+= bytes;		// Increase number of bytes
		string_len		+= 1;			// Increase number of code points
		num_multibytes	+= bytes > 1;	// Increase number of occoured multibytes?
	}
	
	char*	buffer;
	
	// Need heap memory?
	if( data_len > utf8_string::get_sso_capacity() )
	{
		if( utf8_string::is_lut_worth( num_multibytes , string_len , false , false ) )
		{
			// Determine the buffer size (excluding the lut indicator) and the lut width
			width_type	lut_width;
			size_type	buffer_size	= determine_main_buffer_size( data_len , num_multibytes , &lut_width );
			buffer					= t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ]; // The LUT indicator is 
			
			// Set up LUT
			char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
			utf8_string::set_lut_indiciator( lut_iter , true , num_multibytes ); // Set the LUT indicator
			
			// Fill the lut and copy bytes
			char* buffer_iter = buffer;
			const char* str_iter = str;
			const char* str_end = str + data_len; // Compute End of the string
			while( str_iter < str_end )
			{
				width_type bytes = get_codepoint_bytes( *str_iter , str_end - str_iter );
				switch( bytes )
				{
					case 7:	buffer_iter[6] = str_iter[6]; [[fallthrough]]; // Copy data byte
					case 6:	buffer_iter[5] = str_iter[5]; [[fallthrough]]; // Copy data byte
					case 5:	buffer_iter[4] = str_iter[4]; [[fallthrough]]; // Copy data byte
					case 4:	buffer_iter[3] = str_iter[3]; [[fallthrough]]; // Copy data byte
					case 3:	buffer_iter[2] = str_iter[2]; [[fallthrough]]; // Copy data byte
					case 2:	buffer_iter[1] = str_iter[1]; // Copy data byte
						// Set next entry in the LUT!
						utf8_string::set_lut( lut_iter -= lut_width , lut_width , str_iter - str );
						[[fallthrough]];
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
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		
		// Set up LUT
		char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
		utf8_string::set_lut_indiciator( lut_iter , num_multibytes == 0 , 0 ); // Set the LUT indicator
		
		// Set Attributes
		t_non_sso.buffer_size = buffer_size;
		t_non_sso.data_len = data_len;
		set_non_sso_string_len( string_len );
	}
	else{
		buffer = t_sso.data;
		
		// Set Attrbutes
		set_sso_data_len( data_len );
		
		// Set up LUT: Not necessary, since the LUT is automatically inactive,
		// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
	}
	
	//! Fill the buffer
	std::memcpy( buffer , str , data_len );
	buffer[data_len] = '\0';
}

utf8_string::utf8_string( const char* str , size_type data_len , tiny_utf8_detail::read_bytes_tag ) :
	utf8_string()
{
	if( !data_len )
		return;
	
	size_type		num_multibytes = 0;
	size_type		index = 0;
	size_type		string_len = 0;
	
	// Count bytes, multibytes and string length
	while( index < data_len )
	{
		// Read number of bytes of current code point
		width_type bytes = get_codepoint_bytes( str[index] , utf8_string::npos );
		index			+= bytes;		// Increase number of bytes
		string_len		+= 1;			// Increase number of code points
		num_multibytes	+= bytes > 1;	// Increase number of occoured multibytes?
	}
	
	char*	buffer;
	
	// Need heap memory?
	if( data_len > utf8_string::get_sso_capacity() )
	{
		if( utf8_string::is_lut_worth( num_multibytes , string_len , false , false ) )
		{
			// Determine the buffer size (excluding the lut indicator) and the lut width
			width_type	lut_width;
			size_type	buffer_size	= determine_main_buffer_size( data_len , num_multibytes , &lut_width );
			buffer					= t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ]; // The LUT indicator is 
			
			// Set up LUT
			char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
			utf8_string::set_lut_indiciator( lut_iter , true , num_multibytes ); // Set the LUT indicator
			
			// Fill the lut and copy bytes
			char* buffer_iter = buffer;
			const char* str_iter = str;
			const char* str_end = str + data_len; // Compute End of the string
			while( str_iter < str_end )
			{
				width_type bytes = get_codepoint_bytes( *str_iter , str_end - str_iter );
				switch( bytes )
				{
					case 7:	buffer_iter[6] = str_iter[6]; [[fallthrough]]; // Copy data byte
					case 6:	buffer_iter[5] = str_iter[5]; [[fallthrough]]; // Copy data byte
					case 5:	buffer_iter[4] = str_iter[4]; [[fallthrough]]; // Copy data byte
					case 4:	buffer_iter[3] = str_iter[3]; [[fallthrough]]; // Copy data byte
					case 3:	buffer_iter[2] = str_iter[2]; [[fallthrough]]; // Copy data byte
					case 2:	buffer_iter[1] = str_iter[1]; // Copy data byte
						// Set next entry in the LUT!
						utf8_string::set_lut( lut_iter -= lut_width , lut_width , str_iter - str );
						[[fallthrough]];
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
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		
		// Set up LUT
		char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
		utf8_string::set_lut_indiciator( lut_iter , num_multibytes == 0 , 0 ); // Set the LUT indicator
		
		// Set Attributes
		t_non_sso.buffer_size = buffer_size;
		t_non_sso.data_len = data_len;
		set_non_sso_string_len( string_len );
	}
	else{
		buffer = t_sso.data;
		
		// Set Attrbutes
		set_sso_data_len( data_len );
		
		// Set up LUT: Not necessary, since the LUT is automatically inactive,
		// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
	}
	
	//! Fill the buffer
	std::memcpy( buffer , str , data_len );
	buffer[data_len] = '\0';
}

utf8_string::utf8_string( const value_type* str , size_type len ) :
	utf8_string()
{
	if( !len )
		return;
	
	size_type		num_multibytes = 0;
	size_type		data_len = 0;
	size_type		string_len = 0;
	
	// Count bytes, mutlibytes and string length
	while( string_len < len && ( str[string_len] || len != utf8_string::npos ) )
	{
		// Read number of bytes of current code point
		width_type bytes = get_codepoint_bytes( str[string_len] );
		
		data_len		+= bytes;		// Increase number of bytes
		string_len		+= 1;			// Increase number of code points
		num_multibytes	+= bytes > 1 ;	// Increase number of occoured multibytes?
	}
	
	char*	buffer;
	
	// Need heap memory?
	if( data_len > utf8_string::get_sso_capacity() )
	{
		if( utf8_string::is_lut_worth( num_multibytes , string_len , false , false ) )
		{	
			// Determine the buffer size (excluding the lut indicator) and the lut width
			width_type lut_width;
			size_type buffer_size	= determine_main_buffer_size( data_len , num_multibytes , &lut_width );
			buffer					= t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ]; // The LUT indicator is 
			
			//! Fill LUT
			char* lut_iter		= utf8_string::get_lut_base_ptr( buffer , buffer_size );
			char* buffer_iter	= buffer;
			utf8_string::set_lut_indiciator( lut_iter , true , num_multibytes ); // Set the LUT indicator
			
			// Iterate through wide char literal
			for( size_type i = 0 ; i < string_len ; i++ )
			{
				// Encode wide char to utf8
				width_type codepoint_bytes = utf8_string::encode_utf8( str[i] , buffer_iter );
				
				// Push position of character to 'indices'
				if( codepoint_bytes > 1 )
					utf8_string::set_lut( lut_iter -= lut_width , lut_width , buffer_iter - buffer );
				
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
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		
		// Set up LUT
		char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
		utf8_string::set_lut_indiciator( lut_iter , num_multibytes == 0 , 0 ); // Set the LUT indicator
		
		// Set Attributes
		t_non_sso.buffer_size = buffer_size;
		t_non_sso.data_len = data_len;
		set_non_sso_string_len( string_len );
	}
	else{
		buffer = t_sso.data;
		
		// Set Attrbutes
		set_sso_data_len( data_len );
		
		// Set up LUT: Not necessary, since the LUT is automatically inactive,
		// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
	}
	
	char* buffer_iter = buffer;
	
	// Iterate through wide char literal
	for( size_type i = 0 ; i < string_len ; i++ )
		// Encode wide char to utf8 and step forward the number of bytes it took
		buffer_iter += utf8_string::encode_utf8( str[i] , buffer_iter );
	
	*buffer_iter = '\0'; // Set trailing '\0'
}

utf8_string::width_type utf8_string::get_num_bytes_of_utf8_char_before( const char* data_start , size_type index )
{
	data_start += index;
	// Only Check the possibilities, that could appear
	switch( index )
	{
		default:
			if( ((unsigned char)data_start[-7] & 0xFE ) == 0xFC )	// 11111110 seven bytes
				return 7;
			[[fallthrough]];
		case 6:
			if( ((unsigned char)data_start[-6] & 0xFE ) == 0xFC )	// 1111110X six bytes
				return 6;
			[[fallthrough]];
		case 5:
			if( ((unsigned char)data_start[-5] & 0xFC ) == 0xF8 )	// 111110XX five bytes
				return 5;
			[[fallthrough]];
		case 4:
			if( ((unsigned char)data_start[-4] & 0xF8 ) == 0xF0 )	// 11110XXX four bytes
				return 4;
			[[fallthrough]];
		case 3:
			if( ((unsigned char)data_start[-3] & 0xF0 ) == 0xE0 )	// 1110XXXX three bytes
				return 3;
			[[fallthrough]];
		case 2:
			if( ((unsigned char)data_start[-2] & 0xE0 ) == 0xC0 )	// 110XXXXX two bytes
				return 2;
			[[fallthrough]];
		case 1:
		case 0:
			return 1;
	}
}

#if !defined(TINY_UTF8_HAS_CLZ) || TINY_UTF8_HAS_CLZ == false
utf8_string::width_type utf8_string::get_codepoint_bytes( char first_byte , size_type data_left )
{
	// Only Check the possibilities, that could appear
	switch( data_left )
	{
		default:
			if( (first_byte & 0xFF) == 0xFE )	// 11111110 seven bytes
				return 7;
		case 6:
			if( (first_byte & 0xFE) == 0xFC )	// 1111110X six bytes
				return 6;
		case 5:
			if( (first_byte & 0xFC) == 0xF8 )	// 111110XX five bytes
				return 5;
		case 4:
			if( (first_byte & 0xF8) == 0xF0 )	// 11110XXX four bytes
				return 4;
		case 3:
			if( (first_byte & 0xF0) == 0xE0 )	// 1110XXXX three bytes
				return 3;
		case 2:
			if( (first_byte & 0xE0) == 0xC0 )	// 110XXXXX two bytes
				return 2;
		case 1:
		case 0:
			return 1;
	}
}
#endif // !defined(TINY_UTF8_HAS_CLZ) || TINY_UTF8_HAS_CLZ == false

utf8_string& utf8_string::operator=( const utf8_string& str )
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
			const char* str_lut_base_ptr = utf8_string::get_lut_base_ptr( str.t_non_sso.data , str.t_non_sso.buffer_size );
			if( utf8_string::is_lut_active( str_lut_base_ptr ) )
			{
				width_type	lut_width = get_lut_width( t_non_sso.buffer_size ); // Lut width, if the current buffer is used
				size_type	str_lut_len = utf8_string::get_lut_len( str_lut_base_ptr );
				
				// Can the current buffer hold the data and the lut?
				if( utf8_string::determine_main_buffer_size( str.t_non_sso.data_len , str_lut_len , lut_width ) < t_non_sso.buffer_size )
				{
					width_type	str_lut_width = get_lut_width( str.t_non_sso.buffer_size );
					
					// How to copy indices?
					if( lut_width == str_lut_width ){
						str_lut_len *= str_lut_width; // Compute the size in bytes of the lut
						std::memcpy(
							utf8_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size ) - str_lut_len
							, str_lut_base_ptr - str_lut_len
							, str_lut_len + sizeof(indicator_type) // Also copy lut indicator
						);
					}
					else{
						char* lut_iter = utf8_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size );
						for( ; str_lut_len > 0 ; --str_lut_len )
							utf8_string::set_lut(
								lut_iter -= lut_width
								, lut_width
								, utf8_string::get_lut( str_lut_base_ptr -= str_lut_width , str_lut_width )
							);
					}
				}
				else
					goto lbl_replicate_whole_buffer;
			}
			// Is the current buffer too small to hold just the data?
			else if( utf8_string::determine_main_buffer_size( str.t_non_sso.data_len ) > t_non_sso.buffer_size )
				goto lbl_replicate_whole_buffer;
			
			// Copy data and lut indicator only
			std::memcpy( t_non_sso.data , str.t_non_sso.data , str.t_non_sso.data_len + 1 );
			utf8_string::copy_lut_indicator(
				utf8_string::get_lut_base_ptr( t_non_sso.data , t_non_sso.buffer_size )
				, str_lut_base_ptr
			);
			t_non_sso.data_len = str.t_non_sso.data_len;
			t_non_sso.string_len = str.t_non_sso.string_len; // Copy the string_len bit pattern
			return *this;
			
		lbl_replicate_whole_buffer: // Replicate the whole buffer
			delete[] t_non_sso.data;
		}
			[[fallthrough]];
		case 2: // [sso-active] = [sso-inactive]
			t_non_sso.data = new char[ utf8_string::determine_total_buffer_size( str.t_non_sso.buffer_size ) ];
			std::memcpy( t_non_sso.data , str.t_non_sso.data , str.t_non_sso.buffer_size + sizeof(indicator_type) ); // Copy data
			t_non_sso.buffer_size = str.t_non_sso.buffer_size;
			t_non_sso.data_len = str.t_non_sso.data_len;
			t_non_sso.string_len = str.t_non_sso.string_len; // This also disables SSO
			return *this;
		case 1: // [sso-inactive] = [sso-active]
			delete[] t_non_sso.data;
			[[fallthrough]];
		case 0: // [sso-active] = [sso-active]
			if( &str != this )
				std::memcpy( (void*)this , &str , sizeof(utf8_string) ); // Copy data
			return *this;
	}
	return *this;
}

void utf8_string::shrink_to_fit()
{
	if( sso_active() )
		return;
	
	size_type	data_len = size();
	
	if( !data_len )
		return;
	
	size_type	buffer_size = get_buffer_size();
	char*		buffer = get_buffer();
	char*		lut_base_ptr = utf8_string::get_lut_base_ptr( buffer , buffer_size );
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
		t_non_sso.data					= new char[ determine_total_buffer_size( required_buffer_size ) ];
		size_type	old_lut_width		= utf8_string::get_lut_width( buffer_size );
		char*		new_lut_base_ptr	= utf8_string::get_lut_base_ptr( t_non_sso.data , required_buffer_size );
		
		// Does the data type width change?
		if( old_lut_width != new_lut_width ){ // Copy indices one at a time
			utf8_string::set_lut_indiciator( new_lut_base_ptr , true , lut_len );
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
		
		t_non_sso.data = new char[ determine_total_buffer_size( required_buffer_size ) ]; // Allocate new buffer
	}
	
	// Copy BUFFER
	std::memcpy( t_non_sso.data , buffer , data_len + 1 );
	t_non_sso.buffer_size = required_buffer_size; // Set new buffer size
	
	// Delete old buffer
	delete[] buffer;
}

utf8_string::size_type utf8_string::get_non_sso_capacity() const
{
	size_type	data_len		= t_non_sso.data_len;
	size_type	buffer_size		= t_non_sso.buffer_size;
	
	// If empty, assume an average number of bytes per code point of '1' (and an empty lut)
	if( !data_len )
		return buffer_size - 1;
	
	const char*	buffer			= t_non_sso.data;
	size_type	string_len		= get_non_sso_string_len();
	const char*	lut_base_ptr	= utf8_string::get_lut_base_ptr( buffer , buffer_size );
	
	// If the lut is active, add the number of additional bytes to the current data length
	if( utf8_string::is_lut_active( lut_base_ptr ) )
		data_len += utf8_string::get_lut_width( buffer_size ) * utf8_string::get_lut_len( lut_base_ptr );
	
	// Return the buffer size (excluding the potential trailing '\0') divided by the average number of bytes per code point
	return ( buffer_size - 1 ) * string_len / data_len;
}

bool utf8_string::requires_unicode_sso() const
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

std::string utf8_string::cpp_str_bom() const
{
	// Create std::string
	std::string	result = std::string( size() + 3 , ' ' );
	char*		tmp_buffer = const_cast<char*>( result.data() );
	
	// Write BOM
	tmp_buffer[0] = char(0xEF);
	tmp_buffer[1] = char(0xBB);
	tmp_buffer[2] = char(0xBF);
	
	// Copy my data into it
	std::memcpy( tmp_buffer + 3 , get_buffer() , size() + 1 );
	
	return result;
}

utf8_string::size_type utf8_string::get_num_codepoints( size_type index , size_type byte_count ) const 
{
	const char*	buffer;
	size_type	data_len;
	
	if( sso_inactive() )
	{
		buffer					= t_non_sso.data;
		data_len				= t_non_sso.data_len;
		size_type buffer_size	= t_non_sso.buffer_size;
		const char*	lut_iter	= utf8_string::get_lut_base_ptr( buffer , buffer_size );
		
		// Is the LUT active?
		if( utf8_string::is_lut_active( lut_iter ) )
		{
			size_type lut_len = utf8_string::get_lut_len( lut_iter );
			
			if( !lut_len )
				return byte_count;
			
			width_type		lut_width	= utf8_string::get_lut_width( buffer_size );
			const char*		lut_begin	= lut_iter - lut_len * lut_width;
			size_type		end_index	= index + byte_count;
			
			// Iterate to the start of the relevant part of the multibyte table
			while( lut_iter > lut_begin ){
				lut_iter -= lut_width; // Move cursor to the next lut entry
				if( utf8_string::get_lut( lut_iter , lut_width ) >= index )
					break;
			}
			
			// Iterate over relevant multibyte indices
			while( lut_iter >= lut_begin ){
				size_type multibyte_index = utf8_string::get_lut( lut_iter , lut_width );
				if( multibyte_index >= end_index )
					break;
				byte_count -= utf8_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
				lut_iter -= lut_width; // Move cursor to the next lut entry
			}
			
			// Now byte_count is the number of code points
			return byte_count;
		}
	}
	else{
		buffer = t_sso.data;
		data_len = get_sso_data_len();
	}
	
	// Procedure: Reduce the byte count by the number of data bytes within multibytes
	const char*	buffer_iter = buffer + index;
	const char*	fragment_end = buffer_iter + byte_count;
	
	// Iterate the data byte by byte...
	while( buffer_iter < fragment_end ){
		width_type bytes = utf8_string::get_codepoint_bytes( *buffer_iter , fragment_end - buffer_iter );
		buffer_iter += bytes;
		byte_count -= bytes - 1;
	}
	
	// Now byte_count is the number of code points
	return byte_count;
}

utf8_string::size_type utf8_string::get_num_bytes_from_start( size_type cp_count ) const
{
	const char*	buffer;
	size_type	data_len;
	
	if( sso_inactive() )
	{
		buffer					= t_non_sso.data;
		data_len				= t_non_sso.data_len;
		size_type buffer_size	= t_non_sso.buffer_size;
		const char*	lut_iter	= utf8_string::get_lut_base_ptr( buffer , buffer_size );
		
		// Is the lut active?
		if( utf8_string::is_lut_active( lut_iter ) )
		{
			// Reduce the byte count by the number of data bytes within multibytes
			width_type lut_width = utf8_string::get_lut_width( buffer_size );
			
			// Iterate over relevant multibyte indices
			for( size_type lut_len = utf8_string::get_lut_len( lut_iter ) ; lut_len-- > 0 ; )
			{
				size_type multibyte_index = utf8_string::get_lut( lut_iter -= lut_width , lut_width );
				if( multibyte_index >= cp_count )
					break;
				cp_count += utf8_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
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

utf8_string::size_type utf8_string::get_num_bytes( size_type index , size_type cp_count ) const 
{
	size_type	potential_end_index = index + cp_count;
	const char*	buffer;
	size_type	data_len;
	
	// Procedure: Reduce the byte count by the number of utf8 data bytes
	if( sso_inactive() )
	{
		buffer					= t_non_sso.data;
		data_len				= t_non_sso.data_len;
		size_type buffer_size	= t_non_sso.buffer_size;
		const char*	lut_iter	= utf8_string::get_lut_base_ptr( buffer , buffer_size );
		
		// 'potential_end_index < index' is needed because of potential integer overflow in sum
		if( potential_end_index > data_len || potential_end_index < index )
			return data_len - index;
		
		// Is the lut active?
		if( utf8_string::is_lut_active( lut_iter ) )
		{
			size_type orig_index = index;
			size_type lut_len = utf8_string::get_lut_len( lut_iter );
			
			if( !lut_len )
				return cp_count;
			
			// Reduce the byte count by the number of data bytes within multibytes
			width_type		lut_width = utf8_string::get_lut_width( buffer_size );
			const char*		lut_begin = lut_iter - lut_len * lut_width;
			
			// Iterate to the start of the relevant part of the multibyte table
			for( lut_iter -= lut_width /* Move to first entry */ ; lut_iter >= lut_begin ; lut_iter -= lut_width )
				if( utf8_string::get_lut( lut_iter , lut_width ) >= index )
					break;
			
			// Add at least as many bytes as code points
			index += cp_count;
			
			// Iterate over relevant multibyte indices
			while( lut_iter >= lut_begin ){
				size_type multibyte_index = utf8_string::get_lut( lut_iter , lut_width );
				if( multibyte_index >= index )
					break;
				index += utf8_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
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

utf8_string utf8_string::raw_substr( size_type index , size_type byte_count ) const
{
	// Bound checks...
	size_type data_len = size();
	if( index > data_len )
		throw std::out_of_range( "utf8_string::(raw_)substr" );
	size_type		end_index = index + byte_count;
	if( end_index > data_len || end_index < index ){ // 'end_index < index' is needed because of potential integer overflow in sum
		end_index = data_len;
		byte_count = end_index - index;
	}
	
	// If the substring is not a substring
	if( byte_count == data_len )
		return *this;
	
	// If the new string is a small string
	if( byte_count <= utf8_string::get_sso_capacity() )
	{
		utf8_string	result;
		if( byte_count < utf8_string::get_sso_capacity() )
			result.set_sso_data_len( byte_count ); // Set length
		
		// Copy data
		std::memcpy( result.t_sso.data , get_buffer() + index , byte_count );
		result.t_sso.data[byte_count] = '\0';
		
		return result;
	}
	
	// At this point, sso must be inactive, because a substring can only be smaller than this string
	
	size_type mb_index;
	size_type substr_cps;
	size_type substr_mbs		= 0;
	size_type buffer_size		= t_non_sso.buffer_size;
	const char* buffer			= t_non_sso.data;
	const char* lut_base_ptr	= utf8_string::get_lut_base_ptr( buffer , buffer_size );
	bool lut_active				= utf8_string::is_lut_active( lut_base_ptr );
	width_type lut_width; // Ignore uninitialized warning, see [5]
	
	// Count the number of SUBSTRING Multibytes and codepoints
	if( lut_active )
	{
		lut_width	= utf8_string::get_lut_width( buffer_size );
		mb_index	= 0;
		const char* lut_begin	= lut_base_ptr - lut_width * utf8_string::get_lut_len( lut_base_ptr );
		const char* lut_iter	= lut_base_ptr;
		for( lut_iter -= lut_width; lut_iter >= lut_begin ; lut_iter -= lut_width ){
			if( utf8_string::get_lut( lut_iter , lut_width ) >= index )
				break;
			mb_index++;
		}
		substr_cps = byte_count; // Add at least as many bytes as code points
		for( ; lut_iter >= lut_begin ; lut_iter -= lut_width ){ // Iterate over relevant multibyte indices
			size_type multibyte_index = utf8_string::get_lut( lut_iter , lut_width );
			if( multibyte_index >= end_index )
				break;
			substr_cps -= utf8_string::get_codepoint_bytes( buffer[multibyte_index] , data_len - multibyte_index ); // Actually '- 1', but see[4]
			++substr_mbs;
		}
		substr_cps += substr_mbs; // [4]: We subtracted all bytes of the relevant multibytes. We therefore need to re-add substr_mbs code points.
	}
	else
	{
		substr_cps		= 0;
		size_type iter	= index;
		while( iter < end_index ){ // Count REPLACED multibytes and code points
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
	
	char* substr_buffer			= new char[ determine_total_buffer_size( substr_buffer_size ) ];
	char* substr_lut_base_ptr	= utf8_string::get_lut_base_ptr( substr_buffer , substr_buffer_size );
	
	// Copy requested BUFFER part
	std::memcpy( substr_buffer , buffer + index , byte_count );
	substr_buffer[byte_count] = '\0'; // Add trailing '\0'
	
	if( substr_lut_width )
	{
		// Set new lut size and mode
		utf8_string::set_lut_indiciator( substr_lut_base_ptr , true , substr_mbs );
		
		// Reuse old indices?
		if( lut_active )
		{
			// Can we do a plain copy of indices?
			if( index == 0 && substr_lut_width == lut_width ) // [5]: lut_width is initialized, as soon as 'lut_active' is true
				std::memcpy(
					substr_lut_base_ptr - substr_mbs * lut_width
					, lut_base_ptr - ( mb_index + substr_mbs ) * lut_width
					, substr_mbs * lut_width
				);
			else
				for( const char* lut_iter = lut_base_ptr - mb_index * lut_width; substr_mbs-- > 0 ; )
					utf8_string::set_lut(
						substr_lut_base_ptr -= substr_lut_width
						, substr_lut_width
						, utf8_string::get_lut( lut_iter -= lut_width , lut_width ) - index
					);
		}
		else // Fill the lut by iterating over the substrings data
			for( size_type	substr_iter = 0 ; substr_iter < byte_count ; ){
				width_type bytes = get_codepoint_bytes( substr_buffer[substr_iter] , byte_count - substr_iter );
				if( bytes > 1 )
					utf8_string::set_lut( substr_lut_base_ptr -= substr_lut_width , substr_lut_width , substr_iter );
				substr_iter += bytes;
			}
	}
	else // Set substring lut mode
		utf8_string::set_lut_indiciator( substr_lut_base_ptr , substr_mbs == 0 , 0 );
	
	// Prepare result
	utf8_string result;
	result.t_non_sso.data = substr_buffer;
	result.t_non_sso.data_len = byte_count;
	result.t_non_sso.buffer_size = substr_buffer_size;
	result.set_non_sso_string_len( substr_cps );
	
	return result;
}

utf8_string& utf8_string::append( const utf8_string& app )
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
	if( new_data_len <= utf8_string::get_sso_capacity() ){
		std::memcpy( t_sso.data + old_data_len , app.t_sso.data , app_data_len ); // Copy APPENDIX (must have sso active as well)
		t_sso.data[new_data_len] = 0; // Trailing '\0'
		set_sso_data_len( new_data_len ); // Adjust size
		return *this;
	}
	
	//! Ok, obviously no small string, we have to update the data, the lut and the number of code points
	
	
	// Count code points and multibytes of insertion
	bool		app_lut_active;
	const char*	app_buffer;
	const char*	app_lut_base_ptr;
	size_type	app_buffer_size;
	size_type	app_string_len;
	size_type	app_lut_len;
	if( app.sso_inactive() )
	{
		app_buffer_size	= app.t_non_sso.buffer_size;
		app_buffer		= app.t_non_sso.data;
		app_string_len	= app.get_non_sso_string_len();
		
		// Compute the number of multibytes
		app_lut_base_ptr = utf8_string::get_lut_base_ptr( app_buffer , app_buffer_size );
		app_lut_active = utf8_string::is_lut_active( app_lut_base_ptr );
		if( app_lut_active )
			app_lut_len = utf8_string::get_lut_len( app_lut_base_ptr );
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
		app_buffer_size	= utf8_string::get_sso_capacity();
		app_lut_len		= 0;
		for( size_type iter = 0 ; iter < app_data_len ; ){
			width_type bytes = get_codepoint_bytes( app_buffer[iter] , app_data_len - iter );
			app_lut_len += bytes > 1; iter += bytes; ++app_string_len;
		}
	}
	
	// Count code points and multibytes of this string
	char*		old_buffer;
	char*		old_lut_base_ptr; // Ignore uninitialized warning, see [3]
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
		old_lut_base_ptr = utf8_string::get_lut_base_ptr( old_buffer , old_buffer_size );
		if( (old_lut_active = utf8_string::is_lut_active( old_lut_base_ptr )) )
			old_lut_len = utf8_string::get_lut_len( old_lut_base_ptr );
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
		old_buffer_size	= utf8_string::get_sso_capacity();
		old_string_len	= 0;
		old_lut_len		= 0;
		size_type iter	= 0;
		old_lut_active	= false;
		while( iter < old_data_len ){ // Count multibytes and code points
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
	if( utf8_string::is_lut_worth( new_lut_len , new_string_len , old_lut_active , old_sso_inactive ) )
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
			new_lut_width = utf8_string::get_lut_width( old_buffer_size );
			
			// Append new INDICES
			char*		lut_dest_iter = old_lut_base_ptr - old_lut_len * new_lut_width;
			if( app_lut_active )
			{
				size_type	app_lut_width = utf8_string::get_lut_width( app_buffer_size );
				const char*	app_lut_iter = app_lut_base_ptr;
				while( app_lut_len-- > 0 )
					utf8_string::set_lut(
						lut_dest_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( app_lut_iter -= app_lut_width , app_lut_width ) + old_data_len
					);
			}
			else{
				size_type iter = 0;
				while( iter < app_data_len ){
					width_type bytes = get_codepoint_bytes( app_buffer[iter] , app_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , iter + old_data_len );
					iter += bytes;
				}
			}
			
			// Set new lut mode
			utf8_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len );
		}
		else // Set new lut mode
			utf8_string::set_lut_indiciator( old_lut_base_ptr , new_lut_len == 0 , 0 );
		
		// Update buffer and data_len
		std::memcpy( old_buffer + old_data_len , app_buffer , app_data_len ); // Copy BUFFER of the insertion
		old_buffer[new_data_len] = 0; // Trailing '\0'
	}
	else // No, apparently we have to allocate a new buffer...
	{
		new_buffer_size				<<= 1; // Allocate twice as much, in order to amortize allocations (keeping in mind alignment)
		char*	new_buffer			= new char[ determine_total_buffer_size( new_buffer_size ) ];
		char*	new_lut_base_ptr	= utf8_string::get_lut_base_ptr( new_buffer , new_buffer_size );
		
		// Write NEW BUFFER
		std::memcpy( new_buffer , old_buffer , old_data_len ); // Copy current BUFFER
		std::memcpy( new_buffer + old_data_len , app_buffer , app_data_len ); // Copy BUFFER of appendix
		new_buffer[new_data_len] = 0; // Trailing '\0'
		
		// Need to fill the lut? (see [2])
		if( new_lut_width )
		{
			// Update the lut width, since we doubled the buffer size a couple of lines above
			new_lut_width = utf8_string::get_lut_width( new_buffer_size );
			
			// Reuse indices from old lut?
			if( old_lut_active )
			{
				size_type	old_lut_width = utf8_string::get_lut_width( old_buffer_size );
			
				// Copy all old INDICES
				if( new_lut_width != old_lut_width )
				{
					char*		lut_iter = old_lut_base_ptr;
					char*		new_lut_iter = new_lut_base_ptr;
					size_type	num_indices = old_lut_len;
					while( num_indices-- > 0 )
						utf8_string::set_lut(
							new_lut_iter -= new_lut_width
							, new_lut_width
							, utf8_string::get_lut( lut_iter -= old_lut_width , old_lut_width )
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
				char*		new_lut_iter	= new_lut_base_ptr;
				size_type	iter			= 0;
				while( iter < old_data_len ){ // Fill lut with indices BEFORE insertion
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( new_lut_iter -= new_lut_width , new_lut_width , iter );
					iter += bytes;
				}
			}
			
			// Copy INDICES of the insertion
			char*		lut_dest_iter = new_lut_base_ptr - old_lut_len * new_lut_width;
			if( app_lut_active )
			{
				size_type	app_lut_width = utf8_string::get_lut_width( app_buffer_size );
				const char*	app_lut_iter = app_lut_base_ptr;
				while( app_lut_len-- > 0 )
					utf8_string::set_lut(
						lut_dest_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( app_lut_iter -= app_lut_width , app_lut_width ) + old_data_len
					);
			}
			else{
				size_type app_iter = 0;
				while( app_iter < app_data_len ){
					width_type bytes = get_codepoint_bytes( app_buffer[app_iter] , app_data_len - app_iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , app_iter + old_data_len );
					app_iter += bytes;
				}
			}
			
			utf8_string::set_lut_indiciator( new_lut_base_ptr , true , new_lut_len ); // Set new lut mode and len
		}
		else // Set new lut mode
			utf8_string::set_lut_indiciator( new_lut_base_ptr , new_lut_len == 0 , 0 );
	
		// Delete the old buffer?
		if( old_sso_inactive )
			delete[] old_buffer;
		
		// Set new Attributes
		t_non_sso.data			= new_buffer;
		t_non_sso.buffer_size	= new_buffer_size;
	}
	
	// Adjust Attributes
	t_non_sso.data_len = new_data_len;
	set_non_sso_string_len( new_string_len );
	
	return *this;
}

utf8_string& utf8_string::raw_insert( size_type index , const utf8_string& str )
{
	// Bound checks...
	size_type old_data_len = size();
	if( index > old_data_len )
		throw std::out_of_range( "utf8_string::(raw_)insert" );
	
	// Compute the updated metrics
	size_type str_data_len	= str.size();
	size_type new_data_len	= old_data_len + str_data_len;
	
	// Will be empty?
	if( str_data_len == 0 )
		return *this;
	
	// Will be sso string?
	if( new_data_len <= utf8_string::get_sso_capacity() )
	{
		// Copy AFTER inserted part, if it has moved in position
		std::memmove( t_sso.data + index + str_data_len , t_sso.data + index , old_data_len - index );
		
		// Copy INSERTION (Note: Since the resulting string is small, the insertion must be small as well!)
		std::memcpy( t_sso.data + index , str.t_sso.data , str_data_len );
		
		// Finish the new string object
		t_sso.data[new_data_len] = 0; // Trailing '\0'
		set_sso_data_len( new_data_len );
		
		return *this;
	}
	
	//! Ok, obviously no small string, we have to update the data, the lut and the number of code points
	
	
	// Count code points and multibytes of insertion
	bool		str_lut_active;
	const char*	str_buffer;
	const char*	str_lut_base_ptr;
	size_type	str_buffer_size;
	size_type	str_string_len;
	size_type	str_lut_len;
	if( str.sso_inactive() )
	{
		str_buffer_size	= str.t_non_sso.buffer_size;
		str_buffer			= str.t_non_sso.data;
		str_string_len		= str.get_non_sso_string_len();
		
		// Compute the number of multibytes
		str_lut_base_ptr = utf8_string::get_lut_base_ptr( str_buffer , str_buffer_size );
		str_lut_active = utf8_string::is_lut_active( str_lut_base_ptr );
		if( str_lut_active )
			str_lut_len = utf8_string::get_lut_len( str_lut_base_ptr );
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
		str_buffer_size	= utf8_string::get_sso_capacity();
		str_lut_len		= 0;
		for( size_type iter = 0 ; iter < str_data_len ; ){
			width_type bytes = get_codepoint_bytes( str_buffer[iter] , str_data_len - iter );
			str_lut_len += bytes > 1; iter += bytes; ++str_string_len;
		}
	}
	
	// Count code points and multibytes of this string
	char*		old_buffer;
	char*		old_lut_base_ptr; // Ignore uninitialized warning, see [3]
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
		while( iter < index ){ // Count multibytes and code points BEFORE insertion
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			mb_index += bytes > 1; iter += bytes;
		}
		// Count TOTAL multibytes
		old_lut_base_ptr = utf8_string::get_lut_base_ptr( old_buffer , old_buffer_size );
		if( (old_lut_active = utf8_string::is_lut_active( old_lut_base_ptr )) )
			old_lut_len = utf8_string::get_lut_len( old_lut_base_ptr );
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
		old_buffer_size	= utf8_string::get_sso_capacity();
		old_string_len	= 0;
		old_lut_len		= 0;
		size_type iter	= 0;
		old_lut_active	= false;
		while( iter < index ){ // Count multibytes and code points BEFORE insertion
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			mb_index += bytes > 1; iter += bytes; ++old_string_len;
		}
		old_lut_len = mb_index;
		while( iter < old_data_len ){ // Count multibytes and code points AFTER insertion
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
	if( utf8_string::is_lut_worth( new_lut_len , new_string_len , old_lut_active , old_sso_inactive ) )
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
			new_lut_width = utf8_string::get_lut_width( old_buffer_size );
			
			// Reuse indices from old lut?
			if( old_lut_active )
			{
				// Offset all indices
				char*		lut_iter = old_lut_base_ptr - mb_index * new_lut_width;
				size_type	num_indices = old_lut_len - mb_index;
				while( num_indices-- > 0 ){
					lut_iter -= new_lut_width;
					utf8_string::set_lut( lut_iter , new_lut_width , utf8_string::get_lut( lut_iter , new_lut_width ) + str_data_len );
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
					utf8_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set new lut size
				}
			}
			else // We need to fill the lut manually...
			{
				// Fill INDICES BEFORE insertion
				size_type	iter		= 0;
				char*		lut_iter	= old_lut_base_ptr;
				while( iter < index ){ // Fill lut with indices BEFORE insertion
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
					iter += bytes;
				}
				
				// Fill INDICES AFTER insertion
				lut_iter -= str_lut_len * new_lut_width;
				while( iter < old_data_len ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + str_data_len );
					iter += bytes;
				}
				
				utf8_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set lut size
			}
			
			// Copy INDICES of the insertion
			char*		lut_dest_iter = old_lut_base_ptr - mb_index * new_lut_width;
			if( str_lut_active )
			{
				size_type	str_lut_width = utf8_string::get_lut_width( str_buffer_size );
				const char*	str_lut_iter = str_lut_base_ptr;
				while( str_lut_len-- > 0 )
					utf8_string::set_lut(
						lut_dest_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( str_lut_iter -= str_lut_width , str_lut_width ) + index
					);
			}
			else{
				size_type iter = 0;
				while( iter < str_data_len ){
					width_type bytes = get_codepoint_bytes( str_buffer[iter] , str_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , iter + index );
					iter += bytes;
				}
			}
		}
		else // Set new lut mode
			utf8_string::set_lut_indiciator( old_lut_base_ptr , new_lut_len == 0 , 0 );
		
		// Move BUFFER from AFTER the insertion (Note: We don't need to copy before it, because the buffer hasn't changed)
		std::memmove( old_buffer + index + str_data_len , old_buffer + index , old_data_len - index );
		old_buffer[new_data_len] = 0; // Trailing '\0'
		
		// Copy BUFFER of the insertion
		std::memcpy( old_buffer + index , str_buffer , str_data_len );
	}
	else // No, apparently we have to allocate a new buffer...
	{
		new_buffer_size				<<= 1; // Allocate twice as much, in order to amortize allocations (keeping in mind alignment)
		char*	new_buffer			= new char[ determine_total_buffer_size( new_buffer_size ) ];
		char*	new_lut_base_ptr	= utf8_string::get_lut_base_ptr( new_buffer , new_buffer_size );
		
		// Copy BUFFER from BEFORE insertion
		std::memcpy( new_buffer , old_buffer , index );
		
		// Copy BUFFER of insertion
		std::memcpy( new_buffer + index , str_buffer , str_data_len );
		
		// Copy BUFFER from AFTER the insertion
		std::memcpy( new_buffer + index + str_data_len , old_buffer + index , old_data_len - index );
		new_buffer[new_data_len] = 0; // Trailing '\0'
		
		// Need to fill the lut? (see [2])
		if( new_lut_width )
		{
			// Update the lut width, since we doubled the buffer size a couple of lines above
			new_lut_width = utf8_string::get_lut_width( new_buffer_size );
			
			// Reuse indices from old lut?
			if( old_lut_active )
			{
				size_type	old_lut_width = utf8_string::get_lut_width( old_buffer_size );
				
				// Copy all INDICES BEFORE the insertion
				if( new_lut_width != old_lut_width )
				{
					char*		lut_iter = old_lut_base_ptr;
					char*		new_lut_iter = new_lut_base_ptr;
					size_type	num_indices = mb_index;
					while( num_indices-- > 0 )
						utf8_string::set_lut(
							new_lut_iter -= new_lut_width
							, new_lut_width
							, utf8_string::get_lut( lut_iter -= old_lut_width , old_lut_width )
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
				char*		lut_iter = old_lut_base_ptr - mb_index * old_lut_width;
				char*		new_lut_iter = new_lut_base_ptr - ( mb_index + str_lut_len ) * new_lut_width;
				size_type	num_indices = old_lut_len - mb_index;
				while( num_indices-- > 0 )
					utf8_string::set_lut(
						new_lut_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( lut_iter -= old_lut_width , old_lut_width ) + str_data_len
					);
			}
			else // We need to fill the lut manually...
			{
				// Fill INDICES BEFORE insertion
				size_type	iter		= 0;
				char*		lut_iter	= new_lut_base_ptr;
				while( iter < index ){ // Fill lut with indices BEFORE insertion
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
					iter += bytes;
				}
				
				// Fill INDICES AFTER insertion
				lut_iter -= str_lut_len * new_lut_width;
				while( iter < old_data_len ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + str_data_len );
					iter += bytes;
				}
			}
			
			// Copy INDICES of the insertion
			char*		lut_dest_iter = new_lut_base_ptr - mb_index * new_lut_width;
			if( str_lut_active )
			{
				size_type	str_lut_width = utf8_string::get_lut_width( str_buffer_size );
				const char*	str_lut_iter = str_lut_base_ptr;
				while( str_lut_len-- > 0 )
					utf8_string::set_lut(
						lut_dest_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( str_lut_iter -= str_lut_width , str_lut_width ) + index
					);
			}
			else{
				size_type str_iter = 0;
				while( str_iter < str_data_len ){
					width_type bytes = get_codepoint_bytes( str_buffer[str_iter] , str_data_len - str_iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , str_iter + index );
					str_iter += bytes;
				}
			}
			
			utf8_string::set_lut_indiciator( new_lut_base_ptr , true , new_lut_len ); // Set new lut mode and len
		}
		else // Set new lut mode
			utf8_string::set_lut_indiciator( new_lut_base_ptr , new_lut_len == 0 , 0 );
	
		// Delete the old buffer?
		if( old_sso_inactive )
			delete[] old_buffer;
		
		// Set new Attributes
		t_non_sso.data		= new_buffer;
		t_non_sso.buffer_size = new_buffer_size;
	}
	
	// Adjust Attributes
	t_non_sso.data_len	= new_data_len;
	set_non_sso_string_len( new_string_len );
	
	return *this;
}

utf8_string& utf8_string::raw_replace( size_type index , size_type replaced_len , const utf8_string& repl )
{
	// Bound checks...
	size_type old_data_len = size();
	if( index > old_data_len )
		throw std::out_of_range( "utf8_string::(raw_)replace" );
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
	if( new_data_len <= utf8_string::get_sso_capacity() )
	{
		// Was the buffer on the heap and has to be moved now?
		if( old_sso_inactive )
		{
			char*		old_buffer = t_non_sso.data; // Backup old buffer, since we override the pointer to it (see [1])
			
			// Copy BEFORE replaced part
			std::memcpy( t_sso.data , old_buffer , index ); // [1]
			
			// Copy AFTER replaced part
			std::memcpy( t_sso.data + index + repl_data_len , old_buffer + end_index , old_data_len - end_index );
			
			delete[] old_buffer; // Delete the old buffer
		}
		// Copy AFTER replaced part, if it has moved in position
		else if( new_data_len != old_data_len )
			std::memmove( t_sso.data + index + repl_data_len , t_sso.data + index + replaced_len , old_data_len - index );
		
		// Copy REPLACEMENT (Note: Since the resulting string is small, the replacement must be small as well!)
		std::memcpy( t_sso.data + index , repl.t_sso.data , repl_data_len );
		
		// Finish the new string object
		t_sso.data[new_data_len] = 0; // Trailing '\0'
		set_sso_data_len( new_data_len );
		
		return *this;
	}
	
	//! Ok, obviously no small string, we have to update the data, the lut and the number of code points
	
	// Count code points and multibytes of replacement
	bool		repl_lut_active;
	const char*	repl_buffer;
	const char*	repl_lut_base_ptr;
	size_type	repl_buffer_size;
	size_type	repl_string_len;
	size_type	repl_lut_len;
	if( repl.sso_inactive() )
	{
		repl_buffer_size	= repl.t_non_sso.buffer_size;
		repl_buffer			= repl.t_non_sso.data;
		repl_string_len		= repl.get_non_sso_string_len();
		
		// Compute the number of multibytes
		repl_lut_base_ptr = utf8_string::get_lut_base_ptr( repl_buffer , repl_buffer_size );
		repl_lut_active = utf8_string::is_lut_active( repl_lut_base_ptr );
		if( repl_lut_active )
			repl_lut_len = utf8_string::get_lut_len( repl_lut_base_ptr );
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
		repl_buffer_size	= utf8_string::get_sso_capacity();
		repl_lut_len		= 0;
		for( size_type iter = 0 ; iter < repl_data_len ; ){
			width_type bytes = get_codepoint_bytes( repl_buffer[iter] , repl_data_len - iter );
			repl_lut_len += bytes > 1; iter += bytes; ++repl_string_len;
		}
	}
	
	// Count code points and multibytes of this string
	char*		old_buffer;
	char*		old_lut_base_ptr; // Ignore uninitialized warning, see [3]
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
		while( iter < index ){ // Count multibytes and code points BEFORE replacement
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			mb_index += bytes > 1; iter += bytes;
		}
		while( iter < end_index ){ // Count REPLACED multibytes and code points
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			replaced_mbs += bytes > 1; iter += bytes; ++replaced_cps;
		}
		// Count TOTAL multibytes
		old_lut_base_ptr = utf8_string::get_lut_base_ptr( old_buffer , old_buffer_size );
		if( (old_lut_active = utf8_string::is_lut_active( old_lut_base_ptr )) )
			old_lut_len = utf8_string::get_lut_len( old_lut_base_ptr );
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
		old_buffer_size	= utf8_string::get_sso_capacity();
		old_string_len	= 0;
		old_lut_len		= 0;
		size_type iter	= 0;
		old_lut_active	= false;
		while( iter < index ){ // Count multibytes and code points BEFORE replacement
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			mb_index += bytes > 1; iter += bytes; ++old_string_len;
		}
		while( iter < end_index ){ // Count REPLACED multibytes and code points
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			replaced_mbs += bytes > 1; iter += bytes; ++replaced_cps;
		}
		old_lut_len = mb_index + replaced_mbs;
		while( iter < old_data_len ){ // Count multibytes and code points AFTER replacement
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
	if( utf8_string::is_lut_worth( new_lut_len , new_string_len , old_lut_active , old_sso_inactive ) )
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
			new_lut_width = utf8_string::get_lut_width( old_buffer_size );
			
			// Reuse indices from old lut?
			if( old_lut_active )
			{
				size_type	mb_end_index = mb_index + replaced_mbs;
				
				// Need to offset all indices? (This can be, if the replacement data has different size as the replaced data)
				if( delta_len ){
					char*		lut_iter = old_lut_base_ptr - mb_end_index * new_lut_width;
					size_type	num_indices = old_lut_len - mb_end_index;
					while( num_indices-- > 0 ){
						lut_iter -= new_lut_width;
						utf8_string::set_lut( lut_iter , new_lut_width , utf8_string::get_lut( lut_iter , new_lut_width ) + delta_len );
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
					
					utf8_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set new lut size
				}
			}
			else // We need to fill the lut manually...
			{
				// Fill INDICES BEFORE replacement
				size_type	iter		= 0;
				char*		lut_iter	= old_lut_base_ptr;
				while( iter < index ){ // Fill lut with indices BEFORE replacement
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
					iter += bytes;
				}
				
				// Fill INDICES AFTER replacement
				iter += replaced_len;
				lut_iter -= repl_lut_len * new_lut_width;
				while( iter < old_data_len ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + delta_len );
					iter += bytes;
				}
				
				utf8_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set lut size
			}
			
			// Copy INDICES of the replacement
			char*		lut_dest_iter = old_lut_base_ptr - mb_index * new_lut_width;
			if( repl_lut_active )
			{
				size_type	repl_lut_width = utf8_string::get_lut_width( repl_buffer_size );
				const char*	repl_lut_iter = repl_lut_base_ptr;
				while( repl_lut_len-- > 0 )
					utf8_string::set_lut(
						lut_dest_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( repl_lut_iter -= repl_lut_width , repl_lut_width ) + index
					);
			}
			else{
				size_type iter = 0;
				while( iter < repl_data_len ){
					width_type bytes = get_codepoint_bytes( repl_buffer[iter] , repl_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , iter + index );
					iter += bytes;
				}
			}
		}
		else // Set new lut mode
			utf8_string::set_lut_indiciator( old_lut_base_ptr , new_lut_len == 0 , 0 );
		
		// Move BUFFER from AFTER the replacement (Note: We don't need to copy before it, because the buffer hasn't changed)
		if( new_data_len != old_data_len ){
			std::memmove( old_buffer + index + repl_data_len , old_buffer + end_index , old_data_len - end_index );
			t_non_sso.data_len = new_data_len;
			old_buffer[new_data_len] = 0; // Trailing '\0'
		}
		
		// Copy BUFFER of the replacement
		std::memcpy( old_buffer + index , repl_buffer , repl_data_len );
	}
	else // No, apparently we have to allocate a new buffer...
	{
		new_buffer_size				<<= 1; // Allocate twice as much, in order to amortize allocations (keeping in mind alignment)
		char*	new_buffer			= new char[ determine_total_buffer_size( new_buffer_size ) ];
		char*	new_lut_base_ptr	= utf8_string::get_lut_base_ptr( new_buffer , new_buffer_size );
		
		// Copy BUFFER from BEFORE replacement
		std::memcpy( new_buffer , old_buffer , index );
		
		// Copy BUFFER of replacement
		std::memcpy( new_buffer + index , repl_buffer , repl_data_len );
		
		// Copy BUFFER from AFTER the replacement
		std::memcpy( new_buffer + index + repl_data_len , old_buffer + end_index , old_data_len - end_index );
		new_buffer[new_data_len] = 0; // Trailing '\0'
		
		// Need to fill the lut? (see [2])
		if( new_lut_width )
		{
			// Update the lut width, since we doubled the buffer size a couple of lines above
			new_lut_width = utf8_string::get_lut_width( new_buffer_size );
			
			// Reuse indices from old lut?
			if( old_lut_active )
			{
				size_type	mb_end_index = mb_index + replaced_mbs;
				size_type	old_lut_width = utf8_string::get_lut_width( old_buffer_size );
				
				// Copy all INDICES BEFORE the replacement
				if( new_lut_width != old_lut_width )
				{
					char*		lut_iter = old_lut_base_ptr;
					char*		new_lut_iter = new_lut_base_ptr;
					size_type	num_indices = mb_index;
					while( num_indices-- > 0 )
						utf8_string::set_lut(
							new_lut_iter -= new_lut_width
							, new_lut_width
							, utf8_string::get_lut( lut_iter -= old_lut_width , old_lut_width )
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
					char*		lut_iter = old_lut_base_ptr - mb_end_index * old_lut_width;
					char*		new_lut_iter = new_lut_base_ptr - ( mb_index + repl_lut_len ) * new_lut_width;
					size_type	num_indices = old_lut_len - mb_end_index;
					while( num_indices-- > 0 )
						utf8_string::set_lut(
							new_lut_iter -= new_lut_width
							, new_lut_width
							, utf8_string::get_lut( lut_iter -= old_lut_width , old_lut_width ) + delta_len
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
				char*		lut_iter	= new_lut_base_ptr;
				while( iter < index ){ // Fill lut with indices BEFORE replacement
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter );
					iter += bytes;
				}
				
				// Fill INDICES AFTER replacement
				iter += replaced_len;
				lut_iter -= repl_lut_len * new_lut_width;
				while( iter < old_data_len ){
					width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_iter -= new_lut_width , new_lut_width , iter + delta_len );
					iter += bytes;
				}
			}
			
			// Copy INDICES of the replacement
			char*		lut_dest_iter = new_lut_base_ptr - mb_index * new_lut_width;
			if( repl_lut_active )
			{
				size_type	repl_lut_width = utf8_string::get_lut_width( repl_buffer_size );
				const char*	repl_lut_iter = repl_lut_base_ptr;
				while( repl_lut_len-- > 0 )
					utf8_string::set_lut(
						lut_dest_iter -= new_lut_width
						, new_lut_width
						, utf8_string::get_lut( repl_lut_iter -= repl_lut_width , repl_lut_width ) + index
					);
			}
			else{
				size_type repl_iter = 0; // Todo
				while( repl_iter < repl_data_len ){
					width_type bytes = get_codepoint_bytes( repl_buffer[repl_iter] , repl_data_len - repl_iter );
					if( bytes > 1 )
						utf8_string::set_lut( lut_dest_iter -= new_lut_width , new_lut_width , repl_iter + index );
					repl_iter += bytes;
				}
			}
			
			utf8_string::set_lut_indiciator( new_lut_base_ptr , true , new_lut_len ); // Set new lut mode and len
		}
		else // Set new lut mode
			utf8_string::set_lut_indiciator( new_lut_base_ptr , new_lut_len == 0 , 0 );
	
		// Delete the old buffer?
		if( old_sso_inactive )
			delete[] old_buffer;
		
		// Set new Attributes
		t_non_sso.data		= new_buffer;
		t_non_sso.data_len	= new_data_len;
		t_non_sso.buffer_size = new_buffer_size;
	}
	
	// Adjust string length
	set_non_sso_string_len( new_string_len );
	
	return *this;
}

utf8_string& utf8_string::raw_erase( size_type index , size_type len )
{
	// Bound checks...
	size_type old_data_len = size();
	if( index > old_data_len )
		throw std::out_of_range( "utf8_string::(raw_)erase" );
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
	if( new_data_len <= utf8_string::get_sso_capacity() )
	{
		// Was the buffer on the heap and has to be moved now?
		if( old_sso_inactive )
		{
			char*		old_buffer = t_non_sso.data; // Backup old buffer, since we override the pointer to it (see [1])
			
			// Copy BEFORE replaced part
			std::memcpy( t_sso.data , old_buffer , index ); // [1]
			
			// Copy AFTER replaced part
			std::memcpy( t_sso.data + index , old_buffer + end_index , old_data_len - end_index );
			
			delete[] old_buffer; // Delete the old buffer
		}
		// Copy AFTER replaced part, if it has moved in position
		else if( new_data_len != old_data_len )
			std::memmove( t_sso.data + index , t_sso.data + index + len , old_data_len - index );
		
		// Finish the new string object
		t_sso.data[new_data_len] = 0; // Trailing '\0'
		set_sso_data_len( new_data_len );
		
		return *this;
	}
	
	//! Ok, obviously no small string, we have to update the data, the lut and the number of code points.
	//! BUT: We will keep the lut in the mode it is: inactive stay inactive, active stays active
	
	// Count code points and multibytes of this string
	char*		old_buffer = t_non_sso.data;
	size_type	old_buffer_size = t_non_sso.buffer_size;
	char*		old_lut_base_ptr = utf8_string::get_lut_base_ptr( old_buffer , old_buffer_size );
	bool		old_lut_active = utf8_string::is_lut_active( old_lut_base_ptr );
	size_type	replaced_cps = 0;
	
	// Adjust data length
	t_non_sso.data_len -= len;
	
	// Was the lut active? => Keep it active and therefore update the data, the string length and the lut
	if( old_lut_active )
	{
		size_type	old_lut_len = utf8_string::get_lut_len( old_lut_base_ptr );
		size_type	old_lut_width = utf8_string::get_lut_width( old_buffer_size );
		size_type	mb_end_index = 0;
		size_type	replaced_mbs = 0;
		size_type	iter	= 0;
		while( iter < index ){ // Count multibytes and code points BEFORE erased part
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			mb_end_index += bytes > 1; iter += bytes;
		}
		while( iter < end_index ){ // Count REPLACED multibytes and code points
			width_type bytes = get_codepoint_bytes( old_buffer[iter] , old_data_len - iter );
			replaced_mbs += bytes > 1; iter += bytes; ++replaced_cps;
		}
		mb_end_index += replaced_mbs;
		
		// Offset all indices
		char*		lut_iter = old_lut_base_ptr - mb_end_index * old_lut_width;
		size_type	num_indices = old_lut_len - mb_end_index;
		while( num_indices-- > 0 ){
			lut_iter -= old_lut_width;
			utf8_string::set_lut( lut_iter , old_lut_width , utf8_string::get_lut( lut_iter , old_lut_width ) - len );
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
			
			utf8_string::set_lut_indiciator( old_lut_base_ptr , true , new_lut_len ); // Set new lut size
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

utf8_string::size_type utf8_string::raw_rfind( value_type cp , size_type index ) const {
	if( index >= size() )
		index = back_index();
	for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) )
		if( raw_at(it) == cp )
			return it;
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::find_first_of( const value_type* str , size_type start_pos ) const
{
	if( start_pos >= length() )
		return utf8_string::npos;
	
	for( const_iterator it = get( start_pos ), end = cend() ; it < end ; ++it, ++start_pos )
	{
		const value_type*	tmp = str;
		value_type			cur = *it;
		do{
			if( cur == *tmp )
				return start_pos;
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_first_of( const value_type* str , size_type index ) const
{
	if( index >= size() )
		return utf8_string::npos;
	
	for( const_iterator it = raw_get(index), end = cend() ; it < end ; ++it )
	{
		const value_type*	tmp = str;
		value_type			cur = *it;
		do{
			if( cur == *tmp )
				return it.get_index();
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::find_last_of( const value_type* str , size_type start_pos ) const
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
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_last_of( const value_type* str , size_type index ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( index >= size() )
		index = back_index();
	
	for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) ){
		const value_type*	tmp = str;
		value_type			cur = raw_at(it);
		do{
			if( cur == *tmp )
				return it;
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::find_first_not_of( const value_type* str , size_type start_pos ) const
{
	if( start_pos >= length() )
		return utf8_string::npos;
	
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
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_first_not_of( const value_type* str , size_type index ) const
{
	if( index >= size() )
		return utf8_string::npos;
	
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
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::find_last_not_of( const value_type* str , size_type start_pos ) const
{
	if( empty() )
		return utf8_string::npos;
	
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
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_last_not_of( const value_type* str , size_type index ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( index >= size() )
		index = back_index();
	
	for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) )
	{
		const value_type*	tmp = str;
		value_type			cur = raw_at(it);
		
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it; // It will be either non-negative or -1, which equals utf8_string::npos!
		
		continue2:;
	}
	
	return utf8_string::npos;
}

int operator-( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs ){
	utf8_string::difference_type minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::difference_type max_index = std::max( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type num_codepoints = lhs.get_instance()->get_num_codepoints( minIndex , max_index - minIndex );
	return max_index == lhs.get_index() ? num_codepoints : -num_codepoints;
}
int operator-( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs ){
	utf8_string::difference_type	minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::difference_type	max_index = std::max( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type			num_codepoints = lhs.get_instance()->get_num_codepoints( minIndex , max_index - minIndex );
	return max_index == rhs.get_index() ? num_codepoints : -num_codepoints;
}

std::ostream& operator<<( std::ostream& stream , const utf8_string& str ){ return stream << str.c_str(); }
std::istream& operator>>( std::istream& stream , utf8_string& str ){
	std::string tmp;
	stream >> tmp;
	str = move(tmp);
	return stream;
}

#endif // TINY_UTF8_FORWARD_DECLARE_ONLY

#endif // _TINY_UTF8_H_
