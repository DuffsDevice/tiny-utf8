#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <tinyutf8.h>

using namespace std;

int main()
{
	utf8_string anotherFindBug = "who were engaged in “recent narcotics trafficking activity.” Thos” two ”ndividuals were of Newport Beach.";
    utf8_string dot = ".";
	for( std::size_t i = 0 ; i < anotherFindBug.length() ; i++ )
		std::cout << i << " = " << anotherFindBug.find(dot, i) << std::endl;
	return 0;
}