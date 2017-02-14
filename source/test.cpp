#include <iostream>
#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#define _TINY_UTF8_H_USE_IOSTREAM_
#include <tinyutf8.h>

using namespace std;


int main()
{
	ofstream fout = ofstream( "test.txt" , std::ios::binary );
	
	// Prepend the UTF-8 Byte Order Mark
	fout << utf8_string().cpp_str(true);
	
	// Test 1a
	{
		fout << "Test 1a: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"SSO: ツ♫" );
		
		fout << "String: " << str << endl;
		fout << "Length: " << str.length() << endl;
		fout << "Size (bytes): " << str.size() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "MaxSmallStringBytes: " << str.max_sso_bytes() << endl;
		fout << "7th codepoint: " << str[6] << endl;
		fout << endl;
	}
	
	// Test 1b
	{
		fout << "Test 1b: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( 1 , U'ツ' );
		
		fout << "String: " << str << endl;
		fout << "Length: " << str.length() << endl;
		fout << "Size (bytes): " << str.size() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "MaxSmallStringBytes: " << str.max_sso_bytes() << endl;
		fout << "7th codepoint: " << str[6] << endl;
		fout << endl;
	}
	
	// Test 1c
	{
		fout << "Test 1c: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello  ツ  World" );
		
		fout << "String: " << str << endl;
		fout << "Length: " << str.length() << endl;
		fout << "Size (bytes): " << str.size() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "9nd codepoint: " << (char)str[8] << endl;
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
		fout << "SSOActive: " << str.sso_active() << endl;
		
		str.append( U" and ♫ World" );
		
		
		fout << "Appended '♫ World': " << str << endl;
		fout << "Length: " << str.length() << endl;
		fout << "Size (bytes): " << str.size() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		
		fout << endl << endl;
	}
	
	// Test 3
	{
		fout << "Test 3: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World ♫" );
		
		fout << "String: " << str << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
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
		
		fout << endl << "Random access output: ";
		
		for( int i = 0 ; i < str.length() ; i++ )
		{
			char32_t cp = str[i];
			if( cp <= 127 )
				fout << char(cp);
			else
				fout << "U\\" << int(cp);
		}
		
		fout << endl << endl;
	}
	
	// Test 4
	{
		fout << "Test 4: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World" );
		
		fout << "String: " << str << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		
		str[6] = U'🤝';
		
		fout << "Replaced codepoint 6 with 🤝: " << str << endl;
		
		str.replace( 5 , 3 , " " );
		
		fout << "Replaced codepoints 5-7 with ' ': " << str << endl;
		
		str.replace( str.begin() + 4 , str.begin() + 7 , utf8_string(U"~ 🤝 ~") );
		
		fout << "Replaced the space and its neighbours with '🤝': " << str << endl ;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "NumMultibytes: " << str.get_num_multibytes() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl << endl;
	}
	
	// Test 5
	{
		fout << "Test 5: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World♫" );
		
		fout << "String: " << str << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		
		str.erase( 13 );
		
		fout << "Erased 14th codepoint: " << str << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		
		str.erase( str.begin() , str.begin() + 8 );
		
		fout << "Erased codepoints [0,8): " << str << endl << endl;
	}
	
	// Test 6
	{
		fout << "Test 6: " << endl << "-------" << endl;
		
		std::ifstream in = ifstream( "input.txt" , ios::binary );
		
		utf8_string str = std::string( (std::istreambuf_iterator<char>(in)) , std::istreambuf_iterator<char>() );
		
		fout << "String (ANSI): " << str << endl;
		fout << "RequiresUnicode: " << str.requires_unicode() << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
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
		
		fout << endl;
	}
	
	// Test 5
	{
		fout << "Test 8: " << endl << "-------" << endl;
		
		utf8_string fullstr = utf8_string( U"Hello ツ World rg rth rt he rh we gxgre" );
		
		fout << "Full String: " << fullstr << endl;
		
		utf8_string str = fullstr.substr( 3 , 16 );
		fout << "Substring 3[16]: " << str << endl;
		fout << "SSOActive: " << str.sso_active() << endl;
		fout << "MisFormatted: " << str.is_misformatted() << endl;
		fout << "7th codepoint: " << (char)str[6] << endl;
	}
	
	return 0;
}