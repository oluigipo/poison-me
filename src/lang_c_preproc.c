struct C_PpTokenReader
{
	C_PreprocTokenList* list;
	C_PreprocToken tok;
}
typedef C_PpTokenReader;

struct C_PpContext
{
	C_TuContext* tu;
	C_TokenStream output;
	
	C_LoadedFile* current_file;
	
	uint32 last_loc_index;
	
	bool ok;
}
typedef C_PpContext;

enum C_PpBuiltinMacro
{
	C_PpBuiltinMacro_Null = 0,
	
	C_PpBuiltinMacro_Line,
	C_PpBuiltinMacro_File,
}
typedef C_PpBuiltinMacro;

//~ NOTE(ljre): Utils
static void
C_PpPushError(C_PpContext* pp, C_PpTokenReader* rd, const char* fmt, ...)
{
	for Arena_TempScope(pp->tu->scratch_arena)
	{
		va_list args;
		va_start(args, fmt);
		String str = Arena_VPrintf(pp->tu->scratch_arena, fmt, args);
		va_end(args);
		
		Arena_PushString(pp->tu->scratch_arena, Str("\n"));
		str.size++;
		
		C_Log(pp->tu->scratch_arena, str);
	}
}

static String
C_PpStringifyString(Arena* arena, String str)
{
	uint8* const begin = Arena_End(arena);
	Arena_PushString(arena, Str("\""));
	
	for (int32 i = 0; i < str.size; ++i)
	{
		if (str.data[i] == '\\')
			Arena_PushString(arena, Str("\\\\"));
		else if (str.data[i] == '"')
			Arena_PushString(arena, Str("\\\""));
		else
			Arena_PushData(arena, &str.data[i]);
	}
	
	Arena_PushString(arena, Str("\""));
	uint8* const end = Arena_End(arena);
	
	return StrRange(begin, end);
}

static int32
C_PpFindStringInArray(const String* array, uint32 size, String target)
{
	for (int32 i = 0; i < size; ++i)
	{
		if (String_Equals(target, array[i]))
			return i;
	}
	
	return -1;
}

static String
C_PpStringifyToken(Arena* arena, const C_PreprocToken* token)
{
	uint8* const begin = Arena_End(arena);
	
	switch (token->kind)
	{
		case C_TokenKind_StringLiteral: C_PpStringifyString(arena, token->as_string); break;
		default: Arena_PushString(arena, token->as_string); break;
	}
	
	uint8* const end = Arena_End(arena);
	return StrRange(begin, end);
}

static String
C_PpStringifyTokens(Arena* arena, C_PreprocTokenList* tokens, uint32 count)
{
	uint8* const begin = Arena_End(arena);
	Arena_PushString(arena, Str("\""));
	
	C_PreprocTokenList* it = tokens;
	while (count --> 0)
	{
		C_PpStringifyToken(arena, &it->tok);
		it = it->next;
	}
	
	Arena_PushString(arena, Str("\""));
	uint8* const end = Arena_End(arena);
	
	return StrRange(begin, end);
}

static C_PreprocTokenList**
C_PpConcatTokens(C_PpContext* pp, const C_PreprocToken* left, const C_PreprocToken* right, C_PreprocTokenList** out_tokens, C_PreprocHideset* hideset, C_SourceLocation* loc)
{
	C_PreprocTokenList** head = out_tokens;
	
	uint8* const begin = Arena_End(pp->tu->string_arena);
	C_PpStringifyToken(pp->tu->string_arena, left);
	C_PpStringifyToken(pp->tu->string_arena, right);
	uint8* const end = Arena_End(pp->tu->string_arena);
	
	C_PreprocTokenList* list = C_TokenizeForPreproc(pp->tu, pp->tu->scratch_arena, StrRange(begin, end), NULL);
	*head = list;
	
	for (;;)
	{
		list->hideset = hideset;
		list->expanded_from = loc;
		
		if (!list->next)
			break;
		
		list = list->next;
	}
	
	head = &list->next;
	return head;
}

//~ NOTE(ljre): Token reader
static inline void
C_PpNextToken(C_PpTokenReader* rd)
{
	if (!rd->list)
		rd->tok = (C_PreprocToken) { 0 };
	else
	{
		rd->list = rd->list->next;
		
		if (rd->list)
			rd->tok = rd->list->tok;
		else
			rd->tok = (C_PreprocToken) { 0 };
	}
}

static inline C_PreprocToken
C_PpPeekToken(C_PpTokenReader* rd)
{
	if (rd->list && rd->list->next)
		return rd->list->next->tok;
	return (C_PreprocToken) { 0 };
}

static inline bool
C_PpAssertToken(C_PpContext* pp, C_PpTokenReader* rd, C_TokenKind kind)
{
	if (rd->tok.kind != kind)
	{
		// TODO(ljre)
		Assert(false);
		
		return false;
	}
	
	return true;
}

