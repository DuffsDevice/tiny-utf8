#include "tinyutf8.h"

utf8_string::utf8_string( utf8_string::size_type number , utf8_string::value_type ch ) :
	string_len( number )
	, indices_of_multibyte( ch <= 0x7F && number ? nullptr : new utf8_string::size_type[number] )
	, indices_len( ch <= 0x7F ? 0 : number )
	, misformatted( false )
	, static_buffer( false )
{
	if( !number ){
		this->buffer = nullptr;
		this->buffer_len = 0;
		return;
	}
	
	unsigned char num_bytes = get_num_bytes_of_utf8_codepoint( ch );
	this->buffer_len		= ( ch ? number * num_bytes : 0 ) + 1;
	this->buffer			= new char[this->buffer_len];
	
	if( num_bytes == 1 )
		while( number-- > 0 )
			this->buffer[number] = ch;
	else
	{
		char representation[6];
		encode_utf8( ch , representation );
		
		while( number-- > 0 )
		{
			unsigned char byte = num_bytes;
			while( byte-- > 0 )
				this->buffer[ number * num_bytes + byte ] = representation[byte];
			this->indices_of_multibyte[number] = number * num_bytes;
		}
	}
	
	// Set trailling zero
	this->buffer[ this->buffer_len - 1 ] = 0;
}

utf8_string::utf8_string( const char* str , size_type len ) :
	utf8_string()
{
	// Reset some attributes, or else 'get_num_bytes_of_utf8_char' will not work, since it thinks the string is empty
	this->buffer_len = -1;
	this->indices_len = -1;
	
	if( str && str[0] )
	{
		size_type		num_multibytes = 0;
		size_type		num_bytes = 0;
		unsigned char	num_bytes_to_skip = 0;
		
		// Count Multibytes
		for( ; str[num_bytes] && this->string_len < len ; num_bytes++ )
		{
			this->string_len++; // Count number of codepoints
			
			if( static_cast<unsigned char>(str[num_bytes]) <= 0x7F ) // lead bit is zero, must be a single ascii
				continue;
			
			// Compute the number of bytes to skip
			num_bytes_to_skip = get_num_bytes_of_utf8_char( str , num_bytes );
			
			num_multibytes++; // Increase number of occoured multibytes
			
			// Check if the string doesn't end just right inside the multibyte part!
			while( --num_bytes_to_skip > 0 )
			{
				if( !str[num_bytes+1] || ( static_cast<unsigned char>(str[num_bytes+1]) & 0xC0 ) != 0x80 ){
					this->misformatted = true;
					num_multibytes--; // Decrease counter, since that sequence is apparently no ++valid++ utf8
					break;
				}
				num_bytes++;
			}
		}
		
		// Initialize buffer
		this->buffer_len = num_bytes + 1;
		this->buffer = new char[this->buffer_len];
		std::memcpy( this->buffer , str , this->buffer_len );
		
		// initialize indices table
		this->indices_len = num_multibytes;
		this->indices_of_multibyte = this->indices_len ? new size_type[this->indices_len] : nullptr;
		
		size_type multibyteIndex = 0;
		
		// Fill Multibyte Table
		for( size_type index = 0 ; index < num_bytes ; index++ )
		{
			if( static_cast<unsigned char>(buffer[index]) <= 0x7F ) // lead bit is zero, must be a single ascii
				continue;
			
			// Compute number of bytes to skip
			num_bytes_to_skip = get_num_bytes_of_utf8_char( buffer , index ) - 1;
			
			this->indices_of_multibyte[multibyteIndex++] = index;
			
			if( this->misformatted )
				while( num_bytes_to_skip-- > 0 )
				{
					if( index + 1 == num_bytes || ( static_cast<unsigned char>(buffer[index+1]) & 0xC0 ) != 0x80 ){
						multibyteIndex--; // Remove the byte position from the list, since its no ++valid++ multibyte sequence
						break;
					}
					index++;
				}
			else
				index += num_bytes_to_skip;
		}
	}
}

