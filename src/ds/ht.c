#include "ht.h"

struct ht *ht_init(hash_func *hash, equals_func *equals, free_func *free_key,
                  free_func *free_value) {

  struct ht *hashtable = (struct ht *)malloc(sizeof(struct ht));
  if (!hashtable) {
    fprintf(stderr, "No memory\n");
    return NULL;
  }

  memset(hashtable->buckets, 0, BUCKETS_CNT * sizeof(struct ht_entry *));
  hashtable->hash = hash;
  hashtable->equals = equals;
  hashtable->free_key = free_key;
  hashtable->free_value = free_value;
  
  return hashtable;
}

void ht_deinit(struct ht *hashtable) {
  for (size_t i = 0; i < BUCKETS_CNT; ++i) {
    struct ht_entry *entry = hashtable->buckets[i];

    while (entry) {
      struct ht_entry *prev = entry;
      entry = entry->next;

      hashtable->free_key(prev->key);
      hashtable->free_value(prev->value);
      free(prev);
    }
  }

  free(hashtable);
}

struct ht_entry *ht_search(const struct ht *hashtable, void *key) {
  size_t hash = hashtable->hash(key) % BUCKETS_CNT;
  struct ht_entry *entry = hashtable->buckets[hash];
  for (; entry; entry = entry->next)
    if (hashtable->equals(entry->key, key))
      return entry;

  return NULL;
}

bool ht_insert(struct ht *hashtable, void *key, void *value) {
  size_t hash = hashtable->hash(key) % BUCKETS_CNT;

  if (ht_search(hashtable, key))
    return 0;

  struct ht_entry *entry = (struct ht_entry *)malloc(sizeof(*entry));
  if (!entry) {
    fprintf(stderr, "No memory\n");
    exit(1);
  }

  entry->key = key;
  entry->value = value;

  entry->next = hashtable->buckets[hash];
  hashtable->buckets[hash] = entry;

  return 1;
}
