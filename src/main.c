#include <stdio.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <libdwarf.h>

#include "debugger.h"
#include "ds/hashtable.h"

int main(int argc, char *argv[]) {
    if(argc == 1) {
        fprintf(stderr, "USAGE: yadb [TARGET] [ARGS]\n");
        return 1;
    }

    debugger_t debugger;
    // debugger.debugee = fork();
    debugger.debugee = 0;
    debugger.filenames_table = hashtable_init();

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

    if(initialize_debugger_dwarf(&debugger, argv[1]) != DW_DLV_OK)
        return 1;

    debugger_cu_walk(&debugger);
    print_hashtable(debugger.filenames_table);
    // wait(NULL);


    hashtable_free(debugger.filenames_table);
    dwarf_finish(debugger.dw_dbg);
    return 0;
}
