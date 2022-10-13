struct X_SemaError typedef X_SemaError;
struct X_SemaError
{
	X_SemaError* next;
	
	String why;
	uint32 faulty_node;
};

struct X_SemaAst_Error
{
	bool ok;
	uint32 count;
	X_SemaError* first;
	X_SemaError* last;
}
typedef X_SemaAst_Error;

struct X_SemaContext
{
	Arena* output_arena;
	Arena* scratch_arena;
	
	X_Ast* ast;
	Map* sym_map; // (Hash(key) + scope_sym_index) ====> uint32 symbol index
	
	uint32 current_scope;
	uint32* current_scope_inner_decls_counter;
	
	X_SemaAst_Error error;
}
typedef X_SemaContext;

static X_Type X_type_void[1] = { [0].kind = X_TypeKind_Void, };
static X_Type X_type_int8[1] = { [0].kind = X_TypeKind_Int8, };
static X_Type X_type_int16[1] = { [0].kind = X_TypeKind_Int16, };
static X_Type X_type_int32[1] = { [0].kind = X_TypeKind_Int32, };
static X_Type X_type_int64[1] = { [0].kind = X_TypeKind_Int64, };
static X_Type X_type_uint8[1] = { [0].kind = X_TypeKind_UInt8, };
static X_Type X_type_uint16[1] = { [0].kind = X_TypeKind_UInt16, };
static X_Type X_type_uint32[1] = { [0].kind = X_TypeKind_UInt32, };
static X_Type X_type_uint64[1] = { [0].kind = X_TypeKind_UInt64, };
static X_Type X_type_float32[1] = { [0].kind = X_TypeKind_Float32, };
static X_Type X_type_float64[1] = { [0].kind = X_TypeKind_Float64, };
static X_Type X_type_string[1] = { [0].kind = X_TypeKind_String, };

//~ NOTE(ljre): Helpers
static X_AstSymbol*
X_SymbolFromIndex(const X_Ast* ast, uint32 sym_index, bool inc_ref_count)
{
	X_AstSymbolTable* table = ast->symbol_table;
	
	while (table && sym_index >= table->size)
	{
		sym_index -= table->size;
		table = table->next;
	}
	
	if (table)
	{
		Assert(sym_index < table->size);
		X_AstSymbol* result = &table->data[sym_index];
		
		if (inc_ref_count)
			result->ref_count++;
		
		return result;
	}
	
	return NULL;
}

//~ NOTE(ljre): Internal helpers
static X_AstSymbol*
X_SemaPushSymbolStruct(X_SemaContext* sema, const X_AstSymbol* restrict sym, String name, uint32* out_index, bool should_inc_symcount)
{
	X_AstSymbolTable* table = sema->ast->symbol_table;
	uint32 whole_index = sema->ast->symbol_count++;
	uint32 index = whole_index;
	
	while (index >= table->size)
	{
		index -= table->size;
		
		if (!table->next)
		{
			uintsize count = table->size * 2;
			uintsize size = sizeof(X_AstSymbolTable) + sizeof(X_AstSymbol) * count;
			table = table->next = Arena_PushAligned(sema->output_arena, size, alignof(X_AstSymbolTable));
			Assert(index == 0);
			break;
		}
		
		table = table->next;
	}
	
	table->data[index] = *sym;
	
	uint64 hash = Map_HashFunc(name) + sema->current_scope;
	Map_InsertWithHash(sema->sym_map, name, (void*)(uintptr)whole_index, hash);
	
	if (should_inc_symcount && sema->current_scope_inner_decls_counter)
		*sema->current_scope_inner_decls_counter += 1;
	if (out_index)
		*out_index = whole_index;
	
	return &table->data[index];
}

