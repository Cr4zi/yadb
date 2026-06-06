#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#include <sys/types.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "ds/ht.h"
#include "breakpoints.h"

static const uint8_t INT3_OPCODE = 0xCC;

struct die_path_pair {
  char *full_path;
  Dwarf_Die die;
};

#define IS_RUNNING(x) ((x) & 1)
#define SET_RUNNING(x) x = (x) ^ 1

#define IS_EXIT(x) (((x) >> 1) & 1)
#define SET_EXIT(x) x = (x) ^ (1 << 1)

struct debugger {
  struct breakpoints *breakpoints;

  Dwarf_Debug dw_dbg;
  Dwarf_Error dw_err;

  struct ht *srcfiles; /* Key: filename, Value: die_path_pair */

  char *path;
  uintptr_t base_addr;

  pid_t debugee;
  uint8_t state;
};

int32_t debugger_init(struct debugger *restrict debugger, const char *path);
void debugger_deinit(struct debugger *restrict debugger);

bool debugger_get_registers(struct debugger *debugger, struct user_regs_struct *regs);

bool debugger_set_breakpoint(struct debugger *debugger, uintptr_t offset);
bool debugger_enable_breakpoint(struct debugger *debugger, size_t indx);
bool debugger_disable_breakpoint(struct debugger *debugger, size_t indx);
void debugger_enable_all(struct debugger *debugger);

void debugger_continue(struct debugger *debugger);

uintptr_t debugger_get_line_addr(struct debugger *debugger, char *filename, uint64_t line);
uintptr_t debugger_get_func_addr(struct debugger *debugger, char *func_name);
char *debugger_get_func_name(struct debugger *debugger, uintptr_t addr);

int64_t debugger_get_word_at(struct debugger *debugger, uintptr_t addr);

#endif
