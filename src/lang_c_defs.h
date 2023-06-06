#ifndef LANG_C_DEFS_H
#define LANG_C_DEFS_H

struct C_SourceLocation typedef C_SourceLocation;

//~ NOTE(ljre): Generic stuff
struct C_HashMapChunk typedef C_HashMapChunk;
struct C_HashMapChunk
{
	C_HashMapChunk* next;
	
	uint32 size, log2cap;
	uint8 memory[];
};

//~ NOTE(ljre): Token kinds
enum C_TokenKind
{
	C_TokenKind_Null = 0,
	C_TokenKind_Eof = 0,
	
	C_TokenKind__FirstKeyword,
	C_TokenKind_Auto = C_TokenKind__FirstKeyword,
	C_TokenKind_Break,
	C_TokenKind_Case,
	C_TokenKind_Char,
	C_TokenKind_Const,
	C_TokenKind_Continue,
	C_TokenKind_Default,
	C_TokenKind_Do,
	C_TokenKind_Double,
	C_TokenKind_Else,
	C_TokenKind_Enum,
	C_TokenKind_Extern,
	C_TokenKind_Float,
	C_TokenKind_For,
	C_TokenKind_Goto,
	C_TokenKind_If,
	C_TokenKind_Inline,
	C_TokenKind_Int,
	C_TokenKind_Long,
	C_TokenKind_Register,
	C_TokenKind_Restrict,
	C_TokenKind_Return,
	C_TokenKind_Short,
	C_TokenKind_Signed,
	C_TokenKind_Sizeof,
	C_TokenKind_Static,
	C_TokenKind_Struct,
	C_TokenKind_Switch,
	C_TokenKind_Typedef,
	C_TokenKind_Union,
	C_TokenKind_Unsigned,
	C_TokenKind_Void,
	C_TokenKind_Volatile,
	C_TokenKind_While,
	C_TokenKind_Bool,
	C_TokenKind_Complex,
	
	// NOTE(ljre): GCC stuff
	C_TokenKind_GccAttribute, // __attribute __attribute__
	C_TokenKind_GccAsm, // __asm__ asm
	C_TokenKind_GccExtension, // __extension__
	C_TokenKind_GccTypeof, // __typeof__
	C_TokenKind_GccAutoType, // __auto_type
	
	// NOTE(ljre): MSVC stuff
	C_TokenKind_MsvcDeclspec, // __declspec
	C_TokenKind_MsvcAsm, // __asm
	C_TokenKind_MsvcForceinline, // __forceinline
	C_TokenKind_MsvcCdecl, // __cdecl
	C_TokenKind_MsvcStdcall, // __stdcall
	C_TokenKind_MsvcVectorcall, // __vectorcall
	C_TokenKind_MsvcFastcall, // __fastcall
	C_TokenKind_MsvcInt8, // __int8
	C_TokenKind_MsvcInt16, // __int16
	C_TokenKind_MsvcInt32, // __int32
	C_TokenKind_MsvcInt64, // __int64
	C_TokenKind_MsvcPragma, // __pragma
	C_TokenKind__LastKeyword = C_TokenKind_MsvcPragma,
	
	C_TokenKind_IntLiteral,
	C_TokenKind__FirstEncodesString = C_TokenKind_IntLiteral,
	C_TokenKind_LIntLiteral,
	C_TokenKind_LLIntLiteral,
	C_TokenKind_UIntLiteral,
	C_TokenKind_LUIntLiteral,
	C_TokenKind_LLUIntLiteral,
	C_TokenKind_StringLiteral,
	C_TokenKind_WideStringLiteral,
	C_TokenKind_FloatLiteral,
	C_TokenKind_DoubleLiteral,
	C_TokenKind_LongDoubleLiteral,
	C_TokenKind_CharLiteral,
	
	C_TokenKind_Identifier,
	C_TokenKind__LastEncodesString = C_TokenKind_Identifier,
	
	C_TokenKind_UnclosedQuote, // NOTE(ljre): Error state. Needed because #warning what's up
	
	C_TokenKind_LeftParen, // (
	C_TokenKind_RightParen, // )
	C_TokenKind_LeftBrkt, // [
	C_TokenKind_RightBrkt, // ]
	C_TokenKind_LeftCurl, // {
	C_TokenKind_RightCurl, // }
	C_TokenKind_Dot, // .
	C_TokenKind_VarArgs, // ...
	C_TokenKind_Arrow, // ->
	C_TokenKind_Comma, // ,
	C_TokenKind_Colon, // :
	C_TokenKind_Semicolon, // ;
	C_TokenKind_QuestionMark, // ?
	C_TokenKind_Plus, // +
	C_TokenKind_Minus, // -
	C_TokenKind_Mul, // *
	C_TokenKind_Div, // /
	C_TokenKind_Mod, // %
	C_TokenKind_LThan, // <
	C_TokenKind_GThan, // >
	C_TokenKind_LEqual, // <=
	C_TokenKind_GEqual, // >=
	C_TokenKind_Equals, // ==
	C_TokenKind_Inc, // ++
	C_TokenKind_Dec, // --
	C_TokenKind_LNot, // !
	C_TokenKind_LAnd, // &&
	C_TokenKind_LOr, // ||
	C_TokenKind_Not, // ~
	C_TokenKind_And, // &
	C_TokenKind_Or, // |
	C_TokenKind_Xor, // ^
	C_TokenKind_LeftShift, // <<
	C_TokenKind_RightShift, // >>
	C_TokenKind_NotEquals, // !=
	C_TokenKind_Assign, // =
	C_TokenKind_PlusAssign, // +=
	C_TokenKind_MinusAssign, // -=
	C_TokenKind_MulAssign, // *=
	C_TokenKind_DivAssign, // /=
	C_TokenKind_ModAssign, // %=
	C_TokenKind_LeftShiftAssign, // <<=
	C_TokenKind_RightShiftAssign, // >>=
	C_TokenKind_AndAssign, // &=
	C_TokenKind_OrAssign, // |=
	C_TokenKind_XorAssign, // ^=
	
