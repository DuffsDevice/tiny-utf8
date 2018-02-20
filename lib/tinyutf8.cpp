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

utf8_string::utf8_string( utf8_string::size_type count , utf8_string::value_type ch ) :
	t_sso( count )
{
	if( !count )
		return;
	
	unsigned char	num_bytes_per_cp = get_num_bytes_of_utf8_codepoint( ch );
	size_type		data_len = num_bytes_per_cp * count;
	char*			buffer;
	
	// Need to allocate a buffer?
	if( data_len > get_max_sso_data_len() )
	{
		size_type buffer_size = determine_main_buffer_size( data_len , 0 , 0 );
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		set_buffer_size( buffer_size ); // This also disables SSO
		t_non_sso.data_len = data_len;
		t_non_sso.string_len = count;
	}
	else
		buffer = t_sso.data;
	
	// Fill the buffer
	if( num_bytes_per_cp > 1 ){
		buffer += encode_utf8( ch , buffer );
		for( size_type i = 0 ; i < count ; i++ )
			buffer = (char*) std::memcpy( buffer + num_bytes_per_cp , buffer , num_bytes_per_cp );
	}
	else
		std::memset( buffer , ch , count );
	
	// Set trailling zero (in case sso is inactive, this trailing zero also indicates, that no multibyte-tracking is enabled)
	*buffer = 0;
}


utf8_string::utf8_string( const char* str , size_type len , void* ) :
	utf8_string()
{
	size_type		num_multibytes = 0;
	size_type		data_len = 0;
	size_type		string_len = 0;
	
	// Count bytes, mutlibytes and string length
	while( string_len < len && ( str[data_len] || len != utf8_string::npos ) )
	{
		// Read number of bytes of current codepoint
		unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( str[data_len] , utf8_string::npos );
		
		data_len		+= cur_num_bytes;				// Increase number of bytes
		string_len		+= 1;							// Increase number of codepoints
		num_multibytes	+= cur_num_bytes > 1 ? 1 : 0;	// Increase number of occoured multibytes?
	}
	
	char*	buffer;
	
	// Need heap memory?
	if( data_len > get_max_sso_data_len() )
	{
		if( ( num_multibytes - 1 ) < ( string_len / 2 ) ) // Indices Table worth the memory? (Note: num_multibytes is intended to underflow at '0')
		{
			// Assume a lut width of 1 for the buffer size and check the actually needed lut width
			unsigned char lut_width = get_lut_width( determine_main_buffer_size( data_len , num_multibytes , 1 ) );
			
			// Determine the buffer size (excluding the lut indicator)
			size_type buffer_size	= determine_main_buffer_size( data_len , num_multibytes , lut_width );
			buffer					= t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ]; // The LUT indicator is 
			
			// Set up LUT
			char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
			utf8_string::set_lut_indiciator( lut_iter , false , num_multibytes ); // Set the LUT indicator
			
			// Fill the lut and copy bytes
			char* buffer_iter = buffer;
			const char* str_iter = str;
			const char* str_end = str + data_len; // Compute End of the string
			while( str_iter < str_end )
			{
				unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( *str_iter , str_end - str_iter );
				switch( cur_num_bytes )
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
				buffer_iter	+= cur_num_bytes;
				str_iter	+= cur_num_bytes;
			}
			*buffer_iter = '\0'; // Set trailing '\0'
			
			// Set Attributes
			set_buffer_size( buffer_size );
			t_non_sso.data_len = data_len;
			t_non_sso.string_len = string_len;
			
			return; // We have already done everything!
		}
		
		size_type buffer_size = determine_main_buffer_size( data_len , 0 , 1 );
		buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		
		// Set up LUT
		char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
		utf8_string::set_lut_indiciator( lut_iter , num_multibytes , 0 ); // Set the LUT indicator
		
		// Set Attributes
		set_buffer_size( buffer_size );
		t_non_sso.data_len = data_len;
		t_non_sso.string_len = string_len;
	}
	else{
		buffer = t_sso.data;
		
		// Set Attrbutes
		t_sso.set_data_len( data_len );
		
		// Set up LUT: Not necessary, since the LUT is automatically inactive,
		// since SSO is active and the LUT indicator shadows 't_sso.data_len', which has the LSB = 0 (=> LUT inactive).
	}
	
	//! Fill the buffer
	std::memcpy( buffer , str , data_len );
	buffer[data_len] = '\0';
}

