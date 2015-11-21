#include <wstring.h>

wstring::wstring( size_t number , wchar_t ch ) :
	stringLen( number )
	, indicesOfMultibyte( ch <= 0x7F ? nullptr : new size_t[number] )
	, indicesLen( ch <= 0x7F ? 0 : number )
{
	unsigned char numBytes = getNumBytesOfUTF8Codepoint( ch );
	
	this->bufferLen = ( ch ? number * numBytes : 0 ) + 1;
	this->buffer = new char[this->bufferLen];
	
	if( numBytes == 1 )
		while( number-- > 0 )
			this->buffer[number] = ch;
	else
	{
		char representation[6];
		encodeUTF8( ch , representation , this->misFormated );
		
		while( number-- > 0 )
		{
			unsigned char byte = numBytes;
			while( byte-- > 0 )
				this->buffer[ number * numBytes + byte ] = representation[byte];
			this->indicesOfMultibyte[number] = number * numBytes;
		}
	}
	
	// Set trailling zero
	this->buffer[this->bufferLen-1] = 0;
}

wstring::wstring( const char* str , size_t len ) :
	stringLen( 0 )
	, indicesOfMultibyte( nullptr )
	, misFormated( false )
{
	if( str )
	{
		size_t			numMultibytes = 0;
		unsigned char	bytesToSkip = 0;
		size_t	i = 0;
		
		// Count Multibytes
		for( ; str[i] && this->stringLen < len ; i++ )
		{
			stringLen++; // Count number of codepoints
			
			if( str[i] <= 0x7F ) // lead bit is zero, must be a single ascii
				continue;
			
			numMultibytes++; // Increase counter
			
			// Compute the number of bytes to skip
			bytesToSkip = getNumBytesOfUTF8Char( str[i] ) - 1;
			
			// Check if there is no valid starting byte sequence
			if( !bytesToSkip ){
				this->misFormated = true;
				continue;
			}
			
			// Check if the string doesn't end just right inside the multibyte part!
			do{
				if( !str[i+1] || (str[i+1] & 0xC0) != 0x80 )
					break;
				i++;
			}while( --bytesToSkip > 0 );
		}
		
		if( bytesToSkip != 0 ) // Codepoint suddently ended!
			this->misFormated = true;
		
		// Initialize buffer
		this->bufferLen = i + 1;
		this->buffer = new char[this->bufferLen];
		memcpy( this->buffer , str , this->bufferLen - 1 );
		this->buffer[bufferLen-1] = 0;
		
		// initialize indices table
		this->indicesLen = numMultibytes;
		this->indicesOfMultibyte = this->indicesLen ? new size_t[this->indicesLen] : nullptr;
		
		size_t multibyteIndex = 0;
		
		// Fill Multibyte Table
		for( size_t index = 0 ; index < i ; index++ )
		{
			if( buffer[index] <= 0x7F ) // lead bit is zero, must be a single ascii
				continue;
			
			this->indicesOfMultibyte[multibyteIndex++] = index;
			
			// Compute number of bytes to skip
			bytesToSkip = getNumBytesOfUTF8Char( buffer[index] ) - 1;
			
			if( !bytesToSkip )
				continue;
			
			if( this->misFormated ){
				do{
					if( index + 1 == i || (buffer[index+1] & 0xC0) != 0x80 )
						break;
					index++;
				}while( --bytesToSkip > 0 );
			}
			else
				index += bytesToSkip;
		}
	}
	else{
		this->buffer = nullptr;
		this->bufferLen = 0;
		this->indicesLen = 0;
	}
}

wstring::wstring( const wchar_t* str ) :
	indicesOfMultibyte( nullptr )
	, indicesLen( 0 )
	, misFormated( false )
{
	if( str )
	{
		size_t	numMultibytes;
		size_t	nextIndexPosition = 0;
		size_t	stringLen = 0;
		size_t	numBytes = getNumberOfRequiredBytes( str , numMultibytes );
		
		// Initialize the internal buffer
		this->bufferLen = numBytes + 1; // +1 for the trailling zero
		this->buffer = new char[this->bufferLen];
		this->indicesLen = numMultibytes;
		this->indicesOfMultibyte = this->indicesLen ? new size_t[this->indicesLen] : nullptr;
		
		char*	curWriter = this->buffer;
		
		// Iterate through wide char literal
		for( size_t i = 0; str[i] ; i++ )
		{
			// Encode wide char to utf8
			unsigned char numBytes = encodeUTF8( str[i] , curWriter , misFormated );
			
			// Push position of character to 'indicesOfMultibyte'
			if( numBytes > 1 )
				this->indicesOfMultibyte[nextIndexPosition++] = curWriter - this->buffer;
			
			// Step forward with copying to internal utf8 buffer
			curWriter += numBytes;
			
			// Increase string length by 1
			stringLen++;
		}
		
		this->stringLen = stringLen;
		
		// Add trailling \0 char to utf8 buffer
		*curWriter = 0;
	}
	else{
		this->buffer = nullptr;
		this->bufferLen = 0;
	}
}




