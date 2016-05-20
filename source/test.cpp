#include <iostream>
#include <algorithm>
#include <fstream>
#define _TINY_UTF8_H_USE_IOSTREAM_
#include <tinyutf8.h>

using namespace std;

int main(int argc, char ** argv)
{
	std::ofstream fout = ofstream( "test.txt" , std::ios::binary );
	
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
	}
	
	return 0;
}