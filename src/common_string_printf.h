#ifndef COMMON_STRING_PRINTF_H
#define COMMON_STRING_PRINTF_H

#include <stdarg.h>

#define String_VPrintfLocal(size, ...) String_VPrintf((char[size]) { 0 }, size, __VA_ARGS__)
#define String_PrintfLocal(size, ...) String_Printf((char[size]) { 0 }, size, __VA_ARGS__)

static uintsize String_PrintfFunc_(char* buf, uintsize buf_size, const char* restrict fmt, va_list args);

static uintsize
String_VPrintfBuffer(char* buf, uintsize len, const char* fmt, va_list args)
{
	return String_PrintfFunc_(buf, len, fmt, args);
}

static uintsize
String_PrintfBuffer(char* buf, uintsize len, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	uintsize result = String_PrintfFunc_(buf, len, fmt, args);
	
	va_end(args);
	return result;
}

static String
String_VPrintf(char* buf, uintsize len, const char* fmt, va_list args)
{
	String result = {
		.data = (uint8*)buf,
		.size = String_PrintfFunc_(buf, len, fmt, args),
	};
	
	return result;
}

static String
String_Printf(char* buf, uintsize len, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	String result = {
		.data = (uint8*)buf,
		.size = String_PrintfFunc_(buf, len, fmt, args),
	};
	
	va_end(args);
	
	return result;
}

static uintsize
String_VPrintfSize(const char* fmt, va_list args)
{
	return String_PrintfFunc_(NULL, 0, fmt, args);
}

static uintsize
String_PrintfSize(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	uintsize result = String_PrintfFunc_(NULL, 0, fmt, args);
	
	va_end(args);
	return result;
}

