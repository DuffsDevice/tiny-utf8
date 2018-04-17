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



utf8_string::utf8_string( const char* str , size_type len , detail::read_codepoints_tag ) :
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
		if( size_type( num_multibytes - 1 ) < size_type( string_len / 2 ) ) // Indices Table worth the memory loss? (Note: num_multibytes is intended to underflow at '0')
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
					case 7:	buffer_iter[6] = str_iter[6]; // Copy data byte
					case 6:	buffer_iter[5] = str_iter[5]; // Copy data byte
					case 5:	buffer_iter[4] = str_iter[4]; // Copy data byte
					case 4:	buffer_iter[3] = str_iter[3]; // Copy data byte
					case 3:	buffer_iter[2] = str_iter[2]; // Copy data byte
					case 2:	buffer_iter[1] = str_iter[1]; // Copy data byte
						// Set next entry in the LUT!
						utf8_string::set_lut( lut_iter -= lut_width , lut_width , str_iter - str );
					case 1: buffer_iter[0] = str_iter[0]; // Copy data byte
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



utf8_string::utf8_string( const char* str , size_type data_len , detail::read_bytes_tag ) :
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
		if( size_type( num_multibytes - 1 ) < size_type( string_len / 2 ) ) // Indices Table worth the memory loss? (Note: num_multibytes is intended to underflow at '0')
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
					case 7:	buffer_iter[6] = str_iter[6]; // Copy data byte
					case 6:	buffer_iter[5] = str_iter[5]; // Copy data byte
					case 5:	buffer_iter[4] = str_iter[4]; // Copy data byte
					case 4:	buffer_iter[3] = str_iter[3]; // Copy data byte
					case 3:	buffer_iter[2] = str_iter[2]; // Copy data byte
					case 2:	buffer_iter[1] = str_iter[1]; // Copy data byte
						// Set next entry in the LUT!
						utf8_string::set_lut( lut_iter -= lut_width , lut_width , str_iter - str );
					case 1: buffer_iter[0] = str_iter[0]; // Copy data byte
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
		if( size_type( num_multibytes - 1 ) < size_type( string_len / 2 ) ) // Indices Table worth the memory loss? (Note: num_multibytes is intended to underflow at '0')
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
		case 6:
			if( ((unsigned char)data_start[-6] & 0xFE ) == 0xFC )	// 1111110X six bytes
				return 6;
		case 5:
			if( ((unsigned char)data_start[-5] & 0xFC ) == 0xF8 )	// 111110XX five bytes
				return 5;
		case 4:
			if( ((unsigned char)data_start[-4] & 0xF8 ) == 0xF0 )	// 11110XXX four bytes
				return 4;
		case 3:
			if( ((unsigned char)data_start[-3] & 0xF0 ) == 0xE0 )	// 1110XXXX three bytes
				return 3;
		case 2:
			if( ((unsigned char)data_start[-2] & 0xE0 ) == 0xC0 )	// 110XXXXX two bytes
				return 2;
		case 1:
		case 0:
			return 1;
	}
}



#if !defined(_TINY_UTF8_H_HAS_CLZ_) || _TINY_UTF8_H_HAS_CLZ_ == false
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
#endif



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
			if( utf8_string::lut_active( str_lut_base_ptr ) )
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
				std::memcpy( this , &str , sizeof(utf8_string) ); // Copy data
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
	
	if( lut_active( lut_base_ptr ) )
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
	if( utf8_string::lut_active( lut_base_ptr ) )
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
		if( utf8_string::lut_active( lut_iter ) )
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
			while( lut_iter > lut_begin ){
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
	const char*	buffer_end = buffer + data_len;
	
	// Iterate the data byte by byte...
	while( buffer_iter < buffer_end ){
		width_type bytes = utf8_string::get_codepoint_bytes( *buffer_iter , buffer_end - buffer_iter );
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
		if( utf8_string::lut_active( lut_iter ) )
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
		if( utf8_string::lut_active( lut_iter ) )
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
	bool lut_active				= utf8_string::lut_active( lut_base_ptr );
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
		app_lut_active = utf8_string::lut_active( app_lut_base_ptr );
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
		if( (old_lut_active = utf8_string::lut_active( old_lut_base_ptr )) )
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
	
	// Use indices table?
	if( new_lut_len > 0 && ( old_lut_active || !old_sso_inactive ) )
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
		str_lut_active = utf8_string::lut_active( str_lut_base_ptr );
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
		if( (old_lut_active = utf8_string::lut_active( old_lut_base_ptr )) )
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
	if( size_type( new_lut_len - 1 ) < size_type( new_string_len / 2 ) ) // Note: new_lut_len is intended to underflow at '0'
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
		repl_lut_active = utf8_string::lut_active( repl_lut_base_ptr );
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
		if( (old_lut_active = utf8_string::lut_active( old_lut_base_ptr )) )
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
	if( size_type( new_lut_len - 1 ) < size_type( new_string_len / 2 ) ) // Note: new_lut_len is intended to underflow at '0'
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
	bool		old_lut_active = utf8_string::lut_active( old_lut_base_ptr );
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



utf8_string::difference_type utf8_string::compare( const utf8_string& str ) const
{
	const_iterator	it1 = cbegin(), it2 = str.cbegin(), end1 = cend(), end2 = str.cend();
	while( it1 < end1 && it2 < end2 ){
		difference_type diff = *it2 - *it1;
		if( diff )
			return diff;
		++it1;
		++it2;
	}
	return ( it1 == end1 ? 1 : 0 ) - ( it2 == end2 ? 1 : 0 );
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



int operator-( const utf8_string::const_iterator& lhs , const utf8_string::const_iterator& rhs )
{
	utf8_string::difference_type minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::difference_type max_index = std::max( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type num_codepoints = lhs.get_instance()->get_num_codepoints( minIndex , max_index - minIndex );
	return max_index == lhs.get_index() ? num_codepoints : -num_codepoints;
}



int operator-( const utf8_string::const_reverse_iterator& lhs , const utf8_string::const_reverse_iterator& rhs )
{
	utf8_string::difference_type	minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::difference_type	max_index = std::max( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type			num_codepoints = lhs.get_instance()->get_num_codepoints( minIndex , max_index - minIndex );
	return max_index == rhs.get_index() ? num_codepoints : -num_codepoints;
}



std::ostream& operator<<( std::ostream& stream , const utf8_string& str ){
	return stream << str.c_str();
}



std::istream& operator>>( std::istream& stream , utf8_string& str ){
	std::string tmp;
	stream >> tmp;
	str = move(tmp);
	return stream;
}

constexpr utf8_string::size_type utf8_string::npos;
