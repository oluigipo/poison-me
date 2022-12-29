//~ NOTE(ljre): Main lexer
static inline bool
C_IsNumberChar(uint8 ch, int32 base)
{
	switch (base)
	{
		case 2: return (ch == '0' || ch == '1');
		case 8: return (ch >= '0' && ch <= '7');
		case 10: return (ch >= '0' && ch <= '9');
		case 16: return (ch >= '0' && ch <= '9' || ch >= 'A' && ch <= 'F' || ch >= 'a' && ch <= 'f');
	}
	
	Assert(false);
	return 0;
}

static inline bool
C_IsIdentChar(uint8 ch, bool first)
{
	bool result = (ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch == '_');
	
	if (!first)
		result |= (ch >= '0' && ch <= '9');
	
	return result;
}

static inline uintsize
C_IgnoreWhitespaces(const uint8** phead, const uint8* end, bool include_linebreak, bool include_comments)
{
	const uint8* head = *phead;
	Assert(head <= end);
	
	for (;;)
	{
		if (head >= end)
			break;
		
		if (head+2 <= end && head[0] == '/' && head[1] == '/')
		{
			head += 2;
			
			while (head < end && head[0] != '\n')
			{
				if (head+1 < end && head[0] == '\\')
					++head;
				++head;
			}
		}
		
		if (head[0] != ' ' && head[0] != '\t' && head[0] != '\r' && !(include_linebreak && head[0] == '\n'))
			break;
		
		++head;
	}
	
	uintsize count = head - *phead;
	*phead = head;
	
	return count;
}

static inline String
C_TokenizeStringLiteral(const uint8** phead, const uint8* end, uint8 close_char, int32 ignore_first_n)
{
	const uint8* head = *phead + ignore_first_n;
	bool unclosed = true;
	
	while (head < end)
	{
		if (head + 1 < end && head[0] == '\\')
			head += 2;
		else if (head[0] == '\n')
			break;
		else if (head[0] == close_char)
		{
			++head;
			unclosed = false;
		}
		else
			++head;
	}
	
	String result = StrNull;
	
	if (!unclosed)
	{
		result = StrMake(*phead - head, *phead);
		*phead = head;
	}
	else
		*phead += ignore_first_n;
	
	return result;
}

static inline C_TokenKind
C_FindKeywordByName(String name)
{
#define X(name, kind) { (uint8)(sizeof(name)-1), name, kind },
	struct
	{
		uint8 size;
		char name[31];
		C_TokenKind kind;
	}
	static const table[] = { C_GEN_TOKEN_TABLE(X) };
#undef X
	
	int32 left = 0;
	int32 right = ArrayLength(table);
	
	while (left < right)
	{
		int32 index = (left + right) / 2;
		int32 cmp = (int32)name.size - table[index].size;
		
		if (cmp == 0)
		{
			int32 len = name.size;
			
			const uint8* a = name.data;
			const uint8* b = (uint8*)table[index].name;
			
			while (len--)
			{
				if (*a != *b)
				{
					cmp = *a - *b;
					goto lbl_failed;
				}
				
				++a;
				++b;
			}
			
			return table[index].kind;
		}
		
		lbl_failed:;
		if (cmp < 0)
			right = index;
		else if (cmp > 0)
			left = index + 1;
	}
	
	return C_TokenKind_Null;
}

