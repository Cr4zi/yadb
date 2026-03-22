#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <libdwarf.h>

#include "debugger.h"

int main(int argc, char *argv[], char *envp[]) {
    if(argc == 1) {
        fprintf(stderr, "USAGE: yadb [TARGET] [ARGS]\n");
        return 1;
    }

    debugger_t debugger;
    debugger.debugee = fork();

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

    if(initialize_debugger_dwarf(&debugger, argv[1]) != DW_DLV_OK)
        return 1;

    wait(NULL);

    dwarf_finish(debugger.dw_dbg);
    return 0;
}