static X_AstSymbol*
X_SemaPushSymbol(X_SemaContext* sema, X_AstSymbolKind kind, uint32 decl_node, uint32* out_index)
{
	X_AstSymbol sym = {
		.kind = kind,
		.decl_node = decl_node,
		.scope_sym = sema->current_scope,
		.ref_count = 0,
	};
	
	String name = X_TokenAsString(sema->ast->tokens, sema->ast->data[decl_node].any_decl.name_token);
	return X_SemaPushSymbolStruct(sema, &sym, name, out_index, true);
}

static uint32
X_SemaPushScope(X_SemaContext* sema, uint32 scope_node)
{
	X_AstSymbol sym = {
		.kind = X_AstSymbolKind_Scope,
		.decl_node = scope_node,
		.scope_sym = sema->current_scope,
		.ref_count = 0,
	};
	
	uint32 result = sema->current_scope;
	X_AstSymbol* scope = X_SemaPushSymbolStruct(sema, &sym, StrNull, &sema->current_scope, true);
	sema->current_scope_inner_decls_counter = &scope->scope.sym_count;
	
	return result;
}

static void
X_SemaPopScope(X_SemaContext* sema, uint32 outer_scope_sym)
{
	Assert(outer_scope_sym != 0);
	
	sema->current_scope_inner_decls_counter = NULL;
	sema->current_scope = outer_scope_sym;
	
	if (outer_scope_sym)
	{
		X_AstSymbol* outer_scope = X_SymbolFromIndex(sema->ast, outer_scope_sym, false);
		Assert(outer_scope);
		
		sema->current_scope_inner_decls_counter = &outer_scope->scope.sym_count;
	}
}

static void
X_SemaPushError(X_SemaContext* sema, String why, uint32 faulty_node)
{
	sema->error.count++;
	
	X_SemaError* error = Arena_PushStruct(sema->scratch_arena, X_SemaError);
	
	if (!sema->error.first)
		sema->error.first = sema->error.last = error;
	else
		sema->error.last = sema->error.last->next = error;
	
	error->why = why;
	error->faulty_node = faulty_node;
}

static void
X_SemaPushErrorFmt(X_SemaContext* sema, uint32 faulty_node, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	String why = Arena_VSPrintf(sema->scratch_arena, fmt, args);
	va_end(args);
	
	X_SemaPushError(sema, why, faulty_node);
}

static X_AstSymbol*
X_SemaFindSymbolByName(X_SemaContext* sema, String name, uint32* out_index, bool inc_ref_count)
{
	uint64 base_hash = Map_HashFunc(name);
	uint32 scope = sema->current_scope;
	
	while (true)
	{
		uint64 hash = base_hash + scope;
		uint32 result = (uint32)Map_FetchWithHash(sema->sym_map, name, hash);
		
		if (result)
		{
			X_AstSymbol* sym = X_SymbolFromIndex(sema->ast, result, inc_ref_count);
			
			if (out_index)
				*out_index = result;
			
			return sym;
		}
		
		if (!scope)
			break;
		
		X_AstSymbol* scope_sym = X_SymbolFromIndex(sema->ast, scope, false);
		Assert(scope_sym);
		scope = scope_sym->scope_sym;
	}
	
	if (out_index)
		*out_index = 0;
	
	return NULL;
}

//~ NOTE(ljre): Actual semantic analysis
static bool X_SemaDecl(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index);
static bool X_SemaType(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index);
static bool X_SemaStmt(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index);
static bool X_SemaExpr(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index);
static bool X_SemaBlock(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index);
static bool X_SemaConvertableExpr(X_SemaContext* sema, X_AstSymbol* func, uint32 type_node, uint32 expr_node);

static bool
X_SemaType(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index)
{
	X_AstNode* node = &sema->ast->data[node_index];
	bool ok = true;
	
	switch (node->kind)
	{
		
		default: Unreachable(); break;
	}
	
	return ok;
}