wstring::wstring( wstring&& str ) :
	buffer( str.buffer )
	, bufferLen( str.bufferLen )
	, stringLen( str.stringLen )
	, indicesOfMultibyte( str.indicesOfMultibyte )
	, indicesLen( str.indicesLen )
	, misFormated( str.misFormated )
{
	// Reset old string
	str.buffer = nullptr;
	str.indicesOfMultibyte = nullptr;
	str.indicesLen = 0;
	str.bufferLen = 0;
	str.stringLen = 0;
	str.misFormated = true;
}

wstring::wstring( const wstring& str ) :
	buffer( new char[str.bufferLen] )
	, bufferLen( str.bufferLen )
	, stringLen( str.stringLen )
	, indicesOfMultibyte( str.indicesLen ? new size_t[str.indicesLen] : nullptr )
	, indicesLen( str.indicesLen )
	, misFormated( str.misFormated )
{
	// Clone data
	memcpy( this->buffer , str.buffer , str.bufferLen );
	
	// Clone indices
	if( str.indicesLen > 0 )
		memcpy( this->indicesOfMultibyte , str.indicesOfMultibyte , str.indicesLen * sizeof(size_t) );
}




wstring& wstring::operator=( const wstring& str )
{
	char*	newBuffer = new char[str.bufferLen];
	size_t*	newIndices = str.indicesLen ? new size_t[str.indicesLen] : nullptr;
	
	// Clone data
	memcpy( newBuffer , str.buffer , str.bufferLen );
	
	// Clone indices
	if( str.indicesLen > 0 )
		memcpy( newIndices , str.indicesOfMultibyte , str.indicesLen * sizeof(size_t) );
	
	this->misFormated = str.misFormated;
	this->stringLen = str.stringLen;
	resetBuffer( newBuffer , str.bufferLen );
	reset_indices( newIndices , str.indicesLen );
	return *this;
}

wstring& wstring::operator=( wstring&& str )
{
	this->misFormated = str.misFormated;
	this->stringLen = str.stringLen;
	
	// Copy data
	resetBuffer( str.buffer , str.bufferLen );
	reset_indices( str.indicesOfMultibyte , str.indicesLen );
	
	// Reset old string
	str.indicesOfMultibyte = nullptr;
	str.indicesLen = 0;
	str.buffer = nullptr;
	str.bufferLen = 0;
	str.misFormated = true;
	str.stringLen = 0;
	return *this;
}




unsigned char wstring::decodeUTF8( const char* data , wchar_t& dest , bool& hasError )
{
	unsigned char firstChar = *data;
	
	if( !firstChar ){
		dest = 0;
		return 1;
	}
	
	wchar_t			codepoint;
	unsigned char	numBytes;
	
	if( firstChar <= 0x7F ){ // 0XXX XXXX one byte
		dest = firstChar;
		return 1;
	}
	else if( (firstChar & 0xE0) == 0xC0 ){  // 110X XXXX  two bytes
		codepoint = firstChar & 31;
		numBytes = 2;
	}
	else if( (firstChar & 0xF0) == 0xE0 ){  // 1110 XXXX  three bytes
		codepoint = firstChar & 15;
		numBytes = 3;
	}
	else if( (firstChar & 0xF8) == 0xF0 ){  // 1111 0XXX  four bytes
		codepoint = firstChar & 7;
		numBytes = 4;
	}
	else if( (firstChar & 0xFC) == 0xF8 ){  // 1111 10XX  five bytes
		codepoint = firstChar & 3;
		numBytes = 5;
		hasError = true;
	}
	else if( (firstChar & 0xFE) == 0xFC ){  // 1111 110X  six bytes
		codepoint = firstChar & 1;
		numBytes = 6;
		hasError = true;
	}
	else{ // not a valid first byte of a UTF-8 sequence
		codepoint = firstChar;
		numBytes = 1;
		hasError = true;
	}
	
	for( int i = 1 ; i < numBytes ; i++ )
	{
		if( !data[i] || (data[i] & 0xC0) != 0x80 ){
			hasError = true;
			numBytes = i;
			break;
		}
		codepoint = (codepoint << 6) | ( data[i] & 0x3F );
	}
	
	dest = codepoint;
	
	return numBytes;
}




