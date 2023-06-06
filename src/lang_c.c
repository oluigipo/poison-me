#include "internal.h"

#include "lang_c_defs.h"

static void
C_LogFmt(Arena* scratch_arena, const char* fmt, ...)
{
	va_list args, args2;
	va_start(args, fmt);
	va_copy(args2, args);
	
	uintsize size = String_VPrintfSize(fmt, args2);
	char* data = Arena_PushDirtyAligned(scratch_arena, size, 1);
	
	String to_print = String_VPrintf(data, size, fmt, args);
	OS_Error err;
	
	OS_PrintStderr(to_print, scratch_arena, &err);
	// NOTE(ljre): Ignoring error...
	
	va_end(args2);
	va_end(args);
}

static void
C_Log(Arena* scratch_arena, String str)
{
	OS_PrintStderr(str, scratch_arena, &(OS_Error) { 0 });
}

static void
C_FatalError(C_TuContext* tu, const char* fmt, ...)
{
	for Arena_TempScope(tu->scratch_arena)
	{
		va_list args;
		va_start(args, fmt);
		String to_print = Arena_VPrintf(tu->scratch_arena, fmt, args);
		va_end(args);
		
		OS_PrintStderr(to_print, tu->scratch_arena, NULL);
	}
	
	exit(1);
}

static inline bool
C_IsOk(const C_Error* error)
{
	return error->what.size == 0;
}

static inline C_HashMapChunk*
C_AllocHashMapChunk(Arena* arena, uint32 log2cap, uint32 obj_size)
{
	C_HashMapChunk* map = Arena_Push(arena, sizeof(C_HashMapChunk) + obj_size * (1 << log2cap));
	map->log2cap = log2cap;
	return map;
}

#include "lang_c_log.c"
#include "lang_c_token.c"
#include "lang_c_preproc.c"
#include "lang_c_parser.c"

API int32
C_Main(int32 argc, const char* const* argv)
{
	//- basic options
	String filename = StrInit("tests/pp-test.c");
	
	const String include_dirs[] = {
		StrInit("include/"),
	};
	
	const String predefined_macros[] = {
		StrInit("__STDC__ 1"),
		StrInit("__STDC_HOSTED__ 1"),
		StrInit("__STDC_VERSION__ 199901L"),
	};
	
	C_CompilerOptions options = {
		.warnings = { 0 },
		
		.include_dirs = include_dirs,
		.include_dirs_count = ArrayLength(include_dirs),
		
		.predefined_macros = predefined_macros,
		.predefined_macros_count = ArrayLength(predefined_macros),
		
		.abi = {
			.t_bool = { 1, 1, true },
			.t_char = { 1, 1, true },
			.t_schar = { 1, 1, false },
			.t_uchar = { 1, 1, true },
			.t_short = { 2, 2, false },
			.t_ushort = { 2, 2, true },
			.t_int = { 4, 4, false },
			.t_uint = { 4, 4, true },
			.t_long = { 4, 4, false },
			.t_ulong = { 4, 4, true },
			.t_longlong = { 8, 8, false },
			.t_ulonglong = { 8, 8, true },
			.t_float = { 4, 4, false },
			.t_double = { 8, 8, false },
			.t_ptr = { 8, 8, true },
			
			.char_bit = 8,
			.index_sizet = 12,
			.index_ptrdifft = 11,
			.index_uintptr = 12,
			.index_intptr = 11,
		},
	};
	
	C_TuContext tu = {
		.loc_arena = Arena_Create(512ull << 20, 8ull << 20),
		.array_arena = Arena_Create(512ull << 20, 8ull << 20),
		.tree_arena = Arena_Create(512ull << 20, 8ull << 20),
		.stage_arena = Arena_Create(512ull << 20, 8ull << 20),
		.scratch_arena = Arena_Create(512ull << 20, 8ull << 20),
		
		.main_file_name = filename,
		.options = &options,
	};
	
	//- preprocess
	if (tu.error_count == 0)
		C_Preprocess(&tu);
	
	String str = C_WritePreprocessedTokensGnu(&tu, tu.scratch_arena);
	OS_WriteWholeFile(Str("tests/pp-tested.c"), str, tu.scratch_arena, NULL);
	
	//if (tu.error_count == 0)
	//C_Parse(&tu);
	
	//C_PrintAllErrorsAndWarnings(&tu);
	
	if (true)
	{
		C_LogFmt(tu.scratch_arena, "[MEMORY USAGE]\n");
		C_LogFmt(tu.scratch_arena, "\tloc_arena:     %z of %z\n", tu.loc_arena->offset, tu.loc_arena->commited);
		C_LogFmt(tu.scratch_arena, "\tarray_arena:   %z of %z\n", tu.array_arena->offset, tu.array_arena->commited);
		C_LogFmt(tu.scratch_arena, "\ttree_arena:    %z of %z\n", tu.tree_arena->offset, tu.tree_arena->commited);
		C_LogFmt(tu.scratch_arena, "\tstage_arena:  %z of %z\n", tu.stage_arena->offset, tu.stage_arena->commited);
		C_LogFmt(tu.scratch_arena, "\tscratch_arena: %z of %z\n", tu.scratch_arena->offset, tu.scratch_arena->commited);
	}
	
	Debugbreak();
	
	return 0;
}
