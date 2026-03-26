#include "hashtable.h"

/*
 * Taken from http://www.cse.yorku.ca/~oz/hash.html
 */
static size_t djb2_hash(char *str) {
    unsigned long hash = 5381;
    int c;

    while((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash % TABLE_SIZE;
}

hashtable_t *hashtable_init() {
    hashtable_t *table = (hashtable_t *)malloc(sizeof(hashtable_t));
    if(!table) {
        return NULL;
    }

    for(int i = 0; i < TABLE_SIZE; ++i)
        table->buckets[i] = NULL;

    return table;
}

bool hashtable_insert(hashtable_t *table, char *key, Dwarf_Die die, char *full_path) {
    size_t hash = djb2_hash(key);
    if(hash < 0) // shouldn't be possible tbh
        return false;

    if(hashtable_find(table, key)) // if there exists another entry with the same key abort
        return false;

    ht_entry_t *entry = (ht_entry_t *)malloc(sizeof(ht_entry_t));
    if(!entry)
        return false;

    entry->next = table->buckets[hash];

    entry->key = key;
    entry->value = (dwarf_die_path_t *)malloc(sizeof(dwarf_die_path_t));
    if(!entry->value)
        return false;

    entry->value->full_path = full_path;
    entry->value->die = die;

    table->buckets[hash] = entry;

    return true;
}

dwarf_die_path_t *hashtable_find(hashtable_t *table, char *key) {
    size_t hash = djb2_hash(key);
    if(hash < 0) // shouldn't be possible tbh
        return NULL;

    for(ht_entry_t *p = table->buckets[hash]; p != NULL; p = p->next) {
        if(!strcmp(key, p->key))
            return p->value;
    }

    return NULL;
}

dwarf_die_path_t *hashtable_remove(hashtable_t *table, char *key) {
    dwarf_die_path_t *result = NULL;
    
    size_t hash = djb2_hash(key);

    // hash < 0 shouldn't be possible
    if(hash < 0 || !table->buckets[hash])
        return NULL;

    ht_entry_t *p = table->buckets[hash];
    if(strcmp(key, p->key)) { // if they are different
        for(; p->next != NULL; p = p->next) {
            if(!strcmp(p->next->key, key)) {
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
    for(int i = 0 ; i < TABLE_SIZE; ++i) {
        ht_entry_t *p = table->buckets[i];
        while(p) {
            ht_entry_t *prev = p;
            p = p->next;

            free(prev->key);

            // dwarf_finish should take care of the die
            free(prev->value->full_path);
            free(prev->value);
            free(prev);
        }
    }

    free(table);
}

void print_hashtable(hashtable_t *table) {
    for(size_t i = 0; i < TABLE_SIZE; ++i) {
        for(ht_entry_t *entry = table->buckets[i]; entry != NULL; entry = entry->next)
            printf("%s: %s\n", entry->key, entry->value->full_path);
    }
}
