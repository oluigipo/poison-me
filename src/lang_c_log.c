static C_Error*
C_PushErrorOrWarning(C_TuContext* tu, String what, const C_SourceLocation* loc, C_Warning warning)
{
	C_Error* err;
	
	if (!tu->first_error)
		err = tu->first_error = tu->last_error = Arena_PushStruct(tu->tree_arena, C_Error);
	else
		err = tu->last_error = tu->last_error->next = Arena_PushStruct(tu->tree_arena, C_Error);
	
	err->warning = warning;
	err->what = what;
	err->location = loc;
	
	return err;
}

static void
C_PrintAllErrorsAndWarnings(C_TuContext* tu)
{
	for Arena_TempScope(tu->scratch_arena)
	{
		uint8* begin = Arena_End(tu->scratch_arena);
		
		// TODO
		
		String final_str = StrMake((uint8*)Arena_End(tu->scratch_arena) - begin, begin);
		C_Log(tu->scratch_arena, final_str);
	}
}
