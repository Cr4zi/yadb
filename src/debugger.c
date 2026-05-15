#include "debugger.h"
#include "breakpoints.h"
#include "libdwarf.h"
#include <sys/ptrace.h>

// http://www.cse.yorku.ca/~oz/hash.html
static size_t djb2_hash(const void *str);

static bool str_equals(const void *str1, const void *str2);
static void extract_last_element(char *src, char **out, const char *delm);
static struct die_path_pair *die_path_init(char *full_path, Dwarf_Die die);
static void free_die_path_pair(void *pair);
static uintptr_t get_base_addr(pid_t pid);
static int add_srcfiles(struct debugger *debugger, Dwarf_Die die,
                        Dwarf_Half cu_header_type);
static void debugger_cu_walk(struct debugger *debugger);
static int64_t get_word_at(struct debugger *debugger, uintptr_t offset);
static uint8_t set_byte_at(struct debugger *debugger, uintptr_t offset,
                        uint8_t byte);

int32_t debugger_init(struct debugger *restrict debugger, const char *path) {
  int32_t dw_res = dwarf_init_path(path, NULL, 0, DW_GROUPNUMBER_ANY, NULL,
                                   NULL, &debugger->dw_dbg, &debugger->dw_err);
  if (dw_res == DW_DLV_ERROR)
    dwarf_dealloc_error(debugger->dw_dbg, debugger->dw_err);

  if (dw_res != DW_DLV_OK)
    return dw_res;

  debugger->srcfiles = ht_init(djb2_hash, str_equals, free, free_die_path_pair);
  debugger_cu_walk(debugger);

  debugger->breakpoints =
      (struct breakpoints *)malloc(sizeof(struct breakpoints));
  if (!debugger->breakpoints) {
    perror("malloc(breakpoints)");
    return DW_DLV_ERROR;
  }

  debugger->breakpoints->addrs.count = 0;
  debugger->breakpoints->addrs.capacity = 0;
  debugger->breakpoints->addrs.items = NULL;

  debugger->breakpoints->enabled.count = 0;
  debugger->breakpoints->enabled.capacity = 0;
  debugger->breakpoints->enabled.items = NULL;

  debugger->breakpoints->original_byte.count = 0;
  debugger->breakpoints->original_byte.capacity = 0;
  debugger->breakpoints->original_byte.items = NULL;

  return dw_res;
}

void debugger_deinit(struct debugger *restrict debugger) {
  breakpoints_deinit(debugger->breakpoints);
  ht_deinit(debugger->srcfiles);
  dwarf_finish(debugger->dw_dbg);
}

bool debugger_set_breakpoint(struct debugger *debugger, uintptr_t offset) {
  if (find_breakpoint(debugger->breakpoints, offset) != -1)
    return false;

  uint8_t original_byte = 0;

  bool enabled = false;
  if (IS_RUNNING(debugger->state)) {
    enabled = true;
    original_byte = set_byte_at(debugger, offset, INT3_OPCODE);
  }

  printf("Set breakpoint at: %p\n", (void *)offset);

  add_breakpoint(debugger->breakpoints, offset, original_byte, enabled);
  return true;
}

bool debugger_enable_breakpoint(struct debugger *debugger, size_t indx) {
  struct breakpoints *breakpoints = debugger->breakpoints;

  if (!IS_RUNNING(debugger->state) || indx >= breakpoints->enabled.count)
    return false;

  uintptr_t offset = breakpoints->addrs.items[indx];
  uint8_t byte = set_byte_at(debugger, offset, INT3_OPCODE);

  if (breakpoints->original_byte.items[indx] == 0)
    breakpoints->original_byte.items[indx] = byte;

  breakpoints->enabled.items[indx] = true;

  return true;
}

bool debugger_disable_breakpoint(struct debugger *debugger, size_t indx) {
  struct breakpoints *breakpoints = debugger->breakpoints;

  if (!IS_RUNNING(debugger->state) || indx >= breakpoints->enabled.count)
    return false;

  if (!breakpoints->enabled.items[indx])
    return false;

  uintptr_t offset = breakpoints->addrs.items[indx];

  set_byte_at(debugger, offset, breakpoints->original_byte.items[indx]);
  breakpoints->enabled.items[indx] = false;

  return true;
}

