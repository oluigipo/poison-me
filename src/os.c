#include "internal.h"

#ifndef _WIN32
#   error ooonly win32 for now
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static bool
SetErrorInfo(OS_Error* out_err)
{
	DWORD last_error = GetLastError();
	bool ok = (last_error == ERROR_SUCCESS);
	
	if (!out_err)
		return ok;
	
	out_err->code = last_error;
	out_err->ok = ok;
	
	switch (out_err->code)
	{
		case ERROR_SUCCESS: out_err->why = Str("no error"); break;
		case ERROR_FILE_NOT_FOUND: out_err->why = Str("file not found"); break;
		case ERROR_ACCESS_DENIED: out_err->why = Str("access denied"); break;
		case ERROR_FILE_EXISTS: out_err->why = Str("file already exists"); break;
		case ERROR_DISK_FULL: out_err->why = Str("disk is full"); break;
		default: out_err->why = Str("unknown error"); break;
	}
	
	return ok;
}

static bool
PrintToFile(String data, OS_Error* out_err, HANDLE file)
{
	uintsize size = data.size;
	const uint8* head = data.data;
	
	while (size > 0)
	{
		DWORD bytes_written = 0;
		DWORD to_write = Min(size, UINT32_MAX);
		
		if (!WriteFile(file, head, to_write, &bytes_written, NULL))
			return SetErrorInfo(out_err);
		
		size -= bytes_written;
		head += bytes_written;
	}
	
	return SetErrorInfo(out_err);
}

//~ OS API
API bool
OS_ReadWholeFile(String path, String* out_data, Arena* out_arena, OS_Error* out_err)
{
	uint8* arena_end = Arena_End(out_arena);
	
	int32 wpath_len = MultiByteToWideChar(CP_UTF8, 0, (const char*)path.data, path.size, NULL, 0) + 1;
	if (wpath_len <= 0)
		return SetErrorInfo(out_err);
	
	wchar_t* wpath = Arena_PushDirtyAligned(out_arena, wpath_len * sizeof(*wpath), 2);
	MultiByteToWideChar(CP_UTF8, 0, (const char*)path.data, path.size, wpath, wpath_len);
	wpath[wpath_len-1] = 0;
	
	HANDLE file = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		Arena_Pop(out_arena, arena_end);
		return SetErrorInfo(out_err);
	}
	
	LARGE_INTEGER large_int;
	if (!GetFileSizeEx(file, &large_int))
	{
		CloseHandle(file);
		Arena_Pop(out_arena, arena_end);
		return SetErrorInfo(out_err);
	}
	
	uintsize file_size = large_int.QuadPart;
	uint8* file_data = Arena_PushDirtyAligned(out_arena, file_size, 1);
	
	uintsize still_to_read = file_size;
	uint8* p = file_data;
	while (still_to_read > 0)
	{
		DWORD to_read = (DWORD)Min(still_to_read, UINT32_MAX);
		DWORD did_read;
		
		if (!ReadFile(file, p, to_read, &did_read, NULL))
		{
			CloseHandle(file);
			Arena_Pop(out_arena, arena_end);
			return SetErrorInfo(out_err);
		}
		
		still_to_read -= to_read;
		p += to_read;
	}
	
	CloseHandle(file);
	
	Mem_Move(arena_end, file_data, file_size);
	Arena_Pop(out_arena, arena_end + file_size);
	
	out_data->size = file_size;
	out_data->data = arena_end;
	
	return SetErrorInfo(out_err);
}

API bool
OS_WriteWholeFile(String path, String data, Arena* scratch_arena, OS_Error* out_err)
{
	char* arena_end = Arena_End(scratch_arena);
	
	int32 wpath_len = MultiByteToWideChar(CP_UTF8, 0, (const char*)path.data, path.size, NULL, 0) + 1;
	if (wpath_len <= 0)
		return SetErrorInfo(out_err);
	
	wchar_t* wpath = Arena_PushDirtyAligned(scratch_arena, wpath_len * sizeof(*wpath), 2);
	MultiByteToWideChar(CP_UTF8, 0, (const char*)path.data, path.size, wpath, wpath_len);
	wpath[wpath_len-1] = 0;
	
	bool result = true;
	HANDLE file = CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	
	if (file == INVALID_HANDLE_VALUE)
		result = SetErrorInfo(out_err);
	else
	{
		result = PrintToFile(data, out_err, file);
		CloseHandle(file);
	}
	
	Arena_Pop(scratch_arena, arena_end);
	return result;
}

