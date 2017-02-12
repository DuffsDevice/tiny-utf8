#include <iostream>
#include <algorithm>
#include <fstream>
#include <string>
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
		fout << "MisFormatted: " << str.is_misformatted() << endl;
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
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << endl;
	}
	
	// Test 3
	{
		fout << "Test 3: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World ♫" );
		
		fout << "String: " << str << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		
		fout << "Iterative output: ";
		
		std::for_each( str.begin() , str.end()
			, [&fout]( char32_t cp ){
				if( cp <= 127 )
					fout << char(cp);
				else
					fout << "U\\" << int(cp);
			}
		);
		
		fout << endl << "Reverse iterative output: ";
		
		std::for_each( str.rbegin() , str.rend()
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
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		
		str[6] = '~';
		
		fout << "Replaced codepoint 6 with ~: " << str << endl;
		
		str.replace( 5 , 3 , " " );
		
		fout << "Replaced codepoints 5-7 with ' ': " << str << endl;
		
		str.insert( str.begin() , utf8_string(U"ツ ") );
		
		fout << "Inserted at the start 'ツ ': " << str << endl << endl;
	}
	
	// Test 5
	{
		fout << "Test 5: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World ♫" );
		
		fout << "String: " << str << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		
		str.erase( 14 );
		
		fout << "Erased codepoint 14: " << str << endl;
		
		str.erase( str.begin() , str.begin() + 7 );
		
		fout << "Erased codepoints 0 to 7: " << str << endl << endl;
	}
	
	// Test 6
	{
		fout << "Test 6: " << endl << "-------" << endl;
		
		std::ifstream in = ifstream( "input.txt" , ios::binary );
		
		utf8_string str = std::string( (std::istreambuf_iterator<char>(in)) , std::istreambuf_iterator<char>() );
		
		fout << "String (ANSI): " << str << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		
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
		
		std::for_each( str.rbegin() , str.rend()
			, [&fout]( char32_t cp ){
				if( cp <= 127 )
					fout << char(cp);
				else
					fout << "U\\" << int(cp);
			}
		);
		
		fout << endl;
		
		utf8_string corrected{str.wide_literal().get()};
		
		fout << "Converted to UTF-8: " << corrected << endl;
		
		fout << endl;
	}
	
	
	// Test 7
	{
		fout << "Test 7: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello World ツ♫" );
		
		fout << "Find Last Not of ツ:" << str.find_last_not_of( U"ツ♫" ) << endl;
		fout << "Correct Result: " << string("Hello World *+") .find_last_not_of( "*+" ) << endl;
		
		fout << "Find Last of ツ:" << str.find_last_of( U"e" ) << endl;
		fout << "Correct Result: " << string("Hello World *+") .find_last_of( "e" ) << endl;
		
		fout << "Find l:" << str.find( U'l' ) << endl;
		fout << "Correct Result: " << string("Hello World *+") .find( 'l' ) << endl;
		
		fout << "RFind l:" << str.rfind( U'l' ) << endl;
		fout << "Correct Result: " << string("Hello World *+") .rfind( 'l' ) << endl;
	}
	
	return 0;
}