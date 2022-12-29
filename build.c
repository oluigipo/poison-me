#include "src/common.h"
#include <string.h>
#include <stdio.h>

struct
{
	int32 optimize;
	bool asan;
	bool debug_info;
	bool debug_mode;
	bool verbose;
}
static g_opts;

static const char* sources = "src/main.c src/os.c src/lang_c.c";

#ifdef __clang__
static const char* f_cc = "clang -fuse-ld=lld";
static const char* f_optimize[3] = { "-O0", "-O1", "-O2" };
static const char* f_warnings =
"-Wall -Wno-unused-function -Wno-gnu-alignof-expression -Wno-missing-braces -Wno-logical-op-parentheses";
static const char* f_debuginfo = "-g";
static const char* f_define = "-D";
static const char* f_verbose = "-v";

#elif defined(_MSC_VER)
static const char* f_cc = "cl /nologo";
static const char* f_optimize[3] = { "/Og", "/Os", "/O2" };
static const char* f_warnings = "/w";
static const char* f_debuginfo = "/Zi";
static const char* f_define = "/D";
static const char* f_verbose = "";

#elif defined(__GNUC__)
static const char* f_cc = "gcc";
static const char* f_optimize[3] = { "-O0", "-O1", "-O2" };
static const char* f_warnings = "-Wall";
static const char* f_debuginfo = "-g";
static const char* f_define = "-D";
static const char* f_verbose = "-v";

#endif

int
main(int argc, char** argv)
{
	for (int32 i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-g") == 0)
			g_opts.debug_info = true;
		else if (strcmp(argv[i], "-debug") == 0)
			g_opts.debug_mode = true;
		else if (strcmp(argv[i], "-asan") == 0)
			g_opts.asan = true;
		else if (strcmp(argv[i], "-v") == 0)
			g_opts.verbose = true;
		else if (strncmp(argv[i], "-O", 2) == 0)
		{
			char* end;
			int32 opt = strtol(argv[i]+2, &end, 10);
			
			if (end == argv[i]+strlen(argv[i]))
				g_opts.optimize = opt;
		}
	}
	
	char cmd[4096];
	char* end = cmd+sizeof(cmd);
	char* head = cmd;
	
	head += snprintf(head, end-head, "%s %s %s %s", f_cc, sources, f_warnings, f_optimize[g_opts.optimize]);
	if (g_opts.asan)
		head += snprintf(head, end-head, " -fsanitize=address");
	if (g_opts.debug_info)
		head += snprintf(head, end-head, " %s", f_debuginfo);
	if (g_opts.debug_mode)
		head += snprintf(head, end-head, " %s%s", f_define, "DEBUG");
	if (g_opts.verbose)
		head += snprintf(head, end-head, " %s", f_verbose);
	
	return system(cmd);
}
