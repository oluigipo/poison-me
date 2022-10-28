/*

IR FORMAT:

%0 = i32 arg 0
%1 = i32 arg 1
%2 = i32 add %0, %1

ret %2

*/

// NOTE(ljre): Handles for registers and basic blocks
uint32 typedef X_IrReg;
uint32 typedef X_IrLabel;

// NOTE(ljre): instructions
enum X_IrInstKind
{
	X_IrInstKind_Null = 0,
	
	X_IrInstKind__BeginControlFlow, //-
	X_IrInstKind_Branch,
	X_IrInstKind_BranchIf,
	X_IrInstKind_Ret,
	X_IrInstKind__EndControlFlow, //-
	
	X_IrInstKind_Store,
	
	X_IrInstKind__BeginAssignable, //-
	
	X_IrInstKind_Phi2,
	X_IrInstKind_Load,
	X_IrInstKind_Alloca,
	X_IrInstKind_Imm,
	X_IrInstKind_Arg,
	
	X_IrInstKind_Add,
	X_IrInstKind_Sub,
	X_IrInstKind_Mul,
	X_IrInstKind_Div,
	X_IrInstKind_Mod,
	
	X_IrInstKind_And,
	X_IrInstKind_Or,
	X_IrInstKind_Xor,
	
	X_IrInstKind_CmpLt,
	X_IrInstKind_CmpLe,
	X_IrInstKind_CmpGt,
	X_IrInstKind_CmpGe,
	X_IrInstKind_CmpEq,
	X_IrInstKind_CmpNeq,
	
	X_IrInstKind__EndAssignable, //-
}
typedef X_IrInstKind;

enum X_IrType
{
	X_IrType_Null = 0,
	
	X_IrType_Int8,
	X_IrType_Int16,
	X_IrType_Int32,
	X_IrType_Int64,
	
	X_IrType_UInt8,
	X_IrType_UInt16,
	X_IrType_UInt32,
	X_IrType_UInt64,
	
	X_IrType_Float32,
	X_IrType_Float64,
	
	X_IrType_Pointer,
}
typedef X_IrType;

struct X_IrPhiArg
{
	X_IrLabel label;
	X_IrReg reg;
}
typedef X_IrPhiArg;

struct X_IrInst
{
	X_IrInstKind kind : 24;
	X_IrType type : 8;
	
	X_IrReg next;
	
	union
	{
		uint64 imm64;
		uint32 imm32;
		X_IrReg reg;
		
		struct { X_IrReg left, right; } binary;
		
		struct { X_IrReg cond; X_IrLabel label1, label2; } branch;
		struct { X_IrPhiArg p1, p2; } phi2;
		struct { uint32 count, first_index; } phi;
	};
}
typedef X_IrInst;

// NOTE(ljre): Function
struct X_IrFunction
{
	String name;
	
	uint32 labels_count;
	X_IrReg* labels;
	
	uint32 insts_count;
	X_IrInst* insts;
}
typedef X_IrFunction;

