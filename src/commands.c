#include "commands.h"
#include "ds/hashtable.h"

void execute(debugger_t *debugger, char *command) {
    int argc = 0;
    char *args[MAX_ARGC] = {NULL};

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
static void cmd_break_line(dwarf_die_path_t *die_path, char *line_chr) {
    long line = atol(line_chr);
    if(line == 0 && line_chr[0] != '0') {
        fprintf(stderr, "Failed parsing line\n");
        return;
    }

    assert(0 && "Still not finished lmao");
}

static void cmd_break_func(dwarf_die_path_t *die_path, char *func_name) {
    assert(0 && "`break filename func_name` isn't implemented yet");
}

static void cmd_break_helper(debugger_t *debugger, char *filename, char *line_or_func) {
    if(!filename) {
        assert(0 && "break without filename currently not supported");
    }

    // will use later
    dwarf_die_path_t *file_die = hashtable_find(debugger->filenames_table, filename);
    if(!file_die) {
        fprintf(stderr, "Unknown filename %s\n", filename);
        return;
    }

    if(line_or_func[0] >= '0' && line_or_func[0] <= '9')
        cmd_break_line(file_die, line_or_func);
    else
        cmd_break_func(file_die, line_or_func);
}

void cmd_break(debugger_t *debugger, int argc, char **args) {
    if(argc != 1 && argc != 2) {
        fprintf(stderr, "Invalid amounts of arguments for break\nWrite `help break` for more info\n");
        return;
    }

    if(argc == 1)
        cmd_break_helper(debugger, NULL, args[0]);
    else
        cmd_break_helper(debugger, args[0], args[1]);
}

void cmd_run(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_run not implmented yet!");
}

void cmd_step(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_step not implmented yet!");
}

void cmd_continue(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_continue not implmented yet!");
}

void cmd_print(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_print not implmented yet!");
}

void cmd_backtrace(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_backtrace not implmented yet!");
}

void cmd_help(debugger_t *debugger, int argc, char **args) {
    assert(0 && "cmd_help not implmented yet!");
}
#pragma clang diagnostic pop
