#include "tinyutf8.h"

// Explicit template instantiations for utf8_string
template struct tiny_utf8::basic_utf8_string<>;
extern template std::ostream& operator<<( std::ostream& stream , const tiny_utf8::basic_utf8_string<>& str );
extern template std::istream& operator>>( std::istream& stream , tiny_utf8::basic_utf8_string<>& str );