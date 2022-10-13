#ifndef COMPILER_DEFS_H
#define COMPILER_DEFS_H

//~ NOTE(ljre): Language spec data
enum X_TokenKind
{
	X_TokenKind_Eof = 0,
	X_TokenKind_Null = 0,
	
	// X_TokenKind_Comment,
	X_TokenKind_Ident,
	X_TokenKind_NumberLiteral,
	X_TokenKind_StringLiteral,
	
	// NOTE(ljre): Alphabetical order
	X_TokenKind_KwAs,
	X_TokenKind__FirstKeyword = X_TokenKind_KwAs,
	X_TokenKind_KwElse,
	X_TokenKind_KwIf,
	X_TokenKind_KwProc,
	X_TokenKind_KwReturn,
	X_TokenKind_KwType,
	X_TokenKind_KwVar,
	X_TokenKind__LastKeyword = X_TokenKind_KwVar,
	
	X_TokenKind_LeftParen,  // (
	X_TokenKind_RightParen, // )
	X_TokenKind_LeftBrkt,   // [
	X_TokenKind_RightBrkt,  // ]
	X_TokenKind_LeftCurl,   // {
	X_TokenKind_RightCurl,  // }
	X_TokenKind_Semicolon,  // ;
	X_TokenKind_Colon,      // :
	X_TokenKind_Comma,      // ,
	X_TokenKind_Dot,        // .
	X_TokenKind_LogicalNot, // !
	
	X_TokenKind_Assign,     // =
	X_TokenKind_AssignPlus, // +=
	X_TokenKind_AssignMinus,// -=
	X_TokenKind_AssignMul,  // *=
	X_TokenKind_AssignDiv,  // /=
	X_TokenKind_AssignMod,  // %=
	X_TokenKind_AssignLeftShift,// <<=
	X_TokenKind_AssignRightShift,// >>=
	X_TokenKind_AssignAnd,  // &=
	X_TokenKind_AssignOr,   // |=
	X_TokenKind_AssignXor,  // ^=
	
	X_TokenKind_LogicalOr,  // ||
	X_TokenKind_LogicalAnd, // &&
	
	X_TokenKind_Equals,     // ==
	X_TokenKind_NotEquals,  // !=
	X_TokenKind_LThan,      // <
	X_TokenKind_GThan,      // >
	X_TokenKind_LEquals,    // <=
	X_TokenKind_GEquals,    // >=
	
	X_TokenKind_Plus,       // +
	X_TokenKind_Minus,      // -
	X_TokenKind_Or,         // |
	X_TokenKind_Xor,        // ^
	
	X_TokenKind_Mul,        // *
	X_TokenKind_Div,        // /
	X_TokenKind_Mod,        // %
	X_TokenKind_And,        // &
	X_TokenKind_LeftShift,  // <<
	X_TokenKind_RightShift, // >>
	
	X_TokenKind__Count,
}
typedef X_TokenKind;

static const String X_token_str_table[X_TokenKind__Count] = {
	[X_TokenKind_Eof] = StrInit("(end of file)"),
	
	[X_TokenKind_Ident] = StrInit("(identifier)"),
	[X_TokenKind_NumberLiteral] = StrInit("(number literal)"),
	[X_TokenKind_StringLiteral] = StrInit("(string literal)"),
	
	[X_TokenKind_KwAs] = StrInit("as"),
	[X_TokenKind_KwElse] = StrInit("else"),
	[X_TokenKind_KwIf] = StrInit("if"),
	[X_TokenKind_KwProc] = StrInit("proc"),
	[X_TokenKind_KwReturn] = StrInit("return"),
	[X_TokenKind_KwType] = StrInit("type"),
	[X_TokenKind_KwVar] = StrInit("var"),
	
	[X_TokenKind_LeftParen] = StrInit("("),
	[X_TokenKind_RightParen] = StrInit(")"),
	[X_TokenKind_LeftBrkt] = StrInit("["),
	[X_TokenKind_RightBrkt] = StrInit("]"),
	[X_TokenKind_LeftCurl] = StrInit("{"),
	[X_TokenKind_RightCurl] = StrInit("}"),
	[X_TokenKind_Semicolon] = StrInit(";"),
	[X_TokenKind_Colon] = StrInit(":"),
	[X_TokenKind_Comma] = StrInit(","),
	[X_TokenKind_Dot] = StrInit("."),
	
	[X_TokenKind_Assign] = StrInit("="),
	[X_TokenKind_AssignPlus] = StrInit("+="),
	[X_TokenKind_AssignMinus] = StrInit("-="),
	[X_TokenKind_AssignMul] = StrInit("*="),
	[X_TokenKind_AssignDiv] = StrInit("/="),
	[X_TokenKind_AssignMod] = StrInit("%="),
	[X_TokenKind_AssignLeftShift] = StrInit("<<="),
	[X_TokenKind_AssignRightShift] = StrInit(">>="),
	[X_TokenKind_AssignAnd] = StrInit("&="),
	[X_TokenKind_AssignOr] = StrInit("|="),
	[X_TokenKind_AssignXor] = StrInit("^="),
	
	[X_TokenKind_LogicalOr] = StrInit("||"),
	[X_TokenKind_LogicalAnd] = StrInit("&&"),
	
	[X_TokenKind_Equals] = StrInit("=="),
	[X_TokenKind_NotEquals] = StrInit("!="),
	[X_TokenKind_LThan] = StrInit("<"),
	[X_TokenKind_GThan] = StrInit(">"),
	[X_TokenKind_LEquals] = StrInit("<="),
	[X_TokenKind_GEquals] = StrInit(">="),
	
	[X_TokenKind_Plus] = StrInit("+"),
	[X_TokenKind_Minus] = StrInit("-"),
	[X_TokenKind_Or] = StrInit("|"),
	[X_TokenKind_Xor] = StrInit("^"),
	
	[X_TokenKind_Mul] = StrInit("*"),
	[X_TokenKind_Div] = StrInit("/"),
	[X_TokenKind_Mod] = StrInit("%"),
	[X_TokenKind_And] = StrInit("&"),
	[X_TokenKind_LeftShift] = StrInit("<<"),
	[X_TokenKind_RightShift] = StrInit(">>"),
};

