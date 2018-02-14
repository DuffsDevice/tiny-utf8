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

#include <memory> // for std::unique_ptr
#include <cstring> // for std::memcpy, std::strlen
#include <string> // for std::string
#include <limits> // for numeric_limits
#include <cstddef> // for ptrdiff_t, size_t
#include <cstdint> // for uint8_t, uint16_t

// Define the following macro, if you plan to output an utf8_string
// to an ostream or read an utf8_string from an istream
#ifdef _TINY_UTF8_H_USE_IOSTREAM_
#include <iostream> // for ostream/istream overloads
#endif

// Define the following macro, if you want to be guarded against malformed utf8 sequences
#define _TINY_UTF8_H_SAFE_MODE_

class utf8_string
{
	public:
		
		typedef std::size_t				size_type;
		typedef std::ptrdiff_t			difference_type;
		typedef char32_t				value_type;
		typedef value_type				const_reference;
		constexpr static size_type		npos = -1;
		
	private:
		
		class utf8_codepoint_reference
		{
			private:
				
				size_type		index;
				utf8_string*	instance;
				
			public:
				
				//! Ctor
				utf8_codepoint_reference( size_type index , utf8_string* instance ) :
					index( index )
					, instance( instance )
				{}
				
				//! Cast to wide char
				operator value_type() const { return static_cast<const utf8_string*>(this->instance)->at( index ); }
				
				//! Assignment operator
				utf8_codepoint_reference& operator=( value_type ch ){
					instance->replace( index , ch );
					return *this;
				}
		};
		
		class utf8_codepoint_raw_reference
		{
			private:
				
				size_type		raw_index;
				utf8_string*	instance;
				
			public:
				
				//! Ctor
				utf8_codepoint_raw_reference( size_type raw_index , utf8_string* instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Cast to wide char
				operator value_type() const { return static_cast<const utf8_string*>(this->instance)->raw_at( this->raw_index ); }
				
				//! Assignment operator
				utf8_codepoint_raw_reference& operator=( value_type ch ){
					this->instance->raw_replace( this->raw_index , this->instance->get_index_bytes( this->raw_index ) , utf8_string( 1 , ch ) );
					return *this;
				}
		};
		
		class iterator_base
		{
			friend class utf8_string;
			
			public:
				
				typedef utf8_string::value_type			value_type;
				typedef utf8_string::difference_type	difference_type;
				typedef utf8_codepoint_raw_reference	reference;
				typedef void*							pointer;
				typedef std::forward_iterator_tag		iterator_category;
				
				bool operator==( const iterator_base& it ) const { return this->raw_index == it.raw_index; }
				bool operator!=( const iterator_base& it ) const { return this->raw_index != it.raw_index; }
				
				//! Ctor
				iterator_base( difference_type raw_index , utf8_string* instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Default function
				iterator_base() = default;
				iterator_base( const iterator_base& ) = default;
				iterator_base& operator=( const iterator_base& ) = default;
				
				// Getter for the iterator index
				difference_type get_index() const { return raw_index; }
				utf8_string* get_instance() const { return instance; }
				
			protected:
				
				difference_type	raw_index;
				utf8_string*	instance;
				
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
					this->raw_index += this->instance->get_index_bytes( this->raw_index );
				}
				
				void decrement(){
					this->raw_index -= this->instance->get_index_pre_bytes( this->raw_index );
				}
				
				utf8_codepoint_raw_reference get_reference() const {
					return this->instance->raw_at( this->raw_index );
				}
				value_type get_value() const {
					return static_cast<const utf8_string*>(this->instance)->raw_at( this->raw_index );
				}
		};
		
	public:
		
		typedef utf8_codepoint_reference	reference;
		
		class iterator : public iterator_base
		{
			public:
				
				//! Ctor
				iterator( difference_type raw_index , utf8_string* instance ) :
					iterator_base( raw_index , instance )
				{}
				
				//! Default Functions
				iterator() = default;
				iterator( const iterator& ) = default;
				iterator& operator=( const iterator& ) = default;
				
				//! Delete ctor from const iterator types
				iterator( const class const_iterator& ) = delete;
				iterator( const class const_reverse_iterator& ) = delete;
				
				//! Increase the Iterator by one
				iterator&  operator++(){ // prefix ++iter
					increment();
					return *this;
				}
				iterator& operator++( int ){ // postfix iter++
					iterator tmp{ raw_index , instance };
					increment();
					return *this;
				}
				
				//! Decrease the iterator by one
				iterator&  operator--(){ // prefix --iter
					decrement();
					return *this;
				}
				iterator& operator--( int ){ // postfix iter--
					iterator tmp{ raw_index , instance };
					decrement();
					return *this;
				}
				
				//! Increase the Iterator n times
				iterator operator+( difference_type n ) const {
					iterator it{*this};
					it.advance( n );
					return it;
				}
				iterator& operator+=( difference_type n ){
					this->advance( n );
					return *this;
				}
				
				//! Decrease the Iterator n times
				iterator operator-( difference_type n ) const {
					iterator it{*this};
					it.advance( -n );
					return it;
				}
				iterator& operator-=( difference_type n ){
					this->advance( -n );
					return *this;
				}
				
				//! Returns the value of the codepoint behind the iterator
				utf8_codepoint_raw_reference operator*() const { return get_reference(); }
		};
		
		class const_iterator : public iterator
		{
			public:
				
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
		
		class reverse_iterator : public iterator_base
		{
			public:
				
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
				reverse_iterator( const class const_reverse_iterator& ) = delete;
				
				//! Increase the iterator by one
				reverse_iterator&  operator++(){ // prefix ++iter
					decrement();
					return *this;
				}
				reverse_iterator& operator++( int ){ // postfix iter++
					reverse_iterator tmp{ raw_index , instance };
					decrement();
					return *this;
				}
				
