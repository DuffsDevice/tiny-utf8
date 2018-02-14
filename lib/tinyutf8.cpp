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

#include "tinyutf8.h"

utf8_string::utf8_string( utf8_string::size_type number , utf8_string::value_type ch ) :
	utf8_string()
{
	if( !( number && ch ) )
		return;
	
	unsigned char			num_bytes = get_num_bytes_of_utf8_codepoint( ch );
	std::pair<char*,void*>	result = this->new_buffer( number * num_bytes + 1 , num_bytes > 1 ? number : 0 );
	char*					buffer = result.first;
	
	// Set string length
	if( !this->sso_active() )
		this->_string_len = number;
	
	if( num_bytes > 1 )
	{
		char			representation[6];
		char*			orig_buffer = buffer;
		void*			indices = result.second;
		unsigned char	indices_datatype_bytes = get_index_datatype_bytes( this->_buffer_len );
		size_type	idx = 0;
		
		
		// Encode the wide character in utf8
		encode_utf8( ch , representation );
		
		while( number-- > 0 )
		{
			if( indices )
				utf8_string::set_nth_index( indices , indices_datatype_bytes , idx++ , buffer - orig_buffer );
			for( unsigned char byte = 0 ; byte < num_bytes ; byte++ )
				*buffer++ = representation[byte];
		}
	}
	else
		while( number-- > 0 )
			*buffer++ = ch;
	
	// Set trailling zero
	*buffer = 0;
}

utf8_string::utf8_string( const char* str , size_type len , void* ) :
	utf8_string()
{
	size_type		num_multibytes = 0;
	size_type		num_bytes = 0;
	size_type		string_len = 0;
	
	// Count Multibytes
	while( str[num_bytes] && string_len < len )
	{
		string_len++; // Count number of codepoints
		
		// Compute the number of bytes to skip
		unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( str , num_bytes , utf8_string::npos );
		num_bytes += cur_num_bytes;
		if( cur_num_bytes > 1 )
			num_multibytes++; // Increase number of occoured multibytes
	}
	
	// Initialize buffer
	std::pair<char*,void*> buffer = this->new_buffer( num_bytes + 1 , num_multibytes );
	
	// Fill the buffer
	std::memcpy( buffer.first , str , num_bytes );
	buffer.first[num_bytes] = '\0';
	
	if( !this->sso_active() )
	{
		// Set string length
		this->_string_len = string_len;
		
		// Compute the multibyte table
		this->compute_multibyte_table( buffer.second );
	}
}

utf8_string::utf8_string( const value_type* str , size_type len ) :
	utf8_string()
{
	if( !(str && str[0] && len ) )
		return;
	
	size_type	num_multibytes = 0;
	size_type	num_bytes = 0;
	size_type	string_len = 0;
	
	// Count bytes and mutlibytes
	while( string_len < len && str[string_len] )
	{
		unsigned char codepoint_bytes = get_num_bytes_of_utf8_codepoint( str[string_len++] );
		if( codepoint_bytes > 1 )
			num_multibytes++;
		num_bytes += codepoint_bytes;
	}
	
	// Initialize the internal buffer
	std::pair<char*,void*> buffer = this->new_buffer( num_bytes + 1 , num_multibytes ); // +1 for the trailling zero
	
	void*			indices = buffer.second;
	size_type		cur_multibyte_index = 0;
	char*			orig_writer = buffer.first;
	char*			cur_writer = orig_writer;
	unsigned char	indices_datatype_bytes = get_index_datatype_bytes( num_bytes + 1 );
	
	// Iterate through wide char literal
	for( size_type i = 0; i < string_len ; i++ )
	{
		// Encode wide char to utf8
		unsigned char codepoint_bytes = encode_utf8( str[i] , cur_writer );
		// Push position of character to 'indices'
		if( codepoint_bytes > 1 && indices )
			utf8_string::set_nth_index( indices , indices_datatype_bytes , cur_multibyte_index++ , cur_writer - orig_writer );
		
		// Step forward with copying to internal utf8 buffer
		cur_writer += codepoint_bytes;
	}
	
	// Add trailling \0 char to utf8 buffer
	*cur_writer = 0;
	
	// Set string length
	if( !this->sso_active() )
		this->_string_len = string_len;
}




utf8_string::utf8_string( utf8_string&& str ) :
	_buffer_len( str._buffer_len )
	, _capacity( str._capacity )
	, _buffer( str._buffer )
	, _string_len( str._string_len )
	, _indices_len( str._indices_len )
{
	// Reset old string
	str._buffer_len = 0;
}

utf8_string::utf8_string( const utf8_string& str ) :
	_buffer_len( str._buffer_len )
	, _string_len( str._string_len )
	, _indices_len( str._indices_len )
{
	if( !is_small_string( str._buffer_len ) ){
		this->_buffer = new char[str._capacity];
		std::memcpy( this->_buffer , str._buffer , str._capacity );
	}
	else
		this->_buffer = str._buffer;
}




utf8_string& utf8_string::operator=( const utf8_string& str )
{
	// Clear old data
	clear();
	
	if( str.empty() )
		return *this;
	
	this->_buffer_len = str._buffer_len;
	this->_string_len = str._string_len;
	this->_indices_len = str._indices_len;
	
	if( !is_small_string( str._buffer_len ) ){
		this->_buffer = new char[str._capacity];
		std::memcpy( this->_buffer , str._buffer , str._capacity );
	}
	else
		this->_buffer = str._buffer;
	
	return *this;
}


