#include <dirent.h>
#include <stdlib.h>

#include "dir_entry.h"

/*
 * Sort the entries in descending inode number order, and dirs before files.
 * This should give a performance boost.
 */
int dir_entry_comp(const void *a, const void *b) {
    const struct dir_entry *ent_a = a, *ent_b = b;
    if ((ent_a->type == DT_DIR) != (ent_b->type == DT_DIR))
        return ent_b->type == DT_DIR;
    return ent_a->inode > ent_b->inode;
}

void dir_reader_init(struct dir_reader *r, void *data, ssize_t data_len) {
    struct dir_entry *entries = malloc(data_len);
    off64_t off = 0;
    int cnt = 0;
    while (off < data_len) {
        struct dirent *entry = data + off;
        off += entry->d_reclen;
        if (entry->d_name[0] == '.' &&
                     (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
            /* Ignore "." and "..". */
            continue;
        entries[cnt].name = entry->d_name;
        entries[cnt].inode = entry->d_ino;
        entries[cnt].type = entry->d_type;
        cnt++;
    }
    qsort(entries, cnt, sizeof(struct dir_entry), dir_entry_comp);
    r->cur = 0;
    r->entries = entries;
    r->count = cnt;
}

void dir_reader_free(struct dir_reader *r) {
    free(r->entries);
}
