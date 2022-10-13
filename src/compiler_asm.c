enum X_Amd64OperandKind
{
	X_Amd64OperandKind_Null = 0,
	
	X_Amd64OperandKind_Memory,
	X_Amd64OperandKind_Imm32,
	X_Amd64OperandKind_Eax,
}
typedef X_Amd64OperandKind;

struct X_Amd64Operand
{
	X_Amd64OperandKind kind;
	
	union
	{
		uint32 imm32;
	};
}
typedef X_Amd64Operand;

enum X_Amd64InstKind
{
	X_Amd64InstKind_Null = 0,
	
	X_Amd64InstKind_Ret,
	X_Amd64InstKind_Mov,
}
typedef X_Amd64InstKind;

struct X_Amd64Inst
{
	X_Amd64InstKind kind;
	X_Amd64Operand op[3];
}
typedef X_Amd64Inst;

struct X_Amd64Function
{
	String name;
	
	uint32 size;
	X_Amd64Inst* data;
}
typedef X_Amd64Function;

static void
X_AsmPrintOperand(Arena* output_arena, X_Amd64Operand* op)
{
	switch (op->kind)
	{
		case X_Amd64OperandKind_Eax: Arena_SPrintf(output_arena, "eax"); break;
		case X_Amd64OperandKind_Imm32: Arena_SPrintf(output_arena, "%u", op->imm32); break;
		
		default: Unreachable(); break;
	}
}

static String
X_AsmGen(const X_Amd64Function* func, const X_Allocators* allocators)
{
	Arena* output_arena = allocators->output_arena;
	Arena* scratch_arena = allocators->scratch_arena;
	Assert(output_arena && scratch_arena);
	
	char* const begin = Arena_End(output_arena);
	
	{
		Arena_SPrintf(output_arena, "%.*s:\n", StrFmt(func->name));
		
		static const char prologue[] =
			"    push rbp\n"
			"    mov rbp, rsp\n"
			"    sub rsp, 24\n\n";
		
		static const char epilogue[] =
			"\n"
			"    add rsp, 24\n"
			"    mov rsp, rbp\n"
			"    pop rbp\n"
			"    ret\n";
		
		Arena_SPrintf(output_arena, "%s", prologue);
		
		for (uint32 i = 0; i < func->size; ++i)
		{
			X_Amd64Inst* inst = &func->data[i];
			
			switch (inst->kind)
			{
				case X_Amd64InstKind_Ret: Arena_SPrintf(output_arena, "    ret\n"); break;
				case X_Amd64InstKind_Mov:
				{
					Arena_SPrintf(output_arena, "    mov ");
					
					X_AsmPrintOperand(output_arena, &inst->op[0]);
					Arena_SPrintf(output_arena, ", ");
					X_AsmPrintOperand(output_arena, &inst->op[1]);
					
					Arena_SPrintf(output_arena, "\n");
				} break;
				
				default: Unreachable(); break;
			}
		}
		
		Arena_SPrintf(output_arena, "%s", epilogue);
	}
	
	char* const end = Arena_End(output_arena);
	String result = StrMake(end - begin, begin);
	
	return result;
}
