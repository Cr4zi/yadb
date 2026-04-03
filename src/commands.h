#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/personality.h>
#include <sys/user.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "debugger.h"
#include "ds/hashtable.h"

#define MAX_ARGC 10

#define COMMANDS                                                \
    CMD("break", cmd_break)                                     \
    CMD("run", cmd_run)                                         \
    CMD("step", cmd_step)                                       \
    CMD("continue", cmd_continue)                               \
    CMD("print", cmd_print)                                     \
    CMD("backtrace", cmd_backtrace)                             \
    CMD("exit", cmd_exit)                                       \
    CMD("help", cmd_help)

#define CMD(_str, _func) void _func(debugger_t *, int, char **);
COMMANDS
#undef CMD

void execute(debugger_t *debugger, char *command);

#endif // COMMANDS_H_