utf8_string::utf8_string( const value_type* str ) :
	utf8_string()
{
	if( str && str[0] )
	{
		size_type	num_multibytes;
		size_type	nextIndexPosition = 0;
		size_type	string_len = 0;
		size_type	num_bytes = get_num_required_bytes( str , num_multibytes );
		
		// Initialize the internal buffer
		this->buffer_len = num_bytes + 1; // +1 for the trailling zero
		this->buffer = new char[this->buffer_len];
		this->indices_len = num_multibytes;
		this->indices_of_multibyte = this->indices_len ? new size_type[this->indices_len] : nullptr;
		
		char* cur_writer = this->buffer;
		
		// Iterate through wide char literal
		for( size_type i = 0; str[i] ; i++ )
		{
			// Encode wide char to utf8
			unsigned char num_bytes = encode_utf8( str[i] , cur_writer );
			
			// Push position of character to 'indices_of_multibyte'
			if( num_bytes > 1 )
				this->indices_of_multibyte[nextIndexPosition++] = cur_writer - this->buffer;
			
			// Step forward with copying to internal utf8 buffer
			cur_writer += num_bytes;
			
			// Increase string length by 1
			string_len++;
		}
		
		this->string_len = string_len;
		
		// Add trailling \0 char to utf8 buffer
		*cur_writer = 0;
	}
}




utf8_string::utf8_string( utf8_string&& str ) :
	buffer( str.buffer )
	, buffer_len( str.buffer_len )
	, string_len( str.string_len )
	, indices_of_multibyte( str.indices_of_multibyte )
	, indices_len( str.indices_len )
	, misformatted( str.misformatted )
	, static_buffer( str.static_buffer )
{
	// Reset old string
	str.buffer = nullptr;
	str.indices_of_multibyte = nullptr;
	str.indices_len = 0;
	str.buffer_len = 0;
	str.string_len = 0;
}

utf8_string::utf8_string( const utf8_string& str ) :
	buffer( str.empty() ? nullptr : new char[str.buffer_len] )
	, buffer_len( str.buffer_len )
	, string_len( str.string_len )
	, indices_of_multibyte( str.indices_len ? new size_type[str.indices_len] : nullptr )
	, indices_len( str.indices_len )
	, misformatted( str.misformatted )
	, static_buffer( false )
{
	// Clone data
	if( !str.empty() )
		std::memcpy( this->buffer , str.buffer , str.buffer_len );
	
	// Clone indices
	if( str.indices_len )
		std::memcpy( this->indices_of_multibyte , str.indices_of_multibyte , str.indices_len * sizeof(size_type) );
}




utf8_string& utf8_string::operator=( const utf8_string& str )
{
	if( str.empty() ){
		clear();
		return *this;
	}
	
	char*		new_buffer = new char[str.buffer_len];
	size_type*	new_indices = str.indices_len ? new size_type[str.indices_len] : nullptr;
	
	// Clone data
	std::memcpy( new_buffer , str.buffer , str.buffer_len );
	
	// Clone indices
	if( str.indices_len > 0 )
		std::memcpy( new_indices , str.indices_of_multibyte , str.indices_len * sizeof(size_type) );
	
	this->misformatted = str.misformatted;
	this->string_len = str.string_len;
	reset_buffer( new_buffer , str.buffer_len );
	reset_indices( new_indices , str.indices_len );
	return *this;
}

utf8_string& utf8_string::operator=( utf8_string&& str )
{
	// Copy data
	reset_buffer( str.buffer , str.buffer_len );
	reset_indices( str.indices_of_multibyte , str.indices_len );
	this->misformatted = str.misformatted;
	this->string_len = str.string_len;
	this->static_buffer = str.static_buffer;
	
	// Reset old string
	str.indices_of_multibyte = nullptr;
	str.indices_len = 0;
	str.buffer = nullptr;
	str.buffer_len = 0;
	str.string_len = 0;
	return *this;
}


