#include <iostream>
#include <algorithm>
#include <fstream>
#define _TINY_UTF8_H_USE_IOSTREAM_
#include <tinyutf8.h>

using namespace std;

int main(int argc, char ** argv)
{
	ofstream fout = ofstream( "test.txt" , std::ios::binary );
	
	// Prepend the UTF-8 Byte Order Mark
	fout << utf8_string().cpp_str(true);
	
	// Test 1
	{
		fout << "Test 1: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World" );
		
		fout << "String: " << str << endl;
		fout << "Length: " << str.length() << endl;
		fout << "Size (bytes): " << str.size() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << endl;
	}
	
	// Test 2
	{
		fout << "Test 2: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ" );
		str.append( U" ♫ World" );
		
		fout << "String: " << str << endl;
		fout << "Length: " << str.length() << endl;
		fout << "Size (bytes): " << str.size() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << endl;
	}
	
	// Test 3
	{
		fout << "Test 3: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World ♫" );
		
		fout << "String: " << str << endl;
		
		fout << "Iterative output: ";
		
		std::for_each( begin(str) , end(str)
			, [&fout]( char32_t cp ){
				if( cp <= 127 )
					fout << char(cp);
				else
					fout << "U\\" << int(cp);
			}
		);
		
		fout << endl << "Reverse iterative output: ";
		
		std::for_each( rbegin(str) , rend(str)
			, [&fout]( char32_t cp ){
				if( cp <= 127 )
					fout << char(cp);
				else
					fout << "U\\" << int(cp);
			}
		);
		
		fout << endl << endl;
	}
	
	// Test 4
	{
		fout << "Test 4: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World ♫" );
		
		fout << "String: " << str << endl;
		
		str[6] = '~';
		
		fout << "Replaced codepoint 6 with ~: " << str << endl;
		
		str.replace( 5 , 3 , " " );
		
		fout << "Replaced codepoints 5-7 with ' ': " << str << endl;
		
		str.insert( begin(str) , utf8_string(U"ツ ") );
		
		fout << "Inserted at the start 'ツ ': " << str << endl << endl;
	}
	
	// Test 4
	{
		fout << "Test 4: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World ♫" );
		
		fout << "String: " << str << endl;
		
		str.erase( 14 );
		
		fout << "Erased codepoint 14: " << str << endl;
		
		str.erase( str.begin() , str.begin() + 6 );
		
		fout << "Erased codepoints 0 to 6: " << str << endl << endl;
	}
	
	// Test 5
	{
		fout << "Test 5: " << endl << "-------" << endl;
		
		std::ifstream in = ifstream( "input.txt" , ios::binary );
		
		utf8_string str;
		
		in >> str;
		
		fout << "String (ANSI): " << str << endl;
		fout << "MisFormated: " << str.is_misformated() << endl;
		
		fout << "Iterative Output: ";
		
		std::for_each( begin(str) , end(str)
			, [&fout]( char32_t cp ){
				if( cp <= 127 )
					fout << char(cp);
				else
					fout << "U\\" << int(cp);
			}
		);
		
		fout << endl;
		
		fout << "Reverse Iterative Output: ";
		
		std::for_each( rbegin(str) , rend(str)
			, [&fout]( char32_t cp ){
				if( cp <= 127 )
					fout << char(cp);
				else
					fout << "U\\" << int(cp);
			}
		);
		
		fout << endl;
	}
	
	
	return 0;
}