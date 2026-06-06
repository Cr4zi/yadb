#include "commands.h"
#include "debugger.h"

static void print_backtrace(struct debugger *debugger, struct backtrace *backtrace);
static uintptr_t get_base_addr(pid_t pid);
static void cmd_break_line(struct debugger *debugger, char *filename, char *line);
static void cmd_break_func(struct debugger *debugger, char *func);

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

  cmd_break_func(debugger, args[1]);
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
  debugger->base_addr = get_base_addr(debugger->debugee);

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
  if (argc != 2) {
    fprintf(stderr, "Invalid amount of arguments.\nSee `help disable` for more information.\n");
    return;
  }

  size_t indx = atol(args[1]);

  if (debugger_disable_breakpoint(debugger, indx)) {
    // This access is valid since we check it inside debugger_disable_breakpoint
    printf("Successfully disabled breakpoint at %p\n", (void *)debugger->breakpoints->addrs.items[indx]);
  }
}

void cmd_enable(struct debugger *debugger, const size_t argc, char **args) {
  if (argc != 2) {
    fprintf(stderr, "Invalid amount of arguments.\nSee `help enable` for more information.\n");
    return;
  }

  size_t indx = atol(args[1]);

  if (debugger_enable_breakpoint(debugger, indx)) {
    // This access is valid since we check it inside debugger_enable_breakpoint
    printf("Successfully enabled breakpoint at %p\n", (void *)debugger->breakpoints->addrs.items[indx]);
  }
}

void cmd_watchpoint(struct debugger *debugger, const size_t argc,
                    char **args) {
  assert(0 && "cmd_watchpoint not implemented");
}

void cmd_backtrace(struct debugger *debugger, const size_t argc,
                   char **args) {
  if (argc != 1) {
    fprintf(stderr, "Invalid amount of arguments.\nSee `help backtrace` for more information.\n");
    return;
  }

  struct backtrace bt;
  if (!stack_unwind(debugger, &bt)) {
    fprintf(stderr, "Couldn't unwind the stack.\n");
    return;
  }

  print_backtrace(debugger, &bt);
  backtrace_deinit(&bt);
}

void cmd_exit(struct debugger *debugger, const size_t argc, char **args) {
  SET_EXIT(debugger->state);
}

void cmd_help(struct debugger *debugger, const size_t argc, char **args) {
  assert(0 && "cmd_help not implemented");
}

static void print_frames(struct debugger *debugger, struct stack_frame *frame, uint64_t frame_num) {
  if (!frame)
    return;

  print_frames(debugger, frame->next, frame_num + 1);

  uintptr_t addr = frame->fp;
  char *name = debugger_get_func_name(debugger, addr);
  if (!name)
    printf("#%lu couldn't find name at %p\n", frame_num, (void *)addr);
  else {
    printf("#%lu %s () at %p\n", frame_num, name, (void *)addr);
    free(name);
  }
}

static void print_backtrace(struct debugger *debugger, struct backtrace *backtrace) {
  print_frames(debugger, backtrace->frames, 0);
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

static void cmd_break_func(struct debugger *debugger, char *func) {
  uintptr_t addr = debugger_get_func_addr(debugger, func);
  if (!addr) {
    fprintf(stderr, "Couldn't find function name: %s\n", func);
    return;
  }

  if (debugger_set_breakpoint(debugger, addr))
    printf("Successfully set breakpoint at function: %s\n", func);
  else
    fprintf(stderr, "Couldn't set breakpoint.\n");
}

static uintptr_t get_base_addr(pid_t pid) {
#define LENGTH 32
  char path[32];
  snprintf(path, LENGTH, "/proc/%d/maps", pid);
#undef LENGTH

  FILE *f = fopen(path, "r");
  if (!f) {
    perror("fopen(maps)");
    return 0;
  }

  uintptr_t base_addr = 0;
  fscanf(f, "%lx-", &base_addr);
  fclose(f);

  return base_addr;
}