size_t wstring::getNumberOfRequiredBytes( const wchar_t* lit , size_t& numMultibytes )
{
	size_t numBytes = 0;
	numMultibytes = 0;
	for( size_t i = 0 ; lit[i] ; i++ ){
		unsigned char curBytes = getNumBytesOfUTF8Codepoint( lit[i] );
		if( curBytes > 1 )
			numMultibytes++;
		numBytes += curBytes;
	}
	return numBytes;
}

size_t wstring::getNumberOfResultingCodepoints( size_t byteStart , size_t byteCount ) const 
{
	size_t	curIndex = byteStart;
	size_t	codepointCount = 0;
	
	byteCount = std::min<ptrdiff_t>( byteCount , size() - byteStart );
	
	while( byteCount > 0 ){
		unsigned char numBytes = getNumBytesOfUTF8Char( this->buffer[curIndex] );
		curIndex += numBytes;
		byteCount -= numBytes;
		codepointCount++;
	}
	return codepointCount;
}

size_t wstring::getNumberOfResultingBytes( size_t byteStart , size_t codepointCount ) const 
{
	size_t curByte = byteStart;
	
	while( curByte < size() && codepointCount-- > 0 )
		curByte += getIndexBytes( curByte );
	
	return curByte - byteStart;
}




unsigned char wstring::encodeUTF8( wchar_t codepoint , char* dest , bool& hasError )
{
	if( !dest )
		return 1;
	
	unsigned char numBytes = 0;
	
	if( codepoint <= 0x7F ){ // 0XXXXXXX one byte
		dest[0] = char(codepoint);
		numBytes = 1;
	}
	else if( codepoint <= 0x7FF ){ // 110XXXXX  two bytes
		dest[0] = char( 0xC0 | (codepoint >> 6) );
		dest[1] = char( 0x80 | (codepoint & 0x3F) );
		numBytes = 2;
	}
	else if( codepoint <= 0xFFFF ){ // 1110XXXX  three bytes
		dest[0] = char( 0xE0 | (codepoint >> 12) );
		dest[1] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[2] = char( 0x80 | (codepoint & 0x3F) );
		numBytes = 3;
	}
	else if( codepoint <= 0x1FFFFF ){ // 11110XXX  four bytes
		dest[0] = char( 0xF0 | (codepoint >> 18) );
		dest[1] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[3] = char( 0x80 | (codepoint & 0x3F) );
		numBytes = 4;
	}
	else if( codepoint <= 0x3FFFFFF ){ // 111110XX  five bytes
		dest[0] = char( 0xF8 | (codepoint >> 24) );
		dest[1] = char( 0x80 | (codepoint >> 18) );
		dest[2] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[3] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[4] = char( 0x80 | (codepoint & 0x3F) );
		numBytes = 5;
		hasError = true;
	}
	else if( codepoint <= 0x7FFFFFFF ){ // 1111110X  six bytes
		dest[0] = char( 0xFC | (codepoint >> 30) );
		dest[1] = char( 0x80 | ((codepoint >> 24) & 0x3F) );
		dest[2] = char( 0x80 | ((codepoint >> 18) & 0x3F) );
		dest[3] = char( 0x80 | ((codepoint >> 12) & 0x3F) );
		dest[4] = char( 0x80 | ((codepoint >> 6) & 0x3F) );
		dest[5] = char( 0x80 | (codepoint & 0x3F) );
		numBytes = 6;
		hasError = true;
	}
	else
		hasError = true;
	
	//printf(" [%x] -> %db (0x%x)\n",codepoint,numBytes,dest[0]);
	
	return numBytes;
}




size_t wstring::getActualIndex( size_t requestedIndex ) const
{
	size_t indexMultibyteTable = 0;
	size_t currentOverhead = 0;
	
	while( indexMultibyteTable < this->indicesLen )
	{
		size_t multibytePos = this->indicesOfMultibyte[indexMultibyteTable];
		
		if( multibytePos >= requestedIndex + currentOverhead )
			break;
		
		indexMultibyteTable++;
		currentOverhead += getNumBytesOfUTF8Char( this->buffer[multibytePos] ) - 1; // Ad bytes of multibyte to overhead
	}
	
	return requestedIndex + currentOverhead;
}

