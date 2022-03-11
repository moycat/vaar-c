#ifndef VAAR_DIR_ENTRY_H
#define VAAR_DIR_ENTRY_H

#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

/*
 * An entry in a directory, typically a file (regular, directory, or whatever).
 */
struct dir_entry {
    unsigned long inode;
    unsigned char type;
    char *name;
};

/*
 * The reader to parse data returned by get_dir_entries.
 */
struct dir_reader {
    int count;
    int cur;
    struct dir_entry *entries;
};

/*
 * Get entries of an opened directory referred to by fd.
 * Returns the retrieved data length, or -errno on errors. The data should be parsed with dir_reader.
 */
static inline ssize_t get_dir_entries(int fd, void *buf, int buf_len) {
    return syscall(SYS_getdents64, fd, buf, buf_len);
}

/*
 * Initialize a dir reader with the data and data length.
 */
void dir_reader_init(struct dir_reader *r, void *data, ssize_t data_len);

/*
 * Get the next entry in the data.
 * Returns NULL when drained.
 */
static inline struct dir_entry *dir_reader_next(struct dir_reader *r) {
    if (r->cur >= r->count)
        return NULL;
    return r->entries + r->cur++;
}

/*
 * Release all memory allocated by the reader.
 */
void dir_reader_free(struct dir_reader *r);

#endif //VAAR_DIR_ENTRY_H
