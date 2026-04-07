#include "hashtable.h"

hashtable_t *hashtable_init(EqualsFunc equals, FreeKeyFunc free_key,
                            FreeValueFunc free_value, HashFunc hash) {
  hashtable_t *table = (hashtable_t *)malloc(sizeof(hashtable_t));
  if (!table) {
    return NULL;
  }

  for (int i = 0; i < TABLE_SIZE; ++i)
    table->buckets[i] = NULL;

  if (!equals) {
    free(table);
    return NULL;
  }

  table->equals = equals;

  if (!hash) {
    free(table);
    return NULL;
  }

  table->hash = hash;

  // could be done with macros tbh
  table->free_key = free_key;
  table->free_value = free_value;

  return table;
}

bool hashtable_insert(hashtable_t *table, void *key, void *value) {
  size_t hash = table->hash(key) % TABLE_SIZE;
  if (hash < 0) // shouldn't be possible tbh
    return false;

  if (hashtable_find(
          table, key)) // if there exists another entry with the same key abort
    return false;

  ht_entry_t *entry = (ht_entry_t *)malloc(sizeof(ht_entry_t));
  if (!entry)
    return false;

  entry->next = table->buckets[hash];

  entry->key = key;
  entry->value = value;

  table->buckets[hash] = entry;

  return true;
}

void *hashtable_find(hashtable_t *table, void *key) {
  size_t hash = table->hash(key) % TABLE_SIZE;
  if (hash < 0) // shouldn't be possible tbh
    return NULL;

  for (ht_entry_t *p = table->buckets[hash]; p != NULL; p = p->next) {
    if (table->equals(key, p->key))
      return p->value;
  }

  return NULL;
}

void *hashtable_remove(hashtable_t *table, void *key) {
  void *result = NULL;

  size_t hash = table->hash(key) % TABLE_SIZE;

  // hash < 0 shouldn't be possible
  if (hash < 0 || !table->buckets[hash])
    return NULL;

  ht_entry_t *p = table->buckets[hash];
  if (!table->equals(key, p->key)) { // if they are different
    for (; p->next != NULL; p = p->next) {
      if (table->equals(key, p->next->key)) {
        ht_entry_t *tmp = p->next;
        p->next = tmp->next;

        result = tmp->value;
        free(tmp);
        break;
      }
    }
  } else {
    result = p->value;
    table->buckets[hash] = p->next;
    free(p);
  }

  return result;
}

void hashtable_free(hashtable_t *table) {
  for (int i = 0; i < TABLE_SIZE; ++i) {
    ht_entry_t *p = table->buckets[i];
    while (p) {
      ht_entry_t *prev = p;
      p = p->next;

      if (table->free_key)
        table->free_key(prev->key);

      if (table->free_value)
        table->free_value(prev->value);

      free(prev);
    }
  }

  free(table);
}