struct X_OperatorPrecedence
{
	uint8 level: 7; // NOTE(ljre): If == 0, then it's not an operator
	bool right2left: 1;
}
typedef X_OperatorPrecedence;

static_assert(sizeof(X_OperatorPrecedence) == sizeof(uint8));

static const X_OperatorPrecedence X_operator_precedence_table[X_TokenKind__Count] = {
	[X_TokenKind_Comma] = { 1 },
	
	[X_TokenKind_Assign] = { 2, true },
	[X_TokenKind_AssignPlus] = { 2, true },
	[X_TokenKind_AssignMinus] = { 2, true },
	[X_TokenKind_AssignMul] = { 2, true },
	[X_TokenKind_AssignDiv] = { 2, true },
	[X_TokenKind_AssignMod] = { 2, true },
	[X_TokenKind_AssignLeftShift] = { 2, true },
	[X_TokenKind_AssignRightShift] = { 2, true },
	[X_TokenKind_AssignAnd] = { 2, true },
	[X_TokenKind_AssignOr] = { 2, true },
	[X_TokenKind_AssignXor] = { 2, true },
	
	[X_TokenKind_LogicalOr] = { 3 },
	
	[X_TokenKind_LogicalAnd] = { 4 },
	
	[X_TokenKind_Equals] = { 5 },
	[X_TokenKind_NotEquals] = { 5 },
	[X_TokenKind_LThan] = { 5 },
	[X_TokenKind_GThan] = { 5 },
	[X_TokenKind_LEquals] = { 5 },
	[X_TokenKind_GEquals] = { 5 },
	
	[X_TokenKind_Plus] = { 6 },
	[X_TokenKind_Minus] = { 6 },
	[X_TokenKind_Or] = { 6 },
	[X_TokenKind_Xor] = { 6 },
	
	[X_TokenKind_Mul] = { 7 },
	[X_TokenKind_Div] = { 7 },
	[X_TokenKind_Mod] = { 7 },
	[X_TokenKind_And] = { 7 },
	[X_TokenKind_LeftShift] = { 7 },
	[X_TokenKind_RightShift] = { 7 },
};

//~ NOTE(ljre): Tokens
struct X_Token
{
	X_TokenKind kind;
	uint32 str_offset, str_size;
	uint32 leading_spaces;
}
typedef X_Token;

struct X_TokenSlice
{
	uint32 index;
	uint32 len;
}
typedef X_TokenSlice;

struct X_TokenArray
{
	String source;
	
	uint32 size;
	alignas(void*) X_Token data[];
}
typedef X_TokenArray;

//~ NOTE(ljre): AST
enum X_AstNodeKind
{
	X_AstNodeKind_Null = 0,
	
	//- NOTE(ljre): Types
	X_AstNodeKind_TypeVoid,
	X_AstNodeKind_TypeInt8,
	X_AstNodeKind_TypeInt16,
	X_AstNodeKind_TypeInt32,
	X_AstNodeKind_TypeInt64,
	X_AstNodeKind_TypeUInt8,
	X_AstNodeKind_TypeUInt16,
	X_AstNodeKind_TypeUInt32,
	X_AstNodeKind_TypeUInt64,
	X_AstNodeKind_TypeFloat32,
	X_AstNodeKind_TypeFloat64,
	X_AstNodeKind_TypeString,
	
	X_AstNodeKind_TypeName,
	X_AstNodeKind_TypeSlice,
	X_AstNodeKind_TypeArray,
	X_AstNodeKind_TypePointer,
	X_AstNodeKind_TypeProc,
	
