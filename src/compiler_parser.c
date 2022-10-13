struct X_ParserError typedef X_ParserError;
struct X_ParserError
{
	X_ParserError* next;
	
	uint32 token;
	String why;
};

struct X_ParseFile_Error
{
	bool ok;
	uint32 count;
	X_ParserError* first;
	X_ParserError* last;
}
typedef X_ParseFile_Error;

struct X_Parser
{
	const X_TokenArray* tokens;
	X_Ast* ast;
	
	Arena* output_arena;
	Arena* scratch_arena;
	
	const X_Token* tok; // NOTE(ljre): Lookahead
	String tok_str;
	
	X_ParseFile_Error error;
}
typedef X_Parser;

//~ NOTE(ljre): Basic helpers
static uint32
X_AstNodeListLength(const X_Ast* ast, uint32 node)
{
	uint32 head = node;
	uint32 result = 0;
	
	while (head)
	{
		++result;
		head = ast->data[head].next;
	}
	
	return result;
}

static uint32
X_ParserHead(X_Parser* parser)
{
	return (uint32)(parser->tok - parser->tokens->data);
}

static void
X_PushParserError(X_Parser* parser, String why)
{
	++parser->error.count;
	X_ParserError* error = Arena_PushStruct(parser->scratch_arena, X_ParserError);
	
	if (!parser->error.first)
		parser->error.first = parser->error.last = error;
	else
		parser->error.last = parser->error.last->next = error;
	
	error->token = X_ParserHead(parser);
	error->why = why;
}

static void
X_PushParserErrorFmt(X_Parser* parser, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	String why = Arena_VSPrintf(parser->scratch_arena, fmt, args);
	va_end(args);
	
	X_PushParserError(parser, why);
}

static void
X_NextToken(X_Parser* parser)
{
	if (parser->tok->kind)
	{
		++parser->tok;
		parser->tok_str = X_TokenAsString(parser->tokens, X_ParserHead(parser));
	}
}

static bool
X_AssertToken(X_Parser* parser, X_TokenKind kind)
{
	Assert(kind > 0 && kind < X_TokenKind__Count);
	
	if (parser->tok->kind != kind)
	{
		String tok_string = X_TokenAsString(parser->tokens, X_ParserHead(parser));
		X_PushParserErrorFmt(parser, "expected '%.*s', but got '%.*s'",
			StrFmt(X_token_str_table[kind]), StrFmt(tok_string));
		return false;
	}
	
	return true;
}

static bool
X_EatToken(X_Parser* parser, X_TokenKind kind)
{
	Assert(kind > 0 && kind < X_TokenKind__Count);
	
	bool result = X_AssertToken(parser, kind);
	X_NextToken(parser);
	
	return result;
}

static bool
X_TryEatToken(X_Parser* parser, X_TokenKind kind)
{
	Assert(kind > 0 && kind < X_TokenKind__Count);
	
	if (parser->tok->kind == kind)
	{
		X_NextToken(parser);
		return true;
	}
	
	return false;
}

static X_AstNode*
X_MakeAstNode(X_Parser* parser, X_AstNodeKind kind, uint32* restrict out_index)
{
	Assert(kind >= 0 && kind < X_TokenKind__Count);
	Assert(out_index);
	
	X_AstNode* node = Arena_PushStruct(parser->output_arena, X_AstNode);
	node->kind = kind;
	node->token = X_ParserHead(parser);
	
	*out_index = parser->ast->size++;
	
	return node;
}

static X_AstNode*
X_UpdateAstNode(X_Parser* parser, X_AstNodeKind kind, uint32 node_index)
{
	Assert(kind >= 0 && kind < X_TokenKind__Count);
	Assert(node_index < parser->ast->size);
	
	X_AstNode* node = &parser->ast->data[node_index];
	node->kind = kind;
	node->token = X_ParserHead(parser);
	
	return node;
}

//~ NOTE(ljre): Actual parsing
static uint32 X_ParseType(X_Parser* parser);
static uint32 X_ParseExpr(X_Parser* parser, int32 level);
static uint32 X_ParseBlock(X_Parser* parser);
static uint32 X_ParseStmt(X_Parser* parser);
static uint32 X_ParseDecl(X_Parser* parser, bool is_global);

