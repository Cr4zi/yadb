#include "commands.h"
#include "debugger.h"

static void cmd_break_line(struct debugger *debugger, char *filename, char *line);
/* static void cmd_break_func(struct debugger *debugger, char *func); */

void execute(struct debugger *debugger, char *command) {
  char *args[ARG_MAX] = {NULL};

  size_t argc = 0;
  args[argc++] = debugger->path;

  char *cmd = strtok(command, " ");
  if (!cmd) {
    fprintf(stderr, "No command\n");
    return;
  }

  char *token = NULL;
  while ((token = strtok(NULL, " ")) && argc < ARG_MAX)
    args[argc++] = token;

#define CMD(_cmd, _func)                                                       \
  if (!strcmp(_cmd, cmd)) {                                                    \
    _func(debugger, argc, args);                                               \
    return;                                                                    \
  }

  COMMANDS
#undef CMD

  fprintf(stderr, "Unknown command.\n");
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
void cmd_break(struct debugger *debugger, const size_t argc, char **args) {
  if (argc != 2 && argc != 3) {
    fprintf(stderr, "Invalid amount of arguments.\nSee `help break` for more information.\n");
    return;
  }

  if (argc == 3) {
    cmd_break_line(debugger, args[1], args[2]);
    return;
  }
}

void cmd_run(struct debugger *debugger, const size_t argc, char **args) {
  if (IS_RUNNING(debugger->state)) {
    fprintf(stderr, "Killing previously running process\n");
    kill(debugger->debugee, SIGKILL);
  }

  debugger->debugee = fork();

  if (debugger->debugee == 0) {

    // Disable ASLR
    int32_t old_personality = personality(0xffffffff);
    if (personality(old_personality & ADDR_NO_RANDOMIZE) == -1) {
      perror("personality");
      exit(1);
    }

    int64_t traceme = ptrace(PTRACE_TRACEME, 0, NULL, NULL);
    if (traceme == -1) {
      perror("ptrace(TRACEME)");
      exit(1);
    }

    raise(SIGSTOP);
    execve(debugger->path, args, NULL);
    
    perror("execve");
    exit(1);
  }

  int32_t status = 0;
  waitpid(debugger->debugee, &status, 0);

  if (!WIFSTOPPED(status)) {
    fprintf(stderr, "Program ended unexpectedly\n");
    return;
  }

  // When any execve is launched from the debugee we will stop.
  ptrace(PTRACE_SETOPTIONS, debugger->debugee, 0, PTRACE_O_TRACEEXEC);
  ptrace(PTRACE_CONT, debugger->debugee, NULL, NULL);

  waitpid(debugger->debugee, &status, 0);

  if (!WIFSTOPPED(status)) {
    fprintf(stderr, "Program ended unexpectedly\n");
    return;
  }

  SET_RUNNING(debugger->state);

  debugger_enable_all(debugger);

  debugger_continue(debugger);
}

void cmd_step(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_step not implemented");
}

void cmd_continue(struct debugger *debugger, const size_t argc,
                  char **args) {
  if (argc != 1) {
    fprintf(stderr, "Invalid usage of continue.\nSee `help break` for more information.\n");
    return;
  }

  debugger_continue(debugger);
}

void cmd_print(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_print not implemented");
}

void cmd_list(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_list not implemented");
}

void cmd_disable(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_disable not implemented");
}

void cmd_enable(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_enable not implemented");
}

void cmd_watchpoint(struct debugger *debugger, const size_t argc,
                    char **args) {
  assert(0 && "cmd_watchpoint not implemented");
}

void cmd_backtrace(struct debugger *debugger, const size_t argc,
                   char **args) {
  assert(0 && "cmd_backtrace not implemented");
}

void cmd_exit(struct debugger *debugger, const size_t argc, char **args) {
  SET_EXIT(debugger->state);
}

void cmd_help(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_help not implemented");
}

static void cmd_break_line(struct debugger *debugger, char *filename,
                           char *line) {
  uint64_t line_num = atol(line);
  if (line_num == 0 && line[0] != '0') {
    fprintf(stderr, "Couldn't parse line number.\n");
    return;
  }

  uintptr_t addr = debugger_get_line_addr(debugger, filename, line_num);
  if (!addr) {
    fprintf(stderr, "Couldn't find either file/line\n");
    return;
  }

  if (debugger_set_breakpoint(debugger, addr))
    printf("Successfully set breakpoint in %s at %lu.\n", filename, line_num);
  else
    fprintf(stderr, "Couldn't set breakpoint.\n");
}

/* static void cmd_break_func(struct debugger *debugger, char *func) { */


/* } */