static inline bool
C_PpEatToken(C_PpContext* pp, C_PpTokenReader* rd, C_TokenKind kind)
{
	bool result = C_PpAssertToken(pp, rd, kind);
	C_PpNextToken(rd);
	return result;
}

static inline bool
C_PpTryEatToken(C_PpTokenReader* rd, C_TokenKind kind)
{
	if (rd->tok.kind == kind)
	{
		C_PpNextToken(rd);
		return true;
	}
	
	return false;
}

static C_PreprocTokenList**
C_PpQueueToken(C_PreprocTokenList** ptoks, Arena* arena, const C_PreprocToken* token, C_PreprocHideset* hideset, C_SourceLocation* included_from, C_SourceLocation* expanded_from)
{
	C_PreprocTokenList* item = Arena_PushStruct(arena, C_PreprocTokenList);
	item->tok = *token;
	item->next = *ptoks;
	item->hideset = hideset;
	item->included_from = included_from;
	item->expanded_from = expanded_from;
	*ptoks = item;
	
	return &item->next;
}

static inline uint32
C_PpEatTokenBalanced(C_PpTokenReader* rd, C_TokenKind open, C_TokenKind close, int32 start_at)
{
	int32 balance = start_at;
	uint32 eaten = 0;
	
	while (rd->tok.kind)
	{
		if (rd->tok.kind == open)
			++balance;
		else if (rd->tok.kind == close && --balance <= 0)
			break;
		
		++eaten;
		C_PpNextToken(rd);
	}
	
	return eaten;
}

//~ NOTE(ljre): Writing output tokens
static void
C_PpWriteToken(C_PpContext* pp, const C_PreprocToken* pptok, C_SourceLocation* included_from, C_SourceLocation* expanded_from)
{
	C_SourceLocation* loc = &(C_SourceLocation) {
		.included_from = included_from,
		.expanded_from = expanded_from,
		.file = pp->current_file,
		.leading_spaces = pptok->leading_spaces,
		.line = pptok->line,
		.col = pptok->col,
	};
	
	loc = Arena_PushStructData(pp->tu->loc_arena, C_SourceLocation, loc);
	
	C_TokenKind kind = pptok->kind;
	if (kind == C_TokenKind_Identifier)
	{
		C_TokenKind kw = C_FindKeywordByName(pptok->as_string);
		
		if (kw)
			kind = kw;
	}
	
	Arena_PushData(pp->tu->array_arena, &(C_Token) { kind, pptok->as_string.size, pptok->as_string.data, loc });
	pp->output.size++;
}

//~ NOTE(ljre): Macros
static C_Macro*
C_PpFindMacro(C_PpContext* pp, String name)
{
	uint64 hash = Hash_StringHash(name);
	int32 index = (int32)hash;
	C_HashMapChunk* map = pp->tu->macros_hashmap;
	
	for (;;)
	{
		index = Hash_Msi(map->log2cap, hash, index);
		C_Macro* macro = (C_Macro*)map->memory + index;
		
		if (String_Equals(name, macro->name))
		{
			if (!macro->is_defined)
				continue;
			
			return macro;
		}
		
		if (macro->name.size > 0)
			continue;
		
		if (map->size >= 1 << (map->log2cap-1))
		{
			if (!map->next)
				return NULL;
			
			map = map->next;
			index = (int32)hash;
			continue;
		}
		
		return NULL;
	}
}

static C_Macro*
C_PpInsertMacroToHashmap(C_PpContext* pp, const C_Macro* macro_def)
{
	String name = macro_def->name;
	uint64 hash = Hash_StringHash(name);
	int32 index = (int32)hash;
	C_HashMapChunk* map = pp->tu->macros_hashmap;
	
	for (;;)
	{
		index = Hash_Msi(map->log2cap, hash, index);
		C_Macro* macro = (C_Macro*)map->memory + index;
		
		if (String_Equals(macro->name, macro_def->name))
			Assert(false);
		
		if (macro->name.size > 0)
			continue;
		
		if (map->size >= 1 << (map->log2cap-1))
		{
			if (!map->next)
			{
				map->next = C_AllocHashMapChunk(pp->tu->tree_arena, Min(map->log2cap+1, 20), sizeof(C_Macro));
				map = map->next;
				index = Hash_Msi(map->log2cap, hash, (int32)hash);
				macro = (C_Macro*)map->memory + index;
				
				goto insert_in_map;
			}
			
			map = map->next;
			index = (int32)hash;
			continue;
		}
		
		insert_in_map:;
		*macro = *macro_def;
		macro->is_defined = true;
		
		return macro;
	}
	
	return NULL;
}

static void
C_PpDefineBuiltinMacros(C_PpContext* pp)
{
	static const C_Macro arr[] = {
		{ .name = StrInit("__LINE__"), .builtin_id = C_PpBuiltinMacro_Line, },
		{ .name = StrInit("__FILE__"), .builtin_id = C_PpBuiltinMacro_File, },
	};
	
	for (int32 i = 0; i < ArrayLength(arr); ++i)
		C_PpInsertMacroToHashmap(pp, &arr[i]);
}