wstring wstring::raw_substr( size_t byteIndex , size_t tmpByteCount , size_t numCodepoints ) const
{
	ptrdiff_t byteCount;
	if( tmpByteCount < size() - byteIndex )
		byteCount = tmpByteCount;
	else
		byteCount = size() - byteIndex;
	
	if( byteCount <= 0 )
		return wstring();
	
	size_t	actualStartIndex = byteIndex;
	size_t	actualEndIndex = byteIndex + byteCount;
	
	//
	// Manage indices
	//
	size_t	startOfMultibytesInRange;
	
	// Look for start of indices
	size_t tmp = 0;
	while( tmp < this->indicesLen && this->indicesOfMultibyte[tmp] < actualStartIndex )
		tmp++;
	startOfMultibytesInRange = tmp;
	
	// Look for the end
	while( tmp < this->indicesLen && this->indicesOfMultibyte[tmp] < actualEndIndex )
		tmp++;
	
	// Compute number of indices 
	size_t	newIndicesLen = tmp - startOfMultibytesInRange;
	size_t*	newIndices = newIndicesLen ? new size_t[newIndicesLen] : nullptr;
	
	// Copy indices
	for( size_t i = 0 ; i < newIndicesLen ; i++ )
		newIndices[i] = this->indicesOfMultibyte[startOfMultibytesInRange + i] - actualStartIndex;
	//
	// Manage utf8 string
	//
	
	// Allocate new buffer
	char*	newBuffer = new char[byteCount+1];
	
	// Partly copy
	memcpy( newBuffer , this->buffer + actualStartIndex , byteCount );
	
	newBuffer[byteCount] = 0;
	byteCount++;
	
	return wstring( newBuffer , byteCount , numCodepoints , newIndices , newIndicesLen );
}




void wstring::raw_replace( size_t actualStartIndex , size_t replacedBytes , const wstring& replacement )
{
	size_t	replacementBytes = replacement.size();
	size_t	replacementIndices = replacement.indicesLen;
	ptrdiff_t	byteDifference = replacedBytes - replacementBytes;
	size_t	actualEndIndex;
	
	if( replacedBytes < size() - actualStartIndex )
		actualEndIndex = actualStartIndex + replacedBytes;
	else{
		actualEndIndex = size();
		replacedBytes = actualEndIndex - actualStartIndex;
	}
	
	//
	// Manage indices
	//
	size_t	startOfMultibytesInRange;
	size_t	numberOfMultibytesInRange;
	
	// Look for start of indices
	size_t tmp = 0;
	while( tmp < this->indicesLen && this->indicesOfMultibyte[tmp] < actualStartIndex )
		tmp++;
	startOfMultibytesInRange = tmp;
	
	// Look for the end
	while( tmp < this->indicesLen && this->indicesOfMultibyte[tmp] < actualEndIndex )
		tmp++;
	
	// Compute number of indices 
	numberOfMultibytesInRange = tmp - startOfMultibytesInRange;
	
	// Compute difference in number of indices
	ptrdiff_t	indicesDifference = numberOfMultibytesInRange - replacementIndices;
	
	// Create new buffer
	size_t	newIndicesLen = this->indicesLen - indicesDifference;
	size_t*	newIndices = newIndicesLen ? new size_t[ newIndicesLen ] : nullptr;
	
	
	// Copy values before replacement start
	for( size_t i = 0 ; i < this->indicesLen && this->indicesOfMultibyte[i] < actualStartIndex ; i++ )
		newIndices[i] = this->indicesOfMultibyte[i];
	
	// Copy the values after the replacement
	if( indicesDifference != 0 ){
		for(
			size_t i = startOfMultibytesInRange
			; i + numberOfMultibytesInRange < this->indicesLen
			; i++
		)
			newIndices[ i + replacementIndices ] = this->indicesOfMultibyte[ i + numberOfMultibytesInRange ] - byteDifference;
	}
	else{
		size_t i = startOfMultibytesInRange + numberOfMultibytesInRange;
		while( i < this->indicesLen ){
			newIndices[i] = this->indicesOfMultibyte[i] - byteDifference;
			i++;
		}
	}
	
	// Insert indices from replacement
	for( size_t i = 0 ; i < replacementIndices ; i++ )
		newIndices[ startOfMultibytesInRange + i ] = replacement.indicesOfMultibyte[i] + actualStartIndex;
	
	// Move and reset
	reset_indices( newIndices , newIndicesLen );
	
	
	
	//
	// Manage utf8 string
	//
	
	// Allocate new buffer
	size_t	newBufferLen = this->bufferLen + ( replacementBytes - replacedBytes );
	char*	newBuffer = new char[newBufferLen];
	
	// Partly copy
	// Copy string until replacement index
	memcpy( newBuffer , this->buffer , actualStartIndex );
	
	// Copy rest
	memcpy( newBuffer + actualStartIndex + replacementBytes , this->buffer + actualEndIndex , this->bufferLen - actualEndIndex );
	
	// Write bytes
	memcpy( newBuffer + actualStartIndex , replacement.buffer , replacementBytes );
	
	// Adjust string length
	this->stringLen -= getNumberOfResultingCodepoints( actualStartIndex , replacedBytes );
	this->stringLen += replacement.length();
	
	// Rewrite buffer
	resetBuffer( newBuffer , newBufferLen );
	
	//for( int i = 0 ; i < this->indicesLen ; i++ )
	//	printf("%d, ",this->indicesOfMultibyte[i] );
	//printf("\n\n");
}




