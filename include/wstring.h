// Copyright (c) 2014 Jakob Riedle (DuffsDevice)
// 
// Version 1.0
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef _UTF8_LIB_
#define _UTF8_LIB_

#include <memory> // for std::unique_ptr
#include <cstring> // for std::memcpy, std::strlen
#include <string> // for std::string

class wstring
{
	private:
		
		class wstring_reference
		{
			private:
			
				ptrdiff_t	index;
				wstring&	instance;
			
			public:
				
				//! Ctor
				wstring_reference( ptrdiff_t index , wstring& instance ) :
					index( index )
					, instance( instance )
				{}
				
				//! Cast to wide char
				operator wchar_t() const { return instance.at( index ); }
				
				//! Assignment operator
				wstring_reference& operator=( wchar_t ch ){
					instance.replace( index , ch );
					return *this;
				}
		};
		
		class wstring_raw_reference
		{
			private:
			
				ptrdiff_t	raw_index;
				wstring&	instance;
			
			public:
				
				//! Ctor
				wstring_raw_reference( ptrdiff_t raw_index , wstring& instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Cast to wide char
				operator wchar_t() const { return this->instance.raw_at( this->raw_index ); }
				
				//! Assignment operator
				wstring_raw_reference& operator=( wchar_t ch ){
					this->instance.raw_replace( this->raw_index , this->instance.get_index_bytes( this->raw_index ) , wstring( 1 , ch ) );
					return *this;
				}
		};
		
		class wstring_iterator : public std::iterator<std::forward_iterator_tag,size_t>
		{
			protected:
			
				size_t		raw_index;
				wstring&	instance;
			
			public:
			
				//! Ctor
				wstring_iterator( size_t raw_index , wstring& instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Default function
				wstring_iterator( const wstring_iterator& ) = default;
				wstring_iterator& operator=( const wstring_iterator& it ){ this->raw_index = it.raw_index; return *this; }
				
				//! Get the index
				ptrdiff_t index() const {
					return this->raw_index;
				}
				
				//! Set the index
				void set_index( ptrdiff_t raw_index ){
					this->raw_index = raw_index;
				}
				
				//! Returns the wstring instance the iterator refers to
				wstring& get_instance() const { return this->instance; }
				
				//! Increase the Iterator by one
				wstring_iterator&  operator++(){ // prefix ++iter
					this->raw_index += this->instance.get_index_bytes( this->raw_index );
					return *this;
				}
				wstring_iterator& operator++( int ){ // postfix iter++
					this->raw_index += this->instance.get_index_bytes( this->raw_index );
					return *this;
				}
				
				//! Increase the Iterator %nth times
				friend wstring_iterator operator+( const wstring_iterator& it , size_t nth );
				wstring_iterator& operator+=( size_t nth ){
					while( nth-- > 0 )
						this->raw_index += this->instance.get_index_bytes( this->raw_index );
					return *this;
				}
				
				//! Get the difference between two iterators
				friend int operator-( const wstring_iterator& left , const wstring_iterator& right );
				
				//! Compare two iterators
				bool operator==( const wstring_iterator& it ) const { return this->raw_index == it.raw_index; }
				bool operator!=( const wstring_iterator& it ) const { return this->raw_index != it.raw_index; }
				bool operator>( const wstring_iterator& it ) const { return this->raw_index > it.raw_index; }
				bool operator>=( const wstring_iterator& it ) const { return this->raw_index >= it.raw_index; }
				bool operator<( const wstring_iterator& it ) const { return this->raw_index < it.raw_index; }
				bool operator<=( const wstring_iterator& it ) const { return this->raw_index <= it.raw_index; }
				
				//! Returns the value of the codepoint behind the iterator
				wchar_t get(){ return this->instance.raw_at( this->raw_index ); }
				
				//! Returns the value of the codepoint behind the iterator
				wstring_raw_reference operator*(){ return this->instance( this->raw_index ); }
				
				//! Check whether there is a Node following this one
				bool has_next() const { return this->instance.raw_at( this->raw_index + this->instance.get_index_bytes( this->raw_index ) ) != 0; }
				
				//! Sets the codepoint
				void set( wchar_t value ){ instance.raw_replace( this->raw_index , this->instance.get_index_bytes( this->raw_index ) , wstring( 1 , value ) ); }
				
				//! Check if the Iterator is valid
				explicit operator bool() const { return this->valid(); }
				
				//! Check if the Iterator is valid
				bool valid() const { return this->raw_index < this->instance.size(); }
		};
		
		class wstring_const_iterator : public wstring_iterator
		{
			public:
				
				//! Ctor
				wstring_const_iterator( size_t raw_index , const wstring& instance ) :
					wstring_iterator( raw_index , const_cast<wstring&>(instance) )
				{}
				
				//! Cast ctor
				wstring_const_iterator& operator=( const wstring_iterator& it ){ this->raw_index = it.index(); this->instance = it.get_instance(); return *this; }
				wstring_const_iterator( const wstring_iterator& it ) : wstring_iterator( it.index() , it.get_instance() ) { }
				
				//! Default function
				wstring_const_iterator( const wstring_const_iterator& ) = default;
				wstring_const_iterator& operator=( const wstring_const_iterator& it ){ this->raw_index = it.raw_index; return *this; }
				
				//! Remove setter
				void set( wchar_t value ) = delete;
				
				//! Returns the value behind the iterator
				wchar_t operator*(){ return this->instance.raw_at( this->raw_index ); }
		};
		
