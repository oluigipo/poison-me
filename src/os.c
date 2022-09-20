#include "internal.h"

#ifndef _WIN32
#   error ooonly win32 for now
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static bool
SetErrorInfo(OS_Error* out_err)
{
	out_err->code = GetLastError();
	out_err->ok = (out_err->code == ERROR_SUCCESS);
	
	switch (out_err->code)
	{
		case ERROR_SUCCESS: out_err->why = Str("no error"); break;
		case ERROR_FILE_NOT_FOUND: out_err->why = Str("file not found"); break;
		case ERROR_ACCESS_DENIED: out_err->why = Str("access denied"); break;
		case ERROR_FILE_EXISTS: out_err->why = Str("file already exists"); break;
		case ERROR_DISK_FULL: out_err->why = Str("disk is full"); break;
		default: out_err->why = Str("unknown error"); break;
	}
	
	return false;
}

API bool
OS_ReadWholeFile(String path, String* out_data, Arena* out_arena, OS_Error* out_err)
{
	MemSet(out_err, 0, sizeof(*out_err));
	out_err->ok = true;
	
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
	
	MemCopy(arena_end, file_data, file_size);
	Arena_Pop(out_arena, arena_end + file_size);
	
	out_data->size = file_size;
	out_data->data = arena_end;
	
	return true;
}

API bool
OS_WriteWholeFile(String path, String data, Arena* scratch_arena, OS_Error* out_err)
{
	MemSet(out_err, 0, sizeof(*out_err));
	out_err->ok = true;
	
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
		DWORD bytes_written = 0;
		uintsize size = data.size;
		const uint8* head = data.data;
		
		while (size > 0)
		{
			DWORD to_write = Min(size, UINT32_MAX);
			if (!WriteFile(file, head, to_write, &bytes_written, NULL))
			{
				result = SetErrorInfo(out_err);
				break;
			}
			
			size -= bytes_written;
			head = head + bytes_written;
		}
		
		CloseHandle(file);
	}
	
	Arena_Pop(scratch_arena, arena_end);
	return result;
}

