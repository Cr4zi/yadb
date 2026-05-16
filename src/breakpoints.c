#include "breakpoints.h"

ssize_t add_breakpoint(struct breakpoints *lst, uintptr_t addr,
                       uint8_t original_byte, bool enabled) {
  da_append(lst->addrs, addr);
  da_append(lst->original_byte, original_byte);
  da_append(lst->enabled, enabled);

  // since the count is supposed to be equal between all of them
  return lst->addrs.count - 1;
}

ssize_t find_breakpoint(struct breakpoints *lst, uintptr_t addr) {
  for (size_t i = 0; i < lst->addrs.count; ++i)
    if (lst->addrs.items[i] == addr)
      return i;

  return -1;
}

void breakpoints_deinit(struct breakpoints *lst) {
#define PARAM(_name, _type) free(lst->_name.items);
  BREAKPOINTS_PARAMETERS
#undef PARAM

  free(lst);
}