		class wstring_reverse_iterator
		{
			protected:
				
				ptrdiff_t	raw_index;
				wstring&	instance;
				
			public:
				
				//! Ctor
				wstring_reverse_iterator( ptrdiff_t raw_index , wstring& instance ) :
					raw_index( raw_index )
					, instance( instance )
				{}
				
				//! Default function
				wstring_reverse_iterator( const wstring_reverse_iterator& ) = default;
				wstring_reverse_iterator& operator=( const wstring_reverse_iterator& it ){ this->raw_index = it.raw_index; return *this; }
				
				//! From wstring_iterator to wstring_reverse_iterator
				wstring_reverse_iterator& operator=( const wstring_iterator& it ){ this->raw_index = it.index(); this->instance = it.get_instance(); return *this; }
				wstring_reverse_iterator( const wstring_iterator& it ) : raw_index( it.index() ) , instance( it.get_instance() ) { }
				
				//! Get the index
				ptrdiff_t index() const {
					return this->raw_index;
				}
				
				//! Set the index
				void set_index( ptrdiff_t raw_index ){
					this->raw_index = raw_index;
				}
				
				//! Returns the wstring instance the iterator refers to
				wstring& get_instance() const { return this->instance; }
				
				wstring_reverse_iterator& operator++(){ // prefix iter++
					this->raw_index -= this->instance.get_index_pre_bytes( this->raw_index );
					return *this;
				}
				wstring_reverse_iterator& operator++( int ){ // postfix iter++
					this->raw_index -= this->instance.get_index_pre_bytes( this->raw_index );
					return *this;
				}
				friend wstring_reverse_iterator operator+( const wstring_reverse_iterator& it , size_t nth );
				wstring_reverse_iterator& operator+=( size_t nth ){
					while( nth-- > 0 )
						this->raw_index -= this->instance.get_index_pre_bytes( this->raw_index );
					return *this;
				}
				
				//! Get the difference between two iterators
				friend int operator-( const wstring_reverse_iterator& left , const wstring_reverse_iterator& right );
				
				//! Compare two iterators
				bool operator==( const wstring_reverse_iterator& it ) const { return this->raw_index == it.raw_index; }
				bool operator!=( const wstring_reverse_iterator& it ) const { return this->raw_index != it.raw_index; }
				bool operator>( const wstring_reverse_iterator& it ) const { return this->raw_index < it.raw_index; }
				bool operator>=( const wstring_reverse_iterator& it ) const { return this->raw_index <= it.raw_index; }
				bool operator<( const wstring_reverse_iterator& it ) const { return this->raw_index > it.raw_index; }
				bool operator<=( const wstring_reverse_iterator& it ) const { return this->raw_index >= it.raw_index; }
				
				//! Returns the value of the codepoint behind the iterator
				wchar_t get(){ return this->instance.raw_at( this->raw_index ); }
				
				//! Sets the codepoint
				void set( wchar_t value ){ instance.raw_replace( this->raw_index , this->instance.get_index_bytes( this->raw_index ) , wstring( 1 , value ) ); }
				
				//! Returns the value of the codepoint behind the iterator
				wstring_raw_reference operator*(){ return this->instance( this->raw_index ); }
				
				//! Check whether there is a Node following this one
				bool has_next() const { return this->raw_index > 0; }
				
				//! Check if the Iterator is valid
				operator bool() const { return this->valid(); }
				
				//! Check if the Iterator is valid
				bool valid() const { return this->raw_index >= 0; }
		};
		
		class wstring_const_reverse_iterator : public wstring_reverse_iterator
		{
			public:
				
				//! Ctor
				wstring_const_reverse_iterator( size_t raw_index , const wstring& instance ) :
					wstring_reverse_iterator( raw_index , const_cast<wstring&>(instance) )
				{}
				
				//! Cast ctor
				wstring_const_reverse_iterator& operator=( const wstring_reverse_iterator& it ){ this->set_index( it.index() ); this->instance = it.get_instance(); return *this; }
				wstring_const_reverse_iterator( const wstring_reverse_iterator& it ) : wstring_reverse_iterator( it.index() , it.get_instance() ) { }
				
				//! Default functions
				wstring_const_reverse_iterator( const wstring_const_reverse_iterator& ) = default;
				wstring_const_reverse_iterator& operator=( const wstring_const_reverse_iterator& it ){ this->raw_index = it.raw_index; return *this; }
				
				//! Remove setter
				void set( wchar_t value ) = delete;
				
				//! Returns the value behind the iterator
				wchar_t operator*(){ return this->instance.raw_at( this->raw_index ); }
		};
		
		//! Attributes
		char*			buffer;
		size_t			buffer_len;
		size_t			string_len;
		size_t*			indices_of_multibyte;
		size_t			indices_len;
		mutable bool	misformated;
		
		//! Frees the internal buffer of indices and sets it to the supplied value
		void reset_indices( size_t* indices_of_multibyte = nullptr , size_t indices_len = 0 ){
			if( this->indices_of_multibyte )
				delete[] this->indices_of_multibyte;
			this->indices_len = indices_len;
			this->indices_of_multibyte = indices_of_multibyte;
		}
		
		//! Frees the internal buffer of indices
		void reset_buffer( char* buffer = nullptr , size_t buffer_len = 0 ){
			if( this->buffer )
				delete[] this->buffer;
			this->buffer_len = buffer_len;
			this->buffer = buffer;
		}
		