unsigned char utf8_string::get_num_bytes_of_utf8_char_before( const char* data , size_type current_byte_index ) const
{
	if( current_byte_index >= this->buffer_len || !this->requires_unicode() || current_byte_index < 1 )
		return 1;
	
	data += current_byte_index;
	
	// If we know the utf8 string is misformatted, we have to check, how many 
	if( this->misformatted )
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



unsigned char utf8_string::get_num_bytes_of_utf8_char( const char* data , size_type first_byte_index ) const
{
	if( this->buffer_len <= first_byte_index || !this->requires_unicode() )
		return 1;
	
	data += first_byte_index;
	unsigned char first_byte =  static_cast<unsigned char>(data[0]);
	
	// If we know the utf8 string is misformatted, we have to check, how many 
	if( this->misformatted )
	{
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
	}
	else
	{	
		// Only Check the possibilities, that could appear
		switch( this->buffer_len - first_byte_index )
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
				this->misformatted = true;
			case 1:
			case 0:
				break;
		}
	}
	return 1; // one byte
}



unsigned char utf8_string::decode_utf8( const char* data , value_type& dest ) const
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
		misformatted = true;
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
	char bom[4] = { char(0xEF) , char(0xBB) , char(0xBF) , 0 };
	
	if( !this->buffer )
		return std::string( bom );
	
	char* tmp_buffer = new char[this->buffer_len + 3];
	
	tmp_buffer[0] = bom[0];
	tmp_buffer[1] = bom[1];
	tmp_buffer[2] = bom[2];
	
	std::memcpy( tmp_buffer + 3 , this->buffer , this->buffer_len );
	
	std::string result = std::string( tmp_buffer );
	
	delete[] tmp_buffer;
	
	return move(result);
}


utf8_string::size_type utf8_string::get_num_required_bytes( const value_type* lit , size_type& num_multibytes )
{
	size_type num_bytes = 0;
	num_multibytes = 0;
	for( size_type i = 0 ; lit[i] ; i++ ){
		unsigned char cur_bytes = get_num_bytes_of_utf8_codepoint( lit[i] );
		if( cur_bytes > 1 )
			num_multibytes++;
		num_bytes += cur_bytes;
	}
	return num_bytes;
}

utf8_string::size_type utf8_string::get_num_resulting_codepoints( size_type byte_start , size_type byte_count ) const 
{
	if( empty() )
		return 0;
	
	size_type	cur_index = byte_start;
	size_type	codepoint_count = 0;
	
	byte_count = std::min<difference_type>( byte_count , size() - byte_start );
	
	while( byte_count > 0 ){
		unsigned char num_bytes = get_num_bytes_of_utf8_char( this->buffer , cur_index );
		cur_index += num_bytes;
		byte_count -= num_bytes;
		codepoint_count++;
	}
	return codepoint_count;
}

