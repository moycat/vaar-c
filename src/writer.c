#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>

#include "format.h"
#include "path.h"
#include "writer.h"

const int INIT_LINK_LEN = 256;

int write_file_header(int fd, struct file_header *hdr) {
    ssize_t header_size = file_header_length(le16toh(hdr->link_len));
    ssize_t n = write(fd, hdr, header_size);
    if (n != header_size) {
        perror("write");
        return 1;
    }
    return 0;
}

int writer_init(struct writer *w, int fd) {
    w->fd = fd;
    w->hdr_buf = malloc(sizeof(struct file_header));
    if (w->hdr_buf == NULL) {
        perror("malloc");
        return 1;
    }
    w->hdr_buf_len = sizeof(struct file_header);
    w->link_buf = malloc(INIT_LINK_LEN);
    if (w->link_buf == NULL) {
        perror("malloc");
        return 1;
    }
    w->link_buf_len = INIT_LINK_LEN;
    w->link_len = 0;
    return 0;
}

int writer_magic(struct writer *w) {
    ssize_t n = write(w->fd, VAAR_ARCHIVE_MAGIC, VAAR_ARCHIVE_MAGIC_LEN);
    if (n != VAAR_ARCHIVE_MAGIC_LEN) {
        perror("write");
        return 1;
    }
    return 0;
}

int writer_prepare_statx(struct writer *w, const char *path, struct statx *s) {
    if (!is_dir(s) && !is_regular(s) && !is_symlink(s)) {
        fprintf(stderr, "unsupported file type: %s\n", path);
        return 1;
    }

    int hdr_len = file_header_length(is_symlink(s) ? w->link_len : 0);
    char new_path[256];
    if (clean_path(path, new_path) <= 0) {
        fprintf(stderr, "path %s failed validation\n", path);
        return 1;
    }

    if (hdr_len > w->hdr_buf_len) {
        struct file_header *new_hdr_buf = malloc(hdr_len);
        if (new_hdr_buf == NULL) {
            perror("malloc");
            return 1;
        }
        free(w->hdr_buf);
        w->hdr_buf = new_hdr_buf;
        w->hdr_buf_len = hdr_len;
    }

    memset(w->hdr_buf->name, 0, 256);
    memset(w->hdr_buf->uname, 0, 32);
    memset(w->hdr_buf->gname, 0, 32);

    strncpy(w->hdr_buf->name, new_path, 255);
    w->hdr_buf->size = s->stx_size;
    w->hdr_buf->mode = s->stx_mode & 0777; /* strip the high bits */
    w->hdr_buf->mtime.sec = s->stx_mtime.tv_sec;
    w->hdr_buf->mtime.nsec = s->stx_mtime.tv_nsec;
    w->hdr_buf->uid = s->stx_uid;
    w->hdr_buf->gid = s->stx_gid;
    if (is_regular(s)) {
        w->hdr_buf->type = VAAR_REG;
        w->hdr_buf->link_len = 0;
    } else if (is_dir(s)) {
        w->hdr_buf->type = VAAR_DIR;
        w->hdr_buf->size = 0;
        w->hdr_buf->link_len = 0;
    } else /* symlink */ {
        w->hdr_buf->type = VAAR_SYM;
        w->hdr_buf->size = 0;
        w->hdr_buf->link_len = w->link_len;
        strncpy(w->hdr_buf->linkname, w->link_buf, w->link_len);
        w->hdr_buf->linkname[w->link_len] = '\0';
    }

    // TODO: uname & gname

    // TODO: hard link
    w->hdr_buf->link_anchor = 0;

    file_header_encode(w->hdr_buf);

    return 0;
}

int writer_prepare_link(struct writer *w, int dir_fd, const char *path) {
    while (1) {
        ssize_t n = readlinkat(dir_fd, path, w->link_buf, w->link_buf_len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("readlinkat");
            return 1;
        }
        if (n < w->link_buf_len) {
            w->link_len = (int) n;
            return 0;
        }
        int link_buf_len = w->link_buf_len * 2;
        char *new_link_buf = malloc(link_buf_len);
        if (new_link_buf == NULL) {
            perror("malloc");
            return 1;
        }
        w->link_buf = new_link_buf;
        w->link_buf_len = link_buf_len;
    }
}

int writer_execute_fd(struct writer *w, int fd, size_t len) {
    int ret = 0;
    off64_t sent = 0;
    uint64_t size = le64toh(w->hdr_buf->size);
    if (write_file_header(w->fd, w->hdr_buf)) {
        ret = 1;
        goto exit;
    }
    while (size > 0) {
        ssize_t n = sendfile64(w->fd, fd, &sent, size);
        if (n < 0) {
            perror("sendfile64");
            ret = 1;
            goto exit;
        }
        size -= n;
        sent += n;
    }
    exit:
    return ret;
}

int writer_execute_buffer(struct writer *w, void *buf, size_t len) {
    int ret = 0;
    if (write_file_header(w->fd, w->hdr_buf)) {
        ret = 1;
        goto exit;
    }
    ssize_t n = write(w->fd, buf, len);
    if (n != len) {
        perror("write");
        ret = 1;
    }
    exit:
    return ret;
}

void writer_free(struct writer *w) {
    free(w->hdr_buf);
    free(w->link_buf);
}