	C_TokenKind_Hashtag, // #
	C_TokenKind_HashtagPragma, // #pragma
	C_TokenKind_NewLine,
	C_TokenKind_DoubleHashtag, // ##
	
	C_TokenKind__Count,
}
typedef C_TokenKind;

#define C_GEN_TOKEN_TABLE(X) \
X("do", C_TokenKind_Do) \
X("if", C_TokenKind_If) \
X("for", C_TokenKind_For) \
X("int", C_TokenKind_Int) \
X("_Bool", C_TokenKind_Bool) \
X("auto", C_TokenKind_Auto) \
X("case", C_TokenKind_Case) \
X("char", C_TokenKind_Char) \
X("else", C_TokenKind_Else) \
X("enum", C_TokenKind_Enum) \
X("goto", C_TokenKind_Goto) \
X("long", C_TokenKind_Long) \
X("void", C_TokenKind_Void) \
X("__asm", C_TokenKind_MsvcAsm) \
X("break", C_TokenKind_Break) \
X("const", C_TokenKind_Const) \
X("float", C_TokenKind_Float) \
X("short", C_TokenKind_Short) \
X("union", C_TokenKind_Union) \
X("while", C_TokenKind_While) \
X("__int8", C_TokenKind_MsvcInt8) \
X("double", C_TokenKind_Double) \
X("extern", C_TokenKind_Extern) \
X("inline", C_TokenKind_Inline) \
X("return", C_TokenKind_Return) \
X("signed", C_TokenKind_Signed) \
X("sizeof", C_TokenKind_Sizeof) \
X("static", C_TokenKind_Static) \
X("struct", C_TokenKind_Struct) \
X("switch", C_TokenKind_Switch) \
X("__asm__", C_TokenKind_GccAsm) \
X("__cdecl", C_TokenKind_MsvcCdecl) \
X("__int16", C_TokenKind_MsvcInt16) \
X("__int32", C_TokenKind_MsvcInt32) \
X("__int64", C_TokenKind_MsvcInt64) \
X("default", C_TokenKind_Default) \
X("typedef", C_TokenKind_Typedef) \
X("__pragma", C_TokenKind_MsvcPragma) \
X("_Complex", C_TokenKind_Complex) \
X("continue", C_TokenKind_Continue) \
X("register", C_TokenKind_Register) \
X("restrict", C_TokenKind_Restrict) \
X("unsigned", C_TokenKind_Unsigned) \
X("volatile", C_TokenKind_Volatile) \
X("__stdcall", C_TokenKind_MsvcStdcall) \
X("__declspec", C_TokenKind_MsvcDeclspec) \
X("__fastcall", C_TokenKind_MsvcFastcall) \
X("__typeof__", C_TokenKind_GccTypeof) \
X("__auto_type", C_TokenKind_GccAutoType) \
X("__vectorcall", C_TokenKind_MsvcVectorcall) \
X("__attribute__", C_TokenKind_GccAttribute) \
X("__extension__", C_TokenKind_GccExtension) \
X("__forceinline", C_TokenKind_MsvcForceinline) \

//~ NOTE(ljre): Preprocessor
struct C_PreprocToken
{
	C_TokenKind kind;
	uint32 leading_spaces;
	uint32 line, col;
	String as_string;
}
typedef C_PreprocToken;

struct C_PreprocHideset typedef C_PreprocHideset;
struct C_PreprocHideset
{
	C_PreprocHideset* next;
	String name;
};

struct C_PreprocTokenList typedef C_PreprocTokenList;
struct C_PreprocTokenList
{
	C_PreprocTokenList* next;
	C_PreprocHideset* hideset;
	
	C_SourceLocation* included_from;
	C_SourceLocation* expanded_from;
	
	C_PreprocToken tok;
};

enum C_LoadedFileFlags
{
	C_LoadedFileFlags_Null = 0,
	
	C_LoadedFileFlags_PragmaOnce = 1,
	C_LoadedFileFlags_SystemFile = 2,
	C_LoadedFileFlags_RelativeInclude = 4,
}
typedef C_LoadedFileFlags;