				//! Decrease the Iterator by one
				reverse_iterator& operator--(){ // prefix --iter
					increment();
					return *this;
				}
				reverse_iterator& operator--( int ){ // postfix iter--
					reverse_iterator tmp{ raw_index , instance };
					increment();
					return *this;
				}
				
				//! Increase the Iterator n times
				reverse_iterator operator+( difference_type n ) const {
					reverse_iterator it{*this};
					it.advance( -n );
					return it;
				}
				reverse_iterator& operator+=( difference_type n ){
					this->advance( -n );
					return *this;
				}
				
				//! Decrease the Iterator n times
				reverse_iterator operator-( difference_type n ) const {
					reverse_iterator it{*this};
					it.advance( n );
					return it;
				}
				reverse_iterator& operator-=( difference_type n ){
					this->advance( n );
					return *this;
				}
				
				//! Returns the value of the codepoint behind the iterator
				utf8_codepoint_raw_reference operator*() const { return get_reference(); }
				
				//! Get the underlying iterator instance
				iterator base() const {
					return iterator( raw_index , instance );
				}
		};
		
		class const_reverse_iterator : public reverse_iterator
		{
			public:
				
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
					return const_iterator( raw_index , instance );
				}
		};
		
	private:
		
		//! Attributes
		size_type		_buffer_len;
		size_type		_capacity;
		char*			_buffer;
		size_type		_string_len;
		size_type		_indices_len;
		
		//! Get the maximum number of bytes that can be stored within a utf8_string object
		static constexpr size_type get_max_small_string_len(){
			return sizeof(_capacity) + sizeof(_buffer) + sizeof(_string_len) + sizeof(_indices_len);
		}
		
		//! Determine, whether the supplied number of bytes is able to fit inside the utf8_string object itself
		static constexpr bool is_small_string( size_type buffer_len ){
			return buffer_len <= get_max_small_string_len();
		}
		
		//! Determine, whether we will use a 'uint8_t', 'uint16_t', or 'size_type'-based index table
		static constexpr unsigned char get_index_datatype_bytes( size_type buffer_size ){
			return buffer_size <= std::numeric_limits<std::uint8_t>::max()
				? sizeof(std::uint8_t)
				: buffer_size <= std::numeric_limits<std::uint16_t>::max()
					? sizeof(std::uint16_t)
					: sizeof(size_type);
		}
		
		//! Get the nth index within a multibyte index table
		static inline size_type get_nth_index( const void* table , unsigned char table_element_size , size_type idx ){
			switch( table_element_size ){
			case sizeof(std::uint8_t):	return static_cast<const std::uint8_t*> (table)[idx];
			case sizeof(std::uint16_t):	return static_cast<const std::uint16_t*>(table)[idx];
			case sizeof(size_type):
			default:					return static_cast<const size_type*>    (table)[idx];
			}
			return static_cast<const size_type*>(table)[idx];
		}
		static inline void set_nth_index( void* table , unsigned char table_element_size , size_type idx , size_type value ){
			switch( table_element_size ){
			case sizeof(std::uint8_t):	static_cast<std::uint8_t*> (table)[idx] = value; break;
			case sizeof(std::uint16_t):	static_cast<std::uint16_t*>(table)[idx] = value; break;
			case sizeof(size_type):		
			default:					static_cast<size_type*>    (table)[idx] = value; break;
			}
		}
		
		static inline size_type get_required_capacity( size_type buffer_size , size_type indices_size ){
			return buffer_size + indices_size * get_index_datatype_bytes( buffer_size );
		}
		
		//! Get internal buffer
		char* get_sso_buffer(){ return reinterpret_cast<char*>(&this->_buffer); }
		const char* get_sso_buffer() const { return reinterpret_cast<const char*>(&this->_buffer); }
		
		
		//! Get buffer
		const char* get_buffer() const {
			return this->sso_active() ? get_sso_buffer() : this->_buffer;
		}
		char* get_buffer(){
			return const_cast<char*>( static_cast<const utf8_string*>(this)->get_buffer() );
		}
		
		//! Allocates a new buffer
		std::pair<char*,void*> new_buffer( size_type buffer_size , size_type indices_size );
		
		
		//! Get indices
		const void* get_indices() const {
			if( this->sso_active() || !this->_indices_len )
				return nullptr;
			return this->_buffer + this->_capacity // Go to end of buffer
				- ( this->_indices_len * get_index_datatype_bytes( this->_buffer_len ) ) // Subtract the number of bytes that the indices table occupies
			;
		}
		void* get_indices(){
			return const_cast<void*>( static_cast<const utf8_string*>(this)->get_indices() );
		}
		
		//! Returns the length of the indices table
		size_type get_indices_len() const {
			return this->sso_active() ? get_num_multibytes( this->get_buffer() ) : this->_indices_len;
		}
		
		
		// Helpers for the constructors
		template<size_type L>
		using enable_if_small_string = typename std::enable_if<L<=get_max_small_string_len(),int*>::type;
		template<size_type L>
		using enable_if_not_small_string = typename std::enable_if<get_max_small_string_len()<L,int*>::type;
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
		
		/**
		 * Returns an std::string with the UTF-8 BOM prepended
		 */
		std::string cpp_str_bom() const ;
		
		/**
		 * Returns the number of bytes to expect behind this one (including this one) that belong to this utf8 char
		 */
		static unsigned char get_num_bytes_of_utf8_char( const char* data , size_type first_byte_index , size_type buffer_len = utf8_string::npos );
		
