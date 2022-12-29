#include "internal.h"

static void
X_Log(Arena* scratch_arena, const char* fmt, ...)
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

#include "lang_x_defs.h"
#include "lang_x_lexer.c"
#include "lang_x_parser.c"
//#include "lang_x_sema.c"
#include "lang_x_asm.c"

API int32
X_Main(int32 argc, const char* const* argv)
{
	Arena* output_arena = Arena_Create(128 << 20, 32 << 20);
	Arena* scratch_arena = Arena_Create(32 << 20, 16 << 20);
	
	String source;
	X_TokenArray* tokens;
	X_Ast* ast;
	
	// NOTE(ljre): Read source file
	{
		OS_Error os_err;
		OS_ReadWholeFile(Str("tests/simple.xx"), &source, output_arena, &os_err);
		
		if (!os_err.ok)
		{
			X_Log(scratch_arena, "error: %.*s", StrFmt(os_err.why));
			return 1;
		}
	}
	
	// NOTE(ljre): Tokenize source file
	{
		X_TokenizeString_Error tokenize_err;
		Allocators allocators = {
			.output_arena = output_arena,
			.scratch_arena = scratch_arena,
		};
		
		tokens = X_TokenizeString(source, &allocators, &tokenize_err);
		
		if (!tokenize_err.ok)
		{
			X_Log(scratch_arena, "tok error at (offset %z): %.*s\n", tokenize_err.source_offset, StrFmt(tokenize_err.why));
			return 1;
		}
	}
	
	// NOTE(ljre): Parse source file
	{
		X_ParseFile_Error parser_err;
		Allocators allocators = {
			.output_arena = output_arena,
			.leaky_scratch_arena = scratch_arena,
		};
		
		ast = X_ParseFile(tokens, &allocators, &parser_err);
		
		if (!parser_err.ok)
		{
			for (const X_ParserError* it = parser_err.first; it; it = it->next)
			{
				uint32 line, col;
				X_GetLineColumnFromOffset(source, tokens->data[it->token].str_offset, &line, &col);
				
				X_Log(scratch_arena, "parse error at (%u:%u): %.*s\n", line, col, StrFmt(it->why));
			}
			
			return 1;
		}
		
		Arena_Clear(scratch_arena);
	}
	
	// NOTE(ljre): Semantic analysis
#if 0
	{
		X_SemaAst_Error sema_err;
		Allocators allocators = {
			.output_arena = output_arena,
			.leaky_scratch_arena = scratch_arena,
		};
		
		X_SemaAst(ast, &allocators, &sema_err);
		
		if (!sema_err.ok)
		{
			for (const X_SemaError* it = sema_err.first; it; it = it->next)
			{
				const X_TokenArray* tokens = ast->tokens;
				uint32 token = ast->data[it->faulty_node].token;
				
				uint32 line, col;
				X_GetLineColumnFromOffset(tokens->source, tokens->data[token].str_offset, &line, &col);
				
				X_Log(scratch_arena, "sema error at (%u:%u): %.*s\n", line, col, StrFmt(it->why));
			}
			
			return 1;
		}
		
		Arena_Clear(scratch_arena);
	}
#endif
	
	// NOTE(ljre): Code generation
	{
		Allocators allocators = {
			.output_arena = output_arena,
			.scratch_arena = scratch_arena,
		};
		
		X_AsmTestGen(&allocators);
	}
	
	Debugbreak();
	
	return 0;
}
