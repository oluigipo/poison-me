#ifndef COMMON_PRINTF_H
#define COMMON_PRINTF_H

#include <stdarg.h>

#ifdef COMMON_USE_STB_SPRINTF
#   define STB_SPRINTF_STATIC
#   define STB_SPRINTF_IMPLEMENTATION
#   include <ext/stb_sprintf.h>
#elif defined(COMMON_DONT_USE_CRT)
#   error what else?
#else
#   include <stdio.h>
#   define stbsp_vsnprintf vsnprintf
#endif

#define VSPrintfLocal(size, ...) VSPrintfStr((char[size]) { 0 }, size, __VA_ARGS__)
#define SPrintfLocal(size, ...) SPrintfStr((char[size]) { 0 }, size, __VA_ARGS__)

static uintsize
VSPrintf(char* buf, uintsize len, const char* fmt, va_list args)
{
	Assert(len <= INT32_MAX);
	
	return (uintsize)stbsp_vsnprintf(buf, (int32)len, fmt, args);
}

static uintsize
SPrintf(char* buf, uintsize len, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	uintsize result = VSPrintf(buf, len, fmt, args);
	
	va_end(args);
	return result;
}

static String
VSPrintfStr(char* buf, uintsize len, const char* fmt, va_list args)
{
	Assert(len <= INT32_MAX);
	
	String result = {
		.data = (uint8*)buf,
		.size = stbsp_vsnprintf(buf, (int32)len, fmt, args),
	};
	
	return result;
}

static String
SPrintfStr(char* buf, uintsize len, const char* fmt, ...)
{
	Assert(len <= INT32_MAX);
	
	va_list args;
	va_start(args, fmt);
	
	String result = {
		.data = (uint8*)buf,
		.size = stbsp_vsnprintf(buf, (int32)len, fmt, args),
	};
	
	va_end(args);
	
	return result;
}

static uintsize
VSPrintfSize(const char* fmt, va_list args)
{
	return (uintsize)stbsp_vsnprintf(NULL, 0, fmt, args);
}

static uintsize
SPrintfSize(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	uintsize result = VSPrintfSize(fmt, args);
	
	va_end(args);
	return result;
}

#ifndef COMMON_USE_STB_SPRINTF
#   undef stbsp_vsnprintf
#endif

#endif //COMMON_PRINTF_H
