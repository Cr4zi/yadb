#include "debugger.h"
#include "ds/hashtable.h"
#include "dwarf.h"
#include "libdwarf.h"

bool str_equals(void *key1, void *key2) {
    if(!key1 || !key2)
        return false;

    return !strcmp((char *)key1, (char *)key2);
}

void free_die_path(void *ptr) {
    dwarf_die_path_t *die_path = (dwarf_die_path_t *)ptr;

    // libdwarf should take care of the die
    free(die_path->full_path);
    free(ptr);
}

/*
 * Taken from http://www.cse.yorku.ca/~oz/hash.html
 */
size_t djb2_hash(void *ptr) {
    size_t hash = 5381;
    unsigned char *str = (unsigned char *)ptr;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

bool breakpoints_equals(void *key1, void *key2) {
    return (uintptr_t)key1 == (uintptr_t)key2;
}

/*
 * Taken from https://stackoverflow.com/questions/664014
 */
size_t ptr_hash(void *ptr) {
    uintptr_t x = (uintptr_t)ptr;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = ((x >> 16) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;

    return (size_t)x;
}

int debugger_init(debugger_t *debugger, char *path) {
    debugger->debugee = 0;
    debugger->is_running = false;
    debugger->is_exit = false;
    debugger->full_path = path;

    debugger->filenames_table = hashtable_init(str_equals, free, free_die_path, djb2_hash);
    debugger->breakpoints_table = hashtable_init(breakpoints_equals, NULL, free, ptr_hash); // C lambda when?

    int res = initialize_debugger_dwarf(debugger, path);
    if(res != DW_DLV_OK)
        return res;

    debugger_cu_walk(debugger);

    return res;
}

int initialize_debugger_dwarf(debugger_t *debugger, char *path) {
    int res = dwarf_init_path(path, NULL, 0, DW_GROUPNUMBER_ANY, NULL, NULL, &debugger->dw_dbg, &debugger->dw_err);
    if(res == DW_DLV_ERROR) {
        fprintf(stderr, "Error dwarf_init_path: %s\n", dwarf_errmsg(debugger->dw_err));

        dwarf_dealloc_error(debugger->dw_dbg, debugger->dw_err);
    } else if (res == DW_DLV_NO_ENTRY) {
        fprintf(stderr, "Error: Files without dwarf section are not supported yet.\nConsider compiling using -g flag.\n");
    }

    return res;
}

static bool extract_filename(char *in_full_path, char **filename, char **out_full_path) {
    size_t length = strlen(in_full_path);
    *out_full_path = (char *)malloc(length + 1);
    if(*out_full_path == NULL)
        return false;

    memcpy(*out_full_path, in_full_path, length + 1);

    char *token = strtok(in_full_path, "/");
    char *last_token = token;

    while((token = strtok(NULL, "/")) != NULL)
        last_token = token;

    if(!last_token) // we got cooked smh
        return false;

    length = strlen(last_token);
    
    *filename = (char *)malloc(length + 1);
    if(*filename == NULL)
        return false;

    memcpy(*filename, last_token, length + 1);
    
    return true;
}

static bool insert_srcfiles(debugger_t *debugger, Dwarf_Die die) {
    if(!die)
        return false;

    char **srcfiles = NULL;
    Dwarf_Signed file_count = 0;
    Dwarf_Error err = 0;

    // I don't really like this code tbh
    int res = dwarf_srcfiles(die, &srcfiles, &file_count, &err);
    if(res == DW_DLV_OK) {
        for(Dwarf_Signed i = 0; i < file_count; ++i) {
            char *filename = NULL, *full_path = NULL;
            if(!access(srcfiles[i], R_OK) && extract_filename(srcfiles[i], &filename, &full_path)) {
                dwarf_die_path_t *die_path = (dwarf_die_path_t *)malloc(sizeof(dwarf_die_path_t));
                if(!die_path)
                    return NULL;

                die_path->die = die;
                die_path->full_path = full_path;
                
                if(!hashtable_insert(debugger->filenames_table, (void *)filename, (void *)die_path)) {
                    free(filename);
                    free(full_path);
                    free(die_path);
                }
            }

            dwarf_dealloc(debugger->dw_dbg, srcfiles[i], DW_DLA_STRING);
        }
    } else if(res == DW_DLV_ERROR) {
        fprintf(stderr, "Error: %s\n", dwarf_errmsg(err));
        return false;
    }

    dwarf_dealloc(debugger->dw_dbg, srcfiles, DW_DLA_LIST);
    return true;
}

bool debugger_srcfiles(debugger_t *debugger, Dwarf_Die die) {
    int res = DW_DLV_OK;
    Dwarf_Die cur_die = die;
    Dwarf_Die child = 0;

    bool used = insert_srcfiles(debugger, cur_die);

    while(1) {
        Dwarf_Die sib_die = 0;

        res = dwarf_child(cur_die, &child, &debugger->dw_err);
        if(res == DW_DLV_ERROR) {
            fprintf(stderr, "Couldn't open a child: %s\n", dwarf_errmsg(debugger->dw_err));
            return false;
        }

        if(res == DW_DLV_OK) {
            if(!debugger_srcfiles(debugger, child)) {
                dwarf_dealloc(debugger->dw_dbg, child, DW_DLA_DIE);
                child = 0;
            }
        }

        res = dwarf_siblingof_c(cur_die, &sib_die, &debugger->dw_err);
        if(res != DW_DLV_OK) {
            return false;
        }

        if(cur_die != die) {
            dwarf_dealloc(debugger->dw_dbg, cur_die, DW_DLA_DIE);
            cur_die = 0;
        }

        cur_die = sib_die;
        insert_srcfiles(debugger, cur_die);
    }

    return used;
}

void debugger_cu_walk(debugger_t *debugger) {
    Dwarf_Bool is_info = true;
    Dwarf_Die die = 0;
    Dwarf_Unsigned cu_header_length = 0;
    Dwarf_Off abbrev_offset = 0;
    Dwarf_Half version_stamp = 0, address_size = 0, length_size = 0, extension_size = 0, header_cu_type = 0;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset = 0, next_cu_header_offset = 0;
    int res = 0;

    /* This function ... smh */
    while((res = dwarf_next_cu_header_e(debugger->dw_dbg, is_info, &die,
                                        &cu_header_length, &version_stamp, &abbrev_offset,
                                        &address_size, &length_size, &extension_size,
                                        &type_signature, &typeoffset, &next_cu_header_offset,
                                        &header_cu_type, &debugger->dw_err)) == DW_DLV_OK) {

        if(header_cu_type == DW_UT_compile) {
            debugger_srcfiles(debugger, die);
        }
    }

}

uintptr_t debugger_get_base_addr(debugger_t *debugger) {
    #define PATH_LENGTH 64
    char path[PATH_LENGTH];
    snprintf(path, PATH_LENGTH, "/proc/%d/maps", debugger->debugee);
    #undef PATH_LENGTH

    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen maps");
        return 0;
    }

    uintptr_t base_addr = 0;
    fscanf(f, "%lx-", &base_addr);
    fclose(f);

    return base_addr;
}

uintptr_t debugger_get_line_addr(debugger_t *debugger, dwarf_die_path_t *die_path, unsigned long long line) {
    // I assume addr isn't going over 2^32
    Dwarf_Addr addr = 0;

    if(!die_path) {
        fprintf(stderr, "Die Path struct is null. This shouldn't happen\n");
        return addr;
    }

    Dwarf_Unsigned version_out;
    Dwarf_Small table_count;
    Dwarf_Line_Context line_context;
    Dwarf_Line *line_buf;
    Dwarf_Signed line_count;

    int res = dwarf_srclines_b(die_path->die, &version_out, &table_count, &line_context, &debugger->dw_err);
    if(res == DW_DLV_ERROR) {
        fprintf(stderr, "Failed initializing line table: %s\n", dwarf_errmsg(debugger->dw_err));

        dwarf_dealloc_error(debugger->dw_dbg, debugger->dw_err);
        return addr;
    } else if(res == DW_DLV_NO_ENTRY) {
        return addr;
    }

    if(table_count == 0) {
        dwarf_srclines_dealloc_b(line_context);

        return addr;
    } else if(table_count > 1) {
        dwarf_srclines_dealloc_b(line_context);

        fprintf(stderr, "This is some experimental stuff that I don't have power to implement now");
        return addr;
    }


    res = dwarf_srclines_from_linecontext(line_context, &line_buf, &line_count, &debugger->dw_err);
    if(res == DW_DLV_ERROR) {
        fprintf(stderr, "Failed source lines from line context: %s\n", dwarf_errmsg(debugger->dw_err));
        return addr;
    } else if(res == DW_DLV_NO_ENTRY) {
        return addr;
    }

    if(line_count == 0) {
        fprintf(stderr, "Line count is empty?\n");
        return addr;
    }

    for(Dwarf_Signed i = 0; i < line_count; ++i) {
        Dwarf_Unsigned line_number;
        char *src_file;

        res = dwarf_lineno(line_buf[i], &line_number, &debugger->dw_err);
        if(res != DW_DLV_OK) {
            break;
        }

       res = dwarf_linesrc(line_buf[i], &src_file, &debugger->dw_err);
       if(res != DW_DLV_OK) {
           break;
       }

       if(line_number == line && !strcmp(die_path->full_path, src_file)) {
           res = dwarf_lineaddr(line_buf[i], &addr, &debugger->dw_err);
           dwarf_dealloc(debugger->dw_dbg, src_file, DW_DLA_STRING);

           break;
       }

       dwarf_dealloc(debugger->dw_dbg, src_file, DW_DLA_STRING);
    }

    dwarf_srclines_dealloc_b(line_context);
    return addr;
}

static uintptr_t get_func_addr_in_die_helper(debugger_t *debugger, Dwarf_Die die,
                                  char *func_name) {
    char *die_name;
    int res = dwarf_diename(die, &die_name, &debugger->dw_err);
    if(res != DW_DLV_OK)
        return 0;

    Dwarf_Half tag;
    res = dwarf_tag(die, &tag, &debugger->dw_err);
    if(res != DW_DLV_OK)
        return 0;

    if(tag != DW_TAG_subprogram)
        return 0;

    if(strcmp(die_name, func_name))
        return 0;

    // if we came to here then this is the correct die
    Dwarf_Attribute *attr_list;
    Dwarf_Signed attr_count;
    res = dwarf_attrlist(die, &attr_list, &attr_count, &debugger->dw_err);
    if(res != DW_DLV_OK)
        return 0;

    Dwarf_Addr addr = 0;
    for(Dwarf_Signed i = 0; i < attr_count; ++i) {
        Dwarf_Half attr_num;
        res = dwarf_whatattr(attr_list[i], &attr_num, &debugger->dw_err);
        if(res != DW_DLV_OK)
            continue;

        if (attr_num == DW_AT_low_pc) {
            res = dwarf_formaddr(attr_list[i], &addr, &debugger->dw_err);
        }

        dwarf_dealloc_attribute(attr_list[i]);
    }

    dwarf_dealloc(debugger->dw_dbg, attr_list, DW_DLA_LIST);
    return addr;
}

static uintptr_t get_func_in_die(debugger_t *debugger, Dwarf_Die die,
                                 char *func_name) {
    Dwarf_Die iter_die;

    int res = dwarf_child(die, &iter_die, &debugger->dw_err);
    if(res != DW_DLV_OK)
        return 0;

    while(res != DW_DLV_NO_ENTRY) {
        uintptr_t addr =
            get_func_addr_in_die_helper(debugger, iter_die, func_name);

        if (addr != 0) {
                dwarf_dealloc(debugger->dw_dbg, iter_die, DW_DLA_DIE);
                return addr;
        }

        Dwarf_Die prev = iter_die;
        res = dwarf_siblingof_c(prev, &iter_die, &debugger->dw_err);

        dwarf_dealloc(debugger->dw_dbg, prev, DW_DLA_DIE);
    }

    if(res == DW_DLV_OK)
        dwarf_dealloc(debugger->dw_dbg, iter_die, DW_DLA_DIE);

    return 0;
}

uintptr_t get_func_addr(debugger_t *debugger, char *func_name) {
    for(int i = 0; i < TABLE_SIZE; ++i) {
        ht_entry_t *entry = debugger->filenames_table->buckets[i];
        while(entry) {
            dwarf_die_path_t *die_path = (dwarf_die_path_t *)entry->value;
            uintptr_t addr =
                get_func_in_die(debugger, die_path->die, func_name);
            if(addr != 0)
                return addr;

            entry = entry->next;
        }
    }

    return 0;
}

unsigned char set_byte_at_offset(debugger_t *debugger, uintptr_t offset,
                                unsigned char byte) {
    uintptr_t base_addr = debugger_get_base_addr(debugger);

    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, debugger->debugee, (void *)(offset + base_addr), NULL);

    if(word == -1 && errno) {
        perror("ptrace(PEEKDATA)");
        return 0;
    }

    unsigned char original_byte = word & 0xFF;

    word = (word & ~0xFF) | byte;

    if(ptrace(PTRACE_POKEDATA, debugger->debugee, (void *)(offset + base_addr), word) == -1) {
        perror("ptrace(POKEDATA)");
        return 0;
    }

    return original_byte;

}

