#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#define _TINY_UTF8_H_USE_IOSTREAM_
#include <tinyutf8.h>

using namespace std;


int main()
{
	ofstream out = ofstream( "test.txt" , std::ios::binary );
	
	stringstream  cout;
	
	// Prepend the UTF-8 Byte Order Mark
	cout << utf8_string().cpp_str(true);
	
	// Test 1a
	{
		cout << "Test 1a: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"SSO: ツ♫" );
		
		cout << "String: " << str << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Size (bytes): " << str.size() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "7th codepoint: " << str[6] << endl;
		cout << endl;
	}
	
	// Test 1b
	{
		cout << "Test 1b: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( 1 , U'ツ' );
		
		cout << "String: " << str << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Size (bytes): " << str.size() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "1st codepoint: " << str[0] << endl;
		cout << endl;
	}
	
	// Test 1c
	{
		cout << "Test 1c: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello  ツ  World" );
		
		cout << "String: " << str << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Size (bytes): " << str.size() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "9nd codepoint: " << (char)str[8] << endl;
		cout << endl;
	}
	
	
	// Test 2
	{
		cout << "Test 2: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ" );
		str.append( U" ♫ World" );
		
		cout << "String: " << str << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Size (bytes): " << str.size() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		
		utf8_string tmp = utf8_string( U"♫ World" );
		
		str.append( tmp );
		
		cout << "Appended '♫ World': " << str << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Size (bytes): " << str.size() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		
		cout << endl << endl;
	}
	
	// Test 3
	{
		cout << "Test 3: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( "Hello ツ World ♫" );
		cout << "String: " << str << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		
		cout << "Iterative output: ";
		
		std::for_each( str.begin() , str.end()
			, [&cout]( char32_t cp ){
				if( cp <= 127 )
					cout << char(cp);
				else
					cout << "U\\" << int(cp);
			}
		);
		
		cout << endl << "Reverse iterative output: ";
		
		std::for_each( str.rbegin() , str.rend()
			, [&cout]( char32_t cp ){
				if( cp <= 127 )
					cout << char(cp);
				else
					cout << "U\\" << int(cp);
			}
		);
		
		cout << endl << "Random access output: ";
		
		for( unsigned int i = 0 ; i < str.length() ; i++ )
		{
			char32_t cp = str[i];
			if( cp <= 127 )
				cout << char(cp);
			else
				cout << "U\\" << int(cp);
		}
		
		cout << endl << endl;
	}
	
	// Test 4
	{
		cout << "Test 4: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World" );
		cout << "String: " << str.c_str() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "Length: " << str.length() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		
		str[6] = U'🤝';
		
		cout << "Replaced codepoint 6 with 🤝: " << str << endl;
		
		str.replace( 5 , 3 , " " );
		
		cout << "Replaced codepoints 5-7 with ' ': " << str << endl;
		
		str.replace( str.begin() + 5 , str.begin() + 6 , U"~ 🤝 ~" ); // Orig: 
		
		cout << "Replaced the space with '~ 🤝 ~': " << str << endl ;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Random access output: ";
		
		for( unsigned int i = 0 ; i < str.length() ; i++ )
		{
			char32_t cp = str[i];
			if( cp <= 127 )
				cout << char(cp);
			else
				cout << "U\\" << int(cp);
		}
		
		cout << endl << endl;
	}
	
	// Test 5
	{
		cout << "Test 5: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello ツ World♫" );
		
		cout << "String: " << str << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		
		str.erase( 13 );
		
		cout << "Erased 14th codepoint: " << str << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		
		str.erase( str.begin() , str.begin() + 8 );
		
		cout << "Erased codepoints [0,8): " << str << endl << endl;
	}
	
	// Test 6
	{
		cout << "Test 6: " << endl << "-------" << endl;
		
		std::ifstream in = ifstream( "input.txt" , ios::binary );
		
		utf8_string str = std::string( (std::istreambuf_iterator<char>(in)) , std::istreambuf_iterator<char>() );
		
		cout << "String (ANSI): " << str << endl;
		cout << "RequiresUnicode: " << str.requires_unicode() << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		
		cout << "Iterative Output: ";
		
		std::for_each( begin(str) , end(str)
			, [&cout]( char32_t cp ){
				if( cp <= 127 )
					cout << char(cp);
				else
					cout << "U\\" << int(cp);
			}
		);
		
		cout << endl;
		
		cout << "Reverse Iterative Output: ";
		
		std::for_each( str.rbegin() , str.rend()
			, [&cout]( char32_t cp ){
				if( cp <= 127 )
					cout << char(cp);
				else
					cout << "U\\" << int(cp);
			}
		);
		
		cout << endl;
		
		std::unique_ptr<char32_t[]> ptr{ new char32_t[str.length()+1] };
		str.to_wide_literal( ptr.get() );
		utf8_string corrected{ ptr.get() , str.length() };
		
		cout << "Converted to UTF-8: " << corrected << endl;
		
		cout << endl;
	}
	
	// Test 7
	{
		cout << "Test 7: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"Hello World ツ♫" );
		
		cout << "Find Last Not of ツ:" << str.find_last_not_of( U"ツ♫" ) << endl;
		cout << "Correct Result: " << string("Hello World *+") .find_last_not_of( "*+" ) << endl;
		
		cout << "Find Last of ツ:" << str.find_last_of( U"e" ) << endl;
		cout << "Correct Result: " << string("Hello World *+") .find_last_of( "e" ) << endl;
		
		cout << "Find l:" << str.find( U'l' ) << endl;
		cout << "Correct Result: " << string("Hello World *+") .find( 'l' ) << endl;
		
		cout << "RFind l:" << str.rfind( U'l' ) << endl;
		cout << "Correct Result: " << string("Hello World *+") .rfind( 'l' ) << endl;
		
		cout << endl;
	}
	
	// Test 5
	{
		cout << "Test 8: " << endl << "-------" << endl;
		
		utf8_string fullstr = utf8_string( U"Hello ツ World rg rth rt he rh we gxgre" );
		
		cout << "Full String: " << fullstr << endl;
		
		utf8_string str = fullstr.substr( 3 , 16 );
		cout << "Substring 3[16]: " << str << endl;
		cout << "SSOActive: " << str.sso_active() << endl;
		cout << "7th codepoint: " << (char)str[6] << endl << endl;
	}
	
	// Test 6
	{
		cout << "Test 6: " << endl << "-------" << endl;
		
		utf8_string str = utf8_string( U"SSO: ツ♫" );
		
		cout << "String: " << str << endl;
		cout << "Capacity: " << str.capacity() << endl;
		
		for( int i = 0 ; i < 20 ; i++ )
		{
			str.append( U"SSO: ツ♫" );
			cout << "String after insertion: " << str << endl;
			cout << "Capacity after insertion: " << str.capacity() << endl;
			cout << "Length: " << str.length() << endl;
			cout << "Size (bytes): " << str.size() << endl << endl;
		}
		
		str.shrink_to_fit();
		cout << "String after shrinking: " << str << endl;
		cout << "Capacity after shrinking: " << str.capacity() << endl;
		cout << "Length: " << str.length() << endl;
		cout << "Size (bytes): " << str.size() << endl << endl;
		
		cout << endl;
	}
	
	out << cout.rdbuf();
	
	return 0;
}