static void
C_PpPredefineMacros(C_PpContext* pp, const String* macros, uintsize count)
{
	// TODO
}

static bool C_PpTryToExpandMacro(C_PpContext* pp, C_PpTokenReader* rd, uint32* out_added_token_count);

static uint32
C_PpExpandMacro(C_PpContext* pp, C_PpTokenReader* rd, C_Macro* macro)
{
	Assert(macro);
	uint32 result = 0;
	
	C_SourceLocation* included_from = rd->list->included_from;
	C_SourceLocation* expanded_from = rd->list->expanded_from;
	
	if (macro->builtin_id)
	{
		result = 1;
		C_PreprocToken tok = {
			.kind = C_TokenKind_Null,
			.as_string = { 0 },
			
			.leading_spaces = rd->tok.leading_spaces,
			.line = rd->tok.line,
			.col = rd->tok.col,
		};
		
		switch (macro->builtin_id)
		{
			case C_PpBuiltinMacro_Line:
			{
				tok.kind = C_TokenKind_IntLiteral;
				tok.as_string = Arena_Printf(pp->tu->string_arena, "%u", tok.line);
			} break;
			
			case C_PpBuiltinMacro_File:
			{
				tok.kind = C_TokenKind_StringLiteral;
				tok.as_string = C_PpStringifyString(pp->tu->string_arena, pp->current_file->path);
			} break;
			
			default: Unreachable(); break;
		}
		
		C_PpNextToken(rd);
		C_PpQueueToken(&rd->list, pp->tu->scratch_arena, &tok, NULL, included_from, expanded_from);
		rd->tok = rd->list->tok;
		
		return result;
	}
	
	C_SourceLocation* this_loc = &(C_SourceLocation) {
		.included_from = included_from,
		.expanded_from = expanded_from,
		.file = pp->current_file,
		.leading_spaces = rd->tok.leading_spaces,
		.line = rd->tok.line,
		.col = rd->tok.col,
	};
	
	this_loc = Arena_PushStructData(pp->tu->loc_arena, C_SourceLocation, this_loc);
	
	C_PreprocHideset* hideset = Arena_PushStruct(pp->tu->scratch_arena, C_PreprocHideset);
	hideset->next = rd->list->hideset;
	hideset->name = rd->tok.as_string;
	
	if (!macro->is_func_like)
	{
		C_PpNextToken(rd);
		result = macro->replacement_count;
		
		C_PreprocTokenList** head = &rd->list;
		C_PreprocTokenList* macro_head = macro->replacement;
		
		for (int32 i = 0; i < macro->replacement_count; ++i)
		{
			head = C_PpQueueToken(head, pp->tu->scratch_arena, &macro_head->tok, hideset, NULL, this_loc);
			macro_head = macro_head->next;
		}
	}
	else
	{
		C_PpNextToken(rd);
		C_PpEatToken(pp, rd, C_TokenKind_LeftParen);
		
		//- NOTE(ljre): Read arguments
		struct MacroArg
		{
			bool is_va_arg;
			uint32 count;
			C_PreprocTokenList* value;
		}
		typedef MacroArg;
		
		MacroArg* args = Arena_PushArray(pp->tu->scratch_arena, MacroArg, macro->param_count);
		
		for (int32 i = 0; i < macro->param_count - macro->has_va_args; ++i)
		{
			MacroArg* arg = &args[i];
			
			arg->count = 0;
			arg->value = rd->list;
			int32 balance = 1;
			
			while (rd->tok.kind)
			{
				if (balance == 1 && rd->tok.kind == C_TokenKind_Comma)
					break;
				
				if (rd->tok.kind == C_TokenKind_LeftParen)
					++balance;
				else if (rd->tok.kind == C_TokenKind_RightParen && --balance <= 0)
					break;
				
				++arg->count;
				C_PpNextToken(rd);
			}
			
			if (!C_PpTryEatToken(rd, C_TokenKind_Comma))
				break;
		}
		
		if (macro->has_va_args)
		{
			MacroArg* arg = &args[macro->param_count-1];
			
			arg->is_va_arg = true;
			arg->value = rd->list;
			arg->count = C_PpEatTokenBalanced(rd, C_TokenKind_LeftParen, C_TokenKind_RightParen, 1);
		}
		else if (!C_PpTryEatToken(rd, C_TokenKind_RightParen))
		{
			C_PpEatTokenBalanced(rd, C_TokenKind_LeftParen, C_TokenKind_RightParen, 1);
			C_PpEatToken(pp, rd, C_TokenKind_RightParen);
		}
		
		if (macro->inst_count > 0)
		{
			C_PreprocTokenList* replacement = NULL;
			C_PreprocTokenList** head = &replacement;
			
			struct CameFromArg typedef CameFromArg;
			struct CameFromArg
			{
				CameFromArg* next;
				C_PreprocTokenList** tokens;
				uint32 count;
			};
			
			CameFromArg* top_came_from_arg = NULL;
			
			for (int32 i = 0; i < macro->inst_count; ++i)
			{
				C_MacroInst* inst = &macro->insts[i];
				
				switch (inst->kind)
				{
					default: Unreachable(); break;
					
					case C_MacroInstKind_CopyTokens:
					{
						C_PreprocTokenList* it = inst->copy.tokens;
						uint32 count = inst->copy.tokens_count;
						
						while (count --> 0)
						{
							head = C_PpQueueToken(head, pp->tu->scratch_arena, &it->tok, hideset, NULL, this_loc);
							it = it->next;
						}
					} break;
					
					case C_MacroInstKind_Argument:
					{
						int32 param_index = inst->argument.param_index;
						Assert(param_index >= 0 && param_index < macro->param_count);
						
						C_PreprocTokenList** first = head;
						C_PreprocTokenList* it = args[param_index].value;
						uint32 count = args[param_index].count;
						uint32 remaining = count;
						
						while (remaining --> 0)
						{
							head = C_PpQueueToken(head, pp->tu->scratch_arena, &it->tok, hideset, NULL, this_loc);
							it = it->next;
						}
						
						for (int32 i = 0; i < count; ++i)
						{
							C_PpTokenReader* local_rd = &(C_PpTokenReader) { *first, (*first)->tok };
							if ((*first)->tok.kind == C_TokenKind_Identifier)
							{
								uint32 count2;
								if (C_PpTryToExpandMacro(pp, local_rd, &count2))
								{
									*first = local_rd->list;
									if (i == count-1)
									{
										head = first;
										while (count2--)
											head = &(*head)->next;
									}
									else
									{
										while (*head)
											head = &(*head)->next;
									}
								}
							}
							else
								first = &(*first)->next;
						}
						
						if (*first)
						{
							(*first)->tok.leading_spaces = inst->argument.leading_spaces;
							
							CameFromArg* came_from_arg = Arena_PushStruct(pp->tu->scratch_arena, CameFromArg);
							came_from_arg->next = top_came_from_arg;
							came_from_arg->tokens = first;
							came_from_arg->count = 0;
							
							for (C_PreprocTokenList** it = first; it != head; it = &(*it)->next)
								++came_from_arg->count;
							
							top_came_from_arg = came_from_arg;
						}
					} break;
					
					case C_MacroInstKind_Stringify:
					{
						int32 param_index = inst->argument.param_index;
						Assert(param_index >= 0 && param_index < macro->param_count);
						
						C_PreprocTokenList* tokens = args[param_index].value;
						uint32 count = args[param_index].count;
						
						String str = C_PpStringifyTokens(pp->tu->string_arena, tokens, count);
						
						C_PreprocTokenList* tok = Arena_PushStruct(pp->tu->scratch_arena, C_PreprocTokenList);
						tok->next = NULL;
						tok->hideset = hideset;
						tok->expanded_from = this_loc;
						tok->tok = (C_PreprocToken) {
							.kind = C_TokenKind_StringLiteral,
							.line = macro->definition_loc->line,
							.col = macro->definition_loc->col,
							.leading_spaces = inst->stringify.leading_spaces,
							.as_string = str,
						};
						
						*head = tok;
						head = &(*head)->next;
					} break;
					
					case C_MacroInstKind_GlueLeft:
					{
						int32 param_index = inst->glue.param_index;
						Assert(param_index >= 0 && param_index < macro->param_count);
						
						C_PreprocTokenList** first = head;
						C_PreprocTokenList* token_to_concat = inst->glue.token;
						C_PreprocTokenList* it = args[param_index].value;
						uint32 count = args[param_index].count;
						
						if (count == 0)
						{
							// NOTE(ljre): Nothing to concat with, so just copy the token
							head = C_PpQueueToken(head, pp->tu->scratch_arena, &token_to_concat->tok, hideset, NULL, this_loc);
						}
						else
						{
							// NOTE(ljre): Copy all leading tokens of argument, excluding last
							while (count --> 1)
							{
								head = C_PpQueueToken(head, pp->tu->scratch_arena, &it->tok, hideset, NULL, this_loc);
								it = it->next;
							}
							
							// NOTE(ljre): Concat tokens
							head = C_PpConcatTokens(pp, &it->tok, &token_to_concat->tok, head, hideset, this_loc);
						}
						
						(*first)->tok.leading_spaces = inst->glue.leading_spaces;
					} break;
					
					case C_MacroInstKind_GlueRight:
					{
						int32 param_index = inst->glue.param_index;
						Assert(param_index >= 0 && param_index < macro->param_count);
						
						C_PreprocTokenList** first = head;
						C_PreprocTokenList* token_to_concat = inst->glue.token;
						C_PreprocTokenList* it = args[param_index].value;
						uint32 count = args[param_index].count;
						
						if (count == 0)
						{
							// NOTE(ljre): Nothing to concat with, so just copy the token
							head = C_PpQueueToken(head, pp->tu->scratch_arena, &token_to_concat->tok, hideset, NULL, this_loc);
						}
						else
						{
							// NOTE(ljre): Concat tokens
							head = C_PpConcatTokens(pp, &token_to_concat->tok, &it->tok, head, hideset, this_loc);
							it = it->next;
							--count;
							
							// NOTE(ljre): Copy all trailling tokens of argument
							while (count --> 0)
							{
								head = C_PpQueueToken(head, pp->tu->scratch_arena, &it->tok, hideset, NULL, this_loc);
								it = it->next;
							}
						}
						
						(*first)->tok.leading_spaces = inst->glue.leading_spaces;
					} break;
					
					case C_MacroInstKind_GlueArgs:
					{
						int32 param1_index = inst->glue_args.param1_index;
						int32 param2_index = inst->glue_args.param2_index;
						Assert(param1_index >= 0 && param1_index < macro->param_count);
						Assert(param2_index >= 0 && param2_index < macro->param_count);
						
						C_PreprocTokenList** first = head;
						C_PreprocTokenList* it_left = args[param1_index].value;
						C_PreprocTokenList* it_right = args[param2_index].value;
						uint32 count_left = args[param1_index].count;
						uint32 count_right = args[param2_index].count;
						
						if (count_left == 0 || count_right == 0)
						{
							// NOTE(ljre): Nothing to concat with, so just copy the tokens
							C_PreprocTokenList* its[2] = { it_left, it_right, };
							uint32 counts[2] = { count_left, count_right, };
							
							for (int32 j = 0; j < 2; ++j)
							{
								C_PreprocTokenList* it = its[j];
								uint32 count = counts[j];
								
								while (count --> 0)
								{
									C_PreprocTokenList* tok = Arena_PushStructData(pp->tu->scratch_arena, C_PreprocTokenList, it);
									tok->next = NULL;
									tok->hideset = hideset;
									tok->expanded_from = this_loc;
									
									*head = tok;
									head = &(*head)->next;
									it = it->next;
								}
							}
						}
						else
						{
							// NOTE(ljre): Copy all leading tokens of left argument, excluding last
							while (count_left --> 1)
							{
								C_PreprocTokenList* tok = Arena_PushStructData(pp->tu->scratch_arena, C_PreprocTokenList, it_left);
								tok->next = NULL;
								tok->hideset = hideset;
								tok->expanded_from = this_loc;
								
								*head = tok;
								head = &(*head)->next;
								it_left = it_left->next;
							}
							
							// NOTE(ljre): Concat tokens
							head = C_PpConcatTokens(pp, &it_left->tok, &it_right->tok, head, hideset, this_loc);
							it_right = it_right->next;
							--count_right;
							
							// NOTE(ljre): Copy all trailling tokens of right argument
							while (count_right --> 0)
							{
								C_PreprocTokenList* tok = Arena_PushStructData(pp->tu->scratch_arena, C_PreprocTokenList, it_right);
								tok->next = NULL;
								tok->hideset = hideset;
								tok->expanded_from = this_loc;
								
								*head = tok;
								head = &(*head)->next;
								it_right = it_right->next;
							}
						}
						
						if (*first)
						{
							(*first)->tok.leading_spaces = inst->glue_args.leading_spaces;
							
							CameFromArg* came_from_arg = Arena_PushStruct(pp->tu->scratch_arena, CameFromArg);
							came_from_arg->next = top_came_from_arg;
							came_from_arg->tokens = first;
							came_from_arg->count = 0;
							
							for (C_PreprocTokenList** it = first; it != head; it = &(*it)->next)
								++came_from_arg->count;
							
							top_came_from_arg = came_from_arg;
						}
					} break;
					
					case C_MacroInstKind_CommaVarArgs:
					{
						// TODO
					} break;
				}
			}
			
			Assert(replacement);
			
			if (false) for (CameFromArg* arg_it = top_came_from_arg; arg_it; arg_it = arg_it->next)
			{
				C_PpTokenReader* local_rd = &(C_PpTokenReader) { *arg_it->tokens, (*arg_it->tokens)->tok };
				C_PreprocTokenList** local_head = arg_it->tokens;
				uint32 count = arg_it->count;
				
				while (count --> 0)
				{
					bool should_eat = true;
					
					if (local_rd->tok.kind == C_TokenKind_Identifier)
					{
						uint32 added = 0;
						bool expanded = C_PpTryToExpandMacro(pp, local_rd, &added);
						
						if (expanded)
						{
							*local_head = local_rd->list;
							
							if (count == 0 && arg_it == top_came_from_arg)
							{
								C_PreprocTokenList** itp = local_head;
								
								for (int32 i = 0; i < added; ++i)
									itp = &(*itp)->next;
								
								head = itp;
							}
							
							count += added;
							should_eat = false;
						}
					}
					
					if (should_eat)
					{
						local_head = &(*local_head)->next;
						C_PpNextToken(local_rd);
					}
				}
			}
			
			*head = rd->list;
			rd->list = replacement;
			
			result = 0;
			for (C_PreprocTokenList** it = &replacement; it != head; it = &(*it)->next)
				++result;
		}
	}
	
	rd->tok = rd->list->tok;
	return result;
}