static void
X_IrPrintFunction(Arena* arena, const X_IrFunction* func)
{
	Arena_SPrintf(arena, "%.*s():\n", StrFmt(func->name));
	
	for (uint32 label_index = 0; label_index < func->labels_count; ++label_index)
	{
		Arena_SPrintf(arena, "@%u:\n", label_index);
		
		for (uint32 index = func->labels[label_index]; index != 0; index = index ? func->insts[index-1].next : 0)
		{
			X_IrInst* inst = &func->insts[index-1];
			
			Arena_SPrintf(arena, "\t");
			
			if (inst->kind > X_IrInstKind__BeginAssignable && inst->kind < X_IrInstKind__EndAssignable)
				Arena_SPrintf(arena, "%%r%u = ", index);
			
			if (inst->kind < X_IrInstKind__BeginControlFlow || inst->kind > X_IrInstKind__EndControlFlow)
			{
				switch (inst->type)
				{
					case X_IrType_Null: Arena_SPrintf(arena, "void "); break;
					
					case X_IrType_Int8: Arena_SPrintf(arena, "int8 "); break;
					case X_IrType_Int16: Arena_SPrintf(arena, "int16 "); break;
					case X_IrType_Int32: Arena_SPrintf(arena, "int32 "); break;
					case X_IrType_Int64: Arena_SPrintf(arena, "int64 "); break;
					
					case X_IrType_Int8: Arena_SPrintf(arena, "uint8 "); break;
					case X_IrType_Int16: Arena_SPrintf(arena, "uint16 "); break;
					case X_IrType_Int32: Arena_SPrintf(arena, "uint32 "); break;
					case X_IrType_Int64: Arena_SPrintf(arena, "uint64 "); break;
					
					case X_IrType_Float32: Arena_SPrintf(arena, "float32 "); break;
					case X_IrType_Float64: Arena_SPrintf(arena, "float64 "); break;
					
					case X_IrType_Pointer: Arena_SPrintf(arena, "ptr "); break;
					
					default: Unreachable(); break;
				}
			}
			
			switch (inst->kind)
			{
				case X_IrInstKind_Null: Arena_SPrintf(arena, "nop"); break;
				
				case X_IrInstKind_Store: Arena_SPrintf(arena, "store [%%r%u], %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Load: Arena_SPrintf(arena, "load [%%r%u]", inst->reg); break;
				
				case X_IrInstKind_Alloca: Arena_SPrintf(arena, "alloca N * %u", inst->imm32); break;
				case X_IrInstKind_Imm: Arena_SPrintf(arena, "imm 0x%x", inst->imm32); break;
				case X_IrInstKind_Arg: Arena_SPrintf(arena, "arg %u", inst->imm32); break;
				
				case X_IrInstKind_Add: Arena_SPrintf(arena, "add %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Sub: Arena_SPrintf(arena, "sub %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Mul: Arena_SPrintf(arena, "mul %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Div: Arena_SPrintf(arena, "div %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Mod: Arena_SPrintf(arena, "mod %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				
				case X_IrInstKind_And: Arena_SPrintf(arena, "and %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Or: Arena_SPrintf(arena, "or %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_Xor: Arena_SPrintf(arena, "xor %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				
				case X_IrInstKind_CmpLt: Arena_SPrintf(arena, "cmplt %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_CmpLe: Arena_SPrintf(arena, "cmple %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_CmpGt: Arena_SPrintf(arena, "cmpgt %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_CmpGe: Arena_SPrintf(arena, "cmpge %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_CmpEq: Arena_SPrintf(arena, "cmpeq %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				case X_IrInstKind_CmpNeq: Arena_SPrintf(arena, "cmpneq %%r%u, %%r%u", inst->binary.left, inst->binary.right); break;
				
				case X_IrInstKind_Phi2: Arena_SPrintf(arena, "phi [@%u - %%r%u], [@%u - %%r%u]",
					inst->phi2.p1.label, inst->phi2.p1.reg,
					inst->phi2.p2.label, inst->phi2.p2.reg); break;
				
				case X_IrInstKind_Branch: Arena_SPrintf(arena, "br @%u", inst->branch.label1); break;
				case X_IrInstKind_BranchIf: Arena_SPrintf(arena, "br(%%r%u) @%u or @%u", inst->branch.cond, inst->branch.label1, inst->branch.label2); break;
				
				case X_IrInstKind_Ret: Arena_SPrintf(arena, "ret %%r%u", inst->reg); break;
				
				default: Unreachable(); break;
			}
			
			if (inst->kind == X_IrInstKind_BranchIf)
				index = 0;
			
			Arena_SPrintf(arena, "\n");
		}
	}
	
	Arena_SPrintf(arena, "\n");
}

//~ NOTE(ljre): Asm gen
struct X_AsmGenContext
{
	Arena* output_arena;
	Arena* scratch_arena;
	
	const X_IrFunction* func;
}
typedef X_AsmGenContext;

static void
X_AsmGen(X_AsmGenContext* ctx)
{
	// count how many blocks
}

static void
X_AsmTestGen(const X_Allocators* allocators)
{
	Arena* output_arena = allocators->output_arena;
	Arena* scratch_arena = allocators->scratch_arena;
	Assert(output_arena && scratch_arena);
	
	X_IrFunction func = {
		StrInit("add"),
		
		.labels_count = 3,
		.labels = (X_IrReg[]) {
			1, 6, 8,
		},
		
		.insts_count = 4,
		.insts = (X_IrInst[]) {
			//@0
			[0] = { X_IrInstKind_Arg, X_IrType_Int32, 2, .imm32 = 0, },
			[1] = { X_IrInstKind_Arg, X_IrType_Int32, 3, .imm32 = 1, },
			[2] = { X_IrInstKind_Add, X_IrType_Int32, 4, .binary = { 1, 2 } },
			
			[3] = { X_IrInstKind_CmpLt, X_IrType_Int32, 5, .binary = { 3, 0 } },
			[4] = { X_IrInstKind_BranchIf, 0, 0, .branch = { 4, 1, 2 } },
			
			//@1
			[5] = { X_IrInstKind_Sub, X_IrType_Int32, 7, .binary = { 0, 3 } },
			[6] = { X_IrInstKind_Branch, 0, 0, .branch = { 0, 2 } },
			
			//@2
			[7] = { X_IrInstKind_Phi2, X_IrType_Int32, 9, .phi2 = {
					{ .label = 0, .reg = 3 },
					{ .label = 1, .reg = 6 },
				}, },
			[8] = { X_IrInstKind_Ret, X_IrType_Int32, 0, .reg = 8 },
		},
	};
	
	// NOTE(ljre): print ir
	{
		char* const begin = Arena_End(output_arena);
		X_IrPrintFunction(output_arena, &func);
		char* const end = Arena_End(output_arena);
		
		X_Log("============[IR]=============\n%.*s\n============================\n", end - begin, begin);
		Arena_Pop(output_arena, begin);
	}
	
	// NOTE(ljre): gen asm from ir
	{
		char* const begin = Arena_End(output_arena);
		
		X_AsmGenContext* const ctx = &(X_AsmGenContext) {
			.output_arena = output_arena,
			.scratch_arena = scratch_arena,
			
			.func = &func
		};
		
		X_AsmGen(ctx);
		
		char* const end = Arena_End(output_arena);
		X_Log("============[ASM]=============\n%.*s\n============================\n", end - begin, begin);
	}
}
