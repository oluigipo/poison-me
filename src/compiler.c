#include "internal.h"

// NOTE(ljre): When a function has complex allocation requirements, it may need more than one allocator.
//             This struct specifies those allocators for them. To know what allocators a function needs,
//             look for a @Allocators comment right above it's implementation.
struct X_Allocators
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
typedef X_Allocators;

static void
X_LogError(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

static void
X_Log(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
}

#include "compiler_defs.h"
#include "compiler_lexer.c"
#include "compiler_parser.c"
#include "compiler_sema.c"
#include "compiler_gen.c"

API int32
X_Main(int32 argc, const char* const* argv)
{
	Arena* output_arena = Arena_Create(Megabytes(128), Megabytes(32));
	Arena* scratch_arena = Arena_Create(Megabytes(32), Megabytes(16));
	
	String source;
	X_TokenArray* tokens;
	X_Ast* ast;
	X_IrTree* ir;
	
	// NOTE(ljre): Read source file
	{
		OS_Error os_err;
		OS_ReadWholeFile(Str("tests/simple.xx"), &source, output_arena, &os_err);
		
		if (!os_err.ok)
		{
			X_LogError("error: %.*s", StrFmt(os_err.why));
			return 1;
		}
	}
	
	// NOTE(ljre): Tokenize source file
	{
		X_TokenizeString_Error tokenize_err;
		X_Allocators allocators = {
			.output_arena = output_arena,
			.scratch_arena = scratch_arena,
		};
		
		tokens = X_TokenizeString(source, &allocators, &tokenize_err);
		
		if (!tokenize_err.ok)
		{
			X_LogError("tok error at (offset %zu): %.*s\n", tokenize_err.source_offset, StrFmt(tokenize_err.why));
			return 1;
		}
		
		for (uint32 i = 0; i < tokens->size; ++i)
		{
			String as_string = X_TokenAsString(tokens, i);
			uint32 line, col;
			X_GetLineColumnFromOffset(tokens->source, tokens->data[i].str_offset, &line, &col);
			
			X_Log("%u:%u\t%.*s\n", line, col, StrFmt(as_string));
		}
	}
	
	// NOTE(ljre): Parse source file
	{
		X_ParseFile_Error parser_err;
		X_Allocators allocators = {
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
				
				X_LogError("parse error at (%u:%u): %.*s\n", line, col, StrFmt(it->why));
			}
			
			return 1;
		}
		
		Arena_Clear(scratch_arena);
	}
	
	// NOTE(ljre): Semantic analysis
	{
		X_SemaAst_Error sema_err;
		X_Allocators allocators = {
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
				
				X_LogError("sema error at (%u:%u): %.*s\n", line, col, StrFmt(it->why));
			}
			
			return 1;
		}
		
		Arena_Clear(scratch_arena);
	}
	
	// NOTE(ljre): Code generation
	{
		X_IrGenError gen_err;
		X_Allocators allocators = {
			.output_arena = output_arena,
			.scratch_arena = scratch_arena,
		};
		
		ir = X_IrGenFromAst(ast, &allocators, &gen_err);
		
		if (!gen_err.ok)
		{
			const X_TokenArray* tokens = ast->tokens;
			uint32 token = ast->data[gen_err.faulty_node].token;
			
			uint32 line, col;
			X_GetLineColumnFromOffset(tokens->source, tokens->data[token].str_offset, &line, &col);
			
			X_LogError("ir gen error at (%u:%u) node %u: %.*s\n", line, col, gen_err.faulty_node, StrFmt(gen_err.why));
		}
	}
	
	Debugbreak();
	
	return 0;
}