static C_PreprocTokenList*
C_TokenizeForPreproc(C_TuContext* tu, Arena* output_arena, String source, C_Error* out_error)
{
	Arena_Savepoint arena_save = Arena_Save(output_arena);
	C_PreprocTokenList* result = NULL;
	C_PreprocTokenList** tok_head = &result;
	
	bool ok = true;
	
	const uint8* const begin = source.data;
	const uint8* const end = source.data + source.size;
	
	const uint8* head = begin;
	
	uint32 line = 1;
	uint32 col = 1;
	const uint8* prev_head = head;
	
	uint32 leading_spaces;
	
	while (ok && head < end)
	{
		leading_spaces = C_IgnoreWhitespaces(&head, end, false, true);
		if (head >= end)
			break;
		
		while (prev_head < head)
		{
			switch (*prev_head++)
			{
				case '\n': ++line;
				case '\r': col = 1; break;
				default: ++col; break;
			}
		}
		
		const uint8* const token_begin = head;
		C_PreprocToken token = {
			.kind = C_TokenKind_Null,
			.leading_spaces = leading_spaces,
			.line = line,
			.col = col,
		};
		
		leading_spaces = 0;
		
		switch (head[0])
		{
			case '\n':
			{
				token.kind = C_TokenKind_NewLine;
				++head;
			} break;
			
			case '#':
			{
				token.kind = C_TokenKind_Hashtag;
				++head;
				
				if (head < end && head[0] == '#')
				{
					token.kind = C_TokenKind_DoubleHashtag;
					++head;
				}
			} break;
			
			case '"':
			{
				String r = C_TokenizeStringLiteral(&head, end, '"', 1);
				
				if (r.size == 0)
					token.kind = C_TokenKind_UnclosedQuote;
				else
					token.kind = C_TokenKind_StringLiteral;
			} break;
			
			case '\'':
			{
				String r = C_TokenizeStringLiteral(&head, end, '\'', 1);
				
				if (r.size == 0)
					token.kind = C_TokenKind_UnclosedQuote;
				else
					token.kind = C_TokenKind_CharLiteral;
			} break;
			
			case '.':
			{
				if (head + 2 < end && head[1] == '.' && head[2] == '.')
				{
					token.kind = C_TokenKind_VarArgs;
					head += 2;
					break;
				}
				else if (head + 1 < end && !C_IsNumberChar(head[1], 10))
				{
					token.kind = C_TokenKind_Dot;
					++head;
					break;
				}
			} /* fallthrough */
			
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
			{
				int32 base = 10;
				
				if (head[0] == '0')
				{
					++head;
					base = 8;
					
					if (head < end)
					{
						if (head[0] == 'x')
							base = 16, ++head;
						else if (head[0] == 'b')
							base = 2, ++head;
					}
				}
				
				while (head < end && C_IsNumberChar(head[0], base))
					++head;
				
				if (head < end && head[0] == '.')
				{
					++head;
					Assert(base == 10 || base == 16);
					
					while (head < end && C_IsNumberChar(head[0], base))
						++head;
					
					token.kind = C_TokenKind_DoubleLiteral;
					
					if (head < end)
					{
						if (head[0] == 'f' || head[0] == 'F')
							token.kind = C_TokenKind_FloatLiteral, ++head;
						else if (head[0] == 'l' || head[0] == 'L')
							token.kind = C_TokenKind_LongDoubleLiteral, ++head;
					}
				}
				else
				{
					// 000 -- is_unsigned, is_longlong, is_long
					int32 modes = 0;
					
					while (head < end)
					{
						if (!(modes & 4) && head[0] == 'u' || head[0] == 'U')
							modes |= 4, ++head;
						else if (!(modes & 2) && head[0] == 'l' || head[0] == 'L')
							modes += 1, ++head;
						else
							break;
					}
					
					switch (modes)
					{
						case 0: token.kind = C_TokenKind_IntLiteral;    break;
						case 1: token.kind = C_TokenKind_LIntLiteral;   break;
						case 2: token.kind = C_TokenKind_LLIntLiteral;  break;
						case 4: token.kind = C_TokenKind_UIntLiteral;    break;
						case 5: token.kind = C_TokenKind_LUIntLiteral;   break;
						case 6: token.kind = C_TokenKind_LLUIntLiteral;  break;
						
						default: Assert(false); break;
					}
				}
			} break;
			
			case 'L':
			{
				if (head + 1 < end && head[1] == '"')
				{
					String r = C_TokenizeStringLiteral(&head, end, '"', 2);
					
					if (r.size == 0)
						token.kind = C_TokenKind_UnclosedQuote;
					else
						token.kind = C_TokenKind_WideStringLiteral;
					
					break;
				}
				
				goto lbl_parse_ident;
			} break;
			
			case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I':
			case 'J': case 'K': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': 
			case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
			case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i':
			case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
			case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
			case '_': lbl_parse_ident:
			{
				do
					++head;
				while (head < end && C_IsIdentChar(head[0], false));
				
				token.kind = C_TokenKind_Identifier;
			} break;
			
			case '(': token.kind = C_TokenKind_LeftParen; ++head; break;
			case ')': token.kind = C_TokenKind_RightParen; ++head; break;
			case '[': token.kind = C_TokenKind_LeftBrkt; ++head; break;
			case ']': token.kind = C_TokenKind_RightBrkt; ++head; break;
			case '{': token.kind = C_TokenKind_LeftCurl; ++head; break;
			case '}': token.kind = C_TokenKind_RightCurl; ++head; break;
			case ',': token.kind = C_TokenKind_Comma; ++head; break;
			case ':': token.kind = C_TokenKind_Colon; ++head; break;
			case ';': token.kind = C_TokenKind_Semicolon; ++head; break;
			case '?': token.kind = C_TokenKind_QuestionMark; ++head; break;
			case '~': token.kind = C_TokenKind_Not; ++head; break;
			
			case '/':
			{
				token.kind = C_TokenKind_Div;
				++head;
				
				if (head < end && head[0] == '=')
				{
					token.kind = C_TokenKind_DivAssign;
					++head;
				}
			} break;
			
			case '-':
			{
				token.kind = C_TokenKind_Minus;
				++head;
				
				if (head < end)
				{
					if (head[0] == '>')
					{
						token.kind = C_TokenKind_Arrow;
						++head;
					}
					else if (head[0] == '-')
					{
						token.kind = C_TokenKind_Dec;
						++head;
					}
					else if (head[0] == '=')
					{
						token.kind = C_TokenKind_MinusAssign;
						++head;
					}
				}
			} break;
			
			case '+':
			{
				token.kind = C_TokenKind_Plus;
				++head;
				
				if (head < end)
				{
					if (head[0] == '+')
					{
						token.kind = C_TokenKind_Inc;
						++head;
					}
					else if (head[0] == '=')
					{
						token.kind = C_TokenKind_PlusAssign;
						++head;
					}
				}
			} break;
			
			case '*':
			{
				token.kind = C_TokenKind_Mul;
				++head;
				
				if (head < end && head[0] == '=')
				{
					token.kind = C_TokenKind_MulAssign;
					++head;
				}
			} break;
			
			case '%':
			{
				token.kind = C_TokenKind_Mod;
				++head;
				
				if (head < end && head[0] == '=')
				{
					token.kind = C_TokenKind_ModAssign;
					++head;
				}
			} break;
			
			case '<':
			{
				token.kind = C_TokenKind_LThan;
				++head;
				
				if (head < end)
				{
					if (head[0] == '<')
					{
						token.kind = C_TokenKind_LeftShift;
						++head;
						
						if (head < end && head[0] == '=')
						{
							token.kind = C_TokenKind_LeftShiftAssign;
							++head;
						}
					}
					else if (head[0] == '=')
					{
						token.kind = C_TokenKind_LEqual;
						++head;
					}
				}
			} break;
			
			case '>':
			{
				token.kind = C_TokenKind_GThan;
				++head;
				
				if (head < end)
				{
					if (head[0] == '>')
					{
						token.kind = C_TokenKind_RightShift;
						++head;
						
						if (head < end && head[0] == '=')
						{
							token.kind = C_TokenKind_RightShiftAssign;
							++head;
						}
					}
					else if (head[0] == '=')
					{
						token.kind = C_TokenKind_GEqual;
						++head;
					}
				}
			} break;
			
			case '=':
			{
				token.kind = C_TokenKind_Assign;
				++head;
				
				if (head < end && head[0] == '=')
				{
					token.kind = C_TokenKind_Equals;
					++head;
				}
			} break;
			
			case '!':
			{
				token.kind = C_TokenKind_LNot;
				++head;
				
				if (head < end && head[0] == '=')
				{
					token.kind = C_TokenKind_NotEquals;
					++head;
				}
			} break;
			
			case '&':
			{
				token.kind = C_TokenKind_And;
				++head;
				
				if (head < end)
				{
					if (head[0] == '&')
					{
						token.kind = C_TokenKind_LAnd;
						++head;
					}
					else if (head[0] == '=')
					{
						token.kind = C_TokenKind_AndAssign;
						++head;
					}
				}
			} break;
			
			case '|':
			{
				token.kind = C_TokenKind_Or;
				++head;
				
				if (head < end)
				{
					if (head[0] == '|')
					{
						token.kind = C_TokenKind_LOr;
						++head;
					}
					else if (head[0] == '=')
					{
						token.kind = C_TokenKind_OrAssign;
						++head;
					}
				}
			} break;
			
			case '^':
			{
				token.kind = C_TokenKind_Xor;
				++head;
				
				if (head < end && head[0] == '=')
				{
					token.kind = C_TokenKind_XorAssign;
					++head;
				}
			} break;
			
			default:
			{
				ok = false;
			} break;
		}
		
		token.as_string = StrRange(token_begin, head);
		
		C_PreprocTokenList node = { .tok = token };
		
		*tok_head = Arena_PushStructData(output_arena, C_PreprocTokenList, &node);
		tok_head = &(*tok_head)->next;
	}
	
	if (!ok)
	{
		// TODO
		Arena_Restore(arena_save);
		return NULL;
	}
	
	return result;
}

