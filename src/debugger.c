#include "debugger.h"
#include <string.h>

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
                if(!hashtable_insert(debugger->filenames_table, filename, die, full_path)) {
                    free(filename);
                    free(full_path);
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