static uint32
X_ParseType(X_Parser* parser)
{
	uint32 result = 0;
	uint32* head = &result;
	bool should_loop = true;
	
	while (should_loop)
	{
		switch (parser->tok->kind)
		{
			case X_TokenKind_LeftBrkt:
			{
				X_AstNode* node = X_MakeAstNode(parser, 0, head);
				head = &node->next;
				X_NextToken(parser);
				
				if (parser->tok->kind == X_TokenKind_RightBrkt)
					node->kind = X_AstNodeKind_TypeSlice;
				else
				{
					node->kind = X_AstNodeKind_TypeArray;
					node->type.length_node = X_ParseExpr(parser, 1);
				}
				
				X_EatToken(parser, X_TokenKind_RightBrkt);
			} break;
			
			case X_TokenKind_Mul:
			{
				X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_TypeArray, head);
				head = &node->next;
				X_NextToken(parser);
			} break;
			
			case X_TokenKind_Ident:
			{
				X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_TypeName, head);
				node->type.name_token = X_ParserHead(parser);
				X_NextToken(parser);
				should_loop = false;
				
				String ident = X_TokenAsString(parser->ast->tokens, node->type.name_token);
				
				struct
				{
					String name;
					X_AstNodeKind kind;
				}
				static const table[] = {
					{ StrInit("int8"),  X_AstNodeKind_TypeInt8  },
					{ StrInit("int16"), X_AstNodeKind_TypeInt16 },
					{ StrInit("int32"), X_AstNodeKind_TypeInt32 },
					{ StrInit("int64"), X_AstNodeKind_TypeInt64 },
					
					{ StrInit("uint8"),  X_AstNodeKind_TypeUInt8  },
					{ StrInit("uint16"), X_AstNodeKind_TypeUInt16 },
					{ StrInit("uint32"), X_AstNodeKind_TypeUInt32 },
					{ StrInit("uint64"), X_AstNodeKind_TypeUInt64 },
					
					{ StrInit("float32"), X_AstNodeKind_TypeFloat32 },
					{ StrInit("float64"), X_AstNodeKind_TypeFloat64 },
					
					{ StrInit("string"), X_AstNodeKind_TypeString },
				};
				
				for (int32 i = 0; i < ArrayLength(table); ++i)
				{
					if (String_Equals(ident, table[i].name))
					{
						node->kind = table[i].kind;
						break;
					}
				}
			} break;
			
			default:
			{
				X_PushParserErrorFmt(parser, "expected type, but got '%.*s'", StrFmt(parser->tok_str));
				X_NextToken(parser);
				should_loop = false;
			} break;
		}
	}
	
	return result;
}

static X_AstNodeKind
X_TokenKindToBinaryOp(X_TokenKind kind)
{
	switch (kind)
	{
		case X_TokenKind_Comma: return X_AstNodeKind_Expr2Comma;
		case X_TokenKind_Assign: return X_AstNodeKind_Expr2Assign;
		case X_TokenKind_AssignPlus: return X_AstNodeKind_Expr2AssignPlus;
		case X_TokenKind_AssignMinus: return X_AstNodeKind_Expr2AssignMinus;
		case X_TokenKind_AssignMul: return X_AstNodeKind_Expr2AssignMul;
		case X_TokenKind_AssignDiv: return X_AstNodeKind_Expr2AssignDiv;
		case X_TokenKind_AssignMod: return X_AstNodeKind_Expr2AssignMod;
		case X_TokenKind_AssignLeftShift: return X_AstNodeKind_Expr2AssignLeftShift;
		case X_TokenKind_AssignRightShift: return X_AstNodeKind_Expr2AssignRightShift;
		case X_TokenKind_AssignAnd: return X_AstNodeKind_Expr2AssignAnd;
		case X_TokenKind_AssignOr: return X_AstNodeKind_Expr2AssignOr;
		case X_TokenKind_AssignXor: return X_AstNodeKind_Expr2AssignXor;
		
		case X_TokenKind_LogicalOr: return X_AstNodeKind_Expr2LogicalOr;
		case X_TokenKind_LogicalAnd: return X_AstNodeKind_Expr2LogicalAnd;
		
		case X_TokenKind_Equals: return X_AstNodeKind_Expr2Equals;
		case X_TokenKind_NotEquals: return X_AstNodeKind_Expr2NotEquals;
		case X_TokenKind_LThan: return X_AstNodeKind_Expr2LThan;
		case X_TokenKind_GThan: return X_AstNodeKind_Expr2GThan;
		case X_TokenKind_LEquals: return X_AstNodeKind_Expr2LEquals;
		case X_TokenKind_GEquals: return X_AstNodeKind_Expr2GEquals;
		
		case X_TokenKind_Plus: return X_AstNodeKind_Expr2Plus;
		case X_TokenKind_Minus: return X_AstNodeKind_Expr2Minus;
		case X_TokenKind_Or: return X_AstNodeKind_Expr2Or;
		case X_TokenKind_Xor: return X_AstNodeKind_Expr2Xor;
		
		case X_TokenKind_Mul: return X_AstNodeKind_Expr2Mul;
		case X_TokenKind_Div: return X_AstNodeKind_Expr2Div;
		case X_TokenKind_Mod: return X_AstNodeKind_Expr2Mod;
		case X_TokenKind_And: return X_AstNodeKind_Expr2And;
		case X_TokenKind_LeftShift: return X_AstNodeKind_Expr2LeftShift;
		case X_TokenKind_RightShift: return X_AstNodeKind_Expr2RightShift;
		
		default: Unreachable(); break;
	}
}

