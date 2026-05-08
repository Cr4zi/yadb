#ifndef _HT_H_
#define _HT_H_

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define BUCKETS_CNT 128

struct ht_entry {
  void *key;
  void *value;
  struct ht_entry *next;
};

typedef size_t (hash_func)(const void *);
typedef bool (equals_func)(const void *restrict, const void *restrict);
typedef void (free_func)(void *);

struct ht {
  struct ht_entry *buckets[BUCKETS_CNT];
  hash_func *hash;
  equals_func *equals;
  free_func *free_key;
  free_func *free_value;
};

struct ht *ht_init(hash_func *hash, equals_func *equals, free_func *free_key, free_func *free_value);
void ht_deinit(struct ht *hashtable);
struct ht_entry *ht_search(const struct ht *hashtable, void *key);
bool ht_insert(struct ht *hashtable, void *key, void *value);

#endif