static bool
C_PpTryToExpandMacro(C_PpContext* pp, C_PpTokenReader* rd, uint32* out_added_token_count)
{
	Assert(rd->tok.kind == C_TokenKind_Identifier);
	
	String name = rd->tok.as_string;
	
	for (C_PreprocHideset* hset = rd->list->hideset; hset; hset = hset->next)
	{
		if (String_Equals(hset->name, name))
			return false;
	}
	
	C_Macro* macro = C_PpFindMacro(pp, name);
	if (!macro)
		return false;
	
	if (macro->is_func_like && C_PpPeekToken(rd).kind != C_TokenKind_LeftParen)
		return false;
	
	uint32 added_token_count = C_PpExpandMacro(pp, rd, macro);
	if (out_added_token_count)
		*out_added_token_count = added_token_count;
	
	return true;
}

//~ NOTE(ljre): File handling
static C_LoadedFile*
C_PpLoadFile(C_PpContext* pp, String path)
{
	C_HashMapChunk* map = pp->tu->files_hashmap;
	uint64 hash = Hash_StringHash(path);
	int32 index = (int32)hash;
	
	for (;;)
	{
		index = Hash_Msi(map->log2cap, hash, index);
		C_LoadedFile* file = (C_LoadedFile*)map->memory + index;
		
		if (file->path_hash == hash && String_Equals(file->path, path))
		{
			if (!file->tokens)
				return NULL;
			
			return file;
		}
		
		if (file->path.size != 0)
			continue;
		
		if (map->size >= 1u << map->log2cap)
		{
			if (!map->next)
			{
				uint32 cap = Min(map->log2cap + 1, 20);
				map->next = C_AllocHashMapChunk(pp->tu->tree_arena, cap, sizeof(C_LoadedFile));
			}
			
			map = map->next;
			index = (int32)hash;
			continue;
		}
		
		file->path = path;
		
		String contents = { 0 };
		if (!OS_ReadWholeFile(path, &contents, pp->tu->string_arena, NULL))
			return NULL;
		
		file->contents = contents;
		
		C_Error error = { 0 };
		C_PreprocTokenList* tokens = C_TokenizeForPreproc(pp->tu, pp->tu->tree_arena, contents, &error);
		
		if (!C_IsOk(&error))
			return NULL;
		
		file->tokens = tokens;
		return file;
	}
	
	return NULL;
}