static bool
X_SemaExpr(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index)
{
	X_AstNode* node = &sema->ast->data[node_index];
	bool ok = true;
	
	switch (node->kind)
	{
		case X_AstNodeKind_ExprFactorNumber: node->type = X_type_int32; break;
		case X_AstNodeKind_ExprFactorString: node->type = X_type_string; break;
		
		case X_AstNodeKind_ExprFactorIdent:
		{
			String name = X_TokenAsString(sema->ast->tokens, node->token);
			
			uint32 sym_index;
			X_AstSymbol* sym = X_SemaFindSymbolByName(sema, name, &sym_index, true);
			
			if (sym)
			{
				X_AstNode* decl_node = &sema->ast->data[sym->decl_node];
				
				node->ident_expr.ref_sym = sym_index;
				node->type = decl_node->type;
			}
			else
			{
				ok = false;
				X_SemaPushErrorFmt(sema, node_index, "undefined identifier '%.*s'", StrFmt(name));
			}
		} break;
		
		default: Unreachable(); break;
	}
	
	return ok;
}

static bool
X_SemaStmt(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index)
{
	X_AstNode* node = &sema->ast->data[node_index];
	bool ok = true;
	
	switch (node->kind)
	{
		
		default: Unreachable(); break;
	}
	
	return ok;
}

static void
X_SemaDecl(X_SemaContext* sema, X_AstSymbol* func, uint32 node_index)
{
	X_AstNode* node = &sema->ast->data[node_index];
	bool ok = true;
	
	switch (node->kind)
	{
		case X_AstNodeKind_DeclGlobalVar:
		{
			Assert(!func);
			
			String name = X_TokenAsString(sema->ast->tokens, node->any_decl.name_token);
			
			// NOTE(ljre): Check for redefinition
			{
				uint32 previous_def_index;
				X_AstSymbol* previous_def = X_SemaFindSymbolByName(sema, name, &previous_def_index, false);
				
				if (previous_def)
				{
					X_TokenArray* tokens = sema->ast->tokens;
					uint32 str_offset = tokens->data[sema->ast->data[previous_def->decl_node].token].str_offset;
					
					uint32 line, col;
					X_GetLineColumnFromOffset(tokens->source, str_offset, &line, &col);
					
					X_SemaPushErrorFmt(sema, node_index, "redefinition of global variable '%.*s'; previous definition at (%u:%u)", StrFmt(name), line, col);
					ok = false;
					break;
				}
			}
			
			X_SemaPushSymbol(sema, X_AstSymbolKind_GlobalVar, node_index, &node->any_decl.symbol);
			X_Type* desired_type = NULL;
			
			if (node->var_decl.type_node)
			{
				ok &= X_SemaType(sema, NULL, node->var_decl.type_node);
				
				if (ok)
					desired_type = sema->ast->data[node->var_decl.type_node].type;
			}
			
			if (node->var_decl.init_node)
			{
				ok &= X_SemaExpr(sema, NULL, node->var_decl.init_node);
				
				if (ok && node->var_decl.type_node)
					ok &= X_SemaConvertableExpr(sema, NULL, node->var_decl.type_node, node->var_decl.init_node);
				else
					desired_type = sema->ast->data[node->var_decl.init_node].type;
			}
			else if (!node->var_decl.type_node)
			{
				X_SemaPushErrorFmt(sema, node_index, "global variable without an explicit type or initialization to infer from");
				ok = false;
				break;
			}
			
			Assert(desired_type);
			node->type = desired_type;
		} break;
		
		case X_AstNodeKind_DeclVar:
		{
			Assert(func);
			
			X_SemaPushSymbol(sema, X_AstSymbolKind_LocalVar, node_index, &node->any_decl.symbol);
			X_Type* desired_type = NULL;
			
			if (node->var_decl.type_node)
			{
				ok &= X_SemaType(sema, NULL, node->var_decl.type_node);
				
				if (ok)
					desired_type = sema->ast->data[node->var_decl.type_node].type;
			}
			
			if (node->var_decl.init_node)
			{
				ok &= X_SemaExpr(sema, NULL, node->var_decl.init_node);
				
				if (ok && node->var_decl.type_node)
					ok &= X_SemaConvertableExpr(sema, NULL, node->var_decl.type_node, node->var_decl.init_node);
				else
					desired_type = sema->ast->data[node->var_decl.init_node].type;
			}
			else if (!node->var_decl.type_node)
			{
				X_SemaPushErrorFmt(sema, node_index, "variable without an explicit type or initialization to infer from");
				ok = false;
				break;
			}
			
			Assert(desired_type);
			node->type = desired_type;
		} break;
		
		case X_AstNodeKind_DeclProc:
		{
			String name = X_TokenAsString(sema->ast->tokens, node->any_decl.name_token);
			X_AstSymbol* new_func = X_SemaPushSymbol(sema, X_AstSymbolKind_Proc, node_index, &node->any_decl.symbol);
			
			// NOTE(ljre): Proc type
			node->type = Arena_PushStruct(sema->output_arena, X_Type);
			node->type->kind = X_TypeKind_Proc;
			
			// NOTE(ljre): Return type
			{
				bool should_push_void_type = true;
				if (node->proc_decl.rettype_node)
				{
					ok &= X_SemaType(sema, NULL, node->proc_decl.rettype_node, true);
					
					if (ok)
					{
						rettype = sema->ast->data[node->proc_decl.rettype_node].type;
						should_push_void_type = false;
					}
				}
				
				if (should_push_void_type)
					Arena_PushStructData(sema->output_arena, X_type_void);
			}
			
			// NOTE(ljre): Parameters
			{
				uint32 param_head = node->proc_decl.param_nodes;
				uint32 param_count = 0;
				
				while (param_head)
				{
					X_AstNode* param = &sema->ast->data[param_head];
					X_SemaType(sema, new_func, param->var_decl.type_node, true);
					
					param_head = param->next;
					++param_count;
				}
				
				node->type->proc.param_count = param_count;
			}
			
			// NOTE(ljre): Parameters' Symbols
			uint32 param_scope = X_SemaPushScope(sema, node_index);
			{
				uint32 param_head = node->proc_decl.param_nodes;
				
				while (param_head)
				{
					X_AstNode* param = &sema->ast->data[param_head];
					X_SemaPushSymbol(sema, X_AstSymbolKind_ProcParam, param_head, &param->var_decl.symbol);
					
					param_head = param->next;
				}
			}
			
			X_SemaBlock(sema, new_func, node->proc_decl.body_node);
			X_SemaPopScope(sema, param_scope);
		} break;
		
		case X_AstNodeKind_DeclType:
		{
			
		} break;
		
		default: Unreachable(); break;
	}
	
	return ok;
}

