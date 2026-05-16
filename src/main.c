#include <stdio.h>
#include <string.h>

#include <libdwarf.h>

#include "debugger.h"
#include "commands.h"

#define MAX_LINE 4096

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int main(int argc, char *argv[], char *envp[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s [TARGET]\n", argv[0]);
    return 1;
  }

  struct debugger dbg = {
    .path = argv[1],
    .state = 0
  };

  int dw_res = debugger_init(&dbg, argv[1]);
  if (dw_res == DW_DLV_ERROR) {
    fprintf(stderr, "Dwarf error: %s\n", dwarf_errmsg(dbg.dw_err));
    return 1;
  } else if (dw_res == DW_DLV_NO_ENTRY) {
    fprintf(stderr, "No entry what the sigma\n");
    return 2;
  }

  char buff[MAX_LINE];
  while (!IS_EXIT(dbg.state)) {
    memset(buff, 0, MAX_LINE);
    printf("> ");
    if (!fgets(buff, MAX_LINE, stdin)) {
      perror("fgets");
      return 1;
    }

    char *newline = strchr(buff, '\n');
    if (newline)
      *newline = '\0';

    execute(&dbg, buff);
  }

  debugger_deinit(&dbg);
  return 0;
}