wchar_t wstring::at( size_t requestedIndex ) const
{
	if( !requiresUnicode() )
		return (wchar_t) this->buffer[requestedIndex];
	
	// Decode internal buffer at position n
	wchar_t codepoint = 0;
	decodeUTF8( this->buffer + getActualIndex( requestedIndex ) , codepoint , this->misFormated );
	
	return codepoint;
}




std::unique_ptr<wchar_t[]> wstring::toWideLiteral() const
{
	std::unique_ptr<wchar_t[]>	dest = std::unique_ptr<wchar_t[]>( new wchar_t[length()] );
	wchar_t*					tempDest = dest.get();
	char*						source = this->buffer;
	
	while( *source && !this->misFormated )
		source += decodeUTF8( source , *tempDest++ , this->misFormated );
	
	return std::move(dest);
}




ptrdiff_t wstring::compare( const wstring& str ) const
{
	wstring_const_iterator	it1 = cbegin();
	wstring_const_iterator	it2 = str.cbegin();
	wstring_const_iterator	end1 = cend();
	wstring_const_iterator	end2 = str.cend();
	
	while( it1 != end1 && it2 != end2 ){
		ptrdiff_t diff = it2.getValue() - it1.getValue();
		if( diff )
			return diff;
		it1++;
		it2++;
	}
	return ( it1 == end1 ? 1 : 0 ) - ( it2 == end2 ? 1 : 0 );
}

bool wstring::equals( const char* str ) const
{
	const char* it1 = this->buffer;
	const char* it2 = str;
	
	while( *it1 && *it2 ){
		if( *it1 != *it2 )
			return false;
		it1++;
		it2++;
	}
	return *it1 == *it2;
}

bool wstring::equals( const wchar_t* str ) const
{
	wstring_const_iterator	it = cbegin();
	wstring_const_iterator	end = cend();
	
	while( it != end && *str ){
		if( *str != it.getValue() )
			return false;
		it++;
		str++;
	}
	return *it == *str;
}




size_t wstring::find( wchar_t ch , size_t startPos ) const
{
	for( wstring::iterator it = get(startPos) ; it < end() ; it++ )
		if( *it == ch )
			return it - begin();
	return wstring::npos;
}

size_t wstring::raw_find( wchar_t ch , size_t byteStart ) const
{
	for( size_t it = byteStart ; it < size() ; it += getIndexBytes( it ) )
		if( raw_at(it) == ch )
			return it;
	return wstring::npos;
}




size_t wstring::rfind( wchar_t ch , size_t startPos ) const
{
	wstring::reverse_iterator it = ( startPos >= length() ) ? rbegin() : rget( startPos );
	
	while( it < rend() )
	{
		if( *it == ch )
			return it - rbegin();
		it++;
	}
	return wstring::npos;
}

size_t wstring::raw_rfind( wchar_t ch , size_t byteStart ) const
{
	if( byteStart >= size() )
		byteStart = backIndex();
	
	for( ptrdiff_t it = byteStart ; it >= 0 ; it -= getIndexPreBytes( it ) )
		if( raw_at(it) == ch )
			return it;
	
	return wstring::npos;
}




