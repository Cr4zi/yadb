#include "commands.h"
#include "debugger.h"
#include "ds/hashtable.h"
#include <stdlib.h>

static uintptr_t arg_to_uintpr(char *arg) {
    uintptr_t addr;
    if(sscanf(arg, "%lx", &addr) == 0) {
        fprintf(stderr, "Expected a valid address\n");
        return 0;
    }

    return addr;
}

void execute(debugger_t *debugger, char *command) {
    int argc = 0;
    char *args[MAX_ARGC] = {NULL};
    args[argc++] = debugger->full_path;

    char *cmd = strtok(command, " ");
    if(!cmd) {
        fprintf(stderr, "Unkown Command.\nWrite help for help.\n");
        return;
    }

    char *arg = NULL;
    while((arg = strtok(NULL, " ")) != NULL && argc < MAX_ARGC)
        args[argc++] = arg;

#define CMD(_str, _func)                        \
    if(!strcmp(cmd, _str)) {                    \
        _func(debugger, argc, args);            \
        return;                                 \
    }
    COMMANDS
#undef CMD

    fprintf(stderr, "Unkown Command.\nWrite help for help.\n");
    return;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
static void cmd_break_line(debugger_t *debugger, dwarf_die_path_t *die_path, char *line_chr) {
    long line = atol(line_chr);
    if(line == 0 && line_chr[0] != '0') {
        fprintf(stderr, "Failed parsing line\n");
        return;
    }

    if(line < 0) {
        fprintf(stderr, "Invalid line number %ld\n", line);
        return;
    }

    uintptr_t addr = debugger_get_line_addr(debugger, die_path, (unsigned long long)line);
    set_software_breakpoint(debugger, addr);
}

static void cmd_break_func(debugger_t *debugger, char *func_name) {
    uintptr_t addr = get_func_addr(debugger, func_name);
    if (addr == 0)
        printf("Couldn't find function %s\n", func_name);

    set_software_breakpoint(debugger, addr);
}

static void cmd_break_helper(debugger_t *debugger, char *filename, char *line_or_func) {
    dwarf_die_path_t *file_die = NULL;

    if(filename) {
        file_die = (dwarf_die_path_t *)hashtable_find(debugger->filenames_table, (void *)filename);
        if(!file_die) {
            fprintf(stderr, "Unknown filename %s\n", filename);
            return;
        }
    }

    if(line_or_func[0] >= '0' && line_or_func[0] <= '9' && file_die)
        cmd_break_line(debugger, file_die, line_or_func);
    else
        cmd_break_func(debugger, line_or_func);
}

void cmd_break(debugger_t *debugger, int argc, char **args) {
    if(argc != 2 && argc != 3) {
        fprintf(stderr, "Invalid amounts of arguments for break\nWrite `help break` for more info\n");
        return;
    }

    if(argc == 2)
        cmd_break_helper(debugger, NULL, args[1]);
    else
        cmd_break_helper(debugger, args[1], args[2]);
}

void cmd_run(debugger_t *debugger, int argc, char **args) {
    if(debugger->is_running) {
        fprintf(stderr, "Killing previous running process.\n");
        kill(debugger->debugee, SIGKILL);
    }

    debugger->debugee = fork();

    if(debugger->debugee == 0) {
        // Disabling Address Space Layout Randomization

        int old_personality = personality(0xffffffff); // get current
        if (personality(old_personality | ADDR_NO_RANDOMIZE) == -1) {
            perror("personality");
            return;
        }

        long traceme = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        if(traceme == -1) {
            perror("ptrace(TRACEME)");
            return;
        }

        raise(SIGSTOP);
        execve(debugger->full_path, args, NULL);

        perror("execve");
        return;
    }

    int status = 0;
    waitpid(debugger->debugee, &status, 0);
    if(!WIFSTOPPED(status)) {
        fprintf(stderr, "Program ended unexpectedly.\n");
        debugger->is_running = false;
        return;
    }

    ptrace(PTRACE_SETOPTIONS, debugger->debugee, 0, PTRACE_O_TRACEEXEC);
    ptrace(PTRACE_CONT, debugger->debugee, 0, 0);

    waitpid(debugger->debugee, &status, 0);
    debugger->is_running = true;

    reenable_breakpoints(debugger);

    cmd_continue(debugger, argc, args);
}

void cmd_step(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_step not implmented yet!");
}

void cmd_continue(debugger_t *debugger, int argc, char **args) {
    int status = 0;

    if(!debugger->is_running) {
        fprintf(stderr, "Process is not running, use run first.\n");
        return;
    }
    
    struct user_regs_struct regs;
    if(ptrace(PTRACE_GETREGS, debugger->debugee, NULL, &regs) == -1) {
        perror("PTRACE(GETREGS)");
        return;
    }

    uintptr_t base_addr = debugger_get_base_addr(debugger);
    uintptr_t instr = regs.rip - base_addr - 1;

    // if there is a breakpoint at the address of rip
    if(hashtable_find(debugger->breakpoints_table,
                       (void *)(instr))) {
        // since rip already read the modified byte, we want to return it before reading that byte.
        regs.rip = regs.rip - 1;

        if(ptrace(PTRACE_SETREGS, debugger->debugee, NULL, &regs) == -1) {
            perror("PTRACE(SETREGS)");
            return;
        }

        disable_breakpoint(debugger, instr);

        if(ptrace(PTRACE_SINGLESTEP, debugger->debugee, 0, 0) == -1) {
            perror("PTRACE(SINGLESTEP)");
            return;
        }

        waitpid(debugger->debugee, &status, 0);
        enable_breakpoint(debugger, instr);
    }

    if(ptrace(PTRACE_CONT, debugger->debugee, 0, NULL) == -1)
        perror("PTRACE(CONT)");

    waitpid(debugger->debugee, &status, 0);

    if(!WIFSTOPPED(status)) {
        debugger->is_running = false;
    }
}

void cmd_print(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_print not implmented yet!");
}

void cmd_list(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_list not implmented yet!");
}

void cmd_disable(debugger_t *debugger, int argc, char **args) {
    if(argc != 2) {
        fprintf(stderr, "Expected breakpoint address\n");
        return;
    }

    uintptr_t addr = arg_to_uintpr(args[1]);
    if(addr == 0)
        return;

    if(disable_breakpoint(debugger, addr))
        printf("Disabled breakpoint at: %p\n", (void *)addr);
}

void cmd_enable(debugger_t *debugger, int argc, char **args) {
    if(argc != 2) {
        fprintf(stderr, "Expected breakpoint address\n");
        return;
    }

    uintptr_t addr = arg_to_uintpr(args[1]);

    enable_breakpoint(debugger, addr);
    printf("Enabled breakpoint at: %p\n", (void *)addr);
}

void cmd_backtrace(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_backtrace not implmented yet!");
}

void cmd_watchpoint(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_watchpoint not implmented yet!");
}

void cmd_exit(debugger_t *debugger, int argc, char **args) {
    if(debugger->is_running)
        kill(debugger->debugee, SIGKILL);

    debugger->is_exit = true;
}

void cmd_help(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_help not implmented yet!");
}
#pragma clang diagnostic pop
