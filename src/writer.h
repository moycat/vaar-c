#ifndef VAAR_WRITER_H
#define VAAR_WRITER_H

#include <stddef.h>
#include <sys/stat.h>

/*
 * A wrapper for preparing and writing files.
 * The writer itself is for serial writing only. The caller should guarantee the proper order.
 */
struct writer {
    int fd;

    /* buffered header for the next file */
    struct file_header *hdr_buf;
    int hdr_buf_len;

    /* buffered link target for symlinks */
    char *link_buf;
    int link_buf_len;
    int link_len;
};

/*
 * Initialize a writer with an output fd.
 */
int writer_init(struct writer *w, int fd);

/*
 * Write the magic number to output.
 * Each archive MUST have the magic number at start.
 */
int writer_magic(struct writer *w);

/*
 * Prepare the writer for writing a file with its statx info.
 * The path will be cleaned before it's used as the eventual written name. It must be less than 256 characters.
 */
int writer_prepare_statx(struct writer *w, const char *path, struct statx *s);

/*
 * If the file to be written is a symlink, you MUST call this method before preparing it.
 * This reads and stages the linkname of the file.
 */
int writer_prepare_link(struct writer *w, int dir_fd, const char *path);

/*
 * Write the header and file content, given its fd and length.
 */
int writer_execute_fd(struct writer *w, int fd, size_t len);

/*
 * Write the header and file content, given the content buffer and length.
 */
int writer_execute_buffer(struct writer *w, void *buf, size_t len);

/*
 * Destroy a writer and release its space.
 */
void writer_free(struct writer *w);

static inline int is_regular(struct statx *s) { return S_ISREG(s->stx_mode); }

static inline int is_dir(struct statx *s) { return S_ISDIR(s->stx_mode); }

static inline int is_symlink(struct statx *s) { return S_ISLNK(s->stx_mode); }

#endif //VAAR_WRITER_H
