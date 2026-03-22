#ifndef DEBUGGER_H_
#define DEBUGGER_H_

#include <sys/types.h>
#include <stdio.h>

#include <dwarf.h>
#include <libdwarf.h>

typedef struct {
    pid_t debugee;
    Dwarf_Debug dw_dbg;
    
} debugger_t;

int initialize_debugger_dwarf(debugger_t *debugger, char *path);

#endif // DEBUGGER_H_