//~ NOTE(ljre): Preproc directives
static C_Macro*
C_PpDefineMacro(C_PpContext* pp, C_PpTokenReader* rd)
{
	Assert(rd->tok.kind);
	if (rd->tok.kind != C_TokenKind_Identifier)
		C_PpPushError(pp, rd, "expected identifier in macro definition.");
	
	C_Macro macro = {
		.name = rd->tok.as_string,
		.is_func_like = false,
		.has_va_args = false,
		.param_count = 0,
	};
	
	C_PpNextToken(rd);
	
	// NOTE(ljre): If function-like macro, compile it to C_MacroInst array.
	if (rd->tok.kind == C_TokenKind_LeftParen && rd->tok.leading_spaces == 0)
	{
		C_PpNextToken(rd);
		macro.is_func_like = true;
		
		int32 param_count = 0;
		String* params = Arena_EndAligned(pp->tu->scratch_arena, alignof(String));
		
		while (rd->tok.kind && rd->tok.kind != C_TokenKind_RightParen)
		{
			if (rd->tok.kind == C_TokenKind_Identifier)
			{
				Arena_PushStructData(pp->tu->scratch_arena, String, &rd->tok.as_string);
				++param_count;
				
				C_PpNextToken(rd);
			}
			else if (rd->tok.kind == C_TokenKind_VarArgs)
			{
				Arena_PushStructData(pp->tu->scratch_arena, String, &Str("__VA_ARGS__"));
				++param_count;
				macro.has_va_args = true;
				
				C_PpNextToken(rd);
				break;
			}
			
			if (!C_PpTryEatToken(rd, C_TokenKind_Comma))
				break;
		}
		
		C_PpEatToken(pp, rd, C_TokenKind_RightParen);
		
		macro.param_count = param_count;
		macro.insts = Arena_EndAligned(pp->tu->tree_arena, alignof(C_MacroInst));
		
		int32 running_copy = 0;
		C_MacroInst* last_inst = NULL;
		
		while (rd->tok.kind && rd->tok.kind != C_TokenKind_NewLine)
		{
			C_PreprocToken peek = C_PpPeekToken(rd);
			
			if (peek.kind == C_TokenKind_DoubleHashtag)
			{
				// NOTE(ljre): Handle concat/pasting/glue.
				uint32 leading_spaces = rd->tok.leading_spaces;
				int32 param_index = -1;
				
				if (rd->tok.kind == C_TokenKind_Identifier && (param_index = C_PpFindStringInArray(params, param_count, rd->tok.as_string)) != -1)
				{
					C_PpNextToken(rd); // eat this token
					C_PpNextToken(rd); // eat ##
					
					int32 param2_index = -1;
					if (rd->tok.kind == C_TokenKind_Identifier && (param2_index = C_PpFindStringInArray(params, param_count, rd->tok.as_string)) != -1)
					{
						running_copy = 0;
						last_inst = Arena_PushStruct(pp->tu->tree_arena, C_MacroInst);
						last_inst->kind = C_MacroInstKind_GlueArgs;
						last_inst->glue_args.param1_index = param_index;
						last_inst->glue_args.param2_index = param2_index;
						last_inst->glue_args.leading_spaces = leading_spaces;
						C_PpNextToken(rd);
						
						continue;
					}
					else
					{
						running_copy = 0;
						last_inst = Arena_PushStruct(pp->tu->tree_arena, C_MacroInst);
						last_inst->kind = C_MacroInstKind_GlueLeft;
						last_inst->glue.param_index = param_index;
						last_inst->glue.token = rd->list;
						last_inst->glue.leading_spaces = leading_spaces;
						C_PpNextToken(rd);
						
						continue;
					}
				}
				else
				{
					C_PreprocTokenList* token_to_concat = rd->list;
					C_PpNextToken(rd); // eat this token
					C_PpNextToken(rd); // eat ##
					
					if (rd->tok.kind == C_TokenKind_Identifier && (param_index = C_PpFindStringInArray(params, param_count, rd->tok.as_string)) != -1)
					{
						running_copy = 0;
						last_inst = Arena_PushStruct(pp->tu->tree_arena, C_MacroInst);
						last_inst->kind = C_MacroInstKind_GlueRight;
						last_inst->glue.param_index = param_index;
						last_inst->glue.token = token_to_concat;
						last_inst->glue.leading_spaces = leading_spaces;
						C_PpNextToken(rd);
						
						continue;
					}
					else
					{
						C_PpPushError(pp, rd, "expected a macro parameter.");
					}
				}
			}
			else if (rd->tok.kind == C_TokenKind_Hashtag)
			{
				// NOTE(ljre): Handle stringify
				C_PpNextToken(rd);
				
				if (rd->tok.kind != C_TokenKind_Identifier)
				{
					C_PpPushError(pp, rd, "expected a macro parameter.");
					continue;
				}
				
				String name = rd->tok.as_string;
				int32 param_index = C_PpFindStringInArray(params, param_count, name);
				
				if (param_index == -1)
				{
					C_PpPushError(pp, rd, "expected a macro parameter.");
					continue;
				}
				
				running_copy = 0;
				last_inst = Arena_PushStruct(pp->tu->tree_arena, C_MacroInst);
				last_inst->kind = C_MacroInstKind_Stringify;
				last_inst->stringify.param_index = param_index;
				last_inst->stringify.leading_spaces = rd->tok.leading_spaces;
				C_PpNextToken(rd);
				
				continue;
			}
			else if (rd->tok.kind == C_TokenKind_Identifier)
			{
				// NOTE(ljre): Handle argument expansion
				String name = rd->tok.as_string;
				int32 param_index = C_PpFindStringInArray(params, param_count, name);
				
				if (param_index != -1)
				{
					running_copy = 0;
					last_inst = Arena_PushStruct(pp->tu->tree_arena, C_MacroInst);
					last_inst->kind = C_MacroInstKind_Argument;
					last_inst->argument.param_index = param_index;
					last_inst->argument.leading_spaces = rd->tok.leading_spaces;
					C_PpNextToken(rd);
					
					continue;
				}
			}
			
			// NOTE(ljre): Otherwise, just copy the token
			if (running_copy == 0)
			{
				last_inst = Arena_PushStruct(pp->tu->tree_arena, C_MacroInst);
				last_inst->kind = C_MacroInstKind_CopyTokens;
				last_inst->copy.tokens = rd->list;
			}
			
			last_inst->copy.tokens_count += 1;
			C_PpNextToken(rd);
		}
		
		macro.inst_count = (C_MacroInst*)Arena_End(pp->tu->tree_arena) - macro.insts;
	}
	else
	{
		// NOTE(ljre): Take replacement
		macro.replacement = rd->list;
		while (rd->tok.kind && rd->tok.kind != C_TokenKind_NewLine)
		{
			++macro.replacement_count;
			C_PpNextToken(rd);
		}
	}
	
	return C_PpInsertMacroToHashmap(pp, &macro);
}

