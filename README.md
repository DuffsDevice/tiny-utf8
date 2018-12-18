# TINY <img src="https://github.com/DuffsDevice/tinyutf8/raw/master/UTF8.png" width="47" height="47" align="top" alt="UTF8 Art" style="display:inline;">

[![Build Status](https://travis-ci.org/DuffsDevice/tinyutf8.svg?branch=master)](https://travis-ci.org/DuffsDevice/tinyutf8)&nbsp;&nbsp;[![Licence](https://img.shields.io/badge/licence-BSD--3-e20000.svg)](https://github.com/DuffsDevice/tinyutf8/blob/master/LICENCE)

### DESCRIPTION
TINYUTF8 is a library for extremely easy integration of Unicode into an arbitrary C++11 project.
The library consists solely of the class "utf8_string", which acts as a drop-in replacement for std::string.
Its implementation is successfully in the middle between small memory footprint and fast access.

### FEATURES
- **Drop-in replacement for std::string**
- **Very Lightweight** (~4K LOC)
- **Very fast**, i.e. highly optimized decoder, encoder and traversal routines
- **Advanced Memory Layout**, i.e. Random Access is O( #Codepoints > 127 ) for the average case
- **Small String Optimization** (SSO) for strings up to an UTF8-encoded length of `sizeof(utf8_string)-1`
- **Growth in Constant Time** (Amortized)
- **Conversion between UTF32 and UTF8**
- Small Stack Size, i.e. `sizeof(utf8_string)` = 16 Bytes (32Bit) / 32 Bytes (64Bit)
- Codepoint Range of `0x0` - `0xFFFFFFFF`, i.e. 1-7 Code Units/Bytes per Codepoint (Note: This is more than specified by UTF8, but allows for less complex code)
- Single header file, single source file
- Straightforward C++11 Design
- Possibility to prepend the UTF8 BOM (Byte Order Mark) to any string when converting it to an std::string
- Supports raw (Byte-based) access for occasions where Speed is needed.
- Supports `shrink_to_fit()`
- Malformed UTF8 sequences will not lead to undefined behaviour

### WHAT TINYUTF8 DOES NOT
- Conversion between ISO encodings and UTF8
- Other unicode encodings like UTF16
- Visible character comparison (`'ch'` vs. `'c'+'h'`)
- Codepoint normalization

Note: ANSI suppport was dropped in Version 2.0 in favor of execution speed.

### EXAMPLE USAGE

```cpp
#include <iostream>
#include <algorithm>
#include <tinyutf8.h>
using namespace std;

int main()
{
    utf8_string str = u8"!🌍 olleH";
    for_each( str.rbegin() , str.rend() , []( char32_t codepoint ){
      cout << codepoint;
    } );
    return 0;
}
```

### BUGS

If you encounter any bugs, please file a bug report through the "Issues" tab. I'll try to answer it soon!
