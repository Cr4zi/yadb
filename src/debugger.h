#ifndef DEBUGGER_H_
#define DEBUGGER_H_

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "ds/hashtable.h"

typedef struct {
    pid_t debugee;
    Dwarf_Debug dw_dbg;
    Dwarf_Error dw_err;

    hashtable_t *filenames_table;
} debugger_t;

int initialize_debugger_dwarf(debugger_t *debugger, char *path);
bool debugger_srcfiles(debugger_t *debugger, Dwarf_Die die);
void debugger_cu_walk(debugger_t *debugger);

Dwarf_Addr debugger_get_line_addr(debugger_t *debugger, dwarf_die_path_t *die_path, unsigned long long line);

#endif // DEBUGGER_H_