	//- NOTE(ljre): Declarations
	X_AstNodeKind_DeclVar,
	X_AstNodeKind_DeclGlobalVar,
	X_AstNodeKind_DeclProc,
	X_AstNodeKind_DeclType,
	X_AstNodeKind_DeclTypeAlias,
	X_AstNodeKind_DeclParam,
	
	//- NOTE(ljre): Statements
	X_AstNodeKind_StmtEmpty,
	X_AstNodeKind_StmtExpr,
	X_AstNodeKind_StmtIf,
	X_AstNodeKind_StmtWhile,
	X_AstNodeKind_StmtReturn,
	
	//- NOTE(ljre): Expressions
	X_AstNodeKind_ExprFactorNumber,
	X_AstNodeKind_ExprFactorString,
	X_AstNodeKind_ExprFactorIdent,
	
	X_AstNodeKind_Expr1Positive,
	X_AstNodeKind_Expr1Negative,
	X_AstNodeKind_Expr1Ref,
	X_AstNodeKind_Expr1Deref,
	X_AstNodeKind_Expr1Call,
	X_AstNodeKind_Expr1Index,
	
	X_AstNodeKind_Expr2Comma,
	X_AstNodeKind_Expr2Assign,
	X_AstNodeKind_Expr2AssignPlus,
	X_AstNodeKind_Expr2AssignMinus,
	X_AstNodeKind_Expr2AssignMul,
	X_AstNodeKind_Expr2AssignDiv,
	X_AstNodeKind_Expr2AssignMod,
	X_AstNodeKind_Expr2AssignLeftShift,
	X_AstNodeKind_Expr2AssignRightShift,
	X_AstNodeKind_Expr2AssignAnd,
	X_AstNodeKind_Expr2AssignOr,
	X_AstNodeKind_Expr2AssignXor,
	
	X_AstNodeKind_Expr2LogicalOr,
	X_AstNodeKind_Expr2LogicalAnd,
	
	X_AstNodeKind_Expr2Equals,
	X_AstNodeKind_Expr2NotEquals,
	X_AstNodeKind_Expr2LThan,
	X_AstNodeKind_Expr2GThan,
	X_AstNodeKind_Expr2LEquals,
	X_AstNodeKind_Expr2GEquals,
	
	X_AstNodeKind_Expr2Plus,
	X_AstNodeKind_Expr2Minus,
	X_AstNodeKind_Expr2Or,
	X_AstNodeKind_Expr2Xor,
	
	X_AstNodeKind_Expr2Mul,
	X_AstNodeKind_Expr2Div,
	X_AstNodeKind_Expr2Mod,
	X_AstNodeKind_Expr2And,
	X_AstNodeKind_Expr2LeftShift,
	X_AstNodeKind_Expr2RightShift,
}
typedef X_AstNodeKind;

// NOTE(ljre): here's the suffix convention for these fields:
//
//  _node:  it's an index to another X_AstNode
//  _nodes: it's an index to a linked-list (through the .next field) of X_AstNodes.
//  _token: it's an index into a X_TokenArray
//  _sym:   it's an index into a X_AstSymbolTable

struct X_AstNode
{
	X_AstNodeKind kind;
	uint32 token;
	uint32 next;
	
	union
	{
		struct
		{
			uint32 name_token;
			uint32 length_node;
			uint32 param_nodes;
		}
		type;
		
		//- NOTE(ljre): Declaration nodes
		//              All of them shall start with a 'name_token' and 'symbol' field.
		struct { uint32 name_token, symbol; } any_decl;
		
		struct
		{
			uint32 name_token, symbol;
			uint32 def_node;
		}
		type_decl;
		
		struct
		{
			uint32 name_token, symbol;
			uint32 type_node;
			uint32 init_node;
		}
		var_decl;
		
		struct
		{
			uint32 name_token, symbol;
			uint32 rettype_node;
			uint32 param_nodes;
			uint32 body_node;
		}
		proc_decl;
		
		//- NOTE(ljre): Statement nodes
		struct
		{
			uint32 cond_node;
			uint32 then_node;
			uint32 else_node;
		}
		if_stmt;
		
		struct
		{
			uint32 expr_node;
		}
		expr_stmt;
		
		struct
		{
			uint32 stmt_nodes;
			uint32 scope_sym;
		}
		block_stmt;
		
		//- NOTE(ljre): Expression nodes
		struct
		{
			uint32 left_node;
			uint32 right_node;
		}
		expr;
		
		struct
		{
			uint32 ref_sym;
		}
		ident_expr;
	};
}
typedef X_AstNode;

struct X_Ast
{
	const X_TokenArray* tokens;
	
	// NOTE(ljre): Available after X_ParseFile
	uint32 first_node;
	uint32 size;
	X_AstNode* data;
}
typedef X_Ast;

#endif //COMPILER_DEFS_H