void debugger_enable_all(struct debugger *debugger) {
  struct breakpoints *breakpoints = debugger->breakpoints;
  for (size_t i = 0; i < breakpoints->addrs.count; ++i)
    debugger_enable_breakpoint(debugger, i);
}

void debugger_continue(struct debugger *debugger) {
  int32_t status = 0;

  if (!IS_RUNNING(debugger->state)) {
    fprintf(stderr, "Process is not running, use run first.\n");
    return;
  }

  struct user_regs_struct regs;
  if (ptrace(PTRACE_GETREGS, debugger->debugee, NULL, &regs) == -1) {
    perror("ptrace(GETREGS)");
    return;
  }

  uintptr_t base_addr = get_base_addr(debugger->debugee);
  uintptr_t instr = regs.rip - base_addr - 1;

  ssize_t indx;
  if ((indx = find_breakpoint(debugger->breakpoints, instr)) != -1) {
    regs.rip = regs.rip - 1;

    if (ptrace(PTRACE_SETREGS, debugger->debugee, NULL, &regs) == -1) {
      perror("ptrace(SETREGS)");
      return;
    }

    debugger_disable_breakpoint(debugger, indx);

    if (ptrace(PTRACE_SINGLESTEP, debugger->debugee, NULL, NULL) == -1) {
      perror("ptrace(SINGLESTEP)");
      return;
    }

    waitpid(debugger->debugee, &status, 0);
    if (!WIFSTOPPED(status)) {
      fprintf(stderr, "Program stopped unexpectedly.\n");
      SET_RUNNING(debugger->state);
      return;
    }

    debugger_enable_breakpoint(debugger, indx);
  }

  if (ptrace(PTRACE_CONT, debugger->debugee, NULL, NULL) == -1) {
    perror("ptrace(CONT)");
    return;
  }

  waitpid(debugger->debugee, &status, 0);
  if (!WIFSTOPPED(status)) {
    fprintf(stderr, "Program stopped unexpectedly.\n");
    SET_RUNNING(debugger->state);
    return;
  }
}

uintptr_t debugger_get_line_addr(struct debugger *debugger, char *filename,
                                 uint64_t line) {
  Dwarf_Addr addr = 0;
  int dw_res;

  struct ht_entry *entry = NULL;
  if (!(entry = (struct ht_entry *)ht_search(debugger->srcfiles, filename)))
    return addr;

  struct die_path_pair *pair = (struct die_path_pair *)entry->value;

  Dwarf_Unsigned line_version = 0;
  Dwarf_Small table_count = 0;
  Dwarf_Line_Context line_context = 0;

  dw_res = dwarf_srclines_b(pair->die, &line_version, &table_count, &line_context, &debugger->dw_err);
  if (dw_res != DW_DLV_OK)
    return addr;

  // table_count = 0 => a line table without lines
  // table_count = 2 => some experimental stuff
  if (table_count == 0 || table_count == 2) {
    dwarf_srclines_dealloc_b(line_context);
    return addr;
  }

  Dwarf_Line *lines = NULL;
  Dwarf_Signed line_count = 0;

  dw_res = dwarf_srclines_from_linecontext(line_context, &lines, &line_count, &debugger->dw_err);
  if (dw_res != DW_DLV_OK)
    return addr;

  for (Dwarf_Signed i = 0; i < line_count; ++i) {
    Dwarf_Unsigned line_number = 0;
    char *srcfile = NULL;

    dw_res = dwarf_lineno(lines[i], &line_number, &debugger->dw_err);
    if (dw_res != DW_DLV_OK)
      break;

    dw_res = dwarf_linesrc(lines[i], &srcfile, &debugger->dw_err);
    if (dw_res != DW_DLV_OK)
      break;

    if (line_number == line && !strcmp(srcfile, pair->full_path)) {
      dwarf_dealloc(debugger->dw_dbg, srcfile, DW_DLA_STRING);
      dw_res = dwarf_lineaddr(lines[i], &addr, &debugger->dw_err);

      break;
    }

    dwarf_dealloc(debugger->dw_dbg, srcfile, DW_DLA_STRING);
  }

  dwarf_srclines_dealloc_b(line_context);
  return addr;
}