utf8_string::size_type utf8_string::get_num_resulting_bytes( size_type byte_start , size_type codepoint_count ) const 
{
	if( empty() )
		return 0;
	
	if( !requires_unicode() )
		return std::min<size_type>( byte_start + codepoint_count , size() ) - byte_start;
	
	size_type cur_byte = byte_start;
	
	while( cur_byte < size() && codepoint_count-- > 0 )
		cur_byte += get_index_bytes( cur_byte );
	
	return cur_byte - byte_start;
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




utf8_string::size_type utf8_string::get_actual_index( size_type requested_index ) const
{
	if( requested_index >= this->string_len )
		return this->buffer_len - 1;
	
	size_type index_multibyte_table = 0;
	size_type current_overhead = 0;
	
	while( index_multibyte_table < this->indices_len )
	{
		size_type multibyte_pos = this->indices_of_multibyte[index_multibyte_table];
		
		if( multibyte_pos >= requested_index + current_overhead )
			break;
		
		index_multibyte_table++;
		current_overhead += get_num_bytes_of_utf8_char( this->buffer , multibyte_pos ) - 1; // Ad bytes of multibyte to overhead
	}
	
	return requested_index + current_overhead;
}

utf8_string utf8_string::raw_substr( size_type byte_index , size_type tmp_byte_count , size_type num_codepoints ) const
{
	if( byte_index > size() )
		return utf8_string();
	
	difference_type byte_count;
	if( tmp_byte_count < size() - byte_index )
		byte_count = tmp_byte_count;
	else
		byte_count = size() - byte_index;
	
	size_type	actual_start_index = byte_index;
	size_type	actual_end_index = byte_index + byte_count;
	
	//
	// Manage indices
	//
	size_type	start_of_multibytes_in_range;
	
	// Look for start of indices
	size_type tmp = 0;
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actual_start_index )
		tmp++;
	start_of_multibytes_in_range = tmp;
	
	// Look for the end
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actual_end_index )
		tmp++;
	
	// Compute number of indices 
	size_type	indices_len = tmp - start_of_multibytes_in_range;
	size_type*	new_indices = indices_len ? new size_type[indices_len] : nullptr;
	
	// Copy indices
	for( size_type i = 0 ; i < indices_len ; i++ )
		new_indices[i] = this->indices_of_multibyte[start_of_multibytes_in_range + i] - actual_start_index;
	//
	// Manage utf8 string
	//
	
	// Allocate new buffer
	char*	new_buffer = new char[byte_count+1];
	
	// Partly copy
	std::memcpy( new_buffer , this->buffer + actual_start_index , byte_count );
	
	new_buffer[byte_count] = 0;
	byte_count++;
	
	return utf8_string( new_buffer , byte_count , num_codepoints , new_indices , indices_len );
}


void utf8_string::raw_replace( size_type actual_start_index , size_type replaced_bytes , const utf8_string& replacement )
{
	if( actual_start_index > size() )
		actual_start_index = size();
	
	size_type		replacement_bytes = replacement.size();
	size_type		replacement_indices = replacement.indices_len;
	difference_type	byte_difference = replaced_bytes - replacement_bytes;
	size_type		actual_end_index;
	
	if( replaced_bytes < size() - actual_start_index )
		actual_end_index = actual_start_index + replaced_bytes;
	else{
		actual_end_index = size();
		replaced_bytes = actual_end_index - actual_start_index;
	}
	
	//
	// Manage indices
	//
	
	size_type	start_of_multibytes_in_range;
	size_type	number_of_multibytes_in_range;
	
	// Look for start of indices
	size_type tmp = 0;
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actual_start_index )
		tmp++;
	start_of_multibytes_in_range = tmp;
	
	// Look for the end
	while( tmp < this->indices_len && this->indices_of_multibyte[tmp] < actual_end_index )
		tmp++;
	
	// Compute number of indices 
	number_of_multibytes_in_range = tmp - start_of_multibytes_in_range;
	
	// Compute difference in number of indices
	difference_type indices_difference = number_of_multibytes_in_range - replacement_indices;
	
	// Create new buffer
	size_type indices_len = this->indices_len - indices_difference;
	
	if( indices_len )
	{
		size_type*	new_indices = indices_len ? new size_type[ indices_len ] : nullptr;
		
		// Copy values before replacement start
		for( size_type i = 0 ; i < this->indices_len && this->indices_of_multibyte[i] < actual_start_index ; i++ )
			new_indices[i] = this->indices_of_multibyte[i];
		
		// Copy the values after the replacement
		if( indices_difference != 0 ){
			for(
				size_type i = start_of_multibytes_in_range
				; i + number_of_multibytes_in_range < this->indices_len
				; i++
			)
				new_indices[ i + replacement_indices ] = this->indices_of_multibyte[ i + number_of_multibytes_in_range ] - byte_difference;
		}
		else{
			size_type i = start_of_multibytes_in_range + number_of_multibytes_in_range;
			while( i < this->indices_len ){
				new_indices[i] = this->indices_of_multibyte[i] - byte_difference;
				i++;
			}
		}
		
		// Insert indices from replacement
		for( size_type i = 0 ; i < replacement_indices ; i++ )
			new_indices[ start_of_multibytes_in_range + i ] = replacement.indices_of_multibyte[i] + actual_start_index;
		
		// Move and reset
		reset_indices( new_indices , indices_len );
	}
	else
		// Reset because empty
		reset_indices();
	
	//
	// Manage utf8 data
	//
	
	// Allocate new buffer
	size_type normalized_buffer_len = std::max<size_type>( this->buffer_len , 1 );
	size_type new_buffer_len = normalized_buffer_len + ( replacement_bytes - replaced_bytes );
	
	if( new_buffer_len > 1 )
	{
		char* new_buffer = new char[new_buffer_len];
		
		// Partly copy
		// Copy string until replacement index
		std::memcpy( new_buffer , this->buffer , actual_start_index );
		
		// Copy rest
		std::memcpy(
			new_buffer + actual_start_index + replacement_bytes
			, this->buffer + actual_end_index
			, normalized_buffer_len - actual_end_index - 1 // ('-1' for skipping the trailling zero which is not present in case the buffer_len was 0)
		);
		
		// Set trailling zero
		new_buffer[ new_buffer_len - 1 ] = '\0';
		
		// Write bytes
		std::memcpy( new_buffer + actual_start_index , replacement.buffer , replacement_bytes );
		
		// Adjust string length
		this->string_len -= get_num_resulting_codepoints( actual_start_index , replaced_bytes );
		this->string_len += replacement.length();
		
		// Rewrite buffer
		reset_buffer( new_buffer , new_buffer_len );
		
		this->misformatted = this->misformatted || replacement.misformatted;
	}
	else
		// Reset because empty
		clear();
}




