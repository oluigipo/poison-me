//~ NOTE(ljre): X_Tokenize* functions
static uintsize
X_IgnoreWhitespacesLeft(const uint8** phead, const uint8* end)
{
	Assert(phead && *phead <= end);
	
	const uint8* head = *phead;
	
	while (head < end && head[0])
	{
		if (head[0] != ' ' && head[0] != '\t' && head[0] != '\r' && head[0] != '\n')
			break;
		
		++head;
	}
	
	uintsize result = head - *phead;
	*phead = head;
	
	return result;
}

static bool
X_IsCharValidIdent(char ch, bool first)
{
	bool result = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch == '_');
	
	if (!first)
		result = result || (ch >= '0' && ch <= '9');
	
	return result;
}

static X_TokenKind
X_FindKeywordByName(String ident)
{
	int32 left = X_TokenKind__FirstKeyword;
	int32 right = X_TokenKind__LastKeyword + 1;
	
	while (left < right)
	{
		int32 i = (left + right) / 2;
		int32 cmp = String_Compare(ident, X_token_str_table[i]);
		
		if (cmp < 0)
			right = i;
		else if (cmp > 0)
			left = i + 1;
		else
			return i;
	}
	
	return X_TokenKind_Null;
}

static void
X_GetLineColumnFromOffset(String source, uintsize offset, uint32* restrict out_line, uint32* restrict out_col)
{
	Assert(offset <= source.size);
	
	uint32 line = 1;
	uint32 col = 1;
	
	for (uintsize i = 0; i < offset; ++i)
	{
		switch (source.data[i])
		{
			case '\n': ++line;
			case '\r': col = 1; break;
			default: ++col; break;
		}
	}
	
	*out_line = line;
	*out_col = col;
}

struct X_TokenizeString_Error
{
	bool ok;
	uintsize source_offset;
	String why;
}
typedef X_TokenizeString_Error;

