#include "unwind.h"

struct cfi_lists {
  Dwarf_Cie *cie_data;
  Dwarf_Fde *fde_data;

  Dwarf_Signed cie_count;
  Dwarf_Signed fde_count;
};

#define REG_COUNT 8

struct unwinded_registers {
  union {
    struct {
      uint64_t rip, rsp, rbp, rbx;
      uint64_t r12, r13, r14, r15;
    };
    uint64_t regs[REG_COUNT];
  };

  int64_t cfa;
};

enum {
  REGS_RIP,
  REGS_RSP,
  REGS_RBP,
  REGS_RBX,
  REGS_R12,
  REGS_R13,
  REGS_R14,
  REGS_R15
};

// This is a funky hack every other index will return 0 so just reject
static const uint8_t dwarf_to_slot[17] = {
    [3] = REGS_RBX + 1,  [6] = REGS_RBP + 1,  [7] = REGS_RSP + 1,
    [12] = REGS_R12 + 1, [13] = REGS_R13 + 1, [14] = REGS_R14 + 1,
    [15] = REGS_R15 + 1, [16] = REGS_RIP + 1,
};

static const uint8_t slot_to_dwarf[REG_COUNT] = {
    [REGS_RIP] = 16, [REGS_RSP] = 7,  [REGS_RBP] = 6,  [REGS_RBX] = 3,
    [REGS_R12] = 12, [REGS_R13] = 13, [REGS_R14] = 14, [REGS_R15] = 15,
};

static void frames_dealloc(struct stack_frame *frames);
static int32_t get_cfi_lists(struct debugger *debugger,
                             struct cfi_lists *lists_out);
static void cfi_lists_dealloc(struct debugger *debugger,
                              struct cfi_lists *lists);
static void copy_regs(struct user_regs_struct *regs,
                      struct unwinded_registers *unwind_regs);
static struct stack_frame *
get_stack_frame_from_pc(struct debugger *debugger, struct cfi_lists *lists,
                        struct unwinded_registers *regs, bool is_top_frame);

bool stack_unwind(struct debugger *debugger, struct backtrace *backtrace) {
  if (!debugger || !backtrace)
    return false;

  struct cfi_lists lists;
  int32_t dw_res = get_cfi_lists(debugger, &lists);
  if (dw_res != DW_DLV_OK)
    return false;

  struct user_regs_struct regs;
  if (!debugger_get_registers(debugger, &regs)) {
    cfi_lists_dealloc(debugger, &lists);

    return false;
  }

  struct unwinded_registers unwind_regs;
  copy_regs(&regs, &unwind_regs);

  backtrace->frames = get_stack_frame_from_pc(debugger, &lists, &unwind_regs, true);

  cfi_lists_dealloc(debugger, &lists);

  return true;
}

void backtrace_deinit(struct backtrace *backtrace) {
  frames_dealloc(backtrace->frames);
}

static void frames_dealloc(struct stack_frame *frames) {
  if (!frames)
    return;

  frames_dealloc(frames->next);
  free(frames);
}

static int32_t get_cfi_lists(struct debugger *debugger,
                             struct cfi_lists *lists_out) {
  lists_out->cie_data = NULL;
  lists_out->fde_count = 0;

  lists_out->fde_data = NULL;
  lists_out->cie_count = 0;

  // Most elf files will have .eh_frame section over .debug_frame
  return dwarf_get_fde_list_eh(debugger->dw_dbg, &lists_out->cie_data,
                               &lists_out->cie_count, &lists_out->fde_data,
                               &lists_out->fde_count, &debugger->dw_err);
}

static void cfi_lists_dealloc(struct debugger *debugger,
                              struct cfi_lists *lists) {
  dwarf_dealloc_fde_cie_list(debugger->dw_dbg, lists->cie_data,
                             lists->cie_count, lists->fde_data,
                             lists->fde_count);
}

static int32_t get_regtable(struct debugger *debugger, Dwarf_Addr pc,
                            Dwarf_Fde pc_fde, Dwarf_Regtable3 *reg_table_out) {

  Dwarf_Regtable_Entry3 *reg_entry = (Dwarf_Regtable_Entry3 *)calloc(
      DW_REG_TABLE_SIZE, sizeof(Dwarf_Regtable_Entry3));

  reg_table_out->rt3_rules = reg_entry;
  reg_table_out->rt3_reg_table_size = DW_REG_TABLE_SIZE;

  Dwarf_Addr row_pc = 0;

  return dwarf_get_fde_info_for_all_regs3(pc_fde, pc, reg_table_out, &row_pc,
                                          &debugger->dw_err);
}