utf8_string& utf8_string::operator=( utf8_string&& str )
{
	// Reset old data
	clear();
	
	if( !str.empty() )
	{
		// Copy data
		std::memcpy( this , &str , sizeof(utf8_string) );
		
		// Reset old string
		str._buffer_len = 0;
	}
	
	return *this;
}


unsigned char utf8_string::get_num_bytes_of_utf8_char_before( const char* data , size_type current_byte_index , size_type buffer_len )
{
	if( current_byte_index >= buffer_len || current_byte_index < 1 )
		return 1;
	
	data += current_byte_index;
	
#ifdef _TINY_UTF8_H_SAFE_MODE_
	
	// Check if byte is ascii
	if( static_cast<unsigned char>(data[-1]) <= 0x7F )
		return 1;
	
	// Check if byte is no data-blob
	if( ( static_cast<unsigned char>(data[-1]) & 0xC0) != 0x80 || current_byte_index < 2 )	return 1;
	// 110XXXXX - two bytes?
	if( ( static_cast<unsigned char>(data[-2]) & 0xE0) == 0xC0 )	return 2;
	
	// Check if byte is no data-blob
	if( ( static_cast<unsigned char>(data[-2]) & 0xC0) != 0x80 || current_byte_index < 3 )	return 1;
	// 1110XXXX - three bytes?
	if( ( static_cast<unsigned char>(data[-3]) & 0xF0) == 0xE0 )	return 3;
	
	// Check if byte is no data-blob
	if( ( static_cast<unsigned char>(data[-3]) & 0xC0) != 0x80 || current_byte_index < 4 )	return 1;
	// 11110XXX - four bytes?
	if( ( static_cast<unsigned char>(data[-4]) & 0xF8) == 0xF0 )	return 4;
	
	// Check if byte is no data-blob
	if( ( static_cast<unsigned char>(data[-4]) & 0xC0) != 0x80 || current_byte_index < 5 )	return 1;
	// 111110XX - five bytes?
	if( ( static_cast<unsigned char>(data[-5]) & 0xFC) == 0xF8 )	return 5;
	
	// Check if byte is no data-blob
	if( ( static_cast<unsigned char>(data[-5]) & 0xC0) != 0x80 || current_byte_index < 6 )	return 1;
	// 1111110X - six bytes?
	if( ( static_cast<unsigned char>(data[-6]) & 0xFE) == 0xFC )	return 6;
	
#else
	
	// Only Check the possibilities, that could appear
	switch( current_byte_index )
	{
		default:
			if( ( static_cast<unsigned char>(data[-6]) & 0xFE) == 0xFC )  // 1111110X  six bytes
				return 6;
		case 5:
			if( ( static_cast<unsigned char>(data[-5]) & 0xFC) == 0xF8 )  // 111110XX  five bytes
				return 5;
		case 4:
			if( ( static_cast<unsigned char>(data[-4]) & 0xF8) == 0xF0 )  // 11110XXX  four bytes
				return 4;
		case 3:
			if( ( static_cast<unsigned char>(data[-3]) & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
				return 3;
		case 2:
			if( ( static_cast<unsigned char>(data[-2]) & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
				return 2;
		case 1:
			break;
	}
	
#endif
	
	return 1;
}



unsigned char utf8_string::get_num_bytes_of_utf8_char( const char* data , size_type first_byte_index , size_type buffer_len )
{
	if( buffer_len <= first_byte_index )
		return 1;
	
	data += first_byte_index;
	unsigned char first_byte =  static_cast<unsigned char>(data[0]);
	
#ifdef _TINY_UTF8_H_SAFE_MODE_
	if( first_byte <= 0x7F )
		return 1;
	
	// Check if byte is data-blob
	if( ( static_cast<unsigned char>(data[1]) & 0xC0) != 0x80 )		return 1;
	// 110XXXXX - two bytes?
	if( (first_byte & 0xE0) == 0xC0 )	return 2;
	
	// Check if byte is data-blob
	if( ( static_cast<unsigned char>(data[2]) & 0xC0) != 0x80 )		return 1;
	// 1110XXXX - three bytes?
	if( (first_byte & 0xF0) == 0xE0 )	return 3;
	
	// Check if byte is data-blob
	if( ( static_cast<unsigned char>(data[3]) & 0xC0) != 0x80 )		return 1;
	// 11110XXX - four bytes?
	if( (first_byte & 0xF8) == 0xF0 )	return 4;
	
	// Check if byte is data-blob
	if( ( static_cast<unsigned char>(data[4]) & 0xC0) != 0x80 )		return 1;
	// 111110XX - five bytes?
	if( (first_byte & 0xFC) == 0xF8 )	return 5;
	
	// Check if byte is data-blob
	if( ( static_cast<unsigned char>(data[5]) & 0xC0) != 0x80 )		return 1;
	// 1111110X - six bytes?
	if( (first_byte & 0xFE) == 0xFC )	return 6;
#else
	// Only Check the possibilities, that could appear
	switch( buffer_len - first_byte_index )
	{
		default:
			if( (first_byte & 0xFE) == 0xFC )  // 1111110X  six bytes
				return 6;
		case 6:
			if( (first_byte & 0xFC) == 0xF8 )  // 111110XX  five bytes
				return 5;
		case 5:
			if( (first_byte & 0xF8) == 0xF0 )  // 11110XXX  four bytes
				return 4;
		case 4:
			if( (first_byte & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
				return 3;
		case 3:
			if( (first_byte & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
				return 2;
		case 2:
			if( first_byte <= 0x7F )
				return 1;
		case 1:
		case 0:
			break;
	}
#endif
	
	return 1; // one byte
}



unsigned char utf8_string::decode_utf8( const char* data , value_type& dest )
{
	unsigned char first_char =  static_cast<unsigned char>(*data);
	
	if( !first_char ){
		dest = 0;
		return 1;
	}
	
	value_type		codepoint;
	unsigned char	num_bytes;
	
	if( first_char <= 0x7F ){ // 0XXX XXXX one byte
		dest = first_char;
		return 1;
	}
	else if( (first_char & 0xE0) == 0xC0 ){  // 110X XXXX  two bytes
		codepoint = first_char & 31;
		num_bytes = 2;
	}
	else if( (first_char & 0xF0) == 0xE0 ){  // 1110 XXXX  three bytes
		codepoint = first_char & 15;
		num_bytes = 3;
	}
	else if( (first_char & 0xF8) == 0xF0 ){  // 1111 0XXX  four bytes
		codepoint = first_char & 7;
		num_bytes = 4;
	}
	else if( (first_char & 0xFC) == 0xF8 ){  // 1111 10XX  five bytes
		codepoint = first_char & 3;
		num_bytes = 5;
	}
	else if( (first_char & 0xFE) == 0xFC ){  // 1111 110X  six bytes
		codepoint = first_char & 1;
		num_bytes = 6;
	}
	else{ // not a valid first byte of a UTF-8 sequence
		codepoint = first_char;
		num_bytes = 1;
	}
	
#ifdef _TINY_UTF8_H_SAFE_MODE_
	for( int i = 1 ; i < num_bytes ; i++ ){
		if( !data[i] || ( static_cast<unsigned char>(data[i]) & 0xC0) != 0x80 ){
			num_bytes = 1;
			codepoint = first_char;
			break;
		}
		codepoint = (codepoint << 6) | ( static_cast<unsigned char>(data[i]) & 0x3F );
	}
#else
	for( int i = 1 ; i < num_bytes ; i++ )
		codepoint = (codepoint << 6) | (data[i] & 0x3F);	
#endif
	
	dest = codepoint;
	
	return num_bytes;
}

void utf8_string::shrink_to_fit()
{
	if( this->sso_active() )
		return;
	
	size_type	required_capacity = this->get_required_capacity( this->_buffer_len , this->_indices_len );
	size_type	threshold = std::max<size_type>( required_capacity + 10 , required_capacity >> 2 );
	
	// Do we save at least one quarter of the capacity and at least 10 bytes?
	if( this->_capacity < threshold )
		return;
	
	// Allocate new buffer
	char*			new_buffer = new char[required_capacity];
	unsigned char	index_datatype_bytes = get_index_datatype_bytes( this->_buffer_len );
	
	// Copy indices
	if( this->_indices_len )
		std::memcpy(
			new_buffer + required_capacity - this->_indices_len * index_datatype_bytes
			, this->_buffer + this->_capacity - this->_indices_len * index_datatype_bytes
			, this->_indices_len * index_datatype_bytes
		);
	
	// Copy data
	std::memcpy(
		new_buffer
		, this->_buffer
		, this->_buffer_len
	);
	
	this->_capacity = required_capacity;
}



std::string utf8_string::cpp_str_bom() const
{
	constexpr char bom[4] = { char(0xEF) , char(0xBB) , char(0xBF) , 0 };
	
	const char* buffer = this->get_buffer();
	
	if( !buffer )
		return std::string( bom );
	
	// Create std::string
	std::string result = std::string( this->_buffer_len + 3 , ' ' );
	char* tmp_buffer = const_cast<char*>( result.data() );
	tmp_buffer[0] = bom[0];
	tmp_buffer[1] = bom[1];
	tmp_buffer[2] = bom[2];
	
	// Copy my data into it
	std::memcpy( tmp_buffer + 3 , buffer , this->_buffer_len );
	
	return std::move(result);
}

utf8_string::size_type utf8_string::get_num_resulting_codepoints( size_type start_byte , size_type byte_count ) const 
{
	const char* buffer = this->get_buffer();
	
	if( !buffer )
		return 0;
	
	size_type	cur_index = start_byte;
	size_type	end_index = start_byte + byte_count;
	
	byte_count = std::min<difference_type>( byte_count , size() - start_byte );
	
	// Reduce the byte count by the number of bytes by the number of utf8 data bytes
	if( this->sso_active() )
	{
		while( cur_index < end_index ){
			unsigned char num_bytes = get_num_bytes_of_utf8_char( buffer , cur_index , this->_buffer_len );
			cur_index += num_bytes;
			byte_count -= num_bytes - 1;
		}
	}
	else if( size_type indices_len = this->_indices_len )
	{
		size_type		index_multibyte_table = 0;
		const void*		indices = this->get_indices();
		unsigned char	indices_datatype_bytes = get_index_datatype_bytes( this->_buffer_len );
		
		// Iterate to the start of the relevant part of the multibyte table
		while( index_multibyte_table < indices_len )
		{
			if( utf8_string::get_nth_index( indices , indices_datatype_bytes , index_multibyte_table ) >= start_byte )
				break;
			index_multibyte_table++;
		}
		
		// Iterate over relevant multibyte indices
		while( index_multibyte_table < indices_len )
		{
			cur_index = utf8_string::get_nth_index( indices , indices_datatype_bytes , index_multibyte_table );
			
			if( cur_index >= end_index )
				break;
			
			index_multibyte_table++;
			byte_count -= get_num_bytes_of_utf8_char( buffer , cur_index , this->_buffer_len ) - 1; // Remove utf8 data bytes
		}
	}
	
	// Now byte_count is the number of codepoints
	return byte_count;
}

utf8_string::size_type utf8_string::get_num_resulting_bytes( size_type start_byte , size_type codepoint_count ) const 
{
	const char* buffer = this->get_buffer();
	
	if( !buffer )
		return 0;
	
	size_type size = this->size();
	
	if( start_byte + codepoint_count > size )
		return size - start_byte;
	
	size_type cur_byte = start_byte;
	size_type buffer_len = size + 1;
	
	// Reduce the byte count by the number of utf8 data bytes
	if( utf8_string::is_small_string(buffer_len) ){ // this->sso_active(), but we have already cached size()
		while( codepoint_count-- > 0 && cur_byte < size )
			cur_byte += get_num_bytes_of_utf8_char( buffer , cur_byte , buffer_len );
	}
	else if( size_type indices_len = this->_indices_len )
	{
		// Add at least as many bytes as codepoints
		cur_byte += codepoint_count;
		
		size_type 		index_multibyte_table = 0;
		const void*		indices = this->get_indices();
		unsigned char	indices_datatype_bytes = get_index_datatype_bytes(buffer_len);
		
		// Iterate to the start of the relevant part of the multibyte table
		while( index_multibyte_table < indices_len ){
			if( utf8_string::get_nth_index( indices , indices_datatype_bytes , index_multibyte_table ) >= start_byte )
				break;
			index_multibyte_table++;
		}
		
		// Iterate over relevant multibyte indices
		while( index_multibyte_table < indices_len )
		{
			size_type multibyte_pos = utf8_string::get_nth_index( indices , indices_datatype_bytes , index_multibyte_table );
			
			if( multibyte_pos >= cur_byte )
				break;
			
			index_multibyte_table++;
			
			// Add the utf8 data bytes to the number of bytes
			cur_byte += get_num_bytes_of_utf8_char( buffer , multibyte_pos , buffer_len ) - 1; // Add utf8 data bytes
		}
	}
	else
		return codepoint_count;
	
	return cur_byte - start_byte;
}




unsigned char utf8_string::encode_utf8( value_type codepoint , char* dest )
{
	if( codepoint <= 0x7F ){ // 0XXXXXXX one byte
		dest[0] = char(codepoint);
		return 1;
	}
	if( codepoint <= 0x7FF ){ // 110XXXXX  two bytes
		dest[0] = char( 0xC0 | (codepoint >> 6) );
		dest[1] = char( 0x80 | (codepoint & 0x3F) );
		return 2;
	}
	if( codepoint <= 0xFFFF ){ // 1110XXXX  three bytes
		dest[0] = char( 0xE0 | (codepoint >> 12) );
		dest[1] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[2] = char( 0x80 | (codepoint & 0x3F) );
		return 3;
	}
	if( codepoint <= 0x1FFFFF ){ // 11110XXX  four bytes
		dest[0] = char( 0xF0 | (codepoint >> 18) );
		dest[1] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[3] = char( 0x80 | (codepoint & 0x3F) );
		return 4;
	}
	if( codepoint <= 0x3FFFFFF ){ // 111110XX  five bytes
		dest[0] = char( 0xF8 | (codepoint >> 24) );
		dest[1] = char( 0x80 | ((codepoint >> 18) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[3] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[4] = char( 0x80 | (codepoint & 0x3F) );
		return 5;
	}
	if( codepoint <= 0x7FFFFFFF ){ // 1111110X  six bytes
		dest[0] = char( 0xFC | (codepoint >> 30) );
		dest[1] = char( 0x80 | ((codepoint >> 24) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 18) & 0x3F) );
		dest[3] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[4] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[5] = char( 0x80 | (codepoint & 0x3F) );
		return 6;
	}
	
	return 1;
}



std::pair<char*,void*> utf8_string::new_buffer( utf8_string::size_type buffer_size , utf8_string::size_type indices_size )
{
	// Backup
	bool was_small_string = sso_active();
	
	// Set new buffer length
	this->_buffer_len = buffer_size;
	
	// If the new buffer siez is small, return the internal buffer
	if( is_small_string( buffer_size ) )
		return { this->get_sso_buffer() , nullptr };
	
	// Compute required capacity
	size_type required_capacity = get_required_capacity( buffer_size , indices_size );
	
	// Only allocate memory, if we need to
	if( was_small_string || this->_capacity < required_capacity ){
		this->_buffer = new char[required_capacity];
		this->_capacity = required_capacity;
	}
	
	this->_indices_len = indices_size;
	
	// Return memory
	if( indices_size )
		return { this->_buffer , reinterpret_cast<void*>( this->_buffer + this->_capacity - this->_indices_len * get_index_datatype_bytes( buffer_size ) ) };
	
	return { this->_buffer , nullptr };
}


void utf8_string::compute_multibyte_table( void* table )
{
	// initialize indices table
	char*	buffer = this->get_buffer();
	
	// if we could allocate the indices table
	if( table )
	{
		size_type		multibyte_index = 0;
		size_type		buffer_len = this->_buffer_len;
		unsigned char	indices_datatype_bytes = get_index_datatype_bytes( buffer_len );
		
		// Fill Multibyte Table
		for( size_type index = 0 ; index < buffer_len ; )
		{
			unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( buffer , index , buffer_len );
			if( cur_num_bytes > 1 )
				utf8_string::set_nth_index( table , indices_datatype_bytes , multibyte_index++ , index );
			index += cur_num_bytes;
		}
	}
}



utf8_string utf8_string::raw_substr( size_type start_byte , size_type byte_count , size_type num_substr_codepoints ) const
{
	// Compute end index
	size_type	end_index = std::min<size_type>( start_byte + byte_count , size() );
	
	// Boundary check
	if( end_index <= start_byte )
		return utf8_string();
	
	// Compute actual byte_count (with boundaries)
	byte_count = end_index - start_byte;
	
	// Create result instance
	utf8_string	result;
	
	// If the new string is a small string
	if( is_small_string( byte_count + 1 ) )
	{
		char* dest_buffer = result.new_buffer( byte_count + 1 , 0 ).first;
	
		// Partly copy
		std::memcpy( dest_buffer , this->get_buffer() + start_byte , byte_count );
		dest_buffer[byte_count] = 0;
		
		return result;
	}
	
	// BELOW HERE ALL ATTRIBUTES ARE VALID //
	
	if( this->_indices_len > 0 )
	{
		// Look for start of indices
		size_type		index_multibyte_table = 0;
		const void*		my_indices = this->get_indices();
		unsigned char	my_indices_datatype_bytes = get_index_datatype_bytes( this->_buffer_len );
		
		while( index_multibyte_table < this->_indices_len ){
			if( utf8_string::get_nth_index( my_indices , my_indices_datatype_bytes , index_multibyte_table ) >= start_byte )
				break;
			index_multibyte_table++;
		}
		
		size_type		start_of_multibytes_in_range = index_multibyte_table;
		
		// Look for the end
		while( index_multibyte_table < this->_indices_len )
		{
			if( utf8_string::get_nth_index( my_indices , my_indices_datatype_bytes , index_multibyte_table ) >= end_index )
				break;
			index_multibyte_table++;
		}
		
		// Compute number of indices 
		size_type		new_indices_len = index_multibyte_table - start_of_multibytes_in_range;
		
		// Create new buffer
		std::pair<char*,void*> dest_buffer = result.new_buffer( byte_count + 1 , new_indices_len );
		
		// Partly copy character data
		std::memcpy( dest_buffer.first , this->get_buffer() + start_byte , byte_count );
		dest_buffer.first[byte_count] = '\0';
		
		// Partly copy indices
		if( void* indices = dest_buffer.second ){
			unsigned char	indices_datatype_bytes = get_index_datatype_bytes( byte_count + 1 );
			for( size_type i = 0 ; i < new_indices_len ; i++ )
				utf8_string::set_nth_index( indices , indices_datatype_bytes , i , utf8_string::get_nth_index( my_indices , my_indices_datatype_bytes , start_of_multibytes_in_range + i ) - start_byte );
		}
	}
	
	// Set string length
	result._string_len = num_substr_codepoints;
	
	return result;
}


utf8_string& utf8_string::raw_replace( size_type start_byte , size_type replaced_bytes , const utf8_string& replacement )
{
	size_type my_size = size();
	
	// Boundary check
	if( start_byte > my_size )
		start_byte = my_size;
	
	size_type		end_byte = start_byte + replaced_bytes;
	
	// Second operand of || is needed because of potential integer overflow in the above sum
	if( end_byte > my_size || end_byte < start_byte )
		end_byte = my_size;
	
	// Predefs and Backups
	replaced_bytes = end_byte - start_byte;
	size_type		replacement_bytes = replacement.size();
	const char* 	replacement_buffer = replacement.get_buffer();
	size_type		replacement_codepoints = 0;
	difference_type	byte_difference = replaced_bytes - replacement_bytes;
	char*			old_buffer = this->get_buffer();
	size_type		old_buffer_len = this->_buffer_len;
	size_type		old_size = my_size;
	void*			old_indices = this->get_indices();
	unsigned char	old_indices_datatype_bytes = get_index_datatype_bytes( old_buffer_len );
	unsigned char	new_indices_datatype_bytes = 0;
	size_type		old_indices_len = this->_indices_len;
	size_type		old_string_len = 0;
	size_type		new_capacity = 0;
	char*			dest_buffer = old_buffer;
	size_type		normalized_buffer_len = old_size + 1;
	size_type		new_buffer_len = normalized_buffer_len - byte_difference;
	bool			was_small_string = is_small_string( old_buffer_len );
	bool			will_be_small_string = is_small_string( new_buffer_len );
	bool			replacement_small_string = is_small_string( replacement._buffer_len );
	void*			new_indices = old_indices;
	size_type		new_indices_len = 0;
	size_type		replaced_condepoints = 0;
	size_type		num_replacement_multibytes = 0;
	size_type		num_replaced_multibytes = 0;
	bool			delete_buffer = false;
	size_type		replaced_indices_start = 0; // The starting index of the section within the index_of_mutlibyte table that will get replaced
	size_type		replaced_indices_end = 0; // The corresp. end index
	difference_type difference_in_num_multibytes = 0;
	
	
	if( !new_buffer_len ){
		clear(); // Reset because empty
		return *this;
	}
	
	
	//
	// Get information about replacement
	//
	if( replacement_small_string )
	{
		// Count Multibytes of replacement
		for( size_type i = 0 ; i < replacement_bytes ; )
		{
			// Compute the number of bytes to skip
			unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( replacement_buffer , i , replacement._buffer_len );
			i += cur_num_bytes;
			if( cur_num_bytes > 1 )
				num_replacement_multibytes++;
			replacement_codepoints++;
		}
	}
	else{
		replacement_codepoints = replacement._string_len;
		num_replacement_multibytes = replacement._indices_len;
	}
	
	
	//
	// Compute the new size of the indices table and
	// Allocate the new buffer!
	//
	if( will_be_small_string ){
		dest_buffer = this->get_sso_buffer();
		new_indices = nullptr;
	}
	else
	{
		if( was_small_string )
		{
			new_indices_len = num_replacement_multibytes; // The minimal number of indices
			
			// Count Multibytes
			for( size_type idx = 0 ; idx < old_size ; )
			{
				old_string_len++; // Increase number of codepoints
				
				// Compute the number of bytes to skip
				unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( dest_buffer , idx , new_buffer_len );
				
				// Check if were in the range of replaced codepoints
				if( idx >= start_byte && idx < end_byte )
					replaced_condepoints++; // Increase number of codepoints that will get replaced
				else if( cur_num_bytes > 1 )
					new_indices_len++; // Increase number of multibyte indices to store
				
				idx += cur_num_bytes;
			}
			
			// Allocate new buffer
			new_capacity = get_required_capacity( new_buffer_len , new_indices_len );
			new_capacity += new_capacity >> 1; // Allocate some more!
			
			dest_buffer = new char[new_capacity];
		}
		else
		{
			old_string_len = this->_string_len;
			replaced_condepoints = get_num_resulting_codepoints( start_byte , replaced_bytes );
			
			// Look for start of indices
			while( replaced_indices_start < this->_indices_len ){
				if( utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , replaced_indices_start ) >= start_byte )
					break;
				replaced_indices_start++;
			}
			
			replaced_indices_end = replaced_indices_start;
			
			// Look for the end
			while( replaced_indices_end < this->_indices_len ){
				if( utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , replaced_indices_end ) >= end_byte )
					break;
				replaced_indices_end++;
			}
			
			// Compute the number of relevant multibytes
			num_replaced_multibytes = replaced_indices_end - replaced_indices_start;
			
			// Compute difference in number of indices
			difference_in_num_multibytes = num_replaced_multibytes - num_replacement_multibytes;
			
			// Compute new indices table size
			new_indices_len = old_indices_len - difference_in_num_multibytes;
			
			// Compute new capacity
			new_capacity = std::max<size_type>( this->_capacity , get_required_capacity( new_buffer_len , new_indices_len ) );
			
			// Allocate new buffer, if we exceed the old one
			if( this->_capacity != new_capacity ){
				new_capacity += new_capacity >> 1; // Make some room for more: 1.5 times the number of bytes we actually need
				dest_buffer = new char[new_capacity];
			}
		}
		
		// Set destination indices
		new_indices = new_indices_len
			? reinterpret_cast<void*>( dest_buffer + new_capacity - new_indices_len * get_index_datatype_bytes( new_buffer_len ) )
			: nullptr;
	}
	
	
	// 
	// Copy the utf8 data of this string into the possibly new one
	//
	if( dest_buffer != old_buffer ) // Check, if the buffer changed
	{
		// Copy until replacement
		std::memcpy( dest_buffer , old_buffer , start_byte );
		
		// Copy after replacement
		std::memcpy(
			dest_buffer + start_byte + replacement_bytes
			, old_buffer + end_byte
			, normalized_buffer_len - end_byte - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
		);
		
		// If the previous buffer was not "inplace", delete it
		if( !was_small_string )
			delete_buffer = true;
	}
	else if( new_buffer_len != old_buffer_len )
		// Copy after replacement
		std::memmove(
			dest_buffer + start_byte + replacement_bytes
			, old_buffer + end_byte
			, normalized_buffer_len - end_byte - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
		);
	
	
	// In any way: set trailling zero
	dest_buffer[new_buffer_len-1] = '\0';
	
	// Set new buffer len
	this->_buffer_len = new_buffer_len;
	my_size = size();
	
	
	//
	// Copy the utf8 data of the replacement into gap
	//
	std::memcpy( dest_buffer + start_byte , replacement_buffer , replacement_bytes );
	
	
	// If the resulting string is small, we dont compute any additional information,
	// since the attributes are used to store the actual data
	if( will_be_small_string )
		goto END_OF_REPLACE;
	else{
		this->_buffer = dest_buffer;
		this->_capacity = new_capacity;
		this->_indices_len = new_indices_len;
	}
	
	
	
	//
	// Manage multibyte indices
	//
	
	
	// If the previous string was small, we simply have to recompute everything, that's easy
	if( was_small_string )
	{
		// Adjust string length
		this->_string_len = old_string_len - replaced_condepoints + replacement_codepoints;
		
		// Make new multibytes table
		compute_multibyte_table( new_indices );
		
		goto END_OF_REPLACE;
	}
	
	
	//
	// This instance had a multibyte table. Thus, copy existing indices
	//
	
	// If we don't need a new indices table, set some flags and return
	if( !new_indices ){
		this->_string_len = my_size;
		goto END_OF_REPLACE;
	}
	
	new_indices_datatype_bytes = get_index_datatype_bytes( new_buffer_len );
	
	// Copy the values after the replacement
	if( byte_difference != 0 || old_indices != new_indices ) /// @todo create flag, whether there are multibytes after the replaced section
	{
		size_type	num_indices_after_replacement = old_indices_len - replaced_indices_end;
		size_type	source = replaced_indices_end; // old_indices
		size_type	dest = replaced_indices_end - difference_in_num_multibytes; // new_indices
		
		while( num_indices_after_replacement-- > 0 ){
			utf8_string::set_nth_index(
				new_indices
				, new_indices_datatype_bytes
				, dest++
				, utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , source++ ) - byte_difference
			);
		}
	}
	
	// Copy values before replacement
	if( old_indices != new_indices )
	{
		size_type		num_indices_before_replacement = replaced_indices_start;
		
		if( new_indices_datatype_bytes == old_indices_datatype_bytes )
			std::memmove(
				new_indices
				, old_indices
				, num_indices_before_replacement * new_indices_datatype_bytes
			);
		else
			while( num_indices_before_replacement-- > 0 ){
				utf8_string::set_nth_index(
					new_indices
					, new_indices_datatype_bytes
					, num_indices_before_replacement
					, utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , num_indices_before_replacement )
				);
			}
	}
	
	//
	// Insert indices of replacement
	//
	{
		size_type	dest = replaced_indices_start; // new_indices
		
		// Insert indices of replacement
		if( replacement_small_string )
		{
			// Compute the multibyte table
			for( size_type idx = 0 ; idx < replacement_bytes ; )
			{
				unsigned char num_bytes = get_num_bytes_of_utf8_char( replacement_buffer , idx , replacement_bytes );
				
				// Add character index to multibyte table
				if( num_bytes > 1 )
					utf8_string::set_nth_index( new_indices , new_indices_datatype_bytes , dest++ , idx + start_byte );
				
				idx += num_bytes;
			}
		}
		else
		{
			unsigned char	replacement_indices_datatype_bytes = get_index_datatype_bytes( replacement._buffer_len );
			const void*		replacement_indices = replacement.get_indices();
			size_type		source = 0;
			
			// Copy indices of replacement
			while( num_replacement_multibytes-- > 0 )
				utf8_string::set_nth_index(
					new_indices
					, new_indices_datatype_bytes
					, dest++
					, utf8_string::get_nth_index( replacement_indices , replacement_indices_datatype_bytes , source++ ) + start_byte
				);
		}
	}
	
	// Adjust string length
	this->_string_len = old_string_len - replaced_condepoints + replacement_codepoints;
	
	END_OF_REPLACE:
	
	if( delete_buffer )
		delete[] old_buffer;
	
	return *this;
}




utf8_string::value_type utf8_string::at( size_type requested_index ) const
{
	if( requested_index >= size() )
		return 0;
	
	const char* buffer = this->get_buffer();
	
	if( !requires_unicode() )
		return (value_type) buffer[requested_index];
	
	// Decode internal buffer at position n
	value_type codepoint = 0;
	decode_utf8( buffer + get_num_resulting_bytes( 0 , requested_index ) , codepoint );
	
	return codepoint;
}




std::unique_ptr<utf8_string::value_type[]> utf8_string::wide_literal() const
{
	if( empty() )
		return std::unique_ptr<value_type[]>( new value_type[1]{0} );
	
	std::unique_ptr<value_type[]>	dest = std::unique_ptr<value_type[]>( new value_type[length()+1] );
	value_type*						tempDest = dest.get();
	const char*						source = this->get_buffer();
	
	while( *source )
		source += decode_utf8( source , *tempDest++ );
	
	*tempDest = 0;
	
	return dest;
}




utf8_string::difference_type utf8_string::compare( const utf8_string& str ) const
{
	const_iterator	it1 = cbegin();
	const_iterator	it2 = str.cbegin();
	const_iterator	end1 = cend();
	const_iterator	end2 = str.cend();
	
	while( it1 != end1 && it2 != end2 ){
		difference_type diff = *it2 - *it1;
		if( diff )
			return diff;
		it1++;
		it2++;
	}
	return ( it1 == end1 ? 1 : 0 ) - ( it2 == end2 ? 1 : 0 );
}

bool utf8_string::equals( const char* str ) const
{
	const char* it = this->get_buffer();
	
	if( !it )
		return !str || !*str;
	else if( !str )
		return !*it; // '!it' must be false
	
	while( *it && *str ){
		if( *it != *str )
			return false;
		it++;
		str++;
	}
	return *it == *str;
}

bool utf8_string::equals( const value_type* str ) const
{
	const_iterator	it = cbegin();
	const_iterator	end = cend();
	
	while( it != end && *str ){
		if( *str != *it )
			return false;
		it++;
		str++;
	}
	return *it == *str;
}




utf8_string::size_type utf8_string::find( value_type ch , size_type start_pos ) const
{
	for( const_iterator it = get(start_pos) ; it < cend() ; it++ )
		if( *it == ch )
			return it - begin();
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find( value_type ch , size_type start_byte ) const
{
	for( size_type it = start_byte ; it < size() ; it += get_index_bytes( it ) )
		if( raw_at(it) == ch )
			return it;
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::rfind( value_type ch , size_type start_pos ) const
{
	const_reverse_iterator it = ( start_pos >= length() ) ? crbegin() : rget( start_pos );
	while( it < crend() ){
		if( *it == ch )
			return -( it - cbegin() );
		it++;
	}
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_rfind( value_type ch , size_type start_byte ) const
{
	if( start_byte >= size() )
		start_byte = back_index();
	
	for( difference_type it = start_byte ; it >= 0 ; it -= get_index_pre_bytes( it ) )
		if( raw_at(it) == ch )
			return it;
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_first_of( const value_type* str , size_type start_pos ) const
{
	if( start_pos >= length() )
		return utf8_string::npos;
	
	for( const_iterator it = get( start_pos ) ; it < cend() ; it++ )
	{
		const value_type*	tmp = str;
		value_type			cur = *it;
		do{
			if( cur == *tmp )
				return it - begin();
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_first_of( const value_type* str , size_type start_byte ) const
{
	if( start_byte >= size() )
		return utf8_string::npos;
	
	for( size_type it = start_byte ; it < size() ; it += get_index_bytes( it ) )
	{
		const value_type*	tmp = str;
		do{
			if( raw_at(it) == *tmp )
				return it;
		}while( *++tmp );
	}
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_last_of( const value_type* str , size_type start_pos ) const
{
	const_reverse_iterator it = ( start_pos >= length() ) ? crbegin() : rget( start_pos );
	
	while( it < rend() )
	{
		const value_type*	tmp = str;
		value_type			cur = *it;
		do{
			if( cur == *tmp )
				return -( it - begin() );
		}while( *++tmp );
		it++;
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_last_of( const value_type* str , size_type start_byte ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( start_byte >= size() )
		start_byte = back_index();
	
	for( difference_type it = start_byte ; it >= 0 ; it -= get_index_pre_bytes( it ) )
	{
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
	
	for( const_iterator it = get(start_pos) ; it < cend() ; it++ )
	{
		const value_type*	tmp = str;
		value_type			cur = *it;
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it - begin();
		
		continue2:;
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_first_not_of( const value_type* str , size_type start_byte ) const
{
	if( start_byte >= size() )
		return utf8_string::npos;
	
	for( size_type it = start_byte ; it < size() ; it += get_index_bytes( it ) )
	{
		const value_type*	tmp = str;
		value_type			cur = raw_at(it);
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it;
		
		continue2:;
	}
	
	return utf8_string::npos;
}




utf8_string::size_type utf8_string::find_last_not_of( const value_type* str , size_type start_pos ) const
{
	if( empty() )
		return utf8_string::npos;
	
	const_reverse_iterator it = ( start_pos >= length() ) ? crbegin() : rget( start_pos );
	
	while( it < rend() )
	{
		const value_type*	tmp = str;
		value_type			cur = *it;
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return -( it - begin() );
		
		continue2:
		it++;
	}
	
	return utf8_string::npos;
}

utf8_string::size_type utf8_string::raw_find_last_not_of( const value_type* str , size_type start_byte ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( start_byte >= size() )
		start_byte = back_index();
	
	for( difference_type it = start_byte ; it >= 0 ; it -= get_index_pre_bytes( it ) )
	{
		const value_type*	tmp = str;
		value_type			cur = raw_at(it);
		
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it;
		
		continue2:;
	}
	
	return utf8_string::npos;
}




int operator-( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs )
{
	utf8_string::difference_type minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::difference_type max_index = std::max( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type num_codepoints = lhs.get_instance()->get_num_resulting_codepoints( minIndex , max_index - minIndex );
	return max_index == lhs.get_index() ? num_codepoints : -num_codepoints;
}

int operator-( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs )
{
	utf8_string::difference_type	minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::difference_type	max_index = std::max( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type			num_codepoints = lhs.get_instance()->get_num_resulting_codepoints( minIndex , max_index - minIndex );
	return max_index == rhs.get_index() ? num_codepoints : -num_codepoints;
}

constexpr utf8_string::size_type utf8_string::npos;