//~ NOTE(ljre): Utils
static inline bool
C_TokenizeInt(Arena* scratch_arena, String str, uint64* out_value)
{
	bool ok = true;
	uint64 value = 0;
	
	for Arena_TempScope(scratch_arena)
	{
		const char* cstr = (const char*)Arena_PushString(scratch_arena, str).data;
		Arena_PushData(scratch_arena, &""); // NOTE(ljre): Null terminator
		
		char* end;
		uint64 temp = strtoll(cstr, &end, 0);
		
		if (end - cstr != str.size)
			ok = false;
		else
			value = temp;
	}
	
	*out_value = value;
	return ok;
}

static inline bool
C_TokenizeFloat(Arena* scratch_arena, String str, float64* out_value)
{
	bool ok = true;
	float64 value = 0;
	
	for Arena_TempScope(scratch_arena)
	{
		const char* cstr = (const char*)Arena_PushString(scratch_arena, str).data;
		Arena_PushData(scratch_arena, &""); // NOTE(ljre): Null terminator
		
		char* end;
		float64 temp = strtod(cstr, &end);
		
		if (end - cstr != str.size)
			ok = false;
		else
			value = temp;
	}
	
	*out_value = value;
	return ok;
}

static String
C_TokenAsString(C_Token tok)
{ return StrMake(tok.str_size, tok.str_data); }