struct C_LoadedFile
{
	uint64 path_hash;
	C_LoadedFileFlags flags;
	
	// NOTE(ljre): Non-null if entry in hashmap exists and exists
	String path;
	String contents;
	
	// NOTE(ljre): Non-null if file could be tokenized
	C_PreprocTokenList* tokens;
}
typedef C_LoadedFile;

enum C_MacroInstKind
{
	C_MacroInstKind_Null = 0,
	
	C_MacroInstKind_CopyTokens, // token0 token1 token2 ... tokenN
	C_MacroInstKind_Argument, // param
	C_MacroInstKind_Stringify, // # param
	C_MacroInstKind_GlueLeft, // param ## token
	C_MacroInstKind_GlueRight, // token ## param
	C_MacroInstKind_GlueArgs, // param1 ## param2
	C_MacroInstKind_CommaVarArgs, // , ## __VA_ARGS__
}
typedef C_MacroInstKind;

struct C_MacroInst
{
	C_MacroInstKind kind;
	
	union
	{
		struct
		{
			uint32 tokens_count;
			C_PreprocTokenList* tokens;
		}
		copy;
		
		struct
		{
			int32 param_index;
			uint32 leading_spaces;
		}
		argument;
		
		struct
		{
			int32 param_index;
			uint32 leading_spaces;
		}
		stringify;
		
		struct
		{
			int32 param_index;
			uint32 leading_spaces;
			C_PreprocTokenList* token;
		}
		glue;
		
		struct
		{
			int32 param1_index;
			int32 param2_index;
			uint32 leading_spaces;
		}
		glue_args;
	};
}
typedef C_MacroInst;

struct C_Macro
{
	String name;
	C_LoadedFile* file;
	C_SourceLocation* definition_loc;
	
	bool is_defined;
	bool is_func_like;
	bool has_va_args;
	uint8 builtin_id;
	
	uint32 replacement_count;
	int32 param_count;
	C_PreprocTokenList* replacement;
	
	uint32 inst_count;
	C_MacroInst* insts;
}
typedef C_Macro;

//~ NOTE(ljre): Source location
struct C_SourceLocation
{
	C_SourceLocation* included_from;
	C_SourceLocation* expanded_from;
	String filepath;
	uint32 leading_spaces;
	uint32 line, col;
};

//~ NOTE(ljre): Parser tokens
struct C_Token
{
	C_TokenKind kind;
	uint32 str_size;
	const uint8* str_data;
	C_SourceLocation* loc;
}
typedef C_Token;

struct C_TokenStream
{
	uint32 size;
	C_Token* tokens;
}
typedef C_TokenStream;

//~ NOTE(ljre): Warnings and errors
enum C_Warning
{
	C_Warning_Null = 0,
	
	C_Warning_WarningDirective, // #warning
	
	C_Warning__Count,
}
typedef C_Warning;

struct C_Error typedef C_Error;
struct C_Error
{
	C_Error* next;
	C_Error* note;
	
	C_Warning warning;
	String what;
	const C_SourceLocation* location;
	uint32 focus_len, focus_point;
};

//~ NOTE(ljre): Translation unit
struct C_AbiType
{
	uint16 size;
	uint8 alignment;
	bool is_unsigned;
}
typedef C_AbiType;

struct C_Abi
{
	union
	{
		struct
		{
			C_AbiType t_bool;
			C_AbiType t_char;
			C_AbiType t_schar;
			C_AbiType t_uchar;
			C_AbiType t_short;
			C_AbiType t_ushort;
			C_AbiType t_int;
			C_AbiType t_uint;
			C_AbiType t_long;
			C_AbiType t_ulong;
			C_AbiType t_longlong;
			C_AbiType t_ulonglong;
			C_AbiType t_float;
			C_AbiType t_double;
			C_AbiType t_ptr;
		};
		
		C_AbiType t[15];
	};
	
	uint8 char_bit;
	
	int8 index_sizet;
	int8 index_ptrdifft;
	int8 index_uintptr;
	int8 index_intptr;
}
typedef C_Abi;

struct C_CompilerOptions
{
	uint64 warnings[(C_Warning__Count + 63) / 64];
	
	const String* include_dirs;
	uintsize include_dirs_count;
	
	const String* predefined_macros;
	uintsize predefined_macros_count;
	
	C_Abi abi;
}
typedef C_CompilerOptions;

struct C_TuContext
{
	Arena* loc_arena;
	Arena* array_arena;
	Arena* tree_arena;
	
	Arena* stage_arena;
	Arena* scratch_arena;
	
	String main_file_name;
	const C_CompilerOptions* options;
	
	C_HashMapChunk* macros_hashmap;
	C_HashMapChunk* files_hashmap;
	
	C_TokenStream preprocessed_source;
	
	uint32 error_count;
	uint32 warning_count;
	
	C_Error* first_error;
	C_Error* last_error;
	C_Error* first_warning;
	C_Error* last_warning;
}
typedef C_TuContext;

#endif //LANG_C_DEFS_H
