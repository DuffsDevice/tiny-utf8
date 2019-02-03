#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <assert.h>
#include <tinyutf8.h>

using namespace std;


int main()
{
	utf8_string n1("ALF Cen");
	utf8_string n2("BET Cen");
	utf8_string n3("GAM Cen");
	assert(n1 != n2);
	assert(n1 != n3);
	assert(n2 != n3);
	assert(n1 < "BET Cen");
	assert(n1 < n3);
	assert(n2 > n1);
	assert(n3 > n1);
	assert(n1 <= n2);
	assert(n1 <= "GAM Cen");
	assert(n2 >= n1);
	assert(n3 >= n1);
	
	return 0;
}