static String
C_TokenKindAsString(C_TokenKind kind)
{
	String s = StrNull;
	
	switch (kind)
	{
		//case C_TokenKind_Null: Unreachable(); break;
		default: Unreachable(); break;
		
		case C_TokenKind_Auto: s = Str("auto"); break;
		case C_TokenKind_Break: s = Str("break"); break;
		case C_TokenKind_Case: s = Str("case"); break;
		case C_TokenKind_Char: s = Str("char"); break;
		case C_TokenKind_Const: s = Str("const"); break;
		case C_TokenKind_Continue: s = Str("continue"); break;
		case C_TokenKind_Default: s = Str("default"); break;
		case C_TokenKind_Do: s = Str("do"); break;
		case C_TokenKind_Double: s = Str("double"); break;
		case C_TokenKind_Else: s = Str("else"); break;
		case C_TokenKind_Enum: s = Str("enum"); break;
		case C_TokenKind_Extern: s = Str("extern"); break;
		case C_TokenKind_Float: s = Str("float"); break;
		case C_TokenKind_For: s = Str("for"); break;
		case C_TokenKind_Goto: s = Str("goto"); break;
		case C_TokenKind_If: s = Str("if"); break;
		case C_TokenKind_Inline: s = Str("inline"); break;
		case C_TokenKind_Int: s = Str("int"); break;
		case C_TokenKind_Long: s = Str("long"); break;
		case C_TokenKind_Register: s = Str("register"); break;
		case C_TokenKind_Restrict: s = Str("restrict"); break;
		case C_TokenKind_Return: s = Str("return"); break;
		case C_TokenKind_Short: s = Str("short"); break;
		case C_TokenKind_Signed: s = Str("signed"); break;
		case C_TokenKind_Sizeof: s = Str("sizeof"); break;
		case C_TokenKind_Static: s = Str("static"); break;
		case C_TokenKind_Struct: s = Str("struct"); break;
		case C_TokenKind_Switch: s = Str("switch"); break;
		case C_TokenKind_Typedef: s = Str("typedef"); break;
		case C_TokenKind_Union: s = Str("union"); break;
		case C_TokenKind_Unsigned: s = Str("unsigned"); break;
		case C_TokenKind_Void: s = Str("void"); break;
		case C_TokenKind_Volatile: s = Str("volatile"); break;
		case C_TokenKind_While: s = Str("while"); break;
		case C_TokenKind_Bool: s = Str("_Bool"); break;
		case C_TokenKind_Complex: s = Str("_Complex"); break;
		
		case C_TokenKind_GccAttribute: s = Str("__attribute__"); break;
		case C_TokenKind_GccAsm: s = Str("__asm__"); break;
		case C_TokenKind_GccExtension: s = Str("__extension__"); break;
		case C_TokenKind_GccTypeof: s = Str("__typeof__"); break;
		case C_TokenKind_GccAutoType: s = Str("__autotype__"); break;
		
		case C_TokenKind_MsvcDeclspec: s = Str("__declspec"); break;
		case C_TokenKind_MsvcAsm: s = Str("__asm"); break;
		case C_TokenKind_MsvcForceinline: s = Str("__forceinline"); break;
		case C_TokenKind_MsvcCdecl: s = Str("__cdecl"); break;
		case C_TokenKind_MsvcStdcall: s = Str("__stdcall"); break;
		case C_TokenKind_MsvcVectorcall: s = Str("__vectorcall"); break;
		case C_TokenKind_MsvcFastcall: s = Str("__fastcall"); break;
		case C_TokenKind_MsvcPragma: s = Str("__pragma"); break;
		case C_TokenKind_MsvcInt8: s = Str("__int8"); break;
		case C_TokenKind_MsvcInt16: s = Str("__int16"); break;
		case C_TokenKind_MsvcInt32: s = Str("__int32"); break;
		case C_TokenKind_MsvcInt64: s = Str("__int64"); break;
		
		case C_TokenKind_IntLiteral: s = Str("(int literal)"); break;
		case C_TokenKind_LIntLiteral: s = Str("(long literal)"); break;
		case C_TokenKind_LLIntLiteral: s = Str("(long long literal)"); break;
		case C_TokenKind_UIntLiteral: s = Str("(unsigned literal)"); break;
		case C_TokenKind_LUIntLiteral: s = Str("(unsigned long literal)"); break;
		case C_TokenKind_LLUIntLiteral: s = Str("(unsigned long long literal)"); break;
		case C_TokenKind_StringLiteral: s = Str("(char[] literal)"); break;
		case C_TokenKind_WideStringLiteral: s = Str("(wchar_t[] literal)"); break;
		case C_TokenKind_FloatLiteral: s = Str("(float literal)"); break;
		case C_TokenKind_DoubleLiteral: s = Str("(double literal)"); break;
		case C_TokenKind_LongDoubleLiteral: s = Str("(long double literal)"); break;
		case C_TokenKind_CharLiteral: s = Str("(char literal)"); break;
		case C_TokenKind_Identifier: s = Str("(identifier)"); break;
		
		case C_TokenKind_UnclosedQuote: s = Str("(unclosed quote)"); break;
		
		case C_TokenKind_LeftParen: s = Str("("); break;
		case C_TokenKind_RightParen: s = Str(")"); break;
		case C_TokenKind_LeftBrkt: s = Str("["); break;
		case C_TokenKind_RightBrkt: s = Str("]"); break;
		case C_TokenKind_LeftCurl: s = Str("{"); break;
		case C_TokenKind_RightCurl: s = Str("}"); break;
		case C_TokenKind_Dot: s = Str("."); break;
		case C_TokenKind_VarArgs: s = Str("..."); break;
		case C_TokenKind_Arrow: s = Str("->"); break;
		case C_TokenKind_Comma: s = Str(","); break;
		case C_TokenKind_Colon: s = Str(":"); break;
		case C_TokenKind_Semicolon: s = Str(";"); break;
		case C_TokenKind_QuestionMark: s = Str("?"); break;
		case C_TokenKind_Plus: s = Str("+"); break;
		case C_TokenKind_Minus: s = Str("-"); break;
		case C_TokenKind_Mul: s = Str("*"); break;
		case C_TokenKind_Div: s = Str("/"); break;
		case C_TokenKind_Mod: s = Str("%"); break;
		case C_TokenKind_LThan: s = Str("<"); break;
		case C_TokenKind_GThan: s = Str(">"); break;
		case C_TokenKind_LEqual: s = Str("<="); break;
		case C_TokenKind_GEqual: s = Str(">="); break;
		case C_TokenKind_Equals: s = Str("=="); break;
		case C_TokenKind_Inc: s = Str("++"); break;
		case C_TokenKind_Dec: s = Str("--"); break;
		case C_TokenKind_LNot: s = Str("!"); break;
		case C_TokenKind_LAnd: s = Str("&&"); break;
		case C_TokenKind_LOr: s = Str("||"); break;
		case C_TokenKind_Not: s = Str("~"); break;
		case C_TokenKind_And: s = Str("&"); break;
		case C_TokenKind_Or: s = Str("|"); break;
		case C_TokenKind_Xor: s = Str("^"); break;
		case C_TokenKind_LeftShift: s = Str("<<"); break;
		case C_TokenKind_RightShift: s = Str(">>"); break;
		case C_TokenKind_NotEquals: s = Str("!="); break;
		case C_TokenKind_Assign: s = Str("="); break;
		case C_TokenKind_PlusAssign: s = Str("+="); break;
		case C_TokenKind_MinusAssign: s = Str("-="); break;
		case C_TokenKind_MulAssign: s = Str("*="); break;
		case C_TokenKind_DivAssign: s = Str("/="); break;
		case C_TokenKind_ModAssign: s = Str("%="); break;
		case C_TokenKind_LeftShiftAssign: s = Str("<<="); break;
		case C_TokenKind_RightShiftAssign: s = Str(">>="); break;
		case C_TokenKind_AndAssign: s = Str("&="); break;
		case C_TokenKind_OrAssign: s = Str("|="); break;
		case C_TokenKind_XorAssign: s = Str("^="); break;
		
		case C_TokenKind_Hashtag: s = Str("#"); break;
		case C_TokenKind_HashtagPragma: s = Str("#pragma"); break;
		case C_TokenKind_NewLine: s = Str("(line break)"); break;
		case C_TokenKind_DoubleHashtag: s = Str("##"); break;
	}
	
	return s;
}
