# TINYUTF8

### DESCRIPTION
TINYUTF8 is a library for extremely easy integration of Unicode into an arbitrary C++11 project.
The library consists solely of the class "utf8_string", which acts as a drop-in replacement for std::string.
Its implementation is successfully in the middle between small memory footprint and fast access.

Note: ANSI suppport was dropped in favor of execution speed.

### FEATURES
- Drop-in replacement for std::string
- Very Lightweight ~2k LOC
- Very Fast (Random Access is O( Num. Codepoints > 127 ))
- sizeof(utf8_string) = 24bytes (x86)
- Codepoint Range: 0x0 - 0x7FFFFFFF (6 Bytes)
- Single Header file, Single Source file
- NO Exceptions (Defensive Programming)
- Straightforward C++11 design
- Small String Optimization (SSO)
- Malformed UTF-8 sequences will not lead to undefined behaviour
- Prepend the UTF8 BOM (Byte Order Mark) to any string when converting it to an std::string
- Supports raw (Byte-based) access (where Speed is needed)

### WHAT TINYUTF8 DOES NOT
- Conversion between iso encodings and utf8
- Other unicode encodings like UTF16 or UTF32
- Visible character comparison ('ch' vs. 'c'+'h')
- Codepoint normalization