static uint32
X_ParseExprFactor(X_Parser* parser)
{
	uint32 result = 0;
	uint32* head = &result;
	
	//- NOTE(ljre): Parse prefix
	while (true)
	{
		X_AstNodeKind op_to_add = 0;
		
		switch (parser->tok->kind)
		{
			case X_TokenKind_Plus:  op_to_add = X_AstNodeKind_Expr1Positive; break;
			case X_TokenKind_Minus: op_to_add = X_AstNodeKind_Expr1Negative; break;
			case X_TokenKind_And:   op_to_add = X_AstNodeKind_Expr1Ref; break;
			case X_TokenKind_Mul:   op_to_add = X_AstNodeKind_Expr1Deref; break;
			default: break;
		}
		
		if (!op_to_add)
			break;
		else
		{
			X_AstNode* node = X_MakeAstNode(parser, op_to_add, head);
			head = &node->expr.left_node;
			X_NextToken(parser);
		}
	}
	
	//- NOTE(ljre): Parse factor
	switch (parser->tok->kind)
	{
		case X_TokenKind_NumberLiteral:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_ExprFactorNumber, head);
			(void)node;
			X_NextToken(parser);
		} break;
		
		case X_TokenKind_StringLiteral:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_ExprFactorString, head);
			(void)node;
			X_NextToken(parser);
		} break;
		
		case X_TokenKind_LeftParen:
		{
			X_NextToken(parser);
			*head = X_ParseExpr(parser, 0);
			X_EatToken(parser, X_TokenKind_RightParen);
		} break;
		
		case X_TokenKind_Ident:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_ExprFactorIdent, head);
			(void)node;
			X_NextToken(parser);
		} break;
		
		default:
		{
			X_PushParserErrorFmt(parser, "expected expression, but got '%.*s'", StrFmt(parser->tok_str));
			X_NextToken(parser);
		} break;
	}
	
	//- NOTE(ljre): Parse postfix
	while (true)
	{
		switch (parser->tok->kind)
		{
			case X_TokenKind_LeftParen:
			{
				uint32 node_index;
				X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_Expr1Call, &node_index);
				node->expr.left_node = *head;
				X_NextToken(parser);
				
				uint32* param_head = &node->expr.right_node;
				while (parser->tok->kind && parser->tok->kind != X_TokenKind_RightParen)
				{
					*param_head = X_ParseExpr(parser, 1);
					X_AstNode* param = &parser->ast->data[*param_head];
					param_head = &param->next;
					
					if (!X_TryEatToken(parser, X_TokenKind_Comma))
						break;
				}
				
				X_EatToken(parser, X_TokenKind_RightParen);
				
				*head = node_index;
			} continue;
			
			case X_TokenKind_LeftBrkt:
			{
				uint32 node_index;
				X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_Expr1Index, &node_index);
				node->expr.left_node = *head;
				X_NextToken(parser);
				
				node->expr.right_node = X_ParseExpr(parser, 0);
				X_EatToken(parser, X_TokenKind_RightBrkt);
				
				*head = node_index;
			} continue;
			
			default: break;
		}
		
		break;
	}
	
	return result;
}

