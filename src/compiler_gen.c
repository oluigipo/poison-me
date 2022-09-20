struct X_IrGenError
{
	bool ok;
	uint32 faulty_node;
	String why;
}
typedef X_IrGenError;

struct X_IrGenContext
{
	Arena* output_arena;
	Arena* scratch_arena;
	const X_Ast* ast;
	
	uint32 globalvar_index;
	uint32 function_index;
	X_IrTree* tree;
	
	X_IrGenError error;
}
typedef X_IrGenContext;

static void
X_GenDecl(X_IrGenContext* ctx, uint32 node_index)
{
	
}

// @Allocators: output_arena (output), scratch_arena (temp)
static X_IrTree*
X_IrGenFromAst(const X_Ast* ast, const X_Allocators* allocators, X_IrGenError* out_err)
{
	Arena* output_arena = allocators->output_arena;
	Arena* scratch_arena = allocators->scratch_arena;
	Assert(output_arena && scratch_arena);
	void* scratch_arena_save = Arena_End(scratch_arena);
	
	X_IrGenContext* ctx = &(X_IrGenContext) {
		.output_arena = output_arena,
		.scratch_arena = scratch_arena,
		.ast = ast,
		.error = { .ok = 1 },
		.tree = Arena_PushStruct(output_arena, X_IrTree),
	};
	
	uintsize globalvar_count = 0;
	uintsize function_count = 0;
	
	for (uint32 i = 0; i < ast->size; ++i)
	{
		if (ast->data[i].kind == X_AstNodeKind_DeclProc)
			++function_count;
		else if (ast->data[i].kind == X_AstNodeKind_DeclGlobalVar)
			++globalvar_count;
	}
	
	ctx->tree->ast = ast;
	ctx->tree->globalvar_count = 0;
	ctx->tree->function_count = 0;
	ctx->tree->globalvars = Arena_PushStructArray(output_arena, X_IrGlobalVar, globalvar_count);
	ctx->tree->functions = Arena_PushStructArray(output_arena, X_IrFunction, function_count);
	
	uint32 node = ast->first_node;
	while (node && ctx->error.ok)
	{
		X_GenDecl(ctx, node);
		node = ctx->ast->data[node].next;
	}
	
	Arena_Pop(scratch_arena, scratch_arena_save);
	*out_err = ctx->error;
	return ctx->tree;
}
