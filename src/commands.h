#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <assert.h>
#include <string.h>
#include <linux/limits.h>

#include "debugger.h"

#define COMMANDS                                                               \
  CMD("break", cmd_break)                                                      \
  CMD("run", cmd_run)                                                          \
  CMD("step", cmd_step)                                                        \
  CMD("continue", cmd_continue)                                                \
  CMD("print", cmd_print)                                                      \
  CMD("list", cmd_list)                                                        \
  CMD("disable", cmd_disable)                                                  \
  CMD("enable", cmd_enable)                                                    \
  CMD("watchpoint", cmd_watchpoint)                                            \
  CMD("backtrace", cmd_backtrace)                                              \
  CMD("exit", cmd_exit)                                                        \
  CMD("help", cmd_help)

#define CMD(_cmd, _func) void _func(struct debugger *, const size_t, char **);
COMMANDS
#undef CMD

void execute(struct debugger *debugger, char *command);

#endif
