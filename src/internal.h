#ifndef INTERNAL_H
#define INTERNAL_H

#define COMMON_DONT_USE_CRT
#include "common.h"

#define API extern

// NOTE(ljre): When a function has complex allocation requirements, it may need more than one allocator.
//             This struct specifies those allocators for them. To know what allocators a function needs,
//             look for a @Allocators comment right above it's implementation.
struct Allocators
{
	// NOTE(ljre): Where the output will be allocated.
	Arena* output_arena;
	// NOTE(ljre): Auxiliary arena for temporary dynamic allocations. It's offset is restored when
	//             the function returns.
	Arena* scratch_arena;
	// NOTE(ljre): Same as above, except it's offset is *not* restored. Needed when the function might need to
	//             return extra data to the caller that:
	//                 - is not referenced by the output data;
	//                 - may be free'd before the output data.
	//             mainly useful to store errors.
	Arena* leaky_scratch_arena;
}
typedef Allocators;

//- OS API
struct OS_Error
{
	bool ok;
	uint32 code;
	String why;
}
typedef OS_Error;

API bool OS_ReadWholeFile(String path, String* out_data, Arena* out_arena, OS_Error* out_err);
API bool OS_WriteWholeFile(String path, String data, Arena* scratch_arena, OS_Error* out_err);
API uint64 OS_GetPosixTimestamp(void);
API bool OS_PrintStderr(String data, Arena* scratch_arena, OS_Error* out_err);
API bool OS_PrintStdout(String data, Arena* scratch_arena, OS_Error* out_err);
API String OS_ResolveFullPath(String path, Arena* output_arena, OS_Error* out_err);
API void OS_SplitPath(String fullpath, String* out_folder, String* out_file);

//- X API
API int32 X_Main(int32 argc, const char* const* argv);

//- C API
API int32 C_Main(int32 argc, const char* const* argv);

#endif //INTERNAL_H
