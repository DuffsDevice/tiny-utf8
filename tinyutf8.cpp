#include "tinyutf8.h"

// Explicit template instantiations for utf8_string
template struct tiny_utf8::basic_utf8_string<>;
template extern std::ostream& operator<<( std::ostream& stream , const tiny_utf8::basic_utf8_string<>& str );
template extern std::istream& operator>>( std::istream& stream , tiny_utf8::basic_utf8_string<>& str );