		/**
		 * Returns the number of bytes to expect before this one (including this one) that belong to this utf8 char
		 */
		static unsigned char get_num_bytes_of_utf8_char_before( const char* data , size_type current_byte_index , size_type buffer_len = utf8_string::npos );
		
		/**
		 * Returns the number of bytes to expect behind this one that belong to this utf8 char
		 */
		static unsigned char get_num_bytes_of_utf8_codepoint( value_type codepoint )
		{
			if( codepoint <= 0x7F )
				return 1;
			else if( codepoint <= 0x7FF )
				return 2;
			else if( codepoint <= 0xFFFF )
				return 3;
			else if( codepoint <= 0x1FFFFF )
				return 4;
			else if( codepoint <= 0x3FFFFFF )
				return 5;
			else if( codepoint <= 0x7FFFFFFF )
				return 6;
			return 0;
		}
		
		/**
		 * Get the byte index of the last codepoint
		 */
		size_type back_index() const { size_type s = size(); return s - get_index_pre_bytes( s ); }
		/**
		 * Get the byte index of the index behind the last codepoint
		 */
		size_type end_index() const { return size(); }
		
		/**
		 * Decodes a given input of rle utf8 data to a
		 * unicode codepoint and returns the number of bytes it used
		 */
		static unsigned char decode_utf8( const char* data , value_type& codepoint );
		
		/**
		 * Encodes a given codepoint to a character buffer of at least 7 bytes
		 * and returns the number of bytes it used
		 */
		static unsigned char encode_utf8( value_type codepoint , char* dest );
		
		//! Returns the number of multibytes within the given utf8 sequence
		static size_type get_num_multibytes( const char* lit , size_type bytes_count = utf8_string::npos )
		{
			size_type num_multibytes = 0;
			while( *lit && bytes_count-- > 0 ){
				unsigned char num_bytes = get_num_bytes_of_utf8_char( lit , 0 , utf8_string::npos ); // Compute the number of bytes to skip
				lit += num_bytes;
				if( num_bytes > 1 )
					num_multibytes++; // Increase number of occoured multibytes
			}
			return num_multibytes;
		}
		
		//! Fills the multibyte table
		void compute_multibyte_table( void* table );
		
		
		/**
		 * Counts the number of codepoints
		 * that are built up of the supplied amout of bytes
		 */
		size_type get_num_resulting_codepoints( size_type byte_start , size_type byte_count ) const ;
		
		/**
		 * Counts the number of bytes
		 * required to hold the supplied amount of codepoints starting at the supplied byte index
		 */
		size_type get_num_resulting_bytes( size_type byte_start , size_type codepoint_count ) const ;
		
		//! Constructs a utf8_string from a character literal of unknown length
		utf8_string( const char* str , size_type len , void* ); // the (void*) is a dummy to call this function
		
	public:
		
		/**
		 * Default Ctor
		 * 
		 * @note Creates an Instance of type utf8_string that is empty
		 */
		utf8_string() :
			_buffer_len( 0 )
			, _capacity( 0 )
			, _buffer( nullptr )
			, _string_len( 0 )
			, _indices_len( 0 )
		{}
		
