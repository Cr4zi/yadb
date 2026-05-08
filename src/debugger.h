#ifndef _DEBUGGER_H_
#define _DEBUGGER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include <dwarf.h>
#include <libdwarf.h>

#include "ds/ht.h"

struct die_path_pair {
  char *full_path;
  Dwarf_Die die;
};

struct debugger {
  Dwarf_Debug dw_dbg;
  Dwarf_Error dw_err;

  struct ht *srcfiles; /* Key: filename, Value: die_path_pair */
};

int32_t debugger_init(struct debugger *restrict debugger, const char *path);
void debugger_deinit(struct debugger *restrict debugger);

#endif
