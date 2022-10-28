#ifndef INTERNAL_H
#define INTERNAL_H

#include "common.h"

#define API extern

struct OS_Error
{
	bool ok;
	uint32 code;
	String why;
}
typedef OS_Error;

API bool OS_ReadWholeFile(String path, String* out_data, Arena* out_arena, OS_Error* out_err);
API bool OS_WriteWholeFile(String path, String data, Arena* scratch_arena, OS_Error* out_err);
API uint64 OS_GetPosixTimestamp(void);

API int32 X_Main(int32 argc, const char* const* argv);

#endif //INTERNAL_H