// utf8_string::utf8_string( const value_type* str , size_type len ) :
	// utf8_string()
// {
	// if( !str || !str[0] || !len )
		// return;
	
	// size_type		num_multibytes = 0;
	// size_type		data_len = 0;
	// size_type		string_len = 0;
	
	// // Count bytes, mutlibytes and string length
	// while( string_len < len && ( str[data_len] || len == utf8_string::npos ) )
	// {
		// // Read number of bytes of current codepoint
		// unsigned char codepoint_bytes = get_num_bytes_of_utf8_codepoint( str[string_len] );
		
		// data_len		+= cur_num_bytes;				// Increase number of bytes
		// string_len		+= 1;							// Increase number of codepoints
		// num_multibytes	+= cur_num_bytes > 1 ? 1 : 0;	// Increase number of occoured multibytes?
	// }
	
	// // Iterate through wide char literal
	// for( size_type i = 0; i < string_len ; i++ )
	// {
		// // Encode wide char to utf8
		// unsigned char codepoint_bytes = encode_utf8( str[i] , cur_writer );
		// // Push position of character to 'indices'
		// if( codepoint_bytes > 1 && indices )
			// utf8_string::set_nth_index( indices , indices_datatype_bytes , cur_multibyte_index++ , cur_writer - orig_writer );
		
		// // Step forward with copying to internal utf8 buffer
		// cur_writer += codepoint_bytes;
	// }
	
	// // Add trailling \0 char to utf8 buffer
	// *cur_writer = 0;
	
	
	
	
	
	// char*	buffer;
	
	// // Need heap memory?
	// if( data_len > get_max_sso_data_len() )
	// {
		// size_type buffer_size;
		// if( ( num_multibytes - 1 ) < ( string_len / 2 ) ) // Indices Table worth the memory? (Note: num_multibytes is intended to underflow at '0')
		// {
			// // Assume a lut width of 1 for the buffer size and check the actually needed lut width
			// unsigned char lut_width = get_lut_width( determine_main_buffer_size( data_len , num_multibytes , 1 ) );
			
			// // Determine the buffer size (excluding the lut indicator)
			// buffer_size	= determine_main_buffer_size( data_len , num_multibytes , lut_width );
			// buffer		= t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ]; // The LUT indicator is 
			
			// //! Fill LUT
			// char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
			// utf8_string::set_lut( lut_iter , lut_width , utf8_string::construct_lut_indicator( num_multibytes ) ); // Set the LUT indicator
			
			// for( size_type index = 0 ; index < data_len ; ){
				// unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( str , index , buffer_len );
				// if( cur_num_bytes > 1 )
					// utf8_string::set_lut( lut_iter -= lut_width , lut_width , index );
				// index += cur_num_bytes;
			// }
			
			// return;
		// }
		// else{
			// buffer_size = determine_main_buffer_size( data_len , 0 , 1 );
			// unsigned char lut_width = get_lut_width( buffer_size )
			// buffer = t_non_sso.data = new char[ determine_total_buffer_size( buffer_size ) ];
		// }
		
		// // Set Attributes
		// set_buffer_size( buffer_size );
		// t_non_sso.data_len = data_len;
		// t_non_sso.string_len = string_len;
	// }
	// else{
		// buffer = t_sso.data;
		
		// // Set Attrbutes
		// t_sso.set_data_len( data_len );
	// }
	
	// //! Fill the buffer
	// std::memcpy( buffer , str , data_len );
	// buffer[data_len] = '\0';
// }



unsigned char utf8_string::get_num_bytes_of_utf8_char_before( const char* data , const char* data_start )
{
	// Only Check the possibilities, that could appear
	switch( data - data_start )
	{
		default:
			if( ((unsigned char)data[-7] & 0xFE ) == 0xFC )  // 11111110  seven bytes
				return 7;
		case 6:
			if( ((unsigned char)data[-6] & 0xFE ) == 0xFC )  // 1111110X  six bytes
				return 6;
		case 5:
			if( ((unsigned char)data[-5] & 0xFC ) == 0xF8 )  // 111110XX  five bytes
				return 5;
		case 4:
			if( ((unsigned char)data[-4] & 0xF8 ) == 0xF0 )  // 11110XXX  four bytes
				return 4;
		case 3:
			if( ((unsigned char)data[-3] & 0xF0 ) == 0xE0 )  // 1110XXXX  three bytes
				return 3;
		case 2:
			if( ((unsigned char)data[-2] & 0xE0 ) == 0xC0 )  // 110XXXXX  two bytes
				return 2;
		case 1:
		case 0:
			return 1;
	}
}


