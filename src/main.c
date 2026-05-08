#include <stdio.h>

#include <libdwarf.h>

#include "debugger.h"
#include "ds/ht.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int main(int argc, char *argv[], char *envp[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s [TARGET]\n", argv[0]);
    return 1;
  }
  
  struct debugger dbg;
  int dw_res = debugger_init(&dbg, argv[1]);
  if (dw_res == DW_DLV_ERROR) {
    fprintf(stderr, "Dwarf error: %s\n", dwarf_errmsg(dbg.dw_err));
    return 1;
  } else if (dw_res == DW_DLV_NO_ENTRY) {
    fprintf(stderr, "No entry what the sigma\n");
    return 2;
  }

  debugger_deinit(&dbg);
  return 0;
}
