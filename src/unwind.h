#ifndef _UNWIND_H_
#define _UNWIND_H_

#include <sys/user.h>
#include <stddef.h>
#include <stdint.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "debugger.h"

struct stack_frame {
  struct stack_frame *next;
  uintptr_t fp;
};

struct backtrace {
  struct stack_frame *frames;
};

bool stack_unwind(struct debugger *debugger, struct backtrace *backtrace);
void backtrace_deinit(struct backtrace *backtrace);

#endif
