#ifndef STRING_HELPER_H
#define STRING_HELPER_H

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

// Some general purpose multiplatform string helper methods

namespace STRING_HELPER
{

bool getBool(const char *v);

int32_t stringFormatV(char* dst, size_t dstSize, const char* src, va_list arg);
int32_t stringFormat(char* dst, size_t dstSize, const char* format, ...);

const char * skipWhitespace(const char *str);
uint32_t getHex(uint8_t c);
uint8_t getHEX1(const char *foo, const char **endptr);
uint16_t getHEX2(const char *foo, const char **endptr);
uint32_t getHEX4(const char *foo, const char **endptr);
uint32_t getHEX(const char *foo, const char **endptr);

// Convert the input string into a float (handles special case HEX IEEE and flt_min and flt_max notation)
// 'next' if non-null will point immediately past the last one
float	getFloatValue(const char *str, const char **next);
bool	getVec3(const char *str, const char **next, float &x, float &y, float &z);
bool	getVec4(const char *str, const char **next, float &x, float &y, float &z,float &w);

bool	getUint3(const char *str, const char **next, uint32_t &i1, uint32_t &i2, uint32_t &i3);
bool	getUint1(const char *str, const char **next, uint32_t &i1);

uint32_t	getUint32Value(const char *str, const char **next);

void		normalizePathSlashes(char *fname);
const char *stristr(const char *sourceString, const char *subString);

} // end of STRING_HELPER namespace

#endif