static bool
C_PpUndefineMacro(C_PpContext* pp, C_PpTokenReader* rd)
{
	if (rd->tok.kind != C_TokenKind_Identifier)
	{
		C_PpPushError(pp, rd, "expected identifier after #undef directive.");
		return false;
	}
	
	C_Macro* macro = C_PpFindMacro(pp, rd->tok.as_string);
	if (!macro)
		return false;
	
	macro->is_defined = false;
	return true;
}

//~ NOTE(ljre): Main preprocess procs
static void
C_PpPreprocessFile(C_PpContext* pp, String filename, C_SourceLocation* included_from)
{
	C_LoadedFile* previous_file = pp->current_file;
	C_LoadedFile* file = C_PpLoadFile(pp, filename);
	if (!file)
		return;
	
	pp->current_file = file;
	C_PpTokenReader* rd = &(C_PpTokenReader) { file->tokens, file->tokens->tok };
	
	for (Arena_Savepoint scratch_save = Arena_Save(pp->tu->scratch_arena);
		rd->tok.kind;
		Arena_Restore(scratch_save))
	{
		while (rd->tok.kind == C_TokenKind_NewLine)
			C_PpNextToken(rd);
		
		if (!rd->tok.kind)
			continue;
		
		// NOTE(ljre): If line doesn't begin with '#', do normal line preprocessing
		if (rd->tok.kind != C_TokenKind_Hashtag)
		{
			do
			{
				bool should_push = true;
				
				if (rd->tok.kind == C_TokenKind_Identifier)
					should_push = !C_PpTryToExpandMacro(pp, rd, NULL);
				
				if (should_push)
				{
					C_PpWriteToken(pp, &rd->tok, rd->list->included_from, rd->list->expanded_from);
					C_PpNextToken(rd);
				}
			}
			while (rd->tok.kind && rd->tok.kind != C_TokenKind_NewLine);
			
			continue;
		}
		
		// NOTE(ljre): Parse directive
		C_PpEatToken(pp, rd, C_TokenKind_Hashtag);
		
		if (rd->tok.kind != C_TokenKind_Identifier)
		{
			// NOTE(ljre): Treat this as a null directive
			while (rd->tok.kind && rd->tok.kind != C_TokenKind_NewLine)
				C_PpNextToken(rd);
			
			continue;
		}
		
		String directive = rd->tok.as_string;
		
		if (String_Equals(directive, Str("define")))
		{
			C_PpNextToken(rd);
			C_PpDefineMacro(pp, rd);
		}
		else if (String_Equals(directive, Str("undef")))
		{
			C_PpNextToken(rd);
			C_PpUndefineMacro(pp, rd);
		}
		else
		{
			C_PpPushError(pp, rd, "unknown preprocessor directive '%S'\n", directive);
		}
		
		while (rd->tok.kind && rd->tok.kind != C_TokenKind_NewLine)
			C_PpNextToken(rd);
	}
	
	pp->current_file = previous_file;
}

