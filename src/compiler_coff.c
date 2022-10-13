struct X_CoffError
{
	bool ok;
	String why;
}
typedef X_CoffError;

struct X_CoffContext
{
	Arena* output_arena;
	Arena* scratch_arena;
	
	X_CoffError error;
}
typedef X_CoffContext;

static void
X_CoffGen(X_CoffContext* coff)
{
	uint8 header[] = {
		0x64, 0x86, // IMAGE_FILE_MACHINE_AMD64
		
	};
	
	Arena_PushMemory(coff->output_arena, header, sizeof(header));
}

// @Allocators: output_arena (output), leaky_scratch_arena (error, temp)
static String
X_CoffWriteObjectFile(const X_Allocators* allocators, X_CoffError* out_error)
{
	Assert(allocators && allocators->output_arena && allocators->leaky_scratch_arena);
	
	X_CoffContext coff = {
		.output_arena = allocators->output_arena,
		.scratch_arena = allocators->leaky_scratch_arena,
		.error = { .ok = true, },
	};
	
	uint8* const begin = Arena_End(coff.output_arena);
	X_CoffGen(&coff);
	uint8* const end = Arena_End(coff.output_arena);
	
	String result = coff.error.ok ? StrMake(end - begin, begin) : StrNull;
	
	if (out_error)
		*out_error = coff.error;
	else
		Assert(coff.error.ok);
	
	return result;
}
