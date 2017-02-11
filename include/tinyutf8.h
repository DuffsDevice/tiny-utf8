// Copyright (c) 2017 Jakob Riedle (DuffsDevice)
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
#include <cstddef> // for char32_t, ptrdiff_t

// Define the following macro, if you plan to output an utf8_string
// to an ostream or read an utf8_string from an istream
#ifdef _TINY_UTF8_H_USE_IOSTREAM_
#include <iostream> // for ostream/istream overloadings
#endif

class utf8_string
{
	public:
		
		typedef size_t							size_type;
		typedef ptrdiff_t						difference_type;
		typedef char32_t						value_type;
		typedef const value_type&				const_reference;
		constexpr static size_type				npos = -1;
		
		#ifdef _TINY_UTF8_H_USE_IOSTREAM_
		friend std::ostream& operator<<( std::ostream& , const utf8_string& );
		friend std::istream& operator>>( std::istream& , utf8_string& );
		#endif
		
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
				operator value_type() const { return instance->at( index ); }
				
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
				operator value_type() const { return this->instance->raw_at( this->raw_index ); }
				
				//! Assignment operator
				utf8_codepoint_raw_reference& operator=( value_type ch ){
					this->instance->raw_replace( this->raw_index , this->instance->get_index_bytes( this->raw_index ) , utf8_string( 1 , ch ) );
					return *this;
				}
		};
		
		struct iterator_base{
			typedef utf8_string::value_type			value_type;
			typedef utf8_string::difference_type	difference_type;
			typedef utf8_codepoint_raw_reference	reference;
			typedef void*							pointer;
			typedef std::forward_iterator_tag		iterator_category;
		};
		
	public:
		
		typedef utf8_codepoint_reference	reference;
		
		class iterator : public iterator_base
		{
			protected:
				
				difference_type	raw_index;
				utf8_string*	instance;
				
			public:
				
				//! Ctor
				iterator( difference_type raw_index , utf8_string* instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Default Ctor
				iterator() :
					raw_index( 0 )
					, instance( nullptr )
				{}
				
				//! Default function
				iterator( const iterator& ) = default;
				iterator& operator=( const iterator& ) = default;
				
				//! Get the index
				difference_type index() const {
					return this->raw_index;
				}
				
				//! Set the index
				void set_index( difference_type raw_index ){
					this->raw_index = raw_index;
				}
				
				//! Returns the utf8_string instance the iterator refers to
				utf8_string* get_instance() const { return this->instance; }
				
				//! Increase the Iterator by one
				iterator&  operator++(){ // prefix ++iter
					this->raw_index += this->instance->get_index_bytes( this->raw_index );
					return *this;
				}
				iterator& operator++( int ){ // postfix iter++
					this->raw_index += this->instance->get_index_bytes( this->raw_index );
					return *this;
				}
				
				//! Increase the Iterator %nth times
				iterator operator+( difference_type nth ){
					iterator it{*this};
					it += nth;
					return it;
				}
				iterator& operator+=( difference_type nth ){
					while( nth > 0 ){
						this->raw_index += this->instance->get_index_bytes( this->raw_index );
						nth--;
					}
					while( nth < 0 ){
						this->raw_index -= this->instance->get_index_pre_bytes( this->raw_index );
						nth++;
					}
					return *this;
				}
				
				//! Get the difference between two iterators
				friend int operator-( const iterator& left , const iterator& right );
				
				//! Compare two iterators
				bool operator==( const iterator& it ) const { return this->raw_index == it.raw_index; }
				bool operator!=( const iterator& it ) const { return this->raw_index != it.raw_index; }
				bool operator>( const iterator& it ) const { return this->raw_index > it.raw_index; }
				bool operator>=( const iterator& it ) const { return this->raw_index >= it.raw_index; }
				bool operator<( const iterator& it ) const { return this->raw_index < it.raw_index; }
				bool operator<=( const iterator& it ) const { return this->raw_index <= it.raw_index; }
				
				//! Returns the value of the codepoint behind the iterator
				value_type get(){ return this->instance->raw_at( this->raw_index ); }
				
				//! Returns the value of the codepoint behind the iterator
				utf8_codepoint_raw_reference operator*(){ return (*this->instance)( this->raw_index ); }
				
				//! Check whether there is a Node following this one
				bool has_next() const { return this->instance->raw_at( this->raw_index + this->instance->get_index_bytes( this->raw_index ) ) != 0; }
				
				//! Sets the codepoint
				void set( value_type value ){ this->instance->raw_replace( this->raw_index , this->instance->get_index_bytes( this->raw_index ) , utf8_string( 1 , value ) ); }
				
				//! Check if the Iterator is valid
				explicit operator bool() const { return this->valid(); }
				
				//! Check if the Iterator is valid
				bool valid() const { return this->instance && this->raw_index < this->instance->size(); }
		};
		
		class const_iterator : public iterator
		{
			public:
				
				//! Ctor
				const_iterator( difference_type raw_index , const utf8_string* instance ) :
					iterator( raw_index , const_cast<utf8_string*>(instance) )
				{}
				
				//! Default Ctor
				const_iterator() :
					iterator()
				{}
				
				//! Cast ctor
				const_iterator& operator=( const iterator& it ){
					this->raw_index = it.index();
					this->instance = it.get_instance();
					return *this;
				}
				const_iterator( const iterator& it ) : iterator( it.index() , it.get_instance() ) { }
				
				//! Default copy ctor and copy asg
				const_iterator( const const_iterator& ) = default;
				const_iterator& operator=( const const_iterator& ) = default;
				
				//! Remove setter
				void set( value_type value ) = delete;
				
				//! Returns the value behind the iterator
				value_type operator*() const { return this->instance->raw_at( this->raw_index ); }
		};
		
		class reverse_iterator : public iterator_base
		{
			protected:
				
				difference_type	raw_index;
				utf8_string*	instance;
				
			public:
				
				//! Ctor
				reverse_iterator( difference_type raw_index , utf8_string* instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Default Ctor
				reverse_iterator() :
					raw_index( 0 )
					, instance( nullptr )
				{}
				
				//! Default function
				reverse_iterator( const reverse_iterator& ) = default;
				reverse_iterator& operator=( const reverse_iterator& ) = default;
				
				//! From iterator to reverse_iterator
				reverse_iterator& operator=( const iterator& it ){
					this->raw_index = it.index();
					this->instance = it.get_instance();
					return *this;
				}
				reverse_iterator( const iterator& it ) : raw_index( it.index() ) , instance( it.get_instance() ) { }
				
				//! Get the index
				difference_type index() const {
					return this->raw_index;
				}
				
				//! Set the index
				void set_index( difference_type raw_index ){
					this->raw_index = raw_index;
				}
				
				//! Returns the utf8_string instance the iterator refers to
				utf8_string* get_instance() const { return this->instance; }
				
				reverse_iterator& operator++(){ // prefix iter++
					this->raw_index -= this->instance->get_index_pre_bytes( this->raw_index );
					return *this;
				}
				reverse_iterator& operator++( int ){ // postfix iter++
					this->raw_index -= this->instance->get_index_pre_bytes( this->raw_index );
					return *this;
				}
				reverse_iterator operator+( difference_type nth ){
					reverse_iterator it{*this};
					it += nth;
					return it;
				}
				reverse_iterator& operator+=( difference_type nth ){
					while( nth > 0 ){
						this->raw_index -= this->instance->get_index_pre_bytes( this->raw_index );
						nth--;
					}
					while( nth < 0 ){
						this->raw_index += this->instance->get_index_bytes( this->raw_index );
						nth++;
					}
					return *this;
				}
				
				//! Get the difference between two iterators
				friend int operator-( const reverse_iterator& left , const reverse_iterator& right );
				
				//! Compare two iterators
				bool operator==( const reverse_iterator& it ) const { return this->raw_index == it.raw_index; }
				bool operator!=( const reverse_iterator& it ) const { return this->raw_index != it.raw_index; }
				bool operator>( const reverse_iterator& it ) const { return this->raw_index < it.raw_index; }
				bool operator>=( const reverse_iterator& it ) const { return this->raw_index <= it.raw_index; }
				bool operator<( const reverse_iterator& it ) const { return this->raw_index > it.raw_index; }
				bool operator<=( const reverse_iterator& it ) const { return this->raw_index >= it.raw_index; }
				
				//! Returns the value of the codepoint behind the iterator
				value_type get(){ return this->instance->raw_at( this->raw_index ); }
				
				//! Sets the codepoint
				void set( value_type value ){ instance->raw_replace( this->raw_index , this->instance->get_index_bytes( this->raw_index ) , utf8_string( 1 , value ) ); }
				
				//! Returns the value of the codepoint behind the iterator
				utf8_codepoint_raw_reference operator*(){ return (*this->instance)( this->raw_index ); }
				
				//! Check whether there is a Node following this one
				bool has_next() const { return this->raw_index > 0; }
				
				//! Check if the Iterator is valid
				operator bool() const { return this->valid(); }
				
				//! Check if the Iterator is valid
				bool valid() const { return this->instance && this->raw_index >= 0; }
		};
		
		class const_reverse_iterator : public reverse_iterator
		{
			public:
				
				//! Ctor
				const_reverse_iterator( difference_type raw_index , const utf8_string* instance ) :
					reverse_iterator( raw_index , const_cast<utf8_string*>(instance) )
				{}
				
				//! Default Ctor
				const_reverse_iterator() :
					reverse_iterator()
				{}
				
				//! Cast ctor
				const_reverse_iterator& operator=( const reverse_iterator& it ){
					this->raw_index = it.index();
					this->instance = it.get_instance();
					return *this;
				}
				const_reverse_iterator( const reverse_iterator& it ) : reverse_iterator( it.index() , it.get_instance() ) { }
				
				//! Default functions
				const_reverse_iterator( const const_reverse_iterator& ) = default;
				const_reverse_iterator& operator=( const const_reverse_iterator& ) = default;
				
				//! Remove setter
				void set( value_type value ) = delete;
				
				//! Returns the value behind the iterator
				value_type operator*() const { return this->instance->raw_at( this->raw_index ); }
		};
		
	private:
		
		//! Attributes
		char*			buffer;
		size_type		buffer_len;
		size_type		string_len;
		size_type*		indices_of_multibyte;
		size_type		indices_len;
		mutable bool	misformatted;
		bool			static_buffer;
		
		//! Frees the internal buffer of indices and sets it to the supplied value
		void reset_indices( size_type* indices_of_multibyte = nullptr , size_type indices_len = 0 ){
			if( this->indices_of_multibyte )
				delete[] this->indices_of_multibyte;
			this->indices_len = indices_len;
			this->indices_of_multibyte = indices_of_multibyte;
		}
		
		//! Frees the internal buffer of indices
		void reset_buffer( char* buffer = nullptr , size_type buffer_len = 0 ){
			if( this->buffer && !this->static_buffer )
				delete[] this->buffer;
			this->static_buffer = false;
			this->buffer_len = buffer_len;
			this->buffer = buffer;
		}
		
		/**
		 * Returns an std::string with the UTF-8 BOM prepended
		 */
		std::string cpp_str_bom() const ;
		
		/**
		 * Returns the number of bytes to expect behind this one (including this one) that belong to this utf8 char
		 */
		unsigned char get_num_bytes_of_utf8_char( const char* data , size_type first_byte_index ) const ;
		
		/**
		 * Returns the number of bytes to expect before this one (including this one) that belong to this utf8 char
		 */
		unsigned char get_num_bytes_of_utf8_char_before( const char* data , size_type current_byte_index ) const ;
		
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
		
		//! Returns the actual byte index of the supplied codepoint index
		size_type get_actual_index( size_type codepoint_index ) const ;
		
		/**
		 * Get the byte index of the last codepoint
		 */
		size_type back_index() const { return size() - get_index_pre_bytes( size() ); }
		/**
		 * Get the byte index of the index behind the last codepoint
		 */
		size_type end_index() const { return size(); }
		
		/**
		 * Decodes a given input of rle utf8 data to a
		 * unicode codepoint and returns the number of bytes it used
		 */
		unsigned char decode_utf8( const char* data , value_type& codepoint ) const ;
		
		/**
		 * Encodes a given codepoint to a character buffer of at least 7 bytes
		 * and returns the number of bytes it used
		 */
		static unsigned char encode_utf8( value_type codepoint , char* dest );
		
		/**
		 * Counts the number of bytes required
		 * to hold the given wide character string.
		 * numBytes is set to the number of multibyte
		 * codepoints (ones that consume mor than 1 byte).
		 */
		static size_type get_num_required_bytes( const value_type* lit , size_type& numMultibytes );
		
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
		
		//! Ctor from buffer and indices
		utf8_string( char* buffer , size_type buffer_len , size_type string_len , size_type* indices_of_multibyte , size_type indices_len ) :
			buffer( buffer )
			, buffer_len( buffer_len )
			, string_len( string_len )
			, indices_of_multibyte( indices_of_multibyte )
			, indices_len( indices_len )
			, misformatted( false )
			, static_buffer( false )
		{}
		
	public:
		
		/**
		 * Default Ctor
		 * 
		 * @note Creates an Instance of type utf8_string that is empty
		 */
		utf8_string() :
			buffer( nullptr )
			, buffer_len( 0 )
			, string_len( 0 )
			, indices_of_multibyte( nullptr )
			, indices_len( 0 )
			, misformatted( false )
			, static_buffer( false )
		{}
		
		/**
		 * Contructor taking an utf8 sequence and the maximum length to read from it (in number of codepoints)
		 * 
		 * @note	Creates an Instance of type utf8_string that holds the given utf8 sequence
		 * @param	str		The UTF-8 sequence to fill the utf8_string with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		utf8_string( const char* str , size_type len = utf8_string::npos );
		
		
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
		 * Named constructor that constructs a utf8_string from a string literal without
		 * parsing the input string for UTF-8 sequences
		 *
		 * @param	str		The ascii sequence that will be imported
		 */
		static const utf8_string ascii_constant( const char* str ){
			size_type	len = std::strlen( str ) + 1;
			utf8_string wstr = utf8_string( const_cast<char*>( str ) , len , len - 1 , nullptr , 0 );
			wstr.static_buffer = true;
			return (utf8_string&&)wstr;
		}
		/**
		 * Named constructor that constructs a utf8_string from an std::string without
		 * parsing the input string for UTF-8 sequences
		 *
		 * @param	str		The ascii sequence that will be imported
		 */
		static utf8_string ascii_constant( const std::string& str ){
			size_type	len = str.length() + 1;
			char*	buffer = new char[len];
			std::memcpy( buffer , str.data() , len );
			return utf8_string( buffer , len , len - 1 , nullptr , 0 );
		}
		
		
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
		 */
		utf8_string( const value_type* str );
		
		
		/**
		 * Destructor
		 * 
		 * @note	Destructs a utf8_string at the end of its lifetime releasing all held memory
		 */
		~utf8_string(){
			this->reset_indices();
			this->reset_buffer();
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
			this->string_len = 0;
			this->misformatted = false;
			reset_buffer();
			reset_indices();
		}
		
		
		/**
		 * Returns whether there is an UTF-8 error in the string
		 * 
		 * @return	True, if there is an encoding error, false, if the contained data is properly encoded
		 */
		bool is_misformatted() const { return this->misformatted; }
		
		
		/**
		 * Returns the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	The Codepoint at position 'n'
		 */
		value_type at( size_type n ) const ;
		/**
		 * Returns the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	byte_index	The byte position of the codepoint to receive
		 * @return	The codepoint at the supplied position
		 */
		value_type raw_at( size_type byte_index ) const {
			if( !this->requires_unicode() )
				return this->buffer[byte_index];
			value_type dest;
			decode_utf8( this->buffer + byte_index , dest );
			return dest;
		}
		
		
		/**
		 * Returns an iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified codepoint index
		 */
		iterator get( size_type n ){ return iterator( get_actual_index(n) , this ); }
		const_iterator get( size_type n ) const { return const_iterator( get_actual_index(n) , this ); }
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
		reverse_iterator rget( size_type n ){ return reverse_iterator( get_actual_index(n) , this ); }
		const_reverse_iterator rget( size_type n ) const { return const_reverse_iterator( get_actual_index(n) , this ); }
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
		const char* c_str() const { return this->buffer ? this->buffer : ""; }
		const char* data() const { return this->buffer; }
		
		
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
		size_type length() const { return this->string_len; }
		
		
		/**
		 * Get the number of bytes that are used by this utf8_string
		 * 
		 * @note	Returns the number of bytes required to hold the contained wide string,
		 *			That is, without counting the trailling '\0'
		 * @return	Number of bytes (not codepoints!)
		 */
		size_type size() const { return std::max<size_type>( 1 , this->buffer_len ) - 1; }
		
		
		/**
		 * Check, whether this utf8_string is empty
		 * 
		 * @note	Returns True, if this utf8_string is empty, that also is comparing true with ""
		 * @return	True, if this utf8_string is empty, false if its length is >0
		 */
		bool empty() const { return !this->string_len; }
		
		
		/**
		 * Check whether the data inside this utf8_string cannot be iterated by an std::string
		 * 
		 * @note	Returns true, if the utf8_string has codepoints that exceed 7 bits to be stored
		 * @return	True, if there are UTF-8 formatted byte sequences,
		 *			false, if there are only ascii characters (<128) inside
		 */
		bool requires_unicode() const { return this->indices_len > 0; }
		
		
		/**
		 * Returns the number of multibytes within this utf8_string
		 * 
		 * @return	The number of codepoints that take more than one byte
		 */
		size_type get_num_multibytes() const { return this->indices_len; }
		
		
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
		value_type front() const { return raw_at( 0 ); }
		/**
		 * Returns a reference to the last codepoint in the utf8_string
		 * 
		 * @return	A reference wrapper to the last codepoint in the utf8_string
		 */
		utf8_codepoint_raw_reference back(){ return utf8_codepoint_raw_reference( empty() ? 0 : back_index() , this ); }
		value_type back() const { return raw_at( empty() ? 0 : back_index() ); }
		
		
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
			replace( index , 1 , utf8_string( n , repl ) );
			return *this;
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
			replace( index , len , utf8_string( n , repl ) );
			return *this;
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
			raw_replace( first.index() , last.index() - first.index() , utf8_string( n , repl ) );
			return *this;
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
			raw_replace( first.index() , last.index() - first.index() , repl );
			return *this;
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
			size_type actualStartIndex = get_actual_index( index );
			raw_replace(
				actualStartIndex
				, count == utf8_string::npos ? utf8_string::npos : get_num_resulting_bytes( actualStartIndex , count )
				, repl
			);
			return *this;
		}
		/**
		 * Replace a number of bytes of this utf8_string with the contents of the supplied utf8_string
		 * 
		 * @note	As this function is raw, that is not having to compute byte indices,
		 *			it is much faster than the codepoint-base replace function
		 * @param	byte_index	The byte position at which the replacement is being started
		 * @param	byte_count	The number of bytes that are being replaced
		 * @param	repl		The utf8_string to replace all bytes inside the range
		 * @return	A reference to this utf8_string, which now has the replaced part in it
		 */
		void raw_replace( size_type byte_index , size_type byte_count , const utf8_string& repl );
		
		
		/**
		 * Prepend the supplied utf8_string to this utf8_string
		 * 
		 * @param	appendix	The utf8_string to be prepended
		 * @return	A reference to this utf8_string, which now has the supplied string prepended
		 */
		utf8_string& prepend( const utf8_string& prependix ){
			raw_replace( 0 , 0 , prependix );
			return *this;
		}
		
		/**
		 * Appends the supplied utf8_string to the end of this utf8_string
		 * 
		 * @param	appendix	The utf8_string to be appended
		 * @return	A reference to this utf8_string, which now has the supplied string appended
		 */
		utf8_string& append( const utf8_string& appendix ){
			raw_replace( size() , 0 , appendix );
			return *this;
		}
		utf8_string& operator+=( const utf8_string& summand ){
			raw_replace( size() , 0 , summand );
			return *this;
		}
		
		
		/**
		 * Appends the supplied codepoint to the end of this utf8_string
		 * 
		 * @param	ch	The codepoint to be appended
		 * @return	A reference to this utf8_string, which now has the supplied codepoint appended
		 */
		utf8_string& push_back( value_type ch ){
			raw_replace( size() , 0 , utf8_string( 1 , ch ) );
			return *this;
		}
		
		/**
		 * Prepends the supplied codepoint to this utf8_string
		 * 
		 * @param	ch	The codepoint to be prepended
		 * @return	A reference to this utf8_string, which now has the supplied codepoint prepended
		 */
		utf8_string& push_front( value_type ch ){
			raw_replace( 0 , 0 , utf8_string( 1 , ch ) );
			return *this;
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
			return std::move( str );
		}
		
		
		/**
		 * Inserts a given codepoint into this utf8_string at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	ch		The codepoint to be inserted
		 * @return	A reference to this utf8_string, with the supplied codepoint inserted
		 */
		utf8_string& insert( size_type pos , value_type ch ){
			replace( pos , 0 , utf8_string( 1 , ch ) );
			return *this;
		}
		/**
		 * Inserts a given utf8_string into this utf8_string at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	str		The utf8_string to be inserted
		 * @return	A reference to this utf8_string, with the supplied utf8_string inserted
		 */
		utf8_string& insert( size_type pos , const utf8_string& str ){
			replace( pos , 0 , str );
			return *this;
		}
		/**
		 * Inserts a given codepoint into this utf8_string at the supplied iterator position
		 * 
		 * @param	it	The iterator position to insert at
		 * @param	ch	The codepoint to be inserted
		 * @return	A reference to this utf8_string, with the supplied codepoint inserted
		 */
		utf8_string& insert( iterator it , value_type ch ){
			raw_replace( it.index() , 0 , utf8_string( 1 , ch ) );
			return *this;
		}
		/**
		 * Inserts a given utf8_string into this utf8_string at the supplied iterator position
		 * 
		 * @param	it	The iterator position to insert at
		 * @param	ch	The utf8_string to be inserted
		 * @return	A reference to this utf8_string, with the supplied codepoint inserted
		 */
		utf8_string& insert( iterator it , const utf8_string& str ){
			raw_replace( it.index() , 0 , str );
			return *this;
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
			raw_replace( pos , 0 , str );
			return *this;
		}
		
		
		/**
		 * Erases the codepoint at the supplied iterator position
		 * 
		 * @param	pos		The iterator pointing to the position being erased
		 * @return	A reference to this utf8_string, which now has the codepoint erased
		 */
		utf8_string& erase( iterator pos ){
			raw_replace( pos.index() , get_index_bytes( pos.index() )  , utf8_string() );
			return *this;
		}
		/**
		 * Erases the codepoints inside the supplied range
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be erased
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be erased
		 * @return	A reference to this utf8_string, which now has the codepoints erased
		 */
		utf8_string& erase( iterator first , iterator last ){
			raw_replace( first.index() , last.index() - first.index() + get_index_bytes( last.index() ), utf8_string() );
			return *this;
		}
		/**
		 * Erases a portion of this string
		 * 
		 * @param	pos		The codepoint index to start eraseing from
		 * @param	len		The number of codepoints to be erased from this utf8_string
		 * @return	A reference to this utf8_string, with the supplied portion erased
		 */
		utf8_string& erase( size_type pos , size_type len = 1 ){
			replace( pos , len , utf8_string() );
			return *this;
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
			raw_replace( pos , len , utf8_string() );
			return *this;
		}
		
		
		/**
		 * Returns a portion of the utf8_string
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be included in the substring
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint in the substring
		 * @return	The utf8_string holding the specified range
		 */
		utf8_string substr( iterator first , iterator last ) const {
			size_type byte_count = last.index() - first.index();
			return raw_substr( first.index() , byte_count , get_num_resulting_codepoints( first.index() , byte_count ) );
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
			difference_type	actualStartIndex = get_actual_index( pos );
			
			if( len == utf8_string::npos )
				return raw_substr( actualStartIndex , utf8_string::npos );
			
			difference_type	actualEndIndex = len ? get_actual_index( pos + len ) : actualStartIndex;
			
			return raw_substr( actualStartIndex , actualEndIndex - actualStartIndex , len );
		}
		/**
		 * Returns a portion of the utf8_string (indexed on byte-base)
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that substr()
		 * @param	byte_index		The byte position where the substring shall start
		 * @param	byte_count		The number of bytes that the substring shall have
		 * @param	numCodepoints	(Optional) The number of codepoints
		 *							the substring will have, in case this is already known
		 * @return	The utf8_string holding the specified bytes
		 */
		utf8_string raw_substr( size_type byte_index , size_type byte_count , size_type numCodepoints ) const ;
		utf8_string raw_substr( size_type byte_index , size_type byte_count = utf8_string::npos ) const {
			if( byte_count == utf8_string::npos )
				byte_count = size() - byte_index;
			return raw_substr( byte_index , byte_count , get_num_resulting_codepoints( byte_index , byte_count ) );
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
		    size_type actual_start = get_actual_index( start_codepoint );
		    const char* result = std::strstr( buffer + actual_start , pattern.c_str() );
		    if( !result )
		        return utf8_string::npos;
		    return start_codepoint + get_num_resulting_codepoints( actual_start , result - (buffer + actual_start) );
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
		    size_type actual_start = get_actual_index( start_codepoint );
		    const char* result = std::strstr( buffer + actual_start , pattern );
		    if( !result )
		        return utf8_string::npos;
		    return start_codepoint + get_num_resulting_codepoints( actual_start , result - (buffer + actual_start) );
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
			raw_replace( pos , get_index_bytes( pos ) , utf8_string() );
			return *this;
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
			return get_num_bytes_of_utf8_char( this->buffer , get_actual_index(codepoint_index) );
		}
		unsigned char get_index_bytes( size_type byte_index ) const {
			return get_num_bytes_of_utf8_char( this->buffer , byte_index );
		}
		
		
		//! Get the number of bytes before a codepoint, that build up a new codepoint
		unsigned char get_codepoint_pre_bytes( size_type codepoint_index ) const {
			return this->string_len > codepoint_index ? get_num_bytes_of_utf8_char_before( this->buffer , get_actual_index(codepoint_index) ) : 1;
		}
		unsigned char get_index_pre_bytes( size_type byte_index ) const {
			return this->buffer_len > byte_index ? get_num_bytes_of_utf8_char_before( this->buffer , byte_index ) : 1;
		}
		
		
		
		//! Friend iterator difference computation functions
		friend int				operator-( const iterator& left , const iterator& right );
		friend int				operator-( const reverse_iterator& left , const reverse_iterator& right );
};

extern int								operator-( const utf8_string::iterator& left , const utf8_string::iterator& right );
extern int								operator-( const utf8_string::reverse_iterator& left , const utf8_string::reverse_iterator& right );
extern utf8_string::iterator			operator+( const utf8_string::iterator& it , utf8_string::size_type nth );
extern utf8_string::reverse_iterator	operator+( const utf8_string::reverse_iterator& it , utf8_string::size_type nth );

#ifdef _TINY_UTF8_H_USE_IOSTREAM_
std::ostream& operator<<( std::ostream& stream , const utf8_string& str ){
	return stream << str.c_str();
}

std::istream& operator>>( std::istream& stream , utf8_string& str ){
	std::string tmp;
	stream >> tmp;
	str = move(tmp);
	return stream;
}
#endif // ifndef _TINY_UTF8_H_USE_IOSTREAM_

#endif
