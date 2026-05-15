#ifndef _BREAKPOINT_H_
#define _BREAKPOINT_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// source: twitch.tv/tsoding
#define da_append(xs, x)                                                       \
  do {                                                                         \
    if (xs.count >= xs.capacity) {                                             \
      if (xs.capacity == 0)                                                    \
        xs.capacity = 32;                                                      \
      else                                                                     \
        xs.capacity *= 2;                                                      \
      xs.items = realloc(xs.items, xs.capacity * sizeof(*xs.items));           \
    }                                                                          \
    xs.items[xs.count++] = x;                                                  \
    } while(0)

#define BREAKPOINTS_PARAMETERS                                                 \
  PARAM(addrs, uintptr_t)                                                      \
  PARAM(original_byte, uint8_t)                                                \
  PARAM(enabled, bool)

#define PARAM(_name, _type)                                                    \
  struct breakpoints_##_name {                                                 \
    _type *items;                                                              \
    size_t count;                                                              \
    size_t capacity;                                                           \
  };

BREAKPOINTS_PARAMETERS
#undef PARAM

struct breakpoints {
#define PARAM(_name, _type) struct breakpoints_##_name _name;
BREAKPOINTS_PARAMETERS
#undef PARAM
};


ssize_t add_breakpoint(struct breakpoints *lst, uintptr_t addr,
                       int8_t original_byte, bool enabled);
ssize_t find_breakpoint(struct breakpoints *lst, uintptr_t addr);
void breakpoints_deinit(struct breakpoints *lst);

#endif
