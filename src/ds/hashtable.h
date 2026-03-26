#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <dwarf.h>
#include <libdwarf.h>

#define TABLE_SIZE 128

typedef struct {
    char *full_path;
    Dwarf_Die die;
} dwarf_die_path_t;

typedef struct ht_entry {
    struct ht_entry *next;
    char *key;
    dwarf_die_path_t *value;
} ht_entry_t;

typedef struct {
    ht_entry_t *buckets[TABLE_SIZE];
} hashtable_t;

hashtable_t *hashtable_init();
bool hashtable_insert(hashtable_t *table, char *key, Dwarf_Die die, char *full_path);
dwarf_die_path_t *hashtable_find(hashtable_t *table, char *key);
dwarf_die_path_t *hashtable_remove(hashtable_t *table, char *key);
void hashtable_free(hashtable_t *table);

void print_hashtable(hashtable_t *table);

#endif // HASHTABLE_H_