#if !defined(_TINY_UTF8_H_HAS_CLZ_)
unsigned char utf8_string::get_num_bytes_of_utf8_char( char first_byte , size_type data_left )
{
	// Only Check the possibilities, that could appear
	switch( data_left )
	{
		default:
			if( (first_byte & 0xFF) == 0xFE )  // 11111110  seven bytes
				return 7;
		case 6:
			if( (first_byte & 0xFE) == 0xFC )  // 1111110X  six bytes
				return 6;
		case 5:
			if( (first_byte & 0xFC) == 0xF8 )  // 111110XX  five bytes
				return 5;
		case 4:
			if( (first_byte & 0xF8) == 0xF0 )  // 11110XXX  four bytes
				return 4;
		case 3:
			if( (first_byte & 0xF0) == 0xE0 )  // 1110XXXX  three bytes
				return 3;
		case 2:
			if( (first_byte & 0xE0) == 0xC0 )  // 110XXXXX  two bytes
				return 2;
		case 1:
		case 0:
			return 1;
	}
}
#endif



// void utf8_string::shrink_to_fit()
// {
	// if( sso_active() )
		// return;
	
	// size_type	required_capacity = get_required_capacity( _buffer_len , _indices_len );
	// size_type	threshold = std::max<size_type>( required_capacity + 10 , required_capacity >> 2 );
	
	// // Do we save at least one quarter of the capacity and at least 10 bytes?
	// if( _capacity < threshold )
		// return;
	
	// // Allocate new buffer
	// char*			new_buffer = new char[required_capacity];
	// unsigned char	index_datatype_bytes = get_index_datatype_bytes( _buffer_len );
	
	// // Copy indices
	// if( _indices_len )
		// std::memcpy(
			// new_buffer + required_capacity - _indices_len * index_datatype_bytes
			// , _buffer + _capacity - _indices_len * index_datatype_bytes
			// , _indices_len * index_datatype_bytes
		// );
	
	// // Copy data
	// std::memcpy(
		// new_buffer
		// , _buffer
		// , _buffer_len
	// );
	
	// _capacity = required_capacity;
// }



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
	
	return std::move(result);
}

utf8_string::size_type utf8_string::get_num_resulting_codepoints( size_type index , size_type byte_count ) const 
{
	const char*	buffer = get_buffer();
	size_type	data_len = size();
	size_type	buffer_size = get_buffer_size();
	const char*	lut_iter = utf8_string::get_lut_base_ptr( buffer , buffer_size );
	
	// Procedure: Reduce the byte count by the number of data bytes within multibytes
	
	// Is the lut active?
	if( utf8_string::lut_active( lut_iter ) )
	{
		size_type lut_len = utf8_string::get_lut_len( lut_iter );
		
		if( !lut_len )
			return byte_count;
		
		unsigned char	lut_width	= utf8_string::get_lut_width( buffer_size );
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
			byte_count -= utf8_string::get_num_bytes_of_utf8_char( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
			lut_iter -= lut_width; // Move cursor to the next lut entry
		}
	}
	else
	{
		const char*	buffer_iter = buffer + index;
		const char*	buffer_end = buffer + data_len;
		
		// Iterate the data byte by byte...
		while( buffer_iter < buffer_end ){
			unsigned char num_bytes = utf8_string::get_num_bytes_of_utf8_char( *buffer_iter , buffer_end - buffer_iter );
			buffer_iter += num_bytes;
			byte_count -= num_bytes - 1;
		}
	}
	
	// Now byte_count is the number of codepoints
	return byte_count;
}