static uint32
X_ParseExpr(X_Parser* parser, int32 level)
{
	// NOTE(ljre): This is an "Operator Precedence Parser". Yes, it's confusing as hell.
	
	uint32 result = X_ParseExprFactor(parser);
	X_OperatorPrecedence top_prec;
	
	while (top_prec = X_operator_precedence_table[parser->tok->kind],
		top_prec.level > level)
	{
		X_AstNodeKind op = X_TokenKindToBinaryOp(parser->tok->kind);
		X_NextToken(parser);
		
		uint32 right = X_ParseExprFactor(parser);
		X_TokenKind lookahead = parser->tok->kind;
		X_OperatorPrecedence lookahead_prec = X_operator_precedence_table[lookahead];
		
		while (lookahead_prec.level > 0 &&
			(lookahead_prec.level > top_prec.level ||
				(lookahead_prec.level == top_prec.level && lookahead_prec.right2left)))
		{
			uint32 tmp_index;
			X_AstNode* tmp = X_MakeAstNode(parser, X_TokenKindToBinaryOp(lookahead), &tmp_index);
			X_NextToken(parser);
			
			tmp->expr.left_node = right;
			tmp->expr.right_node = X_ParseExpr(parser, lookahead_prec.level - 1);
			
			lookahead = parser->tok->kind;
			lookahead_prec = X_operator_precedence_table[lookahead];
			right = tmp_index;
		}
		
		uint32 tmp_index;
		X_AstNode* tmp = X_MakeAstNode(parser, op, &tmp_index);
		tmp->expr.left_node = result;
		tmp->expr.right_node = right;
		
		result = tmp_index;
	}
	
	return result;
}

static uint32
X_ParseBlock(X_Parser* parser)
{
	Assert(parser->tok->kind == X_TokenKind_LeftCurl);
	
	uint32 result = 0;
	uint32* head = &result;
	X_NextToken(parser);
	
	while (parser->tok->kind && parser->tok->kind != X_TokenKind_RightCurl)
	{
		*head = X_ParseStmt(parser);
		
		if (*head)
			head = &parser->ast->data[*head].next;
	}
	
	X_EatToken(parser, X_TokenKind_RightCurl);
	
	return result;
}

static uint32
X_ParseStmt(X_Parser* parser)
{
	uint32 result = 0;
	
	switch (parser->tok->kind)
	{
		case X_TokenKind_LeftCurl:
		{
			result = X_ParseBlock(parser);
		} break;
		
		case X_TokenKind_KwVar:
		case X_TokenKind_KwProc:
		case X_TokenKind_KwType:
		{
			result = X_ParseDecl(parser, false);
		} break;
		
		case X_TokenKind_KwIf:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_StmtIf, &result);
			X_NextToken(parser);
			
			X_EatToken(parser, X_TokenKind_LeftParen);
			node->if_stmt.cond_node = X_ParseExpr(parser, 0);
			X_EatToken(parser, X_TokenKind_RightParen);
			
			node->if_stmt.then_node = X_ParseStmt(parser);
			
			if (X_TryEatToken(parser, X_TokenKind_KwElse))
				node->if_stmt.else_node = X_ParseStmt(parser);
		} break;
		
		case X_TokenKind_KwReturn:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_StmtReturn, &result);
			X_NextToken(parser);
			
			node->expr_stmt.expr_node = X_ParseExpr(parser, 0);
			X_EatToken(parser, X_TokenKind_Semicolon);
		} break;
		
		case X_TokenKind_Semicolon:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_StmtEmpty, &result);
			X_NextToken(parser);
			
			(void)node;
		} break;
		
		default:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_StmtExpr, &result);
			node->expr_stmt.expr_node = X_ParseExpr(parser, 0);
			X_EatToken(parser, X_TokenKind_Semicolon);
		} break;
	}
	
	return result;
}