		/**
		 * Returns the number of bytes to expect behind this one (including this one) that belong to this utf8 char
		 */
		static unsigned char get_num_bytes_of_utf8_char( unsigned char firstByte )
		{
			if( (firstByte & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
				return 2;
			else if( (firstByte & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
				return 3;
			else if( (firstByte & 0xF8) == 0xF0 )  // 11110XXX  four bytes
				return 4;
			else if( (firstByte & 0xFC) == 0xF8 )  // 111110XX  five bytes
				return 5;
			else if( (firstByte & 0xFE) == 0xFC )  // 1111110X  six bytes
				return 6;
			return 1; // one byte
		}
		
		/**
		 * Returns the number of bytes to expect before this one (including this one) that belong to this utf8 char
		 */
		static unsigned char get_num_bytes_of_utf8_char_before( const char* data )
		{
			if( (data[-2] & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
				return 2;
			else if( (data[-3] & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
				return 3;
			else if( (data[-4] & 0xF8) == 0xF0 )  // 11110XXX  four bytes
				return 4;
			else if( (data[-5] & 0xFC) == 0xF8 )  // 111110XX  five bytes
				return 5;
			else if( (data[-6] & 0xFE) == 0xFC )  // 1111110X  six bytes
				return 6;
			return 1; // one byte
		}
		
		/**
		 * Returns the number of bytes to expect behind this one that belong to this utf8 char
		 */
		static unsigned char get_num_bytes_of_utf8_codepoint( wchar_t codepoint )
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
		size_t get_actual_index( size_t codepoint_index ) const ;
		
		/**
		 * Get the byte index of the last codepoint
		 */
		size_t back_index() const { return size() - get_index_pre_bytes( size() ); }
		/**
		 * Get the byte index of the index behind the last codepoint
		 */
		size_t end_index() const { return size(); }
		
		/**
		 * Decodes a given input of rle utf8 data to a
		 * unicode codepoint and returns the number of bytes it used
		 */
		static unsigned char decode_utf8( const char* data , wchar_t& codepoint , bool& hasError );
		
		/**
		 * Encodes a given codepoint to a character buffer of at least 7 bytes
		 */
		static unsigned char encode_utf8( wchar_t codepoint , char* dest , bool& hasError );
		
		/**
		 * Counts the number of bytes required
		 * to hold the given wide character string.
		 * numBytes is set to the number of multibyte
		 * codepoints (ones that consume mor than 1 byte).
		 */
		static size_t get_num_required_bytes( const wchar_t* lit , size_t& numMultibytes );
		
		/**
		 * Counts the number of codepoints
		 * that are built up of the supplied amout of bytes
		 */
		size_t get_num_resulting_codepoints( size_t byte_start , size_t byte_count ) const ;
		
		/**
		 * Counts the number of bytes
		 * required to hold the supplied amount of codepoints starting at the supplied byte index
		 */
		size_t get_num_resulting_bytes( size_t byte_start , size_t codepoint_count ) const ;
		
		//! Ctor from buffer and indices
		wstring( char* buffer , wchar_t buffer_len , size_t string_len , size_t* indices_of_multibyte , size_t indices_len ) :
			buffer( buffer )
			, buffer_len( buffer_len )
			, string_len( string_len )
			, indices_of_multibyte( indices_of_multibyte )
			, indices_len( indices_len )
			, misformated( false )
		{}
		
	public:
		
		typedef size_t									size_type;
		typedef wchar_t									value_type;
		typedef wstring::wstring_iterator				iterator;
		typedef wstring::wstring_const_iterator			const_iterator;
		typedef wstring::wstring_reverse_iterator		reverse_iterator;
		typedef wstring::wstring_const_reverse_iterator	const_reverse_iterator;
		constexpr static size_t							npos = -1;
		
		
		/**
		 * Default Ctor
		 * 
		 * @note Creates an Instance of type wstring that is empty
		 */
		wstring() :
			buffer( new char[1]{0} )
			, buffer_len( 1 )
			, string_len( 0 )
			, indices_of_multibyte( nullptr )
			, indices_len( 0 )
			, misformated( false )
		{}
		
		/**
		 * Contructor taking an utf8 sequence and the maximum length to read from it (in number of codepoints)
		 * 
		 * @note	Creates an Instance of type wstring that holds the given utf8 sequence
		 * @param	str		The UTF-8 sequence to fill the wstring with
		 * @param	len		(Optional) The maximum number of codepoints to read from the sequence
		 */
		wstring( const char* str , size_t len = wstring::npos );
		
		
		/**
		 * Contructor taking an std::string
		 * 
		 * @note	Creates an Instance of type wstring that holds the given data sequence
		 * @param	str		The byte data, that will be interpreted as UTF-8
		 */
		wstring( std::string str ) :
			wstring( str.c_str() )
		{}
		
		/**
		 * Named constructor that constructs a wstring from a string literal without
		 * parsing the input string for UTF-8 sequences
		 *
		 * @param	str		The ascii sequence that will be imported
		 */
		static wstring ascii_constant( const char* str ){
			size_t	len = std::strlen( str ) + 1;
			char*	buffer = new char[len];
			std::memcpy( buffer , str , len );
			return wstring( buffer , len , len - 1 , nullptr , 0 );
		}
		/**
		 * Named constructor that constructs a wstring from an std::string without
		 * parsing the input string for UTF-8 sequences
		 *
		 * @param	str		The ascii sequence that will be imported
		 */
		static wstring ascii_constant( const std::string& str ){
			size_t	len = str.length() + 1;
			char*	buffer = new char[len];
			std::memcpy( buffer , str.data() , len );
			return wstring( buffer , len , len - 1 , nullptr , 0 );
		}
		
		
		/**
		 * Contructor that fills itself with a certain amount of codepoints
		 * 
		 * @note	Creates an Instance of type wstring that gets filled with 'n' codepoints
		 * @param	n		The number of codepoints generated
		 * @param	ch		The codepoint that the whole buffer will be set to
		 */
		wstring( size_t n , wchar_t ch );
		/**
		 * Contructor that fills itself with a certain amount of characters
		 * 
		 * @note	Creates an Instance of type wstring that gets filled with 'n' characters
		 * @param	n		The number of characters generated
		 * @param	ch		The characters that the whole buffer will be set to
		 */
		wstring( size_t n , char ch ) :
			wstring( n , (wchar_t)ch )
		{}
		
		
		/**
		 * Copy Constructor that copies the supplied wstring to construct itself
		 * 
		 * @note	Creates an Instance of type wstring that has the exact same data as 'str'
		 * @param	str		The wstring to copy from
		 */
		wstring( const wstring& str );
		/**
		 * Move Constructor that moves the supplied wstring content into the new wstring
		 * 
		 * @note	Creates an Instance of type wstring that takes all data from 'str'
		 * 			The supplied wstring is invalid afterwards and may not be used anymore
		 * @param	str		The wstring to move from
		 */
		wstring( wstring&& str );
		
		
		/**
		 * Contructor taking a wide codepoint literal that will be copied to construct this wstring
		 * 
		 * @note	Creates an Instance of type wstring that holds the given codepoints
		 *			The data itself will be first converted to UTF-8
		 * @param	str		The codepoint sequence to fill the wstring with
		 */
		wstring( const wchar_t* str );
		
		
		/**
		 * Destructor
		 * 
		 * @note	Destructs a wstring at the end of its lifetime releasing all held memory
		 */
		~wstring(){
			this->reset_indices();
			this->reset_buffer();
		}
		
		
		/**
		 * Copy Assignment operator that sets the wstring to a copy of the supplied one
		 * 
		 * @note	Assigns a copy of 'str' to this wstring deleting all data that previously was in there
		 * @param	str		The wstring to copy from
		 * @return	A reference to the string now holding the data (*this)
		 */
		wstring& operator=( const wstring& str );
		/**
		 * Move Assignment operator that moves all data out of the supplied and into this wstring
		 * 
		 * @note	Moves all data from 'str' into this wstring deleting all data that previously was in there
		 *			The supplied wstring is invalid afterwards and may not be used anymore
		 * @param	str		The wstring to move from
		 * @return	A reference to the string now holding the data (*this)
		 */
		wstring& operator=( wstring&& str );
		
		
		/**
		 * Swaps the contents of this wstring with the supplied one
		 * 
		 * @note	Swaps all data with the supplied wstring
		 * @param	str		The wstring to swap contents with
		 */
		void swap( wstring& str ){
			std::swap( *this , str );
		}
		
		
		/**
		 * Clears the content of this wstring
		 * 
		 * @note	Resets the data to an empty string ("")
		 */
		void clear(){
			this->string_len = 0;
			this->misformated = false;
			reset_buffer( new char[1]{0} , 1 );
			reset_indices();
		}
		
		
		/**
		 * Returns whether there is an UTF-8 error in the string
		 * 
		 * @return	True, if there is an encoding error, false, if the contained data is properly encoded
		 */
		bool is_misformated() const { return this->misformated; }
		
		
		/**
		 * Returns the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	The Codepoint at position 'n'
		 */
		wchar_t at( size_t n ) const ;
		/**
		 * Returns the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	byte_index	The byte position of the codepoint to receive
		 * @return	The codepoint at the supplied position
		 */
		wchar_t raw_at( size_t byte_index ) const {
			if( !this->requires_unicode() )
				return this->buffer[byte_index];
			wchar_t dest;
			decode_utf8( this->buffer + byte_index , dest , this->misformated );
			return dest;
		}
		
		
		/**
		 * Returns an iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified codepoint index
		 */
		iterator get( size_t n ){ return iterator( get_actual_index(n) , *this ); }
		const_iterator get( size_t n ) const { return const_iterator( get_actual_index(n) , *this ); }
		/**
		 * Returns an iterator pointing to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to get the iterator to
		 * @return	An iterator pointing to the specified byte position
		 */
		iterator raw_get( size_t n ){ return iterator( n , *this ); }
		const_iterator raw_get( size_t n ) const { return const_iterator( n , *this ); }
		
		
		/**
		 * Returns a reverse iterator pointing to the supplied codepoint index
		 * 
		 * @param	n	The index of the codepoint to get the reverse iterator to
		 * @return	A reverse iterator pointing to the specified codepoint index
		 */
		reverse_iterator rget( size_t n ){ return reverse_iterator( get_actual_index(n) , *this ); }
		const_reverse_iterator rget( size_t n ) const { return const_reverse_iterator( get_actual_index(n) , *this ); }
		/**
		 * Returns a reverse iterator pointing to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to get the reverse iterator to
		 * @return	A reverse iterator pointing to the specified byte position
		 */
		reverse_iterator raw_rget( size_t n ){ return reverse_iterator( n , *this ); }
		const_reverse_iterator raw_rget( size_t n ) const { return const_reverse_iterator( n , *this ); }
		
		
		/**
		 * Returns a reference to the codepoint at the supplied index
		 * 
		 * @param	n	The codepoint index of the codepoint to receive
		 * @return	A reference wrapper to the codepoint at position 'n'
		 */
		wstring_reference operator[]( size_t n ){ return wstring_reference( n , *this ); }
		wchar_t operator[]( size_t n ) const { return at( n ); }
		/**
		 * Returns a reference to the codepoint at the supplied byte position
		 * 
		 * @note	As this access is raw, that is not looking up for the actual byte position,
		 *			it is very fast
		 * @param	n	The byte position of the codepoint to receive
		 * @return	A reference wrapper to the codepoint at byte position 'n'
		 */
		wstring_raw_reference operator()( size_t n ){ return wstring_raw_reference( n , *this ); }
		wchar_t operator()( size_t n ) const { return raw_at( n ); }
		
		
		/**
		 * Get the raw data contained in this wstring
		 * 
		 * @note	Returns the UTF-8 formatted content of this wstring
		 * @return	UTF-8 formatted data, trailled by a '\0'
		 */
		const char* c_str() const { return this->buffer; }
		const char* data() const { return this->buffer; }
		
		
		/**
		 * Get the raw data contained in this wstring wrapped by an std::string
		 * 
		 * @note	Returns the UTF-8 formatted content of this wstring
		 * @return	UTF-8 formatted data, wrapped inside an std::string
		 */
		std::string cpp_str() const { return std::string( this->buffer ); }
		
		
		/**
		 * Get the number of codepoints in this wstring
		 * 
		 * @note	Returns the number of codepoints that are taken care of
		 *			For the number of bytes, @see size()
		 * @return	Number of codepoints (not bytes!)
		 */
		size_t length() const { return this->string_len; }
		
		
		/**
		 * Get the number of bytes that are used by this wstring
		 * 
		 * @note	Returns the number of bytes required to hold the contained wide string,
		 *			That is, without counting the trailling '\0'
		 * @return	Number of bytes (not codepoints!)
		 */
		size_t size() const { return this->buffer_len - 1; }
		
		
		/**
		 * Check, whether this wstring is empty
		 * 
		 * @note	Returns True, if this wstring is empty, that also is comparing true with ""
		 * @return	True, if this wstring is empty, false if its length is >0
		 */
		bool empty() const { return !this->buffer[0]; }
		
		
		/**
		 * Check whether the data inside this wstring cannot be iterated by an std::string
		 * 
		 * @note	Returns true, if the wstring has codepoints that exceed 7 bits to be stored
		 * @return	True, if there are UTF-8 formatted byte sequences,
		 *			false, if there are only ANSI characters inside
		 */
		bool requires_unicode() const { return this->indices_len > 0; }
		
		
		/**
		 * Get a wide string literal representing this UTF-8 string
		 * 
		 * @note	As the underlying UTF-8 data has to be converted to a wide character array and 
		 *			as this array has to be allocated, this method is quite slow, that is O(n)
		 * @return	A wrapper class holding the wide character array
		 */
		std::unique_ptr<wchar_t[]> toWideLiteral() const ;
		
		
		/**
		 * Get an iterator to the beginning of the wstring
		 * 
		 * @return	An iterator class pointing to the beginning of this wstring
		 */
		iterator begin(){ return iterator( 0 , *this ); }
		const_iterator begin() const { return const_iterator( 0 , *this ); }
		/**
		 * Get an iterator to the end of the wstring
		 * 
		 * @return	An iterator class pointing to the end of this wstring, that is pointing behind the last codepoint
		 */
		iterator end(){ return iterator( end_index() , *this ); }
		const_iterator end() const { return const_iterator( end_index() , *this ); }
		
		
		/**
		 * Get a reverse iterator to the end of this wstring
		 * 
		 * @return	A reverse iterator class pointing to the end of this wstring,
		 *			that is exactly to the last codepoint
		 */
		reverse_iterator rbegin(){ return reverse_iterator( back_index() , *this ); }
		const_reverse_iterator rbegin() const { return const_reverse_iterator( back_index() , *this ); }
		/**
		 * Get a reverse iterator to the beginning of this wstring
		 * 
		 * @return	A reverse iterator class pointing to the end of this wstring,
		 *			that is pointing before the first codepoint
		 */
		reverse_iterator rend(){ return reverse_iterator( -1 , *this ); }
		const_reverse_iterator rend() const { return const_reverse_iterator( -1 , *this ); }
		
		
		/**
		 * Get a const iterator to the beginning of the wstring
		 * 
		 * @return	A const iterator class pointing to the beginning of this wstring,
		 * 			which cannot alter things inside this wstring
		 */
		wstring_const_iterator cbegin() const { return wstring_const_iterator( 0 , *this ); }
		/**
		 * Get an iterator to the end of the wstring
		 * 
		 * @return	A const iterator class, which cannot alter this wstring, pointing to
		 *			the end of this wstring, that is pointing behind the last codepoint
		 */
		wstring_const_iterator cend() const { return wstring_const_iterator( end_index() , *this ); }
		
		
		/**
		 * Get a const reverse iterator to the end of this wstring
		 * 
		 * @return	A const reverse iterator class, which cannot alter this wstring, pointing to
		 *			the end of this wstring, that is exactly to the last codepoint
		 */
		wstring_const_reverse_iterator crbegin() const { return wstring_const_reverse_iterator( back_index() , *this ); }
		/**
		 * Get a const reverse iterator to the beginning of this wstring
		 * 
		 * @return	A const reverse iterator class, which cannot alter this wstring, pointing to
		 *			the end of this wstring, that is pointing before the first codepoint
		 */
		wstring_const_reverse_iterator crend() const { return wstring_const_reverse_iterator( -1 , *this ); }
		
		
		/**
		 * Returns a reference to the first codepoint in the wstring
		 * 
		 * @return	A reference wrapper to the first codepoint in the wstring
		 */
		wstring_raw_reference front(){ return wstring_raw_reference( 0 , *this ); }
		wchar_t front() const { return raw_at( 0 ); }
		/**
		 * Returns a reference to the last codepoint in the wstring
		 * 
		 * @return	A reference wrapper to the last codepoint in the wstring
		 */
		wstring_raw_reference back(){ return wstring_raw_reference( empty() ? 0 : back_index() , *this ); }
		wchar_t back() const { return raw_at( empty() ? 0 : back_index() ); }
		
		
		/**
		 * Replace a codepoint of this wstring by a number of codepoints
		 * 
		 * @param	index	The codpoint index to be replaced
		 * @param	repl	The wide character that will be used to replace the codepoint
		 * @param	n		The number of codepoint that will be inserted
		 *					instead of the one residing at position 'index'
		 * @return	A reference to this wstring, which now has the replaced part in it
		 */
		wstring& replace( size_t index , wchar_t repl , size_t n = 1 ){
			replace( index , 1 , wstring( n , repl ) );
			return *this;
		}
		/**
		 * Replace a number of codepoints of this wstring by a number of other codepoints
		 * 
		 * @param	index	The codpoint index at which the replacement is being started
		 * @param	len		The number of codepoints that are being replaced
		 * @param	repl	The wide character that will be used to replace the codepoints
		 * @param	n		The number of codepoint that will replace the old ones
		 * @return	A reference to this wstring, which now has the replaced part in it
		 */
		wstring& replace( size_t index , size_t len , wchar_t repl , size_t n = 1 ){
			replace( index , len , wstring( n , repl ) );
			return *this;
		}
		/**
		 * Replace a range of codepoints by a number of codepoints
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be replaced
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be replaced
		 * @param	repl	The wide character that will be used to replace the codepoints in the range
		 * @param	n		The number of codepoint that will replace the old ones
		 * @return	A reference to this wstring, which now has the replaced part in it
		 */
		wstring& replace( wstring_iterator first , wstring_iterator last , wchar_t repl , size_t n = 1 ){
			raw_replace( first.index() , last.index() - first.index() , wstring( n , repl ) );
			return *this;
		}
		/**
		 * Replace a range of codepoints with the contents of the supplied wstring
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be replaced
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be replaced
		 * @param	repl	The wstring to replace all codepoints in the range
		 * @return	A reference to this wstring, which now has the replaced part in it
		 */
		wstring& replace( wstring_iterator first , wstring_iterator last , const wstring& repl ){
			raw_replace( first.index() , last.index() - first.index() , repl );
			return *this;
		}
		/**
		 * Replace a number of codepoints of this wstring with the contents of the supplied wstring
		 * 
		 * @param	index	The codpoint index at which the replacement is being started
		 * @param	len		The number of codepoints that are being replaced
		 * @param	repl	The wstring to replace all codepoints in the range
		 * @return	A reference to this wstring, which now has the replaced part in it
		 */
		wstring& replace( size_t index , size_t count , const wstring& repl )
		{
			ptrdiff_t		actualStartIndex = get_actual_index( index );
			
			if( count == wstring::npos )
				raw_replace( actualStartIndex , wstring::npos , repl );
			else
			{
				ptrdiff_t	actualEndIndex = count ? get_actual_index( index + count ) : actualStartIndex;
				raw_replace( actualStartIndex , actualEndIndex - actualStartIndex , repl );
			}
			
			return *this;
		}
		/**
		 * Replace a number of bytes of this wstring with the contents of the supplied wstring
		 * 
		 * @note	As this function is raw, that is not having to compute byte indices,
		 *			it is much faster than the codepoint-base replace function
		 * @param	byte_index	The byte position at which the replacement is being started
		 * @param	byte_count	The number of bytes that are being replaced
		 * @param	repl		The wstring to replace all bytes inside the range
		 * @return	A reference to this wstring, which now has the replaced part in it
		 */
		void raw_replace( size_t byte_index , size_t byte_count , const wstring& repl );
		
		
		/**
		 * Appends the supplied wstring to the end of this wstring
		 * 
		 * @param	appendix	The wstring to be appended
		 * @return	A reference to this wstring, which now has the supplied string appended
		 */
		wstring& append( const wstring& appendix ){
			replace( length() , 0 , appendix );
			return *this;
		}
		wstring& operator+=( const wstring& summand ){
			replace( length() , 0 , summand );
			return *this;
		}
		
		
		/**
		 * Appends the supplied codepoint to the end of this wstring
		 * 
		 * @param	ch	The codepoint to be appended
		 * @return	A reference to this wstring, which now has the supplied string appended
		 */
		wstring& push_back( wchar_t ch ){
			replace( length() , 0 , wstring( 1 , ch ) );
			return *this;
		}
		
		
		/**
		 * Adds the supplied wstring to a copy of this wstring
		 * 
		 * @param	summand		The wstring to be added 
		 * @return	A reference to the newly created wstring, which now has the supplied string appended
		 */
		wstring operator+( const wstring& summand ) const {
			wstring str = *this;
			str.append( summand );
			return std::move( str );
		}
		
		
		/**
		 * Inserts a given codepoint into this wstring at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	ch		The codepoint to be inserted
		 * @return	A reference to this wstring, with the supplied codepoint inserted
		 */
		wstring& insert( size_t pos , wchar_t ch ){
			replace( pos , 0 , wstring( 1 , ch ) );
			return *this;
		}
		/**
		 * Inserts a given wstring into this wstring at the supplied codepoint index
		 * 
		 * @param	pos		The codepoint index to insert at
		 * @param	str		The wstring to be inserted
		 * @return	A reference to this wstring, with the supplied wstring inserted
		 */
		wstring& insert( size_t pos , const wstring& str ){
			replace( pos , 0 , str );
			return *this;
		}
		/**
		 * Inserts a given wstring into this wstring at the supplied iterator position
		 * 
		 * @param	it	The iterator psoition to insert at
		 * @param	ch	The codepoint to be inserted
		 * @return	A reference to this wstring, with the supplied codepoint inserted
		 */
		wstring& insert( wstring_iterator it , wchar_t ch ){
			raw_replace( it.index() , 0 , wstring( 1 , ch ) );
			return *this;
		}
		/**
		 * Inserts a given wstring into this wstring at the supplied byte position
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that insert()
		 * @param	pos		The byte position index to insert at
		 * @param	str		The wstring to be inserted
		 * @return	A reference to this wstring, with the supplied wstring inserted
		 */
		wstring& raw_insert( size_t pos , wstring& str ){
			raw_replace( pos , 0 , str );
			return *this;
		}
		
		
		/**
		 * Erases the codepoint at the supplied iterator position
		 * 
		 * @param	pos		The iterator pointing to the position being erased
		 * @return	A reference to this wstring, which now has the codepoint erased
		 */
		wstring& erase( wstring_iterator pos ){
			raw_replace( pos.index() , get_index_bytes( pos.index() )  , wstring() );
			return *this;
		}
		/**
		 * Erases the codepoints inside the supplied range
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be erased
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint to be erased
		 * @return	A reference to this wstring, which now has the codepoints erased
		 */
		wstring& erase( wstring_iterator first , wstring_iterator last ){
			raw_replace( first.index() , last.index() - first.index() , wstring() );
			return *this;
		}
		/**
		 * Erases a portion of this string
		 * 
		 * @param	pos		The codepoint index to start eraseing from
		 * @param	len		The number of codepoints to be erased from this wstring
		 * @return	A reference to this wstring, with the supplied portion erased
		 */
		wstring& erase( size_t pos , size_t len ){
			replace( pos , len , wstring() );
			return *this;
		}
		/**
		 * Erases a byte range of this string
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that erase()
		 * @param	pos		The byte position index to start erasing from
		 * @param	len		The number of bytes to be erased from the wstring
		 * @return	A reference to this wstring, with the supplied bytes erased
		 */
		wstring& raw_erase( size_t pos , size_t len ){
			raw_replace( pos , len , wstring() );
			return *this;
		}
		
		
		/**
		 * Returns a portion of the wstring
		 * 
		 * @param	first	An iterator pointing to the first codepoint to be included in the substring
		 * @param	last	An iterator pointing to the codepoint behind the last codepoint in the substring
		 * @return	The wstring holding the specified range
		 */
		wstring substr( wstring_iterator first , wstring_iterator last ) const {
			size_t byte_count = last.index() - first.index();
			return raw_substr( first.index() , byte_count , get_num_resulting_codepoints( first.index() , byte_count ) );
		}
		/**
		 * Returns a portion of the wstring
		 * 
		 * @param	pos		The codepoint index that should mark the start of the substring
		 * @param	len		The number codepoints to be included within the substring
		 * @return	The wstring holding the specified codepoints
		 */
		wstring substr( size_t pos , size_t len ) const
		{
			ptrdiff_t	actualStartIndex = get_actual_index( pos );
			
			if( len == wstring::npos )
				return raw_substr( actualStartIndex , wstring::npos );
			
			ptrdiff_t	actualEndIndex = len ? get_actual_index( pos + len ) : actualStartIndex;
			
			return raw_substr( actualStartIndex , actualEndIndex - actualStartIndex , len );
		}
		/**
		 * Returns a portion of the wstring (indexed on byte-base)
		 * 
		 * @note	As this function is raw, that is without having to compute
		 *			actual byte indices, it is much faster that substr()
		 * @param	byte_index		The byte position where the substring shall start
		 * @param	byte_count		The number of bytes that the substring shall have
		 * @param	numCodepoints	(Optional) The number of codepoints
		 *							the substring will have, in case this is already known
		 * @return	The wstring holding the specified bytes
		 */
		wstring raw_substr( size_t byte_index , size_t byte_count , size_t numCodepoints ) const ;
		wstring raw_substr( size_t byte_index , size_t byte_count ) const {
			if( byte_count == wstring::npos )
				byte_count = size() - byte_index;
			return raw_substr( byte_index , byte_count , get_num_resulting_codepoints( byte_index , byte_count ) );
		}
		
		
		/**
		 * Finds a specific codepoint inside the wstring starting at the supplied codepoint index
		 * 
		 * @param	ch				The codepoint to look for
		 * @param	startCodepoint	The index of the first codepoint to start looking from
		 * @return	The codepoint index where and if the codepoint was found or wstring::npos
		 */
		size_t find( wchar_t ch , size_t startCodepoint = 0 ) const ;
		/**
		 * Finds a specific codepoint inside the wstring starting at the supplied byte position
		 * 
		 * @param	ch			The codepoint to look for
		 * @param	startByte	The byte position of the first codepoint to start looking from
		 * @return	The byte position where and if the codepoint was found or wstring::npos
		 */
		size_t raw_find( wchar_t ch , size_t startByte = 0 ) const ;
		
		//! Find content in string
		//! @todo implement
		//size_t rfind( wstring str , size_t startCodepoint = wstring::npos ) const ;
		//size_t raw_rfind( wstring str , size_t startByte = wstring::npos ) const ;
		//size_t rfind( const char* str , size_t startCodepoint = wstring::npos ) const ;
		//size_t raw_rfind( const char* str , size_t startByte = wstring::npos ) const ;
		size_t rfind( wchar_t ch , size_t startCodepoint = wstring::npos ) const ;
		size_t raw_rfind( wchar_t ch , size_t startByte = wstring::npos ) const ;
		
		//! Find character in string
		size_t find_first_of( const wchar_t* str , size_t startCodepoint = 0 ) const ;
		size_t raw_find_first_of( const wchar_t* str , size_t startByte = 0 ) const ;
		size_t find_last_of( const wchar_t* str , size_t startCodepoint = wstring::npos ) const ;
		size_t raw_find_last_of( const wchar_t* str , size_t startByte = wstring::npos ) const ;
		
		//! Find absence of character in string
		size_t find_first_not_of( const wchar_t* str , size_t startCodepoint = 0 ) const ;
		size_t raw_find_first_not_of( const wchar_t* str , size_t startByte = 0 ) const ;
		size_t find_last_not_of( const wchar_t* str , size_t startCodepoint = wstring::npos ) const ;
		size_t raw_find_last_not_of( const wchar_t* str , size_t startByte = wstring::npos ) const ;
		
		
		//! Removes the last codepoint in the wstring
		wstring& pop_back(){
			size_t pos = back_index();
			raw_replace( pos , get_index_bytes( pos ) , wstring() );
			return *this;
		}
		
		/**
		 * Compares this wstring to the supplied one
		 *
		 * @return	0	They compare equal
		 *			<0	Either the value of the first character that does not match is lower in
		 *			the compared string, or all compared characters match but the compared string is shorter.
		 *			>0	Either the value of the first character that does not match is greater in
		 *			the compared string, or all compared characters match but the compared string is longer.
		 */
		ptrdiff_t compare( const wstring& str ) const ;
		
		//! Returns true, if the supplied string compares equal to this one
		bool equals( const wstring& str ) const { return equals( str.c_str() ); }
		bool equals( const std::string& str ) const { return equals( str.c_str() ); }
		bool equals( const char* str ) const ;
		bool equals( const wchar_t* str ) const ;
		
		//! Compare this wstring to another string
		bool operator==( const wstring& str ) const { return equals( str ); }
		bool operator!=( const wstring& str ) const { return !equals( str ); }
		bool operator==( const char* str ) const { return equals( str ); }
		bool operator!=( const char* str ) const { return !equals( str ); }
		bool operator==( const wchar_t* str ) const { return equals( str ); }
		bool operator!=( const wchar_t* str ) const { return !equals( str ); }
		bool operator==( const std::string& str ) const { return equals( str ); }
		bool operator!=( const std::string& str ) const { return !equals( str ); }
		
		bool operator>( const wstring& str ) const { return str.compare( *this ) > 0; }
		bool operator>=( const wstring& str ) const { return str.compare( *this ) >= 0; }
		bool operator<( const wstring& str ) const { return str.compare( *this ) < 0; }
		bool operator<=( const wstring& str ) const { return str.compare( *this ) <= 0; }
		
		
		//! Get the number of bytes of codepoint in wstring
		unsigned char get_codepoint_bytes( size_t codepoint_index ) const {
			return get_num_bytes_of_utf8_char( this->buffer[ get_actual_index(codepoint_index) ] );
		}
		unsigned char get_index_bytes( size_t byte_index ) const {
			return get_num_bytes_of_utf8_char( this->buffer[byte_index] );
		}
		
		
		//! Get the number of bytes before a codepoint, that build up a new codepoint
		unsigned char get_codepoint_pre_bytes( size_t codepoint_index ) const {
			return codepoint_index > 0 ? get_num_bytes_of_utf8_char_before( this->buffer + get_actual_index(codepoint_index) ) : 1;
		}
		unsigned char get_index_pre_bytes( size_t byte_index ) const {
			return byte_index > 0 ? get_num_bytes_of_utf8_char_before( this->buffer + byte_index ) : 1;
		}
		
		
		
		//! Friend iterator difference computation functions
		friend int				operator-( const iterator& left , const iterator& right );
		friend int				operator-( const reverse_iterator& left , const reverse_iterator& right );
		
		//! Friend summation functions
		friend iterator			operator+( const iterator& it , size_t nth );
		friend reverse_iterator	operator+( const reverse_iterator& it , size_t nth );
};

extern int							operator-( const wstring::iterator& left , const wstring::iterator& right );
extern int							operator-( const wstring::reverse_iterator& left , const wstring::reverse_iterator& right );
extern wstring::iterator			operator+( const wstring::iterator& it , size_t nth );
extern wstring::reverse_iterator	operator+( const wstring::reverse_iterator& it , size_t nth );

#endif