utf8_string::size_type utf8_string::get_num_resulting_bytes( size_type index , size_type codepoint_count ) const 
{
	const char*	buffer		= get_buffer();
	size_type	data_len	= size();
	size_type	buffer_size = get_buffer_size();
	const char*	lut_iter	= utf8_string::get_lut_base_ptr( buffer , buffer_size );
	size_type orig_index	= index;
	
	// Procedure: Reduce the byte count by the number of utf8 data bytes
	
	// Is the lut active?
	if( utf8_string::lut_active( lut_iter ) )
	{
		size_type lut_len = utf8_string::get_lut_len( lut_iter );
		
		if( !lut_len )
			return codepoint_count;
		
		// Add at least as many bytes as codepoints
		index += codepoint_count;
		
		// Reduce the byte count by the number of data bytes within multibytes
		unsigned char	lut_width = utf8_string::get_lut_width( buffer_size );
		const char*		lut_begin = lut_iter - lut_len * lut_width;
		
		// Iterate to the start of the relevant part of the multibyte table
		while( lut_iter > lut_begin ){
			lut_iter -= lut_width; // Move cursor to the next lut entry
			if( utf8_string::get_lut( lut_iter , lut_width ) >= index )
				break;
		}
		
		// Iterate over relevant multibyte indices
		while( lut_iter > lut_begin ){
			size_type multibyte_index = utf8_string::get_lut( lut_iter , lut_width );
			if( multibyte_index >= index )
				break;
			index += utf8_string::get_num_bytes_of_utf8_char( buffer[multibyte_index] , data_len - multibyte_index ) - 1; // Subtract only the utf8 data bytes
			lut_iter -= lut_width; // Move cursor to the next lut entry
		}
	}
	else
		while( codepoint_count-- > 0 )
			index += get_num_bytes_of_utf8_char( buffer[index] , data_len - index );
	
	return index - orig_index;
}




unsigned char utf8_string::encode_utf8( value_type codepoint , char* dest )
{
	unsigned char num_bytes = get_num_bytes_of_utf8_codepoint( codepoint ) - 1;
	
	switch( num_bytes ){
		case 5: dest[num_bytes-4] = char( 0x80 | ((codepoint >> 24) & 0x3F) );
		case 4: dest[num_bytes-3] = char( 0x80 | ((codepoint >> 18) & 0x3F) );
		case 3: dest[num_bytes-2] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		case 2: dest[num_bytes-1] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		case 1: dest[num_bytes] = char( 0x80 | (codepoint & 0x3F) );
			if( sizeof(unsigned int) > 1 )
				dest[0] = ( (unsigned int)0xFF80 >> num_bytes ) | ( codepoint >> ( 6 * num_bytes ) );
			else
				dest[0] = ( (unsigned long)0xFF80L >> num_bytes ) | ( codepoint >> ( 6 * num_bytes ) );
			break;
		case 0: dest[0] = char(codepoint);
	}
	
	return num_bytes + 1;
}



// utf8_string utf8_string::raw_substr( size_type index , size_type byte_count , size_type num_substr_codepoints ) const
// {
	// // Compute end index
	// size_type	end_index = std::min<size_type>( index + byte_count , size() );
	
	// // Boundary check
	// if( end_index <= index )
		// return utf8_string();
	
	// // Compute actual byte_count (with boundaries)
	// byte_count = end_index - index;
	
	// // Create result instance
	// utf8_string	result;
	
	// // If the new string is a small string
	// if( is_small_string( byte_count + 1 ) )
	// {
		// char* dest_buffer = result.new_buffer( byte_count + 1 , 0 ).first;
	
		// // Partly copy
		// std::memcpy( dest_buffer , get_buffer() + index , byte_count );
		// dest_buffer[byte_count] = 0;
		
		// return result;
	// }
	
	// // BELOW HERE ALL ATTRIBUTES ARE VALID //
	
	// if( _indices_len > 0 )
	// {
		// // Look for start of indices
		// size_type		index_multibyte_table = 0;
		// const char*		my_indices = get_indices();
		// unsigned char	my_indices_datatype_bytes = get_index_datatype_bytes( _buffer_len );
		
		// while( index_multibyte_table < _indices_len ){
			// if( utf8_string::get_nth_index( my_indices , my_indices_datatype_bytes , index_multibyte_table ) >= index )
				// break;
			// index_multibyte_table++;
		// }
		
		// size_type		start_of_multibytes_in_range = index_multibyte_table;
		
		// // Look for the end
		// while( index_multibyte_table < _indices_len )
		// {
			// if( utf8_string::get_nth_index( my_indices , my_indices_datatype_bytes , index_multibyte_table ) >= end_index )
				// break;
			// index_multibyte_table++;
		// }
		
		// // Compute number of indices 
		// size_type		new_indices_len = index_multibyte_table - start_of_multibytes_in_range;
		
		// // Create new buffer
		// std::pair<char*,char*> dest_buffer = result.new_buffer( byte_count + 1 , new_indices_len );
		
		// // Partly copy character data
		// std::memcpy( dest_buffer.first , get_buffer() + index , byte_count );
		// dest_buffer.first[byte_count] = '\0';
		
		// // Partly copy indices
		// if( char* indices = dest_buffer.second ){
			// unsigned char	indices_datatype_bytes = get_index_datatype_bytes( byte_count + 1 );
			// for( size_type i = 0 ; i < new_indices_len ; i++ )
				// utf8_string::set_nth_index( indices , indices_datatype_bytes , i , utf8_string::get_nth_index( my_indices , my_indices_datatype_bytes , start_of_multibytes_in_range + i ) - index );
		// }
	// }
	
	// // Set string length
	// result._string_len = num_substr_codepoints;
	
	// return result;