// @Allocators: output_arena, scratch_arena (temp)
static X_TokenArray*
X_TokenizeString(String source, const Allocators* allocators, X_TokenizeString_Error* out_err)
{
	Arena* output_arena = allocators->output_arena;
	Arena* scratch_arena = allocators->scratch_arena;
	Assert(output_arena && scratch_arena);
	void* scratch_arena_save = Arena_End(scratch_arena);
	
	X_TokenArray* const result = Arena_PushStruct(output_arena, X_TokenArray);
	X_TokenizeString_Error error = { .ok = true, };
	
	const uint8* const begin = source.data;
	const uint8* const end = begin + source.size;
	
	const uint8* head = begin;
	uint32 leading_spaces = 0;
	
	while (error.ok)
	{
		leading_spaces = X_IgnoreWhitespacesLeft(&head, end);
		if (head >= end || !head[0])
			break;
		
		const int32 trailling = end - head;
		X_Token token = {
			.leading_spaces = leading_spaces,
			.str_offset = head - begin,
		};
		
		if (X_IsCharValidIdent(head[0], true))
		{
			do
				++head;
			while (head < end && X_IsCharValidIdent(head[0], false));
			
			token.kind = X_TokenKind_Ident;
			
			String as_string = {
				.data = begin + token.str_offset,
				.size = head - (begin + token.str_offset),
			};
			
			X_TokenKind as_keyword = X_FindKeywordByName(as_string);
			if (as_keyword)
				token.kind = as_keyword;
		}
		else if (head[0] >= '0' && head[0] <= '9')
		{
			do
				++head;
			while (head < end && head[0] >= '0' && head[0] <= '9');
			
			token.kind = X_TokenKind_NumberLiteral;
		}
		else switch (head[0])
		{
			case '(': ++head; token.kind = X_TokenKind_LeftParen; break;
			case ')': ++head; token.kind = X_TokenKind_RightParen; break;
			case '[': ++head; token.kind = X_TokenKind_LeftBrkt; break;
			case ']': ++head; token.kind = X_TokenKind_RightBrkt; break;
			case '{': ++head; token.kind = X_TokenKind_LeftCurl; break;
			case '}': ++head; token.kind = X_TokenKind_RightCurl; break;
			case ';': ++head; token.kind = X_TokenKind_Semicolon; break;
			case ':': ++head; token.kind = X_TokenKind_Colon; break;
			case ',': ++head; token.kind = X_TokenKind_Comma; break;
			case '.': ++head; token.kind = X_TokenKind_Dot; break;
			
#define _CaseTwo(ch1, kind1, ch2, kind2) case ch1: \
{ \
if (trailling > 1 && head[1] == ch2) \
head += 2, token.kind = X_TokenKind_ ## kind2; \
else \
head += 1, token.kind = X_TokenKind_ ## kind1; \
} break
			
			_CaseTwo('=', Assign, '=', Equals);
			_CaseTwo('+', Plus, '=', AssignPlus);
			_CaseTwo('-', Minus, '=', AssignMinus);
			_CaseTwo('*', Mul, '=', AssignMul);
			_CaseTwo('/', Div, '=', AssignDiv);
			_CaseTwo('%', Mod, '=', AssignMod);
			_CaseTwo('!', LogicalNot, '=', NotEquals);
			_CaseTwo('^', Xor, '=', AssignXor);
			
#undef _CaseTwo
			
			case '<':
			{
				if (trailling > 2 && head[1] == '<' && head[2] == '=')
					head += 3, token.kind = X_TokenKind_AssignLeftShift;
				else if (trailling > 1 && head[1] == '=')
					head += 2, token.kind = X_TokenKind_LEquals;
				else if (trailling > 1 && head[1] == '<')
					head += 2, token.kind = X_TokenKind_LeftShift;
				else
					head += 1, token.kind = X_TokenKind_LThan;
			} break;
			
			case '>':
			{
				if (trailling > 2 && head[1] == '>' && head[2] == '=')
					head += 3, token.kind = X_TokenKind_AssignRightShift;
				else if (trailling > 1 && head[1] == '=')
					head += 2, token.kind = X_TokenKind_GEquals;
				else if (trailling > 1 && head[1] == '>')
					head += 2, token.kind = X_TokenKind_RightShift;
				else
					head += 1, token.kind = X_TokenKind_GThan;
			} break;
			
			case '&':
			{
				if (trailling > 1 && head[1] == '&')
					head += 2, token.kind = X_TokenKind_LogicalAnd;
				else if (trailling > 1 && head[1] == '=')
					head += 2, token.kind = X_TokenKind_AssignAnd;
				else
					head += 1, token.kind = X_TokenKind_And;
			} break;
			
			case '|':
			{
				if (trailling > 1 && head[1] == '|')
					head += 2, token.kind = X_TokenKind_LogicalOr;
				else if (trailling > 1 && head[1] == '=')
					head += 2, token.kind = X_TokenKind_AssignOr;
				else
					head += 1, token.kind = X_TokenKind_Or;
			} break;
			
			default:
			{
				error.ok = false;
				error.source_offset = head - begin;
				error.why = Str("unknown token");
				continue; // NOTE(ljre): Exit host loop
			} break;
		}
		
		token.str_size = (head - begin) - token.str_offset;
		
		Arena_PushStructData(output_arena, X_Token, &token);
	}
	
	X_Token* eof = Arena_PushStruct(output_arena, X_Token);
	eof->kind = X_TokenKind_Eof;
	eof->str_offset = head - begin;
	eof->str_size = 0;
	eof->leading_spaces = leading_spaces;
	
	result->size = (eof - result->data) + 1;
	result->source = source;
	
	Arena_Pop(scratch_arena, scratch_arena_save);
	
	if (out_err)
		*out_err = error;
	else
		Assert(error.ok);
	
	return result;
}

//~ NOTE(ljre): X_Token utils
static String
X_TokenAsString(const X_TokenArray* array, uint32 index)
{
	Assert(index < array->size);
	
	const X_Token* token = &array->data[index];
	String result = {
		.data = array->source.data + token->str_offset,
		.size = token->str_size,
	};
	
	return result;
}

struct X_TokenAsNumber_Result
{
	bool ok;
	bool is_float;
	
	union
	{
		int64 value_int;
		uint64 value_uint;
		float64 value_float;
	};
}
typedef X_TokenAsNumber_Result;

static void
X_TokenAsNumber(const X_TokenArray* array, uint32 index, Arena* scratch_arena, X_TokenAsNumber_Result* out_result)
{
	X_TokenAsNumber_Result result = { .ok = true };
	
	String str = X_TokenAsString(array, index);
	char* cstr = Arena_PushDirtyAligned(scratch_arena, str.size + 1, 1);
	cstr[str.size] = 0;
	
	for (int32 i = 0; i < str.size; ++i)
	{
		if (str.data[i] == '.')
		{
			result.is_float = true;
			break;
		}
	}
	
	if (result.is_float)
		result.value_float = (float64)strtod(cstr, NULL);
	else
		result.value_uint = (uint64)strtoull(cstr, NULL, 10);
	
	Arena_Pop(scratch_arena, cstr);
	*out_result = result;
}

//~ NOTE(ljre): X_TokenReader