static void
C_Preprocess(C_TuContext* tu)
{
	C_PpContext* pp = &(C_PpContext) {
		.tu = tu,
		
		.output = {
			.size = 0,
			.tokens = Arena_EndAligned(tu->array_arena, alignof(C_Token)),
		},
	};
	
	C_PpDefineBuiltinMacros(pp);
	C_PpPredefineMacros(pp, tu->options->predefined_macros, tu->options->predefined_macros_count);
	
	C_PpPreprocessFile(pp, tu->main_file_name, NULL);
	
	tu->preprocessed_source = pp->output;
}

//~ NOTE(ljre): Stringify token stream
static String
C_WritePreprocessedTokensGnu(C_TuContext* tu, Arena* arena)
{
	C_TokenStream* stream = &tu->preprocessed_source;
	if (stream->size == 0)
		return StrNull;
	
	uint8* const begin = Arena_End(arena);
	C_SourceLocation* last_loc = NULL;
	
	Arena_Printf(arena, "# %u \"%S\"\n", stream->tokens[0].loc->line, stream->tokens[0].loc->file->path);
	
	for (int32 i = 0; i < stream->size; ++i)
	{
		C_Token tok = stream->tokens[i];
		C_SourceLocation* loc = tok.loc;
		
		// Handle preproc shenanigans
		while (loc->expanded_from)
			loc = loc->expanded_from;
		
		if (last_loc)
		{
			if (loc->file != last_loc->file)
				Arena_Printf(arena, "\n# %u \"%S\"\n", tok.loc->line, tok.loc->file->path);
			else if (loc->line != last_loc->line)
			{
				uint32 diff = loc->line - last_loc->line;
				
				if (diff > 4)
					Arena_Printf(arena, "\n# %u \"%S\"\n", loc->line, loc->file->path);
				else
					Arena_PushMemory(arena, "\n\n\n\n", diff);
			}
		}
		
		last_loc = loc;
		
		// Leading Spaces
		uint32 leading_spaces = tok.loc->leading_spaces;
		
		if (leading_spaces > 0)
			Mem_Set(Arena_PushDirtyAligned(arena, leading_spaces, 1), ' ', leading_spaces);
		
		// Token text
		Arena_PushString(arena, C_TokenAsString(tok));
	}
	
	uint8* const end = Arena_End(arena);
	return StrRange(begin, end);
}