// }


// utf8_string& utf8_string::raw_replace( size_type index , size_type replaced_bytes , const utf8_string& replacement )
// {
	// size_type my_size = size();
	
	// // Boundary check
	// if( index > my_size )
		// index = my_size;
	
	// size_type		end_byte = index + replaced_bytes;
	
	// // Second operand of || is needed because of potential integer overflow in the above sum
	// if( end_byte > my_size || end_byte < index )
		// end_byte = my_size;
	
	// // Predefs and Backups
	// replaced_bytes = end_byte - index;
	// size_type		replacement_bytes = replacement.size();
	// const char* 	replacement_buffer = replacement.get_buffer();
	// size_type		replacement_codepoints = 0;
	// difference_type	byte_difference = replaced_bytes - replacement_bytes;
	// char*			old_buffer = get_buffer();
	// size_type		old_buffer_len = _buffer_len;
	// size_type		old_size = my_size;
	// char*			old_indices = get_indices();
	// unsigned char	old_indices_datatype_bytes = get_index_datatype_bytes( old_buffer_len );
	// unsigned char	new_indices_datatype_bytes = 0;
	// size_type		old_indices_len = _indices_len;
	// size_type		old_string_len = 0;
	// size_type		new_capacity = 0;
	// char*			dest_buffer = old_buffer;
	// size_type		normalized_buffer_len = old_size + 1;
	// size_type		new_buffer_len = normalized_buffer_len - byte_difference;
	// bool			was_small_string = is_small_string( old_buffer_len );
	// bool			will_be_small_string = is_small_string( new_buffer_len );
	// bool			replacement_small_string = is_small_string( replacement._buffer_len );
	// char*			new_indices = old_indices;
	// size_type		new_indices_len = 0;
	// size_type		replaced_condepoints = 0;
	// size_type		num_replacement_multibytes = 0;
	// size_type		num_replaced_multibytes = 0;
	// bool			delete_buffer = false;
	// size_type		replaced_indices_start = 0; // The starting index of the section within the index_of_mutlibyte table that will get replaced
	// size_type		replaced_indices_end = 0; // The corresp. end index
	// difference_type difference_in_num_multibytes = 0;
	
	
	// if( !new_buffer_len ){
		// clear(); // Reset because empty
		// return *this;
	// }
	
	
	// //
	// // Get information about replacement
	// //
	// if( replacement_small_string )
	// {
		// // Count Multibytes of replacement
		// for( size_type i = 0 ; i < replacement_bytes ; )
		// {
			// // Compute the number of bytes to skip
			// unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( replacement_buffer , i , replacement._buffer_len );
			// i += cur_num_bytes;
			// if( cur_num_bytes > 1 )
				// num_replacement_multibytes++;
			// replacement_codepoints++;
		// }
	// }
	// else{
		// replacement_codepoints = replacement._string_len;
		// num_replacement_multibytes = replacement._indices_len;
	// }
	
	
	// //
	// // Compute the new size of the indices table and
	// // Allocate the new buffer!
	// //
	// if( will_be_small_string ){
		// dest_buffer = get_sso_buffer();
		// new_indices = nullptr;
	// }
	// else
	// {
		// if( was_small_string )
		// {
			// new_indices_len = num_replacement_multibytes; // The minimal number of indices
			
			// // Count Multibytes
			// for( size_type idx = 0 ; idx < old_size ; )
			// {
				// old_string_len++; // Increase number of codepoints
				
				// // Compute the number of bytes to skip
				// unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( dest_buffer , idx , new_buffer_len );
				
				// // Check if were in the range of replaced codepoints
				// if( idx >= index && idx < end_byte )
					// replaced_condepoints++; // Increase number of codepoints that will get replaced
				// else if( cur_num_bytes > 1 )
					// new_indices_len++; // Increase number of multibyte indices to store
				
				// idx += cur_num_bytes;
			// }
			
			// // Allocate new buffer
			// new_capacity = get_required_capacity( new_buffer_len , new_indices_len );
			// new_capacity += new_capacity >> 1; // Allocate some more!
			
			// dest_buffer = new char[new_capacity];
		// }
		// else
		// {
			// old_string_len = _string_len;
			// replaced_condepoints = get_num_resulting_codepoints( index , replaced_bytes );
			
			// // Look for start of indices
			// while( replaced_indices_start < _indices_len ){
				// if( utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , replaced_indices_start ) >= index )
					// break;
				// replaced_indices_start++;
			// }
			
			// replaced_indices_end = replaced_indices_start;
			
			// // Look for the end
			// while( replaced_indices_end < _indices_len ){
				// if( utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , replaced_indices_end ) >= end_byte )
					// break;
				// replaced_indices_end++;
			// }
			
			// // Compute the number of relevant multibytes
			// num_replaced_multibytes = replaced_indices_end - replaced_indices_start;
			
			// // Compute difference in number of indices
			// difference_in_num_multibytes = num_replaced_multibytes - num_replacement_multibytes;
			
			// // Compute new indices table size
			// new_indices_len = old_indices_len - difference_in_num_multibytes;
			
			// // Compute new capacity
			// new_capacity = std::max<size_type>( _capacity , get_required_capacity( new_buffer_len , new_indices_len ) );
			
			// // Allocate new buffer, if we exceed the old one
			// if( _capacity != new_capacity ){
				// new_capacity += new_capacity >> 1; // Make some room for more: 1.5 times the number of bytes we actually need
				// dest_buffer = new char[new_capacity];
			// }
		// }
		
		// // Set destination indices
		// new_indices = new_indices_len
			// ? reinterpret_cast<char*>( dest_buffer + new_capacity - new_indices_len * get_index_datatype_bytes( new_buffer_len ) )
			// : nullptr;
	// }
	
	
	// // 
	// // Copy the utf8 data of this string into the possibly new one
	// //
	// if( dest_buffer != old_buffer ) // Check, if the buffer changed
	// {
		// // Copy until replacement
		// std::memcpy( dest_buffer , old_buffer , index );
		
		// // Copy after replacement
		// std::memcpy(
			// dest_buffer + index + replacement_bytes
			// , old_buffer + end_byte
			// , normalized_buffer_len - end_byte - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
		// );
		
		// // If the previous buffer was not "inplace", delete it
		// if( !was_small_string )
			// delete_buffer = true;
	// }
	// else if( new_buffer_len != old_buffer_len )
		// // Copy after replacement
		// std::memmove(
			// dest_buffer + index + replacement_bytes
			// , old_buffer + end_byte
			// , normalized_buffer_len - end_byte - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
		// );
	
	
	// // In any way: set trailling zero
	// dest_buffer[new_buffer_len-1] = '\0';
	
	// // Set new buffer len
	// _buffer_len = new_buffer_len;
	// my_size = size();
	
	
	// //
	// // Copy the utf8 data of the replacement into gap
	// //
	// std::memcpy( dest_buffer + index , replacement_buffer , replacement_bytes );
	
	
	// // If the resulting string is small, we dont compute any additional information,
	// // since the attributes are used to store the actual data
	// if( will_be_small_string )
		// goto END_OF_REPLACE;
	// else{
		// _buffer = dest_buffer;
		// _capacity = new_capacity;
		// _indices_len = new_indices_len;
	// }
	
	
	
	// //
	// // Manage multibyte indices
	// //
	
	
	// // If the previous string was small, we simply have to recompute everything, that's easy
	// if( was_small_string )
	// {
		// // Adjust string length
		// _string_len = old_string_len - replaced_condepoints + replacement_codepoints;
		
		// // Make new multibytes table
		// compute_multibyte_table( new_indices );
		
		// goto END_OF_REPLACE;
	// }
	
	
	// //
	// // This instance had a multibyte table. Thus, copy existing indices
	// //
	
	// // If we don't need a new indices table, set some flags and return
	// if( !new_indices ){
		// _string_len = my_size;
		// goto END_OF_REPLACE;
	// }
	
	// new_indices_datatype_bytes = get_index_datatype_bytes( new_buffer_len );
	
	// // Copy the values after the replacement
	// if( byte_difference != 0 || old_indices != new_indices ) /// @todo create flag, whether there are multibytes after the replaced section
	// {
		// size_type	num_indices_after_replacement = old_indices_len - replaced_indices_end;
		// size_type	source = replaced_indices_end; // old_indices
		// size_type	dest = replaced_indices_end - difference_in_num_multibytes; // new_indices
		
		// while( num_indices_after_replacement-- > 0 ){
			// utf8_string::set_nth_index(
				// new_indices
				// , new_indices_datatype_bytes
				// , dest++
				// , utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , source++ ) - byte_difference
			// );
		// }
	// }
	
	// // Copy values before replacement
	// if( old_indices != new_indices )
	// {
		// size_type		num_indices_before_replacement = replaced_indices_start;
		
		// if( new_indices_datatype_bytes == old_indices_datatype_bytes )
			// std::memmove(
				// new_indices
				// , old_indices
				// , num_indices_before_replacement * new_indices_datatype_bytes
			// );
		// else
			// while( num_indices_before_replacement-- > 0 ){
				// utf8_string::set_nth_index(
					// new_indices
					// , new_indices_datatype_bytes
					// , num_indices_before_replacement
					// , utf8_string::get_nth_index( old_indices , old_indices_datatype_bytes , num_indices_before_replacement )
				// );
			// }
	// }
	
	// //
	// // Insert indices of replacement
	// //
	// {
		// size_type	dest = replaced_indices_start; // new_indices
		
		// // Insert indices of replacement
		// if( replacement_small_string )
		// {
			// // Compute the multibyte table
			// for( size_type idx = 0 ; idx < replacement_bytes ; )
			// {
				// unsigned char num_bytes = get_num_bytes_of_utf8_char( replacement_buffer , idx , replacement_bytes );
				
				// // Add character index to multibyte table
				// if( num_bytes > 1 )
					// utf8_string::set_nth_index( new_indices , new_indices_datatype_bytes , dest++ , idx + index );
				
				// idx += num_bytes;
			// }
		// }
		// else
		// {
			// unsigned char	replacement_indices_datatype_bytes = get_index_datatype_bytes( replacement._buffer_len );
			// const char*		replacement_indices = replacement.get_indices();
			// size_type		source = 0;
			
			// // Copy indices of replacement
			// while( num_replacement_multibytes-- > 0 )
				// utf8_string::set_nth_index(
					// new_indices
					// , new_indices_datatype_bytes
					// , dest++
					// , utf8_string::get_nth_index( replacement_indices , replacement_indices_datatype_bytes , source++ ) + index
				// );
		// }
	// }
	
	// // Adjust string length
	// _string_len = old_string_len - replaced_condepoints + replacement_codepoints;
	
	// END_OF_REPLACE:
	
	// if( delete_buffer )
		// delete[] old_buffer;
	
	// return *this;
// }




void utf8_string::to_wide_literal( utf8_string::value_type* dest ) const
{
	const char*	data = get_buffer();
	size_type	data_left = size();
	
	while( data_left-- > 0 )
		data += decode_utf8_and_len( data , *dest++ , data_left );
	
	*dest = 0;
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
	const char* it = get_buffer();
	
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

utf8_string::size_type utf8_string::raw_find( value_type ch , size_type index ) const
{
	for( size_type it = index ; it < size() ; it += get_index_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_rfind( value_type ch , size_type index ) const
{
	if( index >= size() )
		index = back_index();
	
	for( difference_type it = index ; it >= 0 ; it -= get_index_pre_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_find_first_of( const value_type* str , size_type index ) const
{
	if( index >= size() )
		return utf8_string::npos;
	
	for( size_type it = index ; it < size() ; it += get_index_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_find_last_of( const value_type* str , size_type index ) const
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

utf8_string::size_type utf8_string::raw_find_first_not_of( const value_type* str , size_type index ) const
{
	if( index >= size() )
		return utf8_string::npos;
	
	for( size_type it = index ; it < size() ; it += get_index_bytes( it ) )
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