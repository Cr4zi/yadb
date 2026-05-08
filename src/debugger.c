#include "debugger.h"

static size_t djb2_hash(const void *str); // http://www.cse.yorku.ca/~oz/hash.html
static bool str_equals(const void *str1, const void *str2);
static void extract_last_element(char *src, char **out, const char *delm);
static struct die_path_pair *die_path_init(char *full_path, Dwarf_Die die);
static void free_die_path_pair(void *pair);
static int add_srcfiles(struct debugger *debugger, Dwarf_Die die, Dwarf_Half cu_header_type);
static void debugger_cu_walk(struct debugger *debugger);

int32_t debugger_init(struct debugger *restrict debugger, const char *path) {
  int32_t dw_res = dwarf_init_path(path, NULL, 0, DW_GROUPNUMBER_ANY, NULL,
                                   NULL, &debugger->dw_dbg, &debugger->dw_err);
  if (dw_res == DW_DLV_ERROR)
    dwarf_dealloc_error(debugger->dw_dbg, debugger->dw_err);

  if (dw_res != DW_DLV_OK)
    return dw_res;

  debugger->srcfiles = ht_init(djb2_hash, str_equals, free, free_die_path_pair);
  debugger_cu_walk(debugger);

  return dw_res;
}

void debugger_deinit(struct debugger *restrict debugger) {
  ht_deinit(debugger->srcfiles);
  dwarf_finish(debugger->dw_dbg);
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

static void extract_last_element(char *restrict src, char **out, const char *delm) {
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

static int add_srcfiles(struct debugger *debugger, Dwarf_Die die, Dwarf_Half cu_header_type) {
  if (cu_header_type != DW_UT_compile)
    return DW_DLV_OK;

  char **dw_srcfiles = NULL;
  
  Dwarf_Signed filecount = 0;
  int dw_res = dwarf_srcfiles(die, &dw_srcfiles, &filecount, &debugger->dw_err);
  if (dw_res != DW_DLV_OK)
    return dw_res;

  for (Dwarf_Signed i = 0; i < filecount; dwarf_dealloc(debugger->dw_dbg, dw_srcfiles[i], DW_DLA_STRING), ++i) {
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
  Dwarf_Unsigned abbrev_offset = 0, typeoffset = 0, next_cu_header = 0, cu_header_length = 0;
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