static size_t djb2_hash(const void *key) {
  const char *str = (const char *)key;

  size_t hash = 5381;
  int32_t c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

static bool str_equals(const void *str1, const void *str2) {
  return !strcmp((const char *)str1, (const char *)str2);
}

static void extract_last_element(char *restrict src, char **out,
                                 const char *delm) {
  char *tok = strtok(src, delm);
  char *last_tok = tok;
  while (tok) {
    last_tok = tok;
    tok = strtok(NULL, delm);
  }

  *out = strdup(last_tok);
}

static struct die_path_pair *die_path_init(char *full_path, Dwarf_Die die) {
  struct die_path_pair *pair =
      (struct die_path_pair *)malloc(sizeof(struct die_path_pair));

  if (!pair) {
    fprintf(stderr, "No memory\n");
    return NULL;
  }

  pair->full_path = full_path;
  pair->die = die;

  return pair;
}

static void free_die_path_pair(void *pair) {
  struct die_path_pair *conv_pair = (struct die_path_pair *)pair;

  free(conv_pair->full_path);
  /* dwarf_dealloc_die(conv_pair->die); */
  free(conv_pair);
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

static int add_srcfiles(struct debugger *debugger, Dwarf_Die die,
                        Dwarf_Half cu_header_type) {
  if (cu_header_type != DW_UT_compile)
    return DW_DLV_OK;

  char **dw_srcfiles = NULL;

  Dwarf_Signed filecount = 0;
  int dw_res = dwarf_srcfiles(die, &dw_srcfiles, &filecount, &debugger->dw_err);
  if (dw_res != DW_DLV_OK)
    return dw_res;

  for (Dwarf_Signed i = 0; i < filecount;
       dwarf_dealloc(debugger->dw_dbg, dw_srcfiles[i], DW_DLA_STRING), ++i) {
    if (access(dw_srcfiles[i], R_OK))
      continue;

    char *full_path = strdup(dw_srcfiles[i]);
    char *filename = NULL;
    extract_last_element(dw_srcfiles[i], &filename, "/");

    struct die_path_pair *pair = die_path_init(full_path, die);

    if (!ht_insert(debugger->srcfiles, filename, pair)) {
      free(filename);
      free_die_path_pair(pair);
    }
  }

  dwarf_dealloc(debugger->dw_dbg, dw_srcfiles, DW_DLA_LIST);
  return dw_res;
}

static void debugger_cu_walk(struct debugger *debugger) {
  Dwarf_Die die = 0;
  Dwarf_Unsigned abbrev_offset = 0, typeoffset = 0, next_cu_header = 0,
                 cu_header_length = 0;
  Dwarf_Half address_size = 0, version_stamp = 0, offset_size = 0,
             extension_size = 0, header_cu_type = 0;
  Dwarf_Sig8 signature;
  Dwarf_Bool is_info = true;
  int dw_res = 0;

  // Who the fuck decided that this will be a function signature
  while ((dw_res = dwarf_next_cu_header_e(
              debugger->dw_dbg, is_info, &die, &cu_header_length,
              &version_stamp, &abbrev_offset, &address_size, &offset_size,
              &extension_size, &signature, &typeoffset, &next_cu_header,
              &header_cu_type, &debugger->dw_err)) == DW_DLV_OK) {

    add_srcfiles(debugger, die, header_cu_type);
  }
}

static int64_t get_word_at(struct debugger *debugger, uintptr_t offset) {
  return ptrace(PTRACE_PEEKDATA, debugger->debugee,
                offset + get_base_addr(debugger->debugee), NULL);
}

static uint8_t set_byte_at(struct debugger *debugger, uintptr_t offset,
                        uint8_t byte) {
  int64_t word = get_word_at(debugger, offset);
  if (word == -1) {
    fprintf(stderr, "Couldn't get word at %p\n", (void *)offset);
    perror("ptrace(PEEKDATA)");
    return 0;
  }

  uint8_t original_byte = word & 0xFF;

  word = (word & ~0xFF) | byte;

  if (ptrace(PTRACE_POKEDATA, debugger->debugee,
             offset + get_base_addr(debugger->debugee), word)) {
    perror("ptrace(POKEDATA)");
    return 0;
  }

  return original_byte;
}
