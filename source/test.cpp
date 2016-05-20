#include <cstdio>
#include <tinyutf8.h>

using namespace std;

int main(int argc, char ** argv)
{	
	utf8_string str = utf8_string( U"test\u1d33Hallo\ua220Welt!" );
	
	str(7) = 0x1d31;
	str[0] = 'T';
	
	str[4] = ':';
	str[12] = 0x1eee;
	str.append( U"Hal\u3321 lo" );
	
	for( int i = 0 ; ; i++ ){
		char32_t codepoint = str[i];
		if( !codepoint )
			break;
		if( codepoint <= 127 )
			printf("%c",(char)codepoint);
		else
			printf("U\\%x",codepoint);
	}
	printf("\n\n");
	
	for( char32_t codepoint : str ){
		if( codepoint <= 127 )
			printf("%c",(char)codepoint);
		else
			printf("U\\%x",codepoint);
	}
	printf("\n\n");
	
	auto it = str.rbegin();
	while( it != str.rend() ){
		if( *it <= 127 )
			printf("%c",(char)it.get());
		else
			printf("U\\%x",it.get());
		++it;
	}
	printf("\n\n");
	
	printf("Compare: %d, %d, %d\n",utf8_string(U"Jak\u3230ob") < ( utf8_string(U"Jak\u3230oc") ) , utf8_string(U"Jak\u3230ob") <= ( utf8_string(U"Jak\u3230ob") ) , utf8_string(U"Jak\u3230ocf") > ( utf8_string(U"Jak\u3230oc") ));
	
	return 0;
} // End of main()