unsigned char enable_breakpoint(debugger_t *debugger, uintptr_t offset) {
    breakpoint_t *breakpoint = (breakpoint_t *)hashtable_find(debugger->breakpoints_table, (void *)offset);
    if(breakpoint)
        breakpoint->is_enabled = true;

    /* 0xCC - int3 instruction */
    return set_byte_at_offset(debugger, offset, 0xCC);
}

void disable_breakpoint(debugger_t *debugger, uintptr_t offset) {
    breakpoint_t *breakpoint = (breakpoint_t *)hashtable_find(debugger->breakpoints_table, (void *)offset);
    if(!breakpoint) {
        fprintf(stderr, "No breakpoint exists at: %p\n", (void *)offset);
        return;
    }

    if(set_byte_at_offset(debugger, offset, breakpoint->original_byte))
        breakpoint->is_enabled = false;
}

void set_software_breakpoint(debugger_t *debugger, uintptr_t offset) {
    if(hashtable_find(debugger->breakpoints_table, (void *)offset))
        return;

    breakpoint_t *breakpoint = (breakpoint_t *)malloc(sizeof(breakpoint_t));
    if(!breakpoint)
        return;

    if(debugger->is_running)
        breakpoint->original_byte = enable_breakpoint(debugger, offset);

    breakpoint->is_enabled = true;

    hashtable_insert(debugger->breakpoints_table, (void *)offset, breakpoint);
    printf("Successfully set breakpoint at: %p\n", (void *)(offset));
}

void reenable_breakpoints(debugger_t *debugger) {
    hashtable_t *table = debugger->breakpoints_table;
    for(int i = 0; i < TABLE_SIZE; ++i) {
        ht_entry_t *entry = table->buckets[i];

        for(; entry != NULL; entry = entry->next) {
            breakpoint_t *breakpoint = entry->value;
            if(breakpoint->is_enabled) {
                unsigned char original_byte = enable_breakpoint(debugger, (uintptr_t)entry->key);

                if(original_byte != 0xCC) // if that breakpoint was set before running
                    breakpoint->original_byte = original_byte;
            }
        }
    }
}