// @Allocators: output_arena (output), leaky_scratch_arena (error, temp)
static void
X_SemaAst(X_Ast* ast, const X_Allocators* allocators, X_SemaAst_Error* out_error)
{
	Arena* output_arena = allocators->output_arena;
	Arena* scratch_arena = allocators->leaky_scratch_arena;
	Assert(output_arena && scratch_arena);
	
	X_SemaContext* sema = &(X_SemaContext) {
		.output_arena = output_arena,
		.scratch_arena = scratch_arena,
		
		.ast = ast,
		.sym_map = Map_Create(scratch_arena, 1024*4),
		
		.current_scope_inner_decls_counter = NULL,
		
		.error = { .ok = true },
	};
	
	uint32 count = 2048;
	uintsize size = sizeof(X_AstSymbolTable) + sizeof(X_AstSymbol) * count;
	X_AstSymbolTable* table = Arena_PushAligned(sema->output_arena, size, alignof(X_AstSymbolTable));
	
	table->size = count;
	sema->ast->symbol_table = table;
	sema->ast->symbol_count = 1;
	
	uint32 node = ast->first_node;
	while (node)
	{
		X_SemaDecl(sema, NULL, node);
		node = ast->data[node].next;
	}
	
	if (out_error)
		*out_error = sema->error;
	else
		Assert(sema->error.ok);
}
