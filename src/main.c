#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <libdwarf.h>

#include "commands.h"
#include "debugger.h"
#include "ds/hashtable.h"

#define MAX_COMMAND_LENGTH 128

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "USAGE: yadb [TARGET]\n");
    return 1;
  }

  debugger_t debugger;
  if (debugger_init(&debugger, argv[1]) != DW_DLV_OK)
    return 1;

  // wait(NULL);
  char cmd_buff[MAX_COMMAND_LENGTH];
  while (!debugger.is_exit) {
    memset(cmd_buff, 0, MAX_COMMAND_LENGTH);
    /* Up until I have the basics down, I'll use fgets otherwise I'll switch to
     * ncurses */
    printf("> ");
    if (!fgets(cmd_buff, MAX_COMMAND_LENGTH, stdin)) {
      perror("fgets");
      exit(1);
    }

    char *newline = strchr(cmd_buff, '\n');
    if (newline)
      *newline = '\0';

    execute(&debugger, cmd_buff);
  }

  hashtable_free(debugger.breakpoints_table);
  hashtable_free(debugger.filenames_table);
  dwarf_finish(debugger.dw_dbg);
  return 0;
}
#pragma clang diagnostic pop
