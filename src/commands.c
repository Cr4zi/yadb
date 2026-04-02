#include "commands.h"

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

static void cmd_break_func(debugger_t *debugger, dwarf_die_path_t *die_path, char *func_name) {
    assert(0 && "`break filename func_name` isn't implemented yet");
}

static void cmd_break_helper(debugger_t *debugger, char *filename, char *line_or_func) {
    if(!filename) {
        assert(0 && "break without filename currently not supported");
    }

    dwarf_die_path_t *file_die = (dwarf_die_path_t *)hashtable_find(debugger->filenames_table, (void *)filename);
    if(!file_die) {
        fprintf(stderr, "Unknown filename %s\n", filename);
        return;
    }

    if(line_or_func[0] >= '0' && line_or_func[0] <= '9')
        cmd_break_line(debugger, file_die, line_or_func);
    else
        cmd_break_func(debugger, file_die, line_or_func);
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
        return;
    }

    ptrace(PTRACE_SETOPTIONS, debugger->debugee, 0, PTRACE_O_TRACEEXEC);
    ptrace(PTRACE_CONT, debugger->debugee, 0, 0);

    waitpid(debugger->debugee, &status, 0);
    debugger->is_running = true;
    printf("Running: %d\n", debugger->debugee);
}

void cmd_step(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_step not implmented yet!");
}

void cmd_continue(debugger_t *debugger, int argc, char **args) {
    if(!debugger->is_running) {
        fprintf(stderr, "Process is not running, use run first.\n");
        return;
    }
    
    if(ptrace(PTRACE_CONT, debugger->debugee, SIGCONT, NULL) == -1)
        perror("Couldn't continue debugee");

    int status = 0;
    waitpid(debugger->debugee, &status, 0);
}

void cmd_print(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_print not implmented yet!");
}

void cmd_backtrace(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_backtrace not implmented yet!");
}

void cmd_exit(debugger_t *debugger, int argc, char **args) {
    kill(debugger->debugee, SIGKILL);
    debugger->is_exit = true;
}

void cmd_help(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_help not implmented yet!");
}
#pragma clang diagnostic pop