size_t wstring::find_first_of( const wchar_t* str , size_t startPos ) const
{
	if( startPos >= length() )
		return wstring::npos;
	
	for( wstring::iterator it = get( startPos ) ; it < end() ; it++ )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = *it;
		do{
			if( cur == *tmp )
				return it - begin();
		}while( *++tmp );
	}
	
	return wstring::npos;
}

size_t wstring::raw_find_first_of( const wchar_t* str , size_t byteStart ) const
{
	for( size_t it = byteStart ; it < size() ; it += getIndexBytes( it ) )
	{
		const wchar_t*	tmp = str;
		do{
			if( raw_at(it) == *tmp )
				return it;
		}while( *++tmp );
	}
	
	return wstring::npos;
}




size_t wstring::find_last_of( const wchar_t* str , size_t startPos ) const
{
	wstring::reverse_iterator it = ( startPos >= length() ) ? rbegin() : rget( startPos );
	
	while( it < rend() )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = *it;
		do{
			if( cur == *tmp )
				return it - rbegin();
		}while( *++tmp );
		it++;
	}
	
	return wstring::npos;
}

size_t wstring::raw_find_last_of( const wchar_t* str , size_t byteStart ) const
{
	if( byteStart >= size() )
		byteStart = backIndex();
	
	for( ptrdiff_t it = byteStart ; it >= 0 ; it -= getIndexPreBytes( it ) )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = raw_at(it);
		do{
			if( cur == *tmp )
				return it;
		}while( *++tmp );
	}
	
	return wstring::npos;
}




size_t wstring::find_first_not_of( const wchar_t* str , size_t startPos ) const
{
	if( startPos >= length() )
		return wstring::npos;
	
	for( wstring::iterator it = get(startPos) ; it < end() ; it++ )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = *it;
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it - begin();
		
		continue2:;
	}
	
	return wstring::npos;
}

size_t wstring::raw_find_first_not_of( const wchar_t* str , size_t byteStart ) const
{
	if( byteStart >= size() )
		return wstring::npos;
	
	for( size_t it = byteStart ; it < size() ; it += getIndexBytes( it ) )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = raw_at(it);
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it;
		
		continue2:;
	}
	
	return wstring::npos;
}




size_t wstring::find_last_not_of( const wchar_t* str , size_t startPos ) const
{
	wstring::reverse_iterator it = ( startPos >= length() ) ? rbegin() : rget( startPos );
	
	while( it < rend() )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = *it;
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it - rbegin();
		
		continue2:
		it++;
	}
	
	return wstring::npos;
}

size_t wstring::raw_find_last_not_of( const wchar_t* str , size_t byteStart ) const
{
	if( byteStart >= size() )
		byteStart = backIndex();
	
	for( ptrdiff_t it = byteStart ; it >= 0 ; it -= getIndexPreBytes( it ) )
	{
		const wchar_t*	tmp = str;
		wchar_t		cur = raw_at(it);
		
		do{
			if( cur == *tmp )
				goto continue2;
		}while( *++tmp );
		
		return it;
		
		continue2:;
	}
	
	return wstring::npos;
}




int operator-( const wstring::iterator& left , const wstring::iterator& right )
{
	size_t minIndex = std::min( left.raw_index , right.raw_index );
	size_t maxIndex = std::max( left.raw_index , right.raw_index );
	size_t numCodepoints = left.instance.getNumberOfResultingCodepoints( minIndex , maxIndex - minIndex );
	return maxIndex == left.raw_index ? numCodepoints : -numCodepoints;
}

wstring::iterator operator+( const wstring::iterator& it , size_t nth )
{
	if( !it.instance.requiresUnicode() )
		return wstring::iterator( it.raw_index + nth , it.instance );
	return wstring::iterator( it.raw_index + it.instance.getNumberOfResultingBytes( it.raw_index , nth ) , it.instance );
}

int operator-( const wstring::reverse_iterator& left , const wstring::reverse_iterator& right )
{
	ptrdiff_t minIndex = std::min( left.raw_index , right.raw_index );
	ptrdiff_t maxIndex = std::max( left.raw_index , right.raw_index );
	size_t numCodepoints = left.instance.getNumberOfResultingCodepoints( minIndex , maxIndex - minIndex );
	return maxIndex == right.raw_index ? numCodepoints : -numCodepoints;
}

wstring::reverse_iterator operator+( const wstring::reverse_iterator& it , size_t nth )
{
	ptrdiff_t newIndex = it.raw_index;
	while( nth-- > 0 )
		newIndex -= it.instance.getIndexPreBytes( newIndex );
	return wstring::reverse_iterator( newIndex , it.instance );
}