API uint64
OS_GetPosixTimestamp(void)
{
	SYSTEMTIME system_time;
	FILETIME file_time;
	GetSystemTime(&system_time);
	SystemTimeToFileTime(&system_time, &file_time);
	
	FILETIME posix_time;
	SYSTEMTIME posix_system_time = {
		.wYear = 1970,
		.wMonth = 1,
		.wDayOfWeek = 4,
		.wDay = 1,
		.wHour = 0,
		.wMinute = 0,
		.wSecond = 0,
		.wMilliseconds = 0,
	};
	SystemTimeToFileTime(&posix_system_time, &posix_time);
	
	uint64 result = 0;
	result += file_time .dwLowDateTime | (uint64)file_time .dwHighDateTime << 32;
	result -= posix_time.dwLowDateTime | (uint64)posix_time.dwHighDateTime << 32;
	
	return result;
}

API String
OS_ResolveFullPath(String path, Arena* output_arena, OS_Error* out_err)
{
	Mem_Set(out_err, 0, sizeof(*out_err));
	out_err->ok = true;
	uint8* base = Arena_End(output_arena);
	
	int32 wpath_len = MultiByteToWideChar(CP_UTF8, 0, (const char*)path.data, path.size, NULL, 0) + 1;
	if (wpath_len <= 0)
	{
		SetErrorInfo(out_err);
		return StrNull;
	}
	
	wchar_t* wpath = Arena_PushDirtyAligned(output_arena, wpath_len * sizeof(*wpath), 2);
	MultiByteToWideChar(CP_UTF8, 0, (const char*)path.data, path.size, wpath, wpath_len);
	wpath[wpath_len-1] = 0;
	
	wchar_t little_tmp[2];
	DWORD wfullpath_len = GetFullPathNameW(wpath, ArrayLength(little_tmp), little_tmp, NULL) + 1;
	
	wchar_t* wfullpath = Arena_PushDirtyAligned(output_arena, wfullpath_len * sizeof(*wfullpath), 2);
	wfullpath_len = GetFullPathNameW(wpath, wfullpath_len, wfullpath, NULL);
	
	int32 fullpath_len = WideCharToMultiByte(CP_UTF8, 0, wfullpath, wfullpath_len, NULL, 0, NULL, NULL);
	if (fullpath_len <= 0)
	{
		Arena_Pop(output_arena, base);
		SetErrorInfo(out_err);
		return StrNull;
	}
	
	char* fullpath = Arena_PushDirtyAligned(output_arena, fullpath_len, 1);
	WideCharToMultiByte(CP_UTF8, 0, wfullpath, wfullpath_len, fullpath, fullpath_len, NULL, NULL);
	
	Mem_Move(base, fullpath, fullpath_len);
	Arena_Pop(output_arena, base + fullpath_len);
	
	return StrMake(fullpath_len, base);
}

API void
OS_SplitPath(String fullpath, String* out_folder, String* out_file)
{
	String folder = StrNull;
	String file = fullpath;
	
	for (intsize i = fullpath.size-1; i >= 0; --i)
	{
		if (fullpath.data[i] == '/' || fullpath.data[i] == '\\')
		{
			folder = StrMake(i, fullpath.data);
			file = StrMake(fullpath.size - i, fullpath.data + i);
			break;
		}
	}
	
	if (out_folder)
		*out_folder = folder;
	if (out_file)
		*out_file = file;
}

API bool
OS_PrintStderr(String data, Arena* scratch_arena, OS_Error* out_err)
{
	Mem_Set(out_err, 0, sizeof(*out_err));
	out_err->ok = true;
	
	return PrintToFile(data, out_err, GetStdHandle(STD_ERROR_HANDLE));
}

API bool
OS_PrintStdout(String data, Arena* scratch_arena, OS_Error* out_err)
{
	Mem_Set(out_err, 0, sizeof(*out_err));
	out_err->ok = true;
	
	return PrintToFile(data, out_err, GetStdHandle(STD_OUTPUT_HANDLE));
}
