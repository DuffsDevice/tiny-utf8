#include <nds.h>
#include <stdio.h>
#include "type.h"
#include "type.wstring.h"

int main(int argc, char ** argv)
{
	consoleDemoInit();
	
	_u64 time = cpuGetTiming();
	cpuStartTiming(true);
	
	wstring str = wstring( L"test\u1d33Hallo\ua220Welt!" );
	
	//printf("Text: ");
	
	str(7) = 0x1d31;
	str[0] = 'T';
	
	str[4] = ':';
	str[12] = 0x1eee;
	str.append( "Hal\u3321 lo" );
	
	for( int i = 0 ; ; i++ ){
		_wchar codepoint = str[i];
		if( !codepoint )
			break;
		if( codepoint <= 127 )
			printf("%c",(_char)codepoint);
		else
			printf("U\\%x",codepoint);
	}
	printf("\n\n");
	
	for( _wchar codepoint : str ){
		if( codepoint <= 127 )
			printf("%c",(_char)codepoint);
		else
			printf("U\\%x",codepoint);
	}
	printf("\n\n");
	
	auto it = str.rbegin();
	while( it != str.rend() ){
		if( *it <= 127 )
			printf("%c",(_char)it.getValue());
		else
			printf("U\\%x",it.getValue());
		++it;
	}
	printf("\n\n");
	
	printf("Compare: %d, %d, %d\n",wstring(L"Jak\u3230ob") < ( wstring(L"Jak\u3230oc") ) , wstring(L"Jak\u3230ob") <= ( wstring(L"Jak\u3230ob") ) , wstring(L"Jak\u3230ocf") > ( wstring(L"Jak\u3230oc") ));
	
	printf("Time: %lld\n", cpuGetTiming() - time );
	
	while( true );
		swiWaitForVBlank();
	
	return 0;
} // End of main()