		/**
		 * Contructor taking an utf8 sequence and the maximum length to read from it (in number of codepoints)
		 * 
		 * @note	Creates an Instance of type utf8_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 sequence to fill the utf8_string with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		template<typename T>
		utf8_string( T&& str , enable_if_ptr<T,char> = nullptr ) :
			utf8_string( str , utf8_string::npos , (void*)nullptr )
		{}
		utf8_string( const char* str , size_type len ) :
			utf8_string( str , len , (void*)nullptr )
		{}
		/**
		 * Contructor taking an utf8 char literal
		 * 
		 * @note	Creates an Instance of type utf8_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 literal to fill the utf8_string with
		 */
		template<size_type LITLEN>
		utf8_string( const char (&str)[LITLEN] , enable_if_small_string<LITLEN> = nullptr ) : _buffer_len( LITLEN ) {
			std::memcpy( this->get_sso_buffer() , str , LITLEN );
		}
		template<size_type LITLEN>
		utf8_string( const char (&str)[LITLEN] , enable_if_not_small_string<LITLEN> = nullptr ) :
			utf8_string( str , utf8_string::npos , (void*)nullptr ) // Call the one that works on arbitrary character arrays
		{}
		
		
		/**
		 * Contructor taking an std::string
		 * 
		 * @note	Creates an Instance of type utf8_string that holds the given data sequence
		 * @param	str		The byte data, that will be interpreted as UTF-8
		 */
		utf8_string( std::string str ) :
			utf8_string( str.c_str() )
		{}
		
		
		/**
		 * Contructor that fills itself with a certain amount of codepoints
		 * 
		 * @note	Creates an Instance of type utf8_string that gets filled with 'n' codepoints
		 * @param	n		The number of codepoints generated
		 * @param	ch		The codepoint that the whole buffer will be set to
		 */
		utf8_string( size_type n , value_type ch );
		/**
		 * Contructor that fills itself with a certain amount of characters
		 * 
		 * @note	Creates an Instance of type utf8_string that gets filled with 'n' characters
		 * @param	n		The number of characters generated
		 * @param	ch		The characters that the whole buffer will be set to
		 */
		utf8_string( size_type n , char ch ) :
			utf8_string( n , (value_type)ch )
		{}
		
		
		/**
		 * Copy Constructor that copies the supplied utf8_string to construct itself
		 * 
		 * @note	Creates an Instance of type utf8_string that has the exact same data as 'str'
		 * @param	str		The utf8_string to copy from
		 */
		utf8_string( const utf8_string& str );
		/**
		 * Move Constructor that moves the supplied utf8_string content into the new utf8_string
		 * 
		 * @note	Creates an Instance of type utf8_string that takes all data from 'str'
		 * 			The supplied utf8_string is invalid afterwards and may not be used anymore
		 * @param	str		The utf8_string to move from
		 */
		utf8_string( utf8_string&& str );
		
		
		/**
		 * Contructor taking a wide codepoint literal that will be copied to construct this utf8_string
		 * 
		 * @note	Creates an Instance of type utf8_string that holds the given codepoints
		 *			The data itself will be first converted to UTF-8
		 * @param	str		The codepoint sequence to fill the utf8_string with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		utf8_string( const value_type* str , size_type len = utf8_string::npos );
		
		
		/**
		 * Destructor
		 * 
		 * @note	Destructs a utf8_string at the end of its lifetime releasing all held memory
		 */
		~utf8_string(){
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
		utf8_string& operator=( utf8_string&& str );
		
		
		/**
		 * Swaps the contents of this utf8_string with the supplied one
		 * 
		 * @note	Swaps all data with the supplied utf8_string
		 * @param	str		The utf8_string to swap contents with
		 */
		void swap( utf8_string& str ){
			std::swap( *this , str );
		}
		
		
		/**
		 * Clears the content of this utf8_string
		 * 
		 * @note	Resets the data to an empty string ("")
		 */
		void clear(){
			if( !this->sso_active() )
				delete[] this->_buffer;
			this->_buffer_len = 0;
		}
		
		
		/**
		 * [DEPRECATED] Returns whether there is an UTF-8 error in the string
		 * 
		 * @return	True, if there is an encoding error, false, if the contained data is properly encoded
		 * @note	This function is deprecated, because the misformatted flag was removed.
		 */
		[[deprecated("The misformatted flag was removed.")]]
		bool is_misformatted() const {
			return false;
		}
		
		
		/**
		 * Returns the current capacity of this string. That is, the number of codepoints it can hold without reallocation
		 * 
		 * @return	The number of bytes currently allocated
		 */
		size_type capacity() const {
			return this->sso_active()
				? get_max_small_string_len()
				: this->_capacity - this->_indices_len * get_index_datatype_bytes( this->_buffer_len ) - 1; // -1 for the trailling zero we have to store
		}
		
		
		/**
		 * Requests the removal of unused capacity.
		 */
		void shrink_to_fit();
		
		
		/**
		 * Returns the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	The Codepoint at position 'n'
		 */
		value_type at( size_type n ) const ;
		utf8_codepoint_reference at( size_type n ){
			return utf8_codepoint_reference( n , this );
		}
		/**
		 * Returns the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	byte_index	The byte position of the codepoint to receive
		 * @return	The codepoint at the supplied position
		 */
		utf8_codepoint_raw_reference raw_at( size_type byte_index ){
			return utf8_codepoint_raw_reference( byte_index , this );
		}
		value_type raw_at( size_type byte_index ) const {
			if( !this->requires_unicode() )
				return this->get_buffer()[byte_index];
			value_type dest;
			decode_utf8( this->get_buffer() + byte_index , dest );
			return dest;
		}
		
		
		/**
		 * Returns an iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified codepoint index
		 */
		iterator get( size_type n ){ return iterator( get_num_resulting_bytes( 0 , n ) , this ); }
		const_iterator get( size_type n ) const { return const_iterator( get_num_resulting_bytes( 0 , n ) , this ); }
		/**
		 * Returns an iterator pointing to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified byte position
		 */
		iterator raw_get( size_type n ){ return iterator( n , this ); }
		const_iterator raw_get( size_type n ) const { return const_iterator( n , this ); }
		
		
		/**
		 * Returns a reverse iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the reverse iterator to
		 * @return	A reverse iterator pointing to the specified codepoint index
		 */
		reverse_iterator rget( size_type n ){ return reverse_iterator( get_num_resulting_bytes( 0 , n ) , this ); }
		const_reverse_iterator rget( size_type n ) const { return const_reverse_iterator( get_num_resulting_bytes( 0 , n ) , this ); }
		/**
		 * Returns a reverse iterator pointing to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to get the reverse iterator to
		 * @return	A reverse iterator pointing to the specified byte position
		 */
		reverse_iterator raw_rget( size_type n ){ return reverse_iterator( n , this ); }
		const_reverse_iterator raw_rget( size_type n ) const { return const_reverse_iterator( n , this ); }
		
		
		/**
		 * Returns a reference to the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	A reference wrapper to the codepoint at position 'n'
		 */
		utf8_codepoint_reference operator[]( size_type n ){ return utf8_codepoint_reference( n , this ); }
		value_type operator[]( size_type n ) const { return at( n ); }
		/**
		 * Returns a reference to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to receive
		 * @return	A reference wrapper to the codepoint at byte position 'n'
		 */
		utf8_codepoint_raw_reference operator()( size_type n ){ return utf8_codepoint_raw_reference( n , this ); }
		value_type operator()( size_type n ) const { return raw_at( n ); }
		
		
		/**
		 * Get the raw data contained in this utf8_string
		 * 
		 * @note	Returns the UTF-8 formatted content of this utf8_string
		 * @return	UTF-8 formatted data, trailled by a '\0'
		 */
		const char* c_str() const { const char* data = this->get_buffer(); return data ? data : ""; }
		const char* data() const { return this->get_buffer(); }
		char* data(){ return this->get_buffer(); }
		
		
		/**
		 * Get the raw data contained in this utf8_string wrapped by an std::string
		 * 
		 * @note	Returns the UTF-8 formatted content of this utf8_string
		 * @return	UTF-8 formatted data, wrapped inside an std::string
		 */
		std::string cpp_str( bool prepend_bom = false ) const { return prepend_bom ? cpp_str_bom() : std::string( this->c_str() ); }
		
		
		/**
		 * Get the number of codepoints in this utf8_string
		 * 
		 * @note	Returns the number of codepoints that are taken care of
		 *			For the number of bytes, @see size()
		 * @return	Number of codepoints (not bytes!)
		 */
		size_type length() const { return this->sso_active() ? cend() - cbegin() : this->_string_len; }
		
		
		/**
		 * Get the number of bytes that are used by this utf8_string
		 * 
		 * @note	Returns the number of bytes required to hold the contained wide string,
		 *			That is, without counting the trailling '\0'
		 * @return	Number of bytes (not codepoints!)
		 */
		size_type size() const { return std::max<size_type>( 1 , this->_buffer_len ) - 1; }
		
		
		/**
		 * Check, whether this utf8_string is empty
		 * 
		 * @note	Returns True, if this utf8_string is empty, that also is comparing true with ""
		 * @return	True, if this utf8_string is empty, false if its length is >0
		 */
		bool empty() const { return this->_buffer_len <= 1; }
		
		
		/**
		 * Check whether the data inside this utf8_string cannot be iterated by an std::string
		 * 
		 * @note	Returns true, if the utf8_string has codepoints that exceed 7 bits to be stored
		 * @return	True, if there are UTF-8 formatted byte sequences,
		 *			false, if there are only ascii characters (<128) inside
		 */
		bool requires_unicode() const { return this->sso_active() ? length() != size() : this->_indices_len > 0; }
		
		
		
		/**
		 * Determine, if small string optimization is active
		 * @return	True, if the utf8 data is stored within the utf8_string object itself
		 *			false, otherwise
		 */
		bool sso_active() const {
			return is_small_string( this->_buffer_len );
		}
		/**
		 * Get the maximum number of bytes that can be stored within the utf8_string object itself
		 * @return	The maximum number of bytes
		 */
		static constexpr size_type max_sso_bytes(){
			return get_max_small_string_len();
		}
		
		/**
		 * Returns the number of multibytes within this utf8_string
		 * 
		 * @return	The number of codepoints that take more than one byte
		 */
		size_type get_num_multibytes() const {
			if( this->sso_active() )
				return get_num_multibytes( this->get_buffer() , max_sso_bytes() );
			return this->_indices_len;
		}
		
		
		/**
		 * Get a wide string literal representing this UTF-8 string
		 * 
		 * @note	As the underlying UTF-8 data has to be converted to a wide character array and 
		 *			as this array has to be allocated, this method is quite slow, that is O(n)
		 * @return	A wrapper class holding the wide character array
		 */
		std::unique_ptr<value_type[]> wide_literal() const ;
		
		
		/**
		 * Get an iterator to the beginning of the utf8_string
		 * 
		 * @return	An iterator class pointing to the beginning of this utf8_string
		 */
		iterator begin(){ return iterator( 0 , this ); }
		const_iterator begin() const { return const_iterator( 0 , this ); }
		/**
		 * Get an iterator to the end of the utf8_string
		 * 
		 * @return	An iterator class pointing to the end of this utf8_string, that is pointing behind the last codepoint
		 */
		iterator end(){ return iterator( end_index() , this ); }
		const_iterator end() const { return const_iterator( end_index() , this ); }
		
		
		/**
		 * Get a reverse iterator to the end of this utf8_string
		 * 
		 * @return	A reverse iterator class pointing to the end of this utf8_string,
		 *			that is exactly to the last codepoint
		 */
		reverse_iterator rbegin(){ return reverse_iterator( back_index() , this ); }
		const_reverse_iterator rbegin() const { return const_reverse_iterator( back_index() , this ); }
		/**
		 * Get a reverse iterator to the beginning of this utf8_string
		 * 
		 * @return	A reverse iterator class pointing to the end of this utf8_string,
		 *			that is pointing before the first codepoint
		 */
		reverse_iterator rend(){ return reverse_iterator( -1 , this ); }
		const_reverse_iterator rend() const { return const_reverse_iterator( -1 , this ); }
		
		
		/**
		 * Get a const iterator to the beginning of the utf8_string
		 * 
		 * @return	A const iterator class pointing to the beginning of this utf8_string,
		 * 			which cannot alter things inside this utf8_string
		 */
		const_iterator cbegin() const { return const_iterator( 0 , this ); }
		/**
		 * Get an iterator to the end of the utf8_string
		 * 
		 * @return	A const iterator class, which cannot alter this utf8_string, pointing to
		 *			the end of this utf8_string, that is pointing behind the last codepoint
		 */
		const_iterator cend() const { return const_iterator( end_index() , this ); }
		
		
		/**
		 * Get a const reverse iterator to the end of this utf8_string
		 * 
		 * @return	A const reverse iterator class, which cannot alter this utf8_string, pointing to
		 *			the end of this utf8_string, that is exactly to the last codepoint
		 */
		const_reverse_iterator crbegin() const { return const_reverse_iterator( back_index() , this ); }
		/**
		 * Get a const reverse iterator to the beginning of this utf8_string
		 * 
		 * @return	A const reverse iterator class, which cannot alter this utf8_string, pointing to
		 *			the end of this utf8_string, that is pointing before the first codepoint
		 */
		const_reverse_iterator crend() const { return const_reverse_iterator( -1 , this ); }
		
		
		/**
		 * Returns a reference to the first codepoint in the utf8_string
		 * 
		 * @return	A reference wrapper to the first codepoint in the utf8_string
		 */
		utf8_codepoint_raw_reference front(){ return utf8_codepoint_raw_reference( 0 , this ); }
		value_type front() const { return empty() ? 0 : raw_at( 0 ); }
		/**
		 * Returns a reference to the last codepoint in the utf8_string
		 * 
		 * @return	A reference wrapper to the last codepoint in the utf8_string
		 */
		utf8_codepoint_raw_reference back(){ return utf8_codepoint_raw_reference( empty() ? 0 : back_index() , this ); }
		value_type back() const { return empty() ? 0 : raw_at( back_index() ); }
		
		
		/**
		 * Replace a codepoint of this utf8_string by a number of codepoints
		 * 
		 * @param	index	The codpoint index to be replaced
		 * @param	repl	The wide character that will be used to replace the codepoint
		 * @param	n		The number of codepoint that will be inserted
		 *					instead of the one residing at position 'index'
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		utf8_string& replace( size_type index , value_type repl , size_type n = 1 ){
			return replace( index , 1 , repl , n );
		}
		/**
		 * Replace a number of codepoints of this utf8_string by a number of other codepoints
		 * 
		 * @param	index	The codpoint index at which the replacement is being started
		 * @param	len		The number of codepoints that are being replaced
		 * @param	repl	The wide character that will be used to replace the codepoints
		 * @param	n		The number of codepoint that will replace the old ones
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		utf8_string& replace( size_type index , size_type len , value_type repl , size_type n = 1 ){
			return replace( index , len , utf8_string( n , repl ) );
		}
		/**
		 * Replace a range of codepoints by a number of codepoints
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be replaced
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be replaced
		 * @param	repl	The wide character that will be used to replace the codepoints in the range
		 * @param	n		The number of codepoint that will replace the old ones
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		utf8_string& replace( iterator first , iterator last , value_type repl , size_type n = 1 ){
			return raw_replace( first.raw_index , last.raw_index - first.raw_index , utf8_string( n , repl ) );
		}
		/**
		 * Replace a range of codepoints with the contents of the supplied utf8_string
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be replaced
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be replaced
		 * @param	repl	The utf8_string to replace all codepoints in the range
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		utf8_string& replace( iterator first , iterator last , const utf8_string& repl ){
			return raw_replace( first.raw_index , last.raw_index - first.raw_index , repl );
		}
		/**
		 * Replace a number of codepoints of this utf8_string with the contents of the supplied utf8_string
		 * 
		 * @param	index	The codpoint index at which the replacement is being started
		 * @param	len		The number of codepoints that are being replaced
		 * @param	repl	The utf8_string to replace all codepoints in the range
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		utf8_string& replace( size_type index , size_type count , const utf8_string& repl ){
			size_type start_byte = get_num_resulting_bytes( 0 , index );
			return raw_replace(
				start_byte
				, count == utf8_string::npos ? utf8_string::npos : get_num_resulting_bytes( start_byte , count )
				, repl
			);
		}
		/**
		 * Replace a number of bytes of this utf8_string with the contents of the supplied utf8_string
		 * 
		 * @note	As this function is raw, that is not having to compute byte indices,
		 *			it is much faster than the codepoint-base replace function
		 * @param	start_byte	The byte position at which the replacement is being started
		 * @param	byte_count	The number of bytes that are being replaced
		 * @param	repl		The utf8_string to replace all bytes inside the range
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		utf8_string& raw_replace( size_type start_byte , size_type byte_count , const utf8_string& repl );
		
		
		/**
		 * Prepend the supplied utf8_string to this utf8_string
		 * 
		 * @param	appendix	The utf8_string to be prepended
		 * @return	A reference to this utf8_string, which now has the supplied string prepended
		 */
		utf8_string& prepend( const utf8_string& prependix ){
			return raw_replace( 0 , 0 , prependix );
		}
		
		/**
		 * Appends the supplied utf8_string to the end of this utf8_string
		 * 
		 * @param	appendix	The utf8_string to be appended
		 * @return	A reference to this utf8_string, which now has the supplied string appended
		 */
		utf8_string& append( const utf8_string& appendix ){
			return raw_replace( size() , 0 , appendix );
		}
		utf8_string& operator+=( const utf8_string& summand ){
			return raw_replace( size() , 0 , summand );
		}
		
		
		/**
		 * Appends the supplied codepoint to the end of this utf8_string
		 * 
		 * @param	ch	The codepoint to be appended
		 * @return	A reference to this utf8_string, which now has the supplied codepoint appended
		 */
		utf8_string& push_back( value_type ch ){
			return raw_replace( size() , 0 , utf8_string( 1 , ch ) );
		}
		
		/**
		 * Prepends the supplied codepoint to this utf8_string
		 * 
		 * @param	ch	The codepoint to be prepended
		 * @return	A reference to this utf8_string, which now has the supplied codepoint prepended
		 */
		utf8_string& push_front( value_type ch ){
			return raw_replace( 0 , 0 , utf8_string( 1 , ch ) );
		}
		
		
		/**
		 * Adds the supplied utf8_string to a copy of this utf8_string
		 * 
		 * @param	summand		The utf8_string to be added 
		 * @return	A reference to the newly created utf8_string, which now has the supplied string appended
		 */
		utf8_string operator+( const utf8_string& summand ) const {
			utf8_string str = *this;
			str.append( summand );
			return std::move(str);
		}
		
		
		/**
		 * Inserts a given codepoint into this utf8_string at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	ch		The codepoint to be inserted
		 * @return	A reference to this utf8_string, with the supplied codepoint inserted
		 */
		utf8_string& insert( size_type pos , value_type ch ){
			return replace( pos , 0 , utf8_string( 1 , ch ) );
		}
		/**
		 * Inserts a given utf8_string into this utf8_string at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	str		The utf8_string to be inserted
		 * @return	A reference to this utf8_string, with the supplied utf8_string inserted
		 */
		utf8_string& insert( size_type pos , const utf8_string& str ){
			return replace( pos , 0 , str );
		}
		/**
		 * Inserts a given codepoint into this utf8_string at the supplied iterator position
		 * 
		 * @param	it	The iterator position to insert at
		 * @param	ch	The codepoint to be inserted
		 * @return	A reference to this utf8_string, with the supplied codepoint inserted
		 */
		utf8_string& insert( iterator it , value_type ch ){
			return raw_replace( it.raw_index , 0 , utf8_string( 1 , ch ) );
		}
		/**
		 * Inserts a given utf8_string into this utf8_string at the supplied iterator position
		 * 
		 * @param	it	The iterator position to insert at
		 * @param	ch	The utf8_string to be inserted
		 * @return	A reference to this utf8_string, with the supplied codepoint inserted
		 */
		utf8_string& insert( iterator it , const utf8_string& str ){
			return raw_replace( it.raw_index , 0 , str );
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
		utf8_string& raw_insert( size_type pos , const utf8_string& str ){
			return raw_replace( pos , 0 , str );
		}
		
		
		/**
		 * Erases the codepoint at the supplied iterator position
		 * 
		 * @param	pos		The iterator pointing to the position being erased
		 * @return	A reference to this utf8_string, which now has the codepoint erased
		 */
		utf8_string& erase( iterator pos ){
			return raw_replace( pos.raw_index , get_index_bytes( pos.raw_index )  , utf8_string() );
		}
		/**
		 * Erases the codepoints inside the supplied range
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be erased
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be erased
		 * @return	A reference to this utf8_string, which now has the codepoints erased
		 */
		utf8_string& erase( iterator first , iterator last ){
			return raw_replace( first.raw_index , last.raw_index - first.raw_index , utf8_string() );
		}
		/**
		 * Erases a portion of this string
		 * 
		 * @param	pos		The codepoint index to start eraseing from
		 * @param	len		The number of codepoints to be erased from this utf8_string
		 * @return	A reference to this utf8_string, with the supplied portion erased
		 */
		utf8_string& erase( size_type pos , size_type len = 1 ){
			return replace( pos , len , utf8_string() );
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
		utf8_string& raw_erase( size_type pos , size_type len ){
			return raw_replace( pos , len , utf8_string() );
		}
		
		
		/**
		 * Returns a portion of the utf8_string
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be included in the substring
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint in the substring
		 * @return	The utf8_string holding the specified range
		 */
		utf8_string substr( iterator first , iterator last ) const {
			size_type byte_count = last.raw_index - first.raw_index;
			return raw_substr( first.raw_index , byte_count , get_num_resulting_codepoints( first.raw_index , byte_count ) );
		}
		/**
		 * Returns a portion of the utf8_string
		 * 
		 * @param	pos		The codepoint index that should mark the start of the substring
		 * @param	len		The number codepoints to be included within the substring
		 * @return	The utf8_string holding the specified codepoints
		 */
		utf8_string substr( size_type pos , size_type len = utf8_string::npos ) const
		{
			size_type byte_start = get_num_resulting_bytes( 0 , pos );
			
			if( len == utf8_string::npos )
				return raw_substr( byte_start , utf8_string::npos );
			
			size_type byte_count = get_num_resulting_bytes( byte_start , len );
			
			return raw_substr( byte_start , byte_count , len );
		}
		/**
		 * Returns a portion of the utf8_string (indexed on byte-base)
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that substr()
		 * @param	start_byte		The byte position where the substring shall start
		 * @param	byte_count		The number of bytes that the substring shall have
		 * @param	num_substr_codepoints
		 *							(Optional) The number of codepoints
		 *							the substring will have, in case this is already known
		 * @return	The utf8_string holding the specified bytes
		 */
		utf8_string raw_substr( size_type start_byte , size_type byte_count , size_type num_substr_codepoints ) const ;
		utf8_string raw_substr( size_type start_byte , size_type byte_count = utf8_string::npos ) const {
			if( byte_count == utf8_string::npos )
				byte_count = size() - start_byte;
			return raw_substr( start_byte , byte_count , get_num_resulting_codepoints( start_byte , byte_count ) );
		}
		
		
		/**
		 * Finds a specific codepoint inside the utf8_string starting at the supplied codepoint index
		 * 
		 * @param	ch				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the codepoint was found or utf8_string::npos
		 */
		size_type find( value_type ch , size_type start_codepoint = 0 ) const ;
		/**
		 * Finds a specific pattern within the utf8_string starting at the supplied codepoint index
		 * 
		 * @param	ch				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the pattern was found or utf8_string::npos
		 */
		size_type find( const utf8_string& pattern , size_type start_codepoint = 0 ) const {
			if( start_codepoint >= length() )
		        return utf8_string::npos;
		    size_type actual_start = get_num_resulting_bytes( 0 , start_codepoint );
			const char* buffer = this->get_buffer();
		    const char* result = std::strstr( buffer + actual_start , pattern.c_str() );
		    if( !result )
		        return utf8_string::npos;
		    return start_codepoint + get_num_resulting_codepoints( actual_start , result - ( buffer + actual_start ) );
		}
		/**
		 * Finds a specific pattern within the utf8_string starting at the supplied codepoint index
		 * 
		 * @param	ch				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the pattern was found or utf8_string::npos
		 */
		size_type find( const char* pattern , size_type start_codepoint = 0 ) const {
			if( start_codepoint >= length() )
		        return utf8_string::npos;
		    size_type actual_start = get_num_resulting_bytes( 0 , start_codepoint );
			const char* buffer = this->get_buffer();
		    const char* result = std::strstr( buffer + actual_start , pattern );
		    if( !result )
		        return utf8_string::npos;
		    return start_codepoint + get_num_resulting_codepoints( actual_start , result - ( buffer + actual_start ) );
		}
		/**
		 * Finds a specific codepoint inside the utf8_string starting at the supplied byte position
		 * 
		 * @param	ch			The codepoint to look for
		 * @param	start_byte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the codepoint was found or utf8_string::npos
		 */
		size_type raw_find( value_type ch , size_type start_byte = 0 ) const ;
		/**
		 * Finds a specific pattern within the utf8_string starting at the supplied byte position
		 * 
		 * @param	needle		The pattern to look for
		 * @param	start_byte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the pattern was found or utf8_string::npos
		 */
		size_type raw_find( const utf8_string& pattern , size_type start_byte = 0 ) const {
			if( start_byte >= size() )
		        return utf8_string::npos;
			const char* buffer = this->get_buffer();
		    const char* result = std::strstr( buffer + start_byte , pattern.c_str() );
		    if( !result )
		        return utf8_string::npos;
		    return result - buffer;
		}
		/**
		 * Finds a specific pattern within the utf8_string starting at the supplied byte position
		 * 
		 * @param	needle		The pattern to look for
		 * @param	start_byte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the pattern was found or utf8_string::npos
		 */
		size_type raw_find( const char* pattern , size_type start_byte = 0 ) const {
			if( start_byte >= size() )
		        return utf8_string::npos;
			const char* buffer = this->get_buffer();
		    const char* result = std::strstr( buffer + start_byte , pattern );
		    if( !result )
		        return utf8_string::npos;
		    return result - buffer;
		}
		
		/**
		 * Finds the last occourence of a specific codepoint inside the
		 * utf8_string starting backwards at the supplied codepoint index
		 * 
		 * @param	ch				The codepoint to look for
		 * @param	start_codepoint	The index of the first codepoint to start looking from (backwards)
		 * @return	The codepoint index where and if the codepoint was found or utf8_string::npos
		 */
		size_type rfind( value_type ch , size_type start_codepoint = utf8_string::npos ) const ;
		/**
		 * Finds the last occourence of a specific codepoint inside the
		 * utf8_string starting backwards at the supplied byte index
		 * 
		 * @param	ch				The codepoint to look for
		 * @param	start_codepoint	The byte index of the first codepoint to start looking from (backwards)
		 * @return	The codepoint index where and if the codepoint was found or utf8_string::npos
		 */
		size_type raw_rfind( value_type ch , size_type start_byte = utf8_string::npos ) const ;
		
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
		
		
		//! Removes the last codepoint in the utf8_string
		utf8_string& pop_back(){
			size_type pos = back_index();
			return raw_replace( pos , get_index_bytes( pos ) , utf8_string() );
		}
		
		/**
		 * Compares this utf8_string to the supplied one
		 *
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		difference_type compare( const utf8_string& str ) const ;
		
		//! Returns true, if the supplied string compares equal to this one
		bool equals( const utf8_string& str ) const { return equals( str.c_str() ); }
		bool equals( const std::string& str ) const { return equals( str.c_str() ); }
		bool equals( const char* str ) const ;
		bool equals( const value_type* str ) const ;
		
		//! Compare this utf8_string to another string
		bool operator==( const utf8_string& str ) const { return equals( str ); }
		bool operator!=( const utf8_string& str ) const { return !equals( str ); }
		bool operator==( const char* str ) const { return equals( str ); }
		bool operator!=( const char* str ) const { return !equals( str ); }
		bool operator==( const value_type* str ) const { return equals( str ); }
		bool operator!=( const value_type* str ) const { return !equals( str ); }
		bool operator==( const std::string& str ) const { return equals( str ); }
		bool operator!=( const std::string& str ) const { return !equals( str ); }
		
		bool operator>( const utf8_string& str ) const { return str.compare( *this ) > 0; }
		bool operator>=( const utf8_string& str ) const { return str.compare( *this ) >= 0; }
		bool operator<( const utf8_string& str ) const { return str.compare( *this ) < 0; }
		bool operator<=( const utf8_string& str ) const { return str.compare( *this ) <= 0; }
		
		
		//! Get the number of bytes of codepoint in utf8_string
		unsigned char get_codepoint_bytes( size_type codepoint_index ) const {
			return get_num_bytes_of_utf8_char( this->get_buffer() , get_num_resulting_bytes( 0 , codepoint_index ) , this->_buffer_len );
		}
		unsigned char get_index_bytes( size_type byte_index ) const {
			return get_num_bytes_of_utf8_char( this->get_buffer() , byte_index , this->_buffer_len );
		}
		
		
		//! Get the number of bytes before a codepoint, that build up a new codepoint
		unsigned char get_codepoint_pre_bytes( size_type codepoint_index ) const {
			return this->_string_len > codepoint_index ? get_num_bytes_of_utf8_char_before( this->get_buffer() , get_num_resulting_bytes( 0 , codepoint_index ) , this->_buffer_len ) : 1;
		}
		unsigned char get_index_pre_bytes( size_type byte_index ) const {
			return this->_buffer_len > byte_index ? get_num_bytes_of_utf8_char_before( this->get_buffer() , byte_index , this->_buffer_len ) : 1;
		}
		
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

#ifdef _TINY_UTF8_H_USE_IOSTREAM_
static inline std::ostream& operator<<( std::ostream& stream , const utf8_string& str ){
	return stream << str.c_str();
}

static inline std::istream& operator>>( std::istream& stream , utf8_string& str ){
	std::string tmp;
	stream >> tmp;
	str = move(tmp);
	return stream;
}
#endif // ifndef _TINY_UTF8_H_USE_IOSTREAM_

#endif