utf8_string::value_type utf8_string::at( size_type requested_index ) const
{
	if( requested_index >= size() )
		return 0;
	
	if( !requires_unicode() )
		return (value_type) this->buffer[requested_index];
	
	// Decode internal buffer at position n
	value_type codepoint = 0;
	decode_utf8( this->buffer + get_actual_index( requested_index ) , codepoint );
	
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
		source += decode_utf8( source , *tempDest++ );
	
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

utf8_string::size_type utf8_string::raw_find( value_type ch , size_type byte_start ) const
{
	for( size_type it = byte_start ; it < size() ; it += get_index_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_rfind( value_type ch , size_type byte_start ) const
{
	if( byte_start >= size() )
		byte_start = back_index();
	
	for( difference_type it = byte_start ; it >= 0 ; it -= get_index_pre_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_find_first_of( const value_type* str , size_type byte_start ) const
{
	if( byte_start >= size() )
		return utf8_string::npos;
	
	for( size_type it = byte_start ; it < size() ; it += get_index_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_find_last_of( const value_type* str , size_type byte_start ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( byte_start >= size() )
		byte_start = back_index();
	
	for( difference_type it = byte_start ; it >= 0 ; it -= get_index_pre_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_find_first_not_of( const value_type* str , size_type byte_start ) const
{
	if( byte_start >= size() )
		return utf8_string::npos;
	
	for( size_type it = byte_start ; it < size() ; it += get_index_bytes( it ) )
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

utf8_string::size_type utf8_string::raw_find_last_not_of( const value_type* str , size_type byte_start ) const
{
	if( empty() )
		return utf8_string::npos;
	
	if( byte_start >= size() )
		byte_start = back_index();
	
	for( difference_type it = byte_start ; it >= 0 ; it -= get_index_pre_bytes( it ) )
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
	utf8_string::size_type minIndex = std::min( lhs.get_index() , rhs.get_index() );
	utf8_string::size_type max_index = std::max( lhs.get_index() , rhs.get_index() );
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
