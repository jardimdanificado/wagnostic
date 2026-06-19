/* wasm3 amalgamation - single source file
 * Usage:
 *   #define M3_IMPLEMENTATION
 *   #include "m3.c"
 *
 * Or compile directly:
 *   gcc -DM3_IMPLEMENTATION m3.c ...
 */

#ifdef M3_IMPLEMENTATION
#ifndef M3_AMALGAM_DONE
#define M3_AMALGAM_DONE
#include <stdio.h>
#define M3_IMPLEMENT_ERROR_STRINGS
#include "wasm3.h"
#include "m3_env.h"

#include "m3_bind.c"
#include "m3_code.c"
#include "m3_compile.c"
#include "m3_core.c"
#include "m3_env.c"
#include "m3_exec.c"
#include "m3_function.c"
#include "m3_info.c"
#include "m3_module.c"
#include "m3_parse.c"
#include "m3_api_libc.c"

#endif /* M3_AMALGAM_DONE */
#endif /* M3_IMPLEMENTATION */
