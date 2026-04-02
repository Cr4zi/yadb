#ifndef DEBUGGER_H_
#define DEBUGGER_H_

#include <sys/types.h>
#include <sys/ptrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "ds/hashtable.h"

typedef struct {
    char *full_path;
    Dwarf_Die die;
} dwarf_die_path_t;

typedef struct {
    char *full_path;

    Dwarf_Debug dw_dbg;
    Dwarf_Error dw_err;
    hashtable_t *filenames_table; /* Key: char *, Value: dwarf_die_path_t * */

    // These are going be only software breakpoints
    hashtable_t *breakpoints_table; /* Key: uintptr_t, Value: char(original byte) */

    pid_t debugee;
    bool is_running;
    bool is_exit;
} debugger_t;

int debugger_init(debugger_t *debugger, char *path);
int initialize_debugger_dwarf(debugger_t *debugger, char *path);
bool debugger_srcfiles(debugger_t *debugger, Dwarf_Die die);
void debugger_cu_walk(debugger_t *debugger);

uintptr_t debugger_get_base_addr(debugger_t *debugger);
uintptr_t debugger_get_line_addr(debugger_t *debugger, dwarf_die_path_t *die_path, unsigned long long line);
void set_software_breakpoint(debugger_t *debugger, uintptr_t offset);

#endif // DEBUGGER_H_