static uint32
X_ParseDecl(X_Parser* parser, bool is_global)
{
	uint32 result = 0;
	
	switch (parser->tok->kind)
	{
		case X_TokenKind_KwVar:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_DeclGlobalVar, &result);
			X_NextToken(parser);
			
			if (!X_AssertToken(parser, X_TokenKind_Ident))
				break;
			
			node->var_decl.name_token = X_ParserHead(parser);
			X_NextToken(parser);
			
			if (X_TryEatToken(parser, X_TokenKind_Colon))
				node->var_decl.type_node = X_ParseType(parser);
			
			if (X_TryEatToken(parser, X_TokenKind_Assign))
				node->var_decl.init_node = X_ParseExpr(parser, 1);
			
			X_EatToken(parser, X_TokenKind_Semicolon);
		} break;
		
		case X_TokenKind_KwType:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_DeclType, &result);
			X_NextToken(parser);
			
			if (!X_AssertToken(parser, X_TokenKind_Ident))
				break;
			
			node->var_decl.name_token = X_ParserHead(parser);
			X_NextToken(parser);
			
			if (X_TryEatToken(parser, X_TokenKind_Assign))
				node->var_decl.init_node = X_ParseType(parser);
			
			X_EatToken(parser, X_TokenKind_Semicolon);
		} break;
		
		case X_TokenKind_KwProc:
		{
			X_AstNode* node = X_MakeAstNode(parser, X_AstNodeKind_DeclProc, &result);
			X_NextToken(parser);
			
			if (!X_AssertToken(parser, X_TokenKind_Ident))
				break;
			
			node->proc_decl.name_token = X_ParserHead(parser);
			X_NextToken(parser);
			
			X_EatToken(parser, X_TokenKind_LeftParen);
			uint32* param_head = &node->proc_decl.param_nodes;
			while (parser->tok->kind && parser->tok->kind != X_TokenKind_RightParen)
			{
				X_AstNode* param = X_MakeAstNode(parser, X_AstNodeKind_DeclParam, param_head);
				param->var_decl.name_token = X_ParserHead(parser);
				X_NextToken(parser);
				
				X_EatToken(parser, X_TokenKind_Colon);
				param->var_decl.type_node = X_ParseType(parser);
				
				if (X_TryEatToken(parser, X_TokenKind_Assign))
					param->var_decl.init_node = X_ParseExpr(parser, 1);
				
				if (!X_TryEatToken(parser, X_TokenKind_Comma))
					break;
			}
			
			X_EatToken(parser, X_TokenKind_RightParen);
			
			node->proc_decl.body_node = X_ParseBlock(parser);
		} break;
		
		default:
		{
			X_PushParserErrorFmt(parser, "expected declaration, but got '%.*s'", StrFmt(parser->tok_str));
			X_NextToken(parser);
		} break;
	}
	
	return result;
}

//~ NOTE(ljre): Internal API
// @Allocators: output_arena (output), leaky_scratch_arena (errors, temp)
static X_Ast*
X_ParseFile(const X_TokenArray* tokens, const X_Allocators* allocators, X_ParseFile_Error* out_error)
{
	Arena* output_arena = allocators->output_arena;
	Arena* scratch_arena = allocators->leaky_scratch_arena;
	Assert(output_arena && scratch_arena);
	
	X_Parser* const parser = &(X_Parser) {
		.tokens = tokens,
		.ast = Arena_PushStruct(output_arena, X_Ast),
		.output_arena = output_arena,
		.scratch_arena = scratch_arena,
		.tok = tokens->data,
		
		.error = { .ok = true },
	};
	
	parser->ast->tokens = tokens;
	parser->ast->data = Arena_EndAligned(output_arena, alignof(X_AstNode));
	
	{
		uint32 zero;
		X_MakeAstNode(parser, X_AstNodeKind_Null, &zero);
		(void)zero;
	}
	
	uint32* next = &parser->ast->first_node;
	while (parser->tok->kind)
	{
		*next = X_ParseDecl(parser, true);
		
		if (*next)
			next = &parser->ast->data[*next].next;
	}
	
	if (out_error)
		*out_error = parser->error;
	else
		Assert(parser->error.ok);
	
	return parser->ast;
}
