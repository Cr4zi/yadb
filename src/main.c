#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <libdwarf.h>

#include "debugger.h"
#include "commands.h"
#include "ds/hashtable.h"

#define MAX_COMMAND_LENGTH 128

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
int main(int argc, char *argv[], char *envp[]) {
    if(argc == 1) {
        fprintf(stderr, "USAGE: yadb [TARGET] [ARGS]\n");
        return 1;
    }

    debugger_t debugger;
    // debugger.debugee = fork();

    /*
    if(debugger.debugee == 0) {
        long traceme = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if(traceme == -1) {
            perror("ptrace(TRACEME)\n");
            return 1;
        }

        if(execve(argv[1], argv + 1, envp) == -1)
            perror("execve\n");

        return 1;
    }
    */
    char cmd_buff[MAX_COMMAND_LENGTH];
    memset(cmd_buff, 0, MAX_COMMAND_LENGTH);
    /* Up until I have the basics down, I'll use fgets otherwise I'll switch to ncurses */
    printf("> ");
    if(!fgets(cmd_buff, MAX_COMMAND_LENGTH, stdin)) {
        perror("fgets");
        exit(1);
    }

    char *newline = strchr(cmd_buff, '\n');
    if(newline)
        *newline = '\0';

    
    debugger.filenames_table = hashtable_init();

    if(initialize_debugger_dwarf(&debugger, argv[1]) != DW_DLV_OK)
        return 1;

    debugger_cu_walk(&debugger);
    // wait(NULL);
    execute(&debugger, cmd_buff);

    hashtable_free(debugger.filenames_table);
    dwarf_finish(debugger.dw_dbg);
    return 0;
}
#pragma clang diagnostic pop
