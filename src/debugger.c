#include "debugger.h"

int initialize_debugger_dwarf(debugger_t *debugger, char *path) {
    Dwarf_Error dw_err = 0;

    int res = dwarf_init_path(path, NULL, 0, DW_GROUPNUMBER_ANY, NULL, NULL, &debugger->dw_dbg, &dw_err);
    if(res == DW_DLV_ERROR) {
        fprintf(stderr, "Error dwarf_init_path: %s\n", dwarf_errmsg(dw_err));
        dwarf_dealloc_error(debugger->dw_dbg, dw_err);

        return DW_DLV_ERROR;
    } else if (res == DW_DLV_NO_ENTRY) {
        fprintf(stderr, "Error: Files without dwarf section are not supported yet.\nConsider compiling using -g flag.\n");

        return DW_DLV_NO_ENTRY;
    }

    return DW_DLV_OK;
}
