#include "tinyutf8.h"
#include <limits>
#include <stdio.h>

utf8_string::utf8_string( utf8_string::size_type number , utf8_string::value_type ch ) :
	utf8_string()
{
	if( !( number && ch ) )
		return;
	
	unsigned char	num_bytes = get_num_bytes_of_utf8_codepoint( ch );
	char*			buffer = this->new_buffer( number * num_bytes + 1 );
	
	// Set string length
	if( !is_small_string( this->buffer_len ) )
		this->string_len = number;
	
	if( num_bytes > 1 )
	{
		size_type*	indices = this->new_indices_table( num_bytes > 1 ? number : 0 );
		char		representation[6];
		char*		orig_buffer = buffer;
		
		// Encode the wide character in utf8
		encode_utf8( ch , representation );
		
		while( number-- > 0 )
		{
			if( indices )
				*indices++ = buffer - orig_buffer;
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
	if( !(str && str[0] && len ) )
		return;
	
	size_type		num_multibytes = 0;
	size_type		num_bytes = 0;
	size_type		string_len = 0;
	bool			misformatted = false;
	
	// Count Multibytes
	while( str[num_bytes] && string_len < len )
	{
		string_len++; // Count number of codepoints
		
		// Compute the number of bytes to skip
		unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( str , num_bytes , utf8_string::npos , &misformatted );
		num_bytes += cur_num_bytes;
		if( cur_num_bytes > 1 )
			num_multibytes++; // Increase number of occoured multibytes
	}
	
	// Initialize buffer
	char* buffer = this->new_buffer( num_bytes + 1 );
	std::memcpy( buffer , str , this->buffer_len );
	
	// Set string length
	if( !is_small_string( this->buffer_len ) )
		this->string_len = string_len;
	
	// Compute the multibyte table
	this->compute_multibyte_table( num_multibytes , misformatted ? &misformatted : nullptr );
	
	// Set misformatted flag
	this->misformatted = misformatted;
}

utf8_string::utf8_string( const value_type* str , size_type len ) :
	utf8_string()
{
	if( !(str && str[0] && len ) )
		return;
	
	size_type	num_multibytes = 0;
	size_type	cur_multibyte_index = 0;
	size_type	string_len = 0;
	size_type	num_bytes = 0;
	
	for( size_type i = 0 ; str[i] ; i++ ){
		unsigned char cur_bytes = get_num_bytes_of_utf8_codepoint( str[i] );
		if( cur_bytes > 1 )
			num_multibytes++;
		num_bytes += cur_bytes;
	}
	
	// Initialize the internal buffer
	char* buffer = this->new_buffer( num_bytes + 1 ); // +1 for the trailling zero
	size_type* indices = this->new_indices_table( num_multibytes );
	
	char* cur_writer = buffer;
	
	// Iterate through wide char literal
	for( size_type i = 0; str[i] && string_len < len ; i++ )
	{
		// Encode wide char to utf8
		unsigned char num_bytes = encode_utf8( str[i] , cur_writer );
		
		// Push position of character to 'indices'
		if( num_bytes > 1 && indices )
			indices[cur_multibyte_index++] = cur_writer - buffer;
		
		// Step forward with copying to internal utf8 buffer
		cur_writer += num_bytes;
		
		// Increase string length by 1
		string_len++;
	}
	
	// Add trailling \0 char to utf8 buffer
	*cur_writer = 0;
	
	// set string length
	if( !is_small_string( this->buffer_len ) )
		this->string_len = string_len;
}




utf8_string::utf8_string( utf8_string&& str ) :
	misformatted( str.misformatted )
	, buffer_len( str.buffer_len )
	, buffer( str.buffer )
	, string_len( str.string_len )
	, indices( str.indices )
	, indices_len( str.indices_len )
{
	// Reset old string
	str.buffer = nullptr;
	str.indices = nullptr;
	str.indices_len = 0;
	str.buffer_len = 0;
	str.string_len = 0;
}

utf8_string::utf8_string( const utf8_string& str ) :
	misformatted( str.misformatted )
	, buffer_len( str.buffer_len )
	, buffer( nullptr )
	, string_len( str.string_len )
	, indices( nullptr )
	, indices_len( 0 )
{
	// Clone data
	if( const char* buffer = str.get_buffer() )
		if( char* my_buffer = this->new_buffer( str.buffer_len ) )
			std::memcpy( my_buffer , buffer , str.buffer_len * sizeof(char) );
	
	// Clone indices
	if( const size_type* indices = str.get_indices_of_multibyte() )
		if( size_type* my_indices = this->new_indices_table( str.indices_len ) )
			std::memcpy( my_indices , indices , str.indices_len * sizeof(size_type) );
}




utf8_string& utf8_string::operator=( const utf8_string& str )
{
	// Clear old data
	clear();
	
	if( str.empty() )
		return *this;
	
	char*		buffer = this->new_buffer( str.buffer_len );
	
	// Clone data
	std::memcpy( buffer , str.get_buffer() , this->buffer_len );
	
	// Clone indices
	if( !is_small_string( this->buffer_len ) )
		if( size_type* indices = this->new_indices_table( str.get_indices_len() ) )
			std::memcpy( indices , str.indices , this->indices_len * sizeof(size_type) );
	
	this->misformatted = str.misformatted;
	
	// Set string length
	if( !is_small_string( this->buffer_len ) )
		this->string_len = str.string_len;
	
	return *this;
}


utf8_string& utf8_string::operator=( utf8_string&& str )
{
	// Reset old data
	clear();
	
	// Copy data
	memcpy( this , &str , sizeof(utf8_string) );
	
	// Reset old string
	str.indices = nullptr;
	str.indices_len = 0;
	str.buffer = nullptr;
	str.buffer_len = 0;
	str.string_len = 0;
	return *this;
}


unsigned char utf8_string::get_num_bytes_of_utf8_char_before( const char* data , size_type current_byte_index , size_type buffer_len , bool* misformatted )
{
	if( current_byte_index >= buffer_len || current_byte_index < 1 )
		return 1;
	
	data += current_byte_index;
	
	// If we know the utf8 string is misformatted, we have to check, how many 
	if( !misformatted || *misformatted )
	{
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
	}
	else
	{
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
	}
	return 1;
}



unsigned char utf8_string::get_num_bytes_of_utf8_char( const char* data , size_type first_byte_index , size_type buffer_len , bool* misformatted )
{
	if( buffer_len <= first_byte_index )
		return 1;
	
	data += first_byte_index;
	unsigned char first_byte =  static_cast<unsigned char>(data[0]);
	
	// If we know the utf8 string is misformatted, we have to check, how many 
	if( misformatted )
	{
		if( first_byte <= 0x7F )
			return 1;
		
		// Check if byte is data-blob
		if( ( static_cast<unsigned char>(data[1]) & 0xC0) != 0x80 )		return *misformatted = true;
		// 110XXXXX - two bytes?
		if( (first_byte & 0xE0) == 0xC0 )	return 2;
		
		// Check if byte is data-blob
		if( ( static_cast<unsigned char>(data[2]) & 0xC0) != 0x80 )		return *misformatted = true;
		// 1110XXXX - three bytes?
		if( (first_byte & 0xF0) == 0xE0 )	return 3;
		
		// Check if byte is data-blob
		if( ( static_cast<unsigned char>(data[3]) & 0xC0) != 0x80 )		return *misformatted = true;
		// 11110XXX - four bytes?
		if( (first_byte & 0xF8) == 0xF0 )	return 4;
		
		// Check if byte is data-blob
		if( ( static_cast<unsigned char>(data[4]) & 0xC0) != 0x80 )		return *misformatted = true;
		// 111110XX - five bytes?
		if( (first_byte & 0xFC) == 0xF8 )	return 5;
		
		// Check if byte is data-blob
		if( ( static_cast<unsigned char>(data[5]) & 0xC0) != 0x80 )		return *misformatted = true;
		// 1111110X - six bytes?
		if( (first_byte & 0xFE) == 0xFC )	return 6;
	}
	else
	{	
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
	}
	return 1; // one byte
}



unsigned char utf8_string::decode_utf8( const char* data , value_type& dest , bool misformatted )
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
	
	if( !misformatted )
		for( int i = 1 ; i < num_bytes ; i++ )
			codepoint = (codepoint << 6) | (data[i] & 0x3F);
	else
		for( int i = 1 ; i < num_bytes ; i++ ){
			if( !data[i] || ( static_cast<unsigned char>(data[i]) & 0xC0) != 0x80 ){
				num_bytes = 1;
				codepoint = first_char;
				break;
			}
			codepoint = (codepoint << 6) | ( static_cast<unsigned char>(data[i]) & 0x3F );
		}
	
	dest = codepoint;
	
	return num_bytes;
}



std::string utf8_string::cpp_str_bom() const
{
	constexpr char bom[4] = { char(0xEF) , char(0xBB) , char(0xBF) , 0 };
	
	const char* buffer = this->get_buffer();
	
	if( !buffer )
		return std::string( bom );
	
	// Create std::string
	std::string result = std::string( this->buffer_len + 3 , ' ' );
	char* tmp_buffer = const_cast<char*>( result.data() );
	tmp_buffer[0] = bom[0];
	tmp_buffer[1] = bom[1];
	tmp_buffer[2] = bom[2];
	
	// Copy my data into it
	std::memcpy( tmp_buffer + 3 , buffer , this->buffer_len );
	
	return std::move(result);
}

utf8_string::size_type utf8_string::get_num_resulting_codepoints( size_type start_byte , size_type byte_count ) const 
{
	const char* buffer = this->get_buffer();
	
	if( !buffer )
		return 0;
	
	bool		misformatted;
	bool*		check_misformatted = this->misformatted ? &misformatted : nullptr;
	size_type	cur_index = start_byte;
	size_type	end_index = start_byte + byte_count;
	
	byte_count = std::min<difference_type>( byte_count , size() - start_byte );
	
	// Reduce the byte count by the number of bytes by the number of utf8 data bytes
	if( is_small_string( this->buffer_len ) )
	{
		while( cur_index < end_index ){
			unsigned char num_bytes = get_num_bytes_of_utf8_char( buffer , cur_index , this->buffer_len , check_misformatted );
			cur_index += num_bytes;
			byte_count -= num_bytes - 1;
		}
	}
	else if( this->indices_len > 0 )
	{
		size_type index_multibyte_table = 0;
		
		// Iterate to the start of the relevant part of the multibyte table
		for( ; index_multibyte_table < this->indices_len && this->indices[index_multibyte_table] < start_byte ; index_multibyte_table++ );
		
		// Iterate over relevant multibyte indices
		while( index_multibyte_table < this->indices_len )
		{
			cur_index = this->indices[index_multibyte_table];
			
			if( cur_index > end_index )
				break;
			
			index_multibyte_table++;
			byte_count -= get_num_bytes_of_utf8_char( buffer , cur_index , this->buffer_len , check_misformatted ) - 1; // Remove utf8 data bytes
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
	
	bool misformatted;
	bool* check_misformatted = this->misformatted ? &misformatted : nullptr;
	size_type cur_byte = start_byte;
	
	// Reduce the byte count by the number of utf8 data bytes
	if( is_small_string( this->buffer_len ) )
		while( codepoint_count-- > 0 && cur_byte < size )
			cur_byte += get_num_bytes_of_utf8_char( buffer , cur_byte , this->buffer_len , check_misformatted );
	else if( this->indices_len > 0 )
	{
		if( codepoint_count >= this->buffer_len )
			return size;
		
		// Add at least as many bytes as codepoints
		cur_byte += codepoint_count;
		
		size_type index_multibyte_table = 0;
		
		// Iterate to the start of the relevant part of the multibyte table
		for( ; index_multibyte_table < this->indices_len && this->indices[index_multibyte_table] < start_byte ; index_multibyte_table++ );
		
		// Iterate over relevant multibyte indices
		while( index_multibyte_table < this->indices_len )
		{
			size_type multibyte_pos = this->indices[index_multibyte_table];
		
			if( multibyte_pos >= cur_byte )
				break;
			
			index_multibyte_table++;
			
			// Add the utf8 data bytes to the number of bytes
			cur_byte += get_num_bytes_of_utf8_char( buffer , multibyte_pos , this->buffer_len , check_misformatted ) - 1; // Add utf8 data bytes
		}
	}
	
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

void utf8_string::compute_multibyte_table( size_type num_multibytes , bool* misformatted )
{
	// initialize indices table
	size_type*	indices = this->new_indices_table( num_multibytes );
	char*		buffer = this->get_buffer();
	
	// if we could allocate the indices table
	if( indices )
	{
		size_type multibyte_index = 0;
		
		// Fill Multibyte Table
		for( size_type index = 0 ; index < this->buffer_len ; index++ )
		{
			unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( buffer , index , this->buffer_len , misformatted ) - 1;
			if( cur_num_bytes > 1 )
				indices[multibyte_index++] = index;
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
	utf8_string result;
	
	//
	// Manage utf8 string
	//
	char* dest_buffer = result.new_buffer( byte_count + 1 );
	
	// Partly copy
	std::memcpy( dest_buffer , this->get_buffer() + start_byte , byte_count );
	dest_buffer[byte_count] = 0;
	
	
	//
	// Manage indices
	//
	if( !is_small_string( byte_count + 1 ) )
	{
		if( this->indices_len > 0 )
		{
			// Look for start of indices
			size_type index_multibyte_table = 0;
			while( index_multibyte_table < this->indices_len && this->indices[index_multibyte_table] < start_byte )
				index_multibyte_table++;
			
			size_type	start_of_multibytes_in_range = index_multibyte_table;
			
			// Look for the end
			while( index_multibyte_table < this->indices_len && this->indices[index_multibyte_table] < end_index )
				index_multibyte_table++;
			
			// Compute number of indices 
			size_type new_indices_len = index_multibyte_table - start_of_multibytes_in_range;
			
			// Create new indices
			size_type* dest_indices = result.new_indices_table( new_indices_len );
			
			// Copy indices
			for( size_type i = 0 ; i < new_indices_len ; i++ )
				dest_indices[i] = this->indices[start_of_multibytes_in_range + i] - start_byte;
		}
		
		// Set string length
		result.string_len = num_substr_codepoints;
	}
	
	return std::move(result);
}


void utf8_string::raw_replace( size_type start_byte , size_type replaced_bytes , const utf8_string& replacement )
{
	size_type my_size = size();
	// Boundary check
	if( start_byte > my_size )
		start_byte = my_size;
	
	size_type		end_byte = start_byte + replaced_bytes;
	
	if( end_byte > my_size )
		end_byte = my_size;
	
	// Predefs and Backups
	size_type		replacement_bytes = replacement.size();
	const char* 	replacement_buffer = replacement.get_buffer();
	difference_type	byte_difference = replaced_bytes - replacement_bytes;
	char*			old_buffer = this->get_buffer();
	size_type		old_buffer_len = this->buffer_len;
	size_type*		old_indices = this->indices;
	size_type		old_indices_len = this->indices_len;
	char*			dest_buffer = old_buffer;
	size_type		normalized_buffer_len = std::max<size_type>( this->buffer_len , 1 );
	size_type		new_buffer_len = normalized_buffer_len - byte_difference;
	bool			was_small_string = is_small_string( old_buffer_len );
	replaced_bytes = end_byte - start_byte;
	
	
	//
	// Manage utf8 data
	//
	
	if( new_buffer_len > 1 )
	{
		// Allocate new buffer
		if( new_buffer_len != old_buffer_len )
		{
			// Allocate new buffer
			if( is_small_string( new_buffer_len ) )
				dest_buffer = reinterpret_cast<char*>( &this->buffer );
			else
				dest_buffer = new char[new_buffer_len];
			
			// Copy after replacement
			std::memmove(
				dest_buffer + start_byte + replacement_bytes
				, old_buffer + end_byte
				, normalized_buffer_len - end_byte - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
			);
			
			// Check, if the buffer changed
			if( dest_buffer != old_buffer )
			{
				// Copy until replacement
				std::memcpy( dest_buffer , old_buffer , start_byte );
				
				// If the previous buffer was not "inplace", delete it
				if( !was_small_string )
					delete[] old_buffer;
				
				// Set the new buffer
				if( !is_small_string( new_buffer_len ) )
					this->buffer = dest_buffer;
			}
			
			this->buffer_len = new_buffer_len;
			my_size = size();
			
			// Set trailling zero
			dest_buffer[my_size] = '\0';
		}
		
		// Copy replacement into gap
		std::memcpy( dest_buffer + start_byte , replacement_buffer , replacement_bytes );
	}
	else{
		// Reset because empty
		clear();
		return;
	}
	
	// If the resulting string is small, we dont compute any additional information,
	// since the attributes are used to store the actual data
	if( is_small_string( new_buffer_len ) )
	{
		// If the old string was not small, it had an indices table that we need to delete
		if( !was_small_string )
			delete[] old_indices;
		
		return;
	}
	
	
	
	//
	// Manage indices
	//
	
	// If the previous string was small, we simply have to compute everything, that's quick
	if( was_small_string )
	{
		size_type		num_multibytes = 0;
		size_type		string_len = 0;
		bool			misformatted = false;
		size_type		new_size = new_buffer_len - 1;
		
		// Count Multibytes
		for( size_type idx = 0 ; idx < new_size ; )
		{
			string_len++; // Increase number of codepoints
			
			// Compute the number of bytes to skip
			unsigned char cur_num_bytes = get_num_bytes_of_utf8_char( dest_buffer , idx , new_buffer_len , &misformatted );
			if( cur_num_bytes > 1 )
				num_multibytes++; // Increase number of occoured multibytes
			idx += cur_num_bytes;
		}
		
		// Make new multibytes table
		compute_multibyte_table( num_multibytes , misformatted ? &misformatted : nullptr );
		
		// Set misformatted flag
		this->misformatted = misformatted;
		this->string_len = string_len;
		
		return;
	}
	
	
	
	// BELOW HERE, ALL BACKUP-ATTRIBUTES ARE VALID //
	
	
	
	// The starting index of the section within the index_of_mutlibyte table that will get replaced
	size_type		replaced_indices_start = 0;
	
	// Look for start of indices
	while( replaced_indices_start < this->indices_len && this->indices[replaced_indices_start] < start_byte )
		replaced_indices_start++;
	
	// Look for the end
	size_type replaced_indices_end = replaced_indices_start;
	while( replaced_indices_end < this->indices_len && this->indices[replaced_indices_end] < end_byte )
		replaced_indices_end++;
	
	// Compute the number of relevant multibytes
	size_type		num_replacement_multibytes = replacement.get_indices_len();
	size_type		num_replaced_multibytes = replaced_indices_end - replaced_indices_start;
	
	// Compute difference in number of indices
	difference_type difference_in_num_multibytes = num_replaced_multibytes - num_replacement_multibytes;
	
	// Compute new indices table size
	size_type		new_indices_len = old_indices_len - difference_in_num_multibytes;
	size_type*		new_indices = old_indices;
	
	// if we don't need a new indices table, set some flages and return
	if( !new_indices_len )
	{
		this->string_len = my_size;
		this->misformatted |= replacement.misformatted;
		
		// Since the old string was not small, it had an allocated table of multibytes that we need to delete
		delete[] old_indices;
		
		this->indices = nullptr;
		this->indices_len = 0;
		
		return;
	}
	
	// If the size of the indices table changes
	if( old_indices_len != new_indices_len )
	{
		// Allocate new buffer
		new_indices = this->new_indices_table( new_indices_len );
		
		size_type		num_indices_before_replacement = replaced_indices_start;
		size_type		num_indices_after_replacement = old_indices_len - replaced_indices_end;
		
		// Copy the values after the replacement
		{
			size_type*		source = old_indices + replaced_indices_end;
			size_type*		dest = new_indices + replaced_indices_end - difference_in_num_multibytes;
			
			if( byte_difference )
				while( num_indices_after_replacement-- > 0 )
					*dest++ = *source++ - byte_difference;
			else
				memcpy( dest , source , num_indices_after_replacement );
		}
		
		// Copy values before replacement
		memcpy( new_indices , old_indices , num_indices_before_replacement );
		
		// Since the old string was not small, it had an allocated table of multibytes that has to be deleted
		delete[] old_indices;
	}
	
	
	// Predefs
	bool misformatted = this->misformatted;
	size_type*		dest = new_indices + replaced_indices_start;
	size_type		replacement_codepoints = 0;
	
	// Insert indices of replacement
	if( is_small_string( replacement.buffer_len ) )
	{
		// Compute the multibyte table
		for( size_type idx = 0 ; idx < replacement_bytes ; )
		{
			unsigned char num_bytes = get_num_bytes_of_utf8_char( replacement_buffer , idx , replacement_bytes , &misformatted );
			
			// Add character index to multibyte table
			if( num_bytes > 1 )
				*dest++ = idx;
			
			idx += num_bytes;
			replacement_codepoints++;
		}
	}
	else
	{
		// Set flags
		misformatted |= replacement.is_misformatted();
		replacement_codepoints = replacement.length();
		
		// Copy indices of replacement
		size_type*		source = replacement.indices;
		while( num_replacement_multibytes-- > 0 )
			*dest++ = *source++ + start_byte;
	}
	
	// Set misformatted flag
	this->misformatted = misformatted;
	
	// Adjust string length
	this->string_len -= get_num_resulting_codepoints( start_byte , replaced_bytes );
	this->string_len += replacement_codepoints;
}




utf8_string::value_type utf8_string::at( size_type requested_index ) const
{
	if( requested_index >= size() )
		return 0;
	
	if( !is_small_string( this->buffer_len ) && !requires_unicode() )
		return (value_type) this->buffer[requested_index];
	
	// Decode internal buffer at position n
	value_type codepoint = 0;
	decode_utf8( this->get_buffer() + get_num_resulting_bytes( 0 , requested_index ) , codepoint , this->misformatted );
	
	return codepoint;
}




std::unique_ptr<utf8_string::value_type[]> utf8_string::wide_literal() const
{
	if( empty() )
		return std::unique_ptr<value_type[]>( new value_type[1]{0} );
	
	std::unique_ptr<value_type[]>	dest = std::unique_ptr<value_type[]>( new value_type[length()] );
	value_type*						tempDest = dest.get();
	char*							source = this->buffer;
	
	while( *source )
		source += decode_utf8( source , *tempDest++ , this->misformatted );
	
	return std::move(dest);
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
	const char* it1 = this->buffer;
	
	if( !it1 || !str )
		return it1 == str;
	
	while( *it1 && *str ){
		if( *it1 != *str )
			return false;
		it1++;
		str++;
	}
	return *it1 == *str;
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