//- NOTE(ljre): Actual implementation for snprintf alternative.
static uintsize
String_PrintfFunc_(char* buf, uintsize buf_size, const char* restrict fmt, va_list args)
{
	uintsize result = 0;
	const char* fmt_end = fmt + Mem_Strlen(fmt);
	
	if (!buf || buf_size == 0)
	{
		// NOTE(ljre): Just calculates the size.
		
		while (*fmt)
		{
			// NOTE(ljre): Count all leading chars up to '%'.
			{
				const char* ch = (const char*)Mem_FindByte(fmt, '%', fmt_end - fmt);
				
				if (ch)
				{
					result += ch - fmt;
					fmt = ch;
				}
				else
				{
					result += fmt_end - fmt;
					fmt = fmt_end;
					break;
				}
			}
			
			if (*fmt != '%')
				break;
			
			++fmt;
			switch (*fmt++)
			{
				default: ++result; Assert(false); break;
				case '%': ++result; break;
				case '0': ++result; break;
				
				case 'i':
				{
					int32 arg = va_arg(args, int32);
					
					if (arg == INT_MIN)
					{
						result += ArrayLength("-9223372036854775808") - 1;
						break;
					}
					else if (arg < 0)
					{
						result += 1;
						arg = -arg;
					}
					else if (arg == 0)
					{
						result += 1;
						break;
					}
					
					while (arg > 0)
					{
						++result;
						arg /= 10;
					}
				} break;
				
				case 'I':
				{
					int64 arg = va_arg(args, int64);
					
					if (arg == INT64_MIN)
					{
						result += ArrayLength("-") - 1;
						break;
					}
					else if (arg < 0)
					{
						result += 1;
						arg = -arg;
					}
					else if (arg == 0)
					{
						result += 1;
						break;
					}
					
					while (arg > 0)
					{
						++result;
						arg /= 10;
					}
				} break;
				
				case 'u':
				{
					uint32 arg = va_arg(args, uint32);
					
					if (arg == 0)
					{
						result += 1;
						break;
					}
					
					while (arg > 0)
					{
						++result;
						arg /= 10;
					}
				} break;
				
				case 'U':
				{
					uint64 arg = va_arg(args, uint64);
					
					if (arg == 0)
					{
						result += 1;
						break;
					}
					
					while (arg > 0)
					{
						++result;
						arg /= 10;
					}
				} break;
				
				case 'x': case 'X':
				{
					uint32 arg = va_arg(args, uint32);
					
					if (arg == 0)
					{
						result += 1;
						break;
					}
					
					while (arg > 0)
					{
						++result;
						arg >>= 4;
					}
				} break;
				
				case 's':
				{
					const char* arg = va_arg(args, const char*);
					uintsize len = Mem_Strlen(arg);
					
					result += len;
				} break;
				
				case 'S':
				{
					String arg = va_arg(args, String);
					
					result += arg.size;
				} break;
				
				case '.':
				{
					Assert(*fmt == '*'); ++fmt;
					Assert(*fmt == 's'); ++fmt;
					
					int32 len = va_arg(args, int32);
					const char* arg = va_arg(args, const char*);
					
					int32 arg_len = 0;
					while (arg_len < len && arg[arg_len])
						++arg_len;
					
					result += arg_len;
				} break;
				
				case 'p':
				{
					result += sizeof(void*) * 8;
				} break;
				
				case 'f':
				{
					Assert(false);
				} break;
			}
		}
	}
	else
	{
		char* const begin = buf;
		char* const end = buf + buf_size;
		char* p = begin;
		
		while (p < end && *fmt)
		{
			// NOTE(ljre): Copy all leading chars up to '%'
			{
				const char* ch = (const char*)Mem_FindByte(fmt, '%', fmt_end - fmt);
				if (!ch)
					ch = fmt_end;
				
				intsize count = Min(ch - fmt, end - p);
				Mem_Copy(p, fmt, count);
				p += count;
				
				fmt = ch;
			}
			
			if (p >= end)
				break;
			if (*fmt != '%')
				continue;
			
			++fmt;
			switch (*fmt++)
			{
				default: *p++ = fmt[-1]; Assert(false); break;
				case '%': *p++ = '%'; break;
				case '0': *p++ = 0;   break;
				
				//- NOTE(ljre): Signed decimal int.
				case 'i':
				{
					int32 arg = va_arg(args, int32);
					bool negative = false;
					
					// NOTE(ljre): Handle special case for INT_MIN
					if (arg == INT_MIN)
					{
						static_assert(INT_MIN == -2147483648ll);
						char intmin[] = "-2147483648";
						
						intsize count = Min(end - p, ArrayLength(intmin)-1);
						Mem_Copy(p, intmin, count);
						p += count;
						
						break;
					}
					else if (arg < 0)
					{
						arg = -arg; // NOTE(ljre): this would break if 'arg == INT_MIN'
						negative = true;
					}
					else if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = (arg % 10) + '0';
						arg /= 10;
					}
					
					if (p < end && negative)
						*p++ = '-';
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				case 'I':
				{
					int64 arg = va_arg(args, int64);
					bool negative = false;
					
					// NOTE(ljre): Handle special case for INT_MIN
					if (arg == INT64_MIN)
					{
						static_assert(INT64_MIN == -9223372036854775808ll);
						char intmin[] = "-9223372036854775808";
						
						intsize count = Min(end - p, ArrayLength(intmin)-1);
						Mem_Copy(p, intmin, count);
						p += count;
						
						break;
					}
					else if (arg < 0)
					{
						arg = -arg; // NOTE(ljre): this would break if 'arg == INT64_MIN'
						negative = true;
					}
					else if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = (arg % 10) + '0';
						arg /= 10;
					}
					
					if (p < end && negative)
						*p++ = '-';
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				//- NOTE(ljre): Unsigned decimal int.
				case 'u':
				{
					uint32 arg = va_arg(args, uint32);
					
					if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = (arg % 10) + '0';
						arg /= 10;
					}
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				case 'U':
				{
					uint64 arg = va_arg(args, uint64);
					
					if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = (arg % 10) + '0';
						arg /= 10;
					}
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				//- NOTE(ljre): size_t format.
				case 'z':
				{
					uintsize arg = va_arg(args, uintsize);
					
					if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = (arg % 10) + '0';
						arg /= 10;
					}
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				//- NOTE(ljre): Unsigned hexadecimal int.
				case 'x':
				{
					const char* chars = "0123456789ABCDEF";
					
					uint32 arg = va_arg(args, uint32);
					
					if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = chars[arg & 0xf];
						arg >>= 4;
					}
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				case 'X':
				{
					const char* chars = "0123456789ABCDEF";
					
					uint64 arg = va_arg(args, uint64);
					
					if (arg == 0)
					{
						*p++ = '0';
						break;
					}
					
					char tmpbuf[32];
					int32 index = sizeof(tmpbuf);
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = chars[arg & 0xf];
						arg >>= 4;
					}
					
					intsize count = Min(end - p, ArrayLength(tmpbuf) - index);
					Mem_Copy(p, tmpbuf + index, count);
					p += count;
				} break;
				
				//- NOTE(ljre): Strings.
				case 's':
				{
					const char* arg = va_arg(args, const char*);
					uintsize len = Mem_Strlen(arg);
					
					intsize count = Min(end - p, len);
					Mem_Copy(p, arg, count);
					p += count;
				} break;
				
				case 'S':
				{
					String arg = va_arg(args, String);
					
					intsize count = Min(end - p, arg.size);
					Mem_Copy(p, arg.data, count);
					p += count;
				} break;
				
				//- NOTE(ljre): %.*s format
				case '.':
				{
					Assert(*fmt == '*'); ++fmt;
					Assert(*fmt == 's'); ++fmt;
					
					// NOTE(ljre): Write, at most, 'len' chars.
					int32 len = va_arg(args, int32);
					const char* arg = va_arg(args, const char*);
					
					int32 arg_len = 0;
					while (arg_len < len && arg[arg_len])
						++arg_len;
					
					intsize count = Min(end - p, arg_len);
					Mem_Copy(p, arg, count);
					p += count;
				} break;
				
				//- NOTE(ljre): Pointer.
				case 'p':
				{
					uintptr arg = (uintptr)va_arg(args, void*);
					char tmpbuf[sizeof(void*) * 8 / 16];
					int32 index = sizeof(tmpbuf);
					
					Mem_Set(tmpbuf, '0', sizeof(tmpbuf));
					
					while (index > 0 && arg > 0)
					{
						tmpbuf[--index] = arg & 0xf;
						arg >>= 4;
					}
					
					intsize count = Min(end - p, sizeof(tmpbuf));
					Mem_Copy(p, tmpbuf, count);
					p += count;
				} break;
				
				//- TODO(ljre): Floating-point.
				case 'f':
				{
					Assert(false);
				} break;
			}
		}
		
		result = p - begin;
	}
	
	return result;
}

#endif //COMMON_STRING_PRINTF_H