#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dwarf.h>
#include <libdwarf.h>

#define TABLE_SIZE 128

typedef size_t (*HashFunc)(void *);
typedef bool (*EqualsFunc)(void *, void *);
typedef void (*FreeKeyFunc)(void *);
typedef void (*FreeValueFunc)(void *);

typedef struct ht_entry {
  struct ht_entry *next;
  void *key;
  void *value;
} ht_entry_t;

typedef struct {
  ht_entry_t *buckets[TABLE_SIZE];
  EqualsFunc equals;
  FreeKeyFunc free_key;
  FreeValueFunc free_value;
  HashFunc hash;
} hashtable_t;

/*
 * equals is mandatory
 * free functions are optional if null - use free
 */
hashtable_t *hashtable_init(EqualsFunc equals, FreeKeyFunc free_key,
                            FreeValueFunc free_value, HashFunc hash);
bool hashtable_insert(hashtable_t *table, void *key, void *value);
void *hashtable_find(hashtable_t *table, void *key);
void *hashtable_remove(hashtable_t *table, void *key);
void hashtable_free(hashtable_t *table);

#endif // HASHTABLE_H_