static void copy_regs(struct user_regs_struct *regs,
                      struct unwinded_registers *unwind_regs) {
  unwind_regs->rip = regs->rip;
  unwind_regs->rsp = regs->rsp;
  unwind_regs->rbp = regs->rbp;
  unwind_regs->rbx = regs->rbx;
  unwind_regs->r12 = regs->r12;
  unwind_regs->r13 = regs->r13;
  unwind_regs->r14 = regs->r14;
  unwind_regs->r15 = regs->r15;
  unwind_regs->cfa = 0;
}

static bool unwind_cfa(struct debugger *, struct unwinded_registers *regs,
                       Dwarf_Regtable_Entry3 *cfa, uint64_t *val) {
  switch (cfa->dw_value_type) {
  case DW_EXPR_OFFSET:
    *val = regs->regs[dwarf_to_slot[cfa->dw_regnum] - 1] + (Dwarf_Signed)cfa->dw_offset;
    return true;
  default:
    assert(0 && "??");
    return false;
  }
}

static bool unwind_register(struct debugger *debugger,
                            struct unwinded_registers *regs,
                            Dwarf_Regtable_Entry3 *entry, uint64_t *val) {
  if (entry->dw_regnum == DW_FRAME_UNDEFINED_VAL)
    return false;

  if (entry->dw_regnum == DW_FRAME_SAME_VAL)
    return true;

  switch (entry->dw_value_type) {
  case DW_EXPR_OFFSET:
    if (entry->dw_offset_relevant) {
      *val = (uint64_t)debugger_get_word_at(
          debugger, regs->cfa + (Dwarf_Signed)entry->dw_offset);
      break;
    }

    assert(entry->dw_regnum < 17 && "How is regnum larger than 17?");

    uint8_t indx = dwarf_to_slot[entry->dw_regnum];
    assert(indx && "Tried to use a register that wasn't been tracked on.");

    *val = regs->regs[indx - 1];
    break;
  case DW_EXPR_VAL_OFFSET:
    *val = regs->cfa + (Dwarf_Signed)entry->dw_offset;
    break;
  default:
    assert(0 && "expressions are not implemented yet!");
  }

  return true;
}

/*
 * Will return false when we cannot unwind CFA which means we cannot unwind
 * anymore.
 */
static bool unwind_registers(struct debugger *debugger,
                             struct unwinded_registers *regs,
                             Dwarf_Regtable3 *reg_table) {
  struct unwinded_registers original = *regs;

  uint64_t reg_value = 0;
  if (!unwind_cfa(debugger, &original, &reg_table->rt3_cfa_rule,
                       &reg_value)) {
    return false;
  }

  regs->cfa = reg_value;
  original.cfa = reg_value;

  regs->rsp = reg_value;

  for (uint8_t i = 0; i < REG_COUNT; ++i) {
    uint8_t reg_indx = slot_to_dwarf[i];
    reg_value = regs->regs[i];

    if (i == REGS_RSP)
      continue;

    if (!reg_indx)
      continue;

    if (!unwind_register(debugger, &original, &reg_table->rt3_rules[reg_indx],
                         &reg_value)) {
      continue;
    }

    regs->regs[i] = reg_value;
  }

  return true;
}

static struct stack_frame *
get_stack_frame_from_pc(struct debugger *debugger, struct cfi_lists *lists,
                        struct unwinded_registers *regs, bool is_top_frame) {
  struct stack_frame *sf = NULL;

  Dwarf_Addr pc = regs->rip - debugger->base_addr;
  pc = is_top_frame ? pc : pc - 1;

  Dwarf_Addr low_pc = 0, high_pc = 0;
  Dwarf_Fde pc_fde;
  int32_t dw_res = dwarf_get_fde_at_pc(lists->fde_data, pc, &pc_fde, &low_pc,
                                       &high_pc, &debugger->dw_err);

  if (dw_res != DW_DLV_OK)
    return sf;

  Dwarf_Regtable3 reg_table;
  if (get_regtable(debugger, pc, pc_fde, &reg_table) != DW_DLV_OK) {
    free(reg_table.rt3_rules);
    return sf;
  }

  if (!unwind_registers(debugger, regs, &reg_table)) {
    free(reg_table.rt3_rules);
    return sf;
  }

  sf = (struct stack_frame *)malloc(sizeof(struct stack_frame));
  if (!sf) {
    fprintf(stderr, "Could not allocate memory for stack frame struct\n");
    free(reg_table.rt3_rules);
    return sf;
  }

  sf->fp = pc;
  sf->next = get_stack_frame_from_pc(debugger, lists, regs, false);

  free(reg_table.rt3_rules);
  return sf;
}
