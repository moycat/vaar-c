#include <dirent.h>
#include <fcntl.h>
#include <liburing.h>
#include <malloc.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "archive.h"
#include "buf_pool.h"
#include "dir_entry.h"
#include "path.h"
#include "writer.h"

const int DIR_BUF_SIZE = 1 << 20; // 1 MiB
const int MAX_DIR_DEPTH = 256;

const int RING_DEPTH = 8192;
const int SUBMIT_THRESHOLD = 4096;

/*
 * The context of writing the archive. Shared among threads.
 */
struct archive_context {
    pthread_mutex_t *lock;
    struct io_uring *ring;
    struct buf_pool *item_pool;
    int emitted, done;
};

/*
 * A file being processed. Used in user data of io_uring.
 */
struct item {
    char name[256];
    char buf[4096];
    struct statx sbuf;
    int fd;
    int bytes;
    int cnt;
};

const int ITEM_BUF_SIZE = sizeof(struct item);
const int MAX_ITEM_COUNT = (RING_DEPTH * 2);

int walk_path(struct archive_context *ctx, struct writer *w, const char *path, int dir_fd, struct buf_pool *pool) {
    void *buf = buf_pool_get(pool);
    ssize_t n;
    while ((n = get_dir_entries(dir_fd, buf, DIR_BUF_SIZE)) > 0) {
        struct dir_reader r;
        dir_reader_init(&r, buf, n);
        struct dir_entry *e;
        while ((e = dir_reader_next(&r))) {
            if (e->type == DT_REG) {
                int file_fd = openat(dir_fd, e->name, O_RDONLY);
                if (file_fd < 0) {
                    perror("openat");
                    dir_reader_free(&r);
                    buf_pool_put(pool, buf);
                    return 1;
                }

                struct item *res = buf_pool_get(ctx->item_pool);
                join_path(path, e->name, res->name);
                res->fd = file_fd;
                res->cnt = 2;

                struct io_uring_sqe *sqe;
                while (!(sqe = io_uring_get_sqe(ctx->ring))) {}
                io_uring_prep_statx(sqe, file_fd, "", AT_EMPTY_PATH, STATX_ALL, &res->sbuf);
                io_uring_sqe_set_data(sqe, res);
                while (!(sqe = io_uring_get_sqe(ctx->ring))) {}
                io_uring_prep_read(sqe, file_fd, res->buf, 4096, 0);
                io_uring_sqe_set_data(sqe, res);

                __atomic_add_fetch(&ctx->emitted, 1, __ATOMIC_RELEASE);
                while (io_uring_sq_ready(ctx->ring) >= SUBMIT_THRESHOLD)
                    if (io_uring_submit(ctx->ring) > 0)
                        break;
                continue;
            }

            /* Synced operation for non-regular files. */
            char file_path[256];
            join_path(path, e->name, file_path);
            struct statx sbuf;
            size_t ret = statx(dir_fd, e->name, AT_SYMLINK_NOFOLLOW, STATX_ALL, &sbuf);
            if (ret) {
                perror("statx");
                dir_reader_free(&r);
                buf_pool_put(pool, buf);
                return 1;
            }
            int file_fd = 0;
            if (!is_symlink(&sbuf)) {
                file_fd = openat(dir_fd, e->name, O_RDONLY);
                if (file_fd < 0) {
                    perror("openat");
                    dir_reader_free(&r);
                    buf_pool_put(pool, buf);
                    return 1;
                }
            }
            if (is_symlink(&sbuf))
                if (writer_prepare_link(w, dir_fd, e->name)) {
                    dir_reader_free(&r);
                    buf_pool_put(pool, buf);
                    return 1;
                }
            if (writer_prepare_statx(w, path, &sbuf)) {
                dir_reader_free(&r);
                buf_pool_put(pool, buf);
                return 1;
            }
            if (pthread_mutex_lock(ctx->lock)) {
                perror("pthread_mutex_lock");
                return 1;
            }
            if (writer_execute_fd(w, file_fd, sbuf.stx_size)) {
                dir_reader_free(&r);
                buf_pool_put(pool, buf);
                return 1;
            }
            if (pthread_mutex_unlock(ctx->lock)) {
                perror("pthread_mutex_unlock");
                return 1;
            }
            if (is_dir(&sbuf))
                walk_path(ctx, w, file_path, file_fd, pool);
            if (file_fd > 0)
                if (close(file_fd)) {
                    perror("close");
                    dir_reader_free(&r);
                    buf_pool_put(pool, buf);
                    return 1;
                }
        }
        dir_reader_free(&r);
    }
    buf_pool_put(pool, buf);
    return 0;
}

struct item_handler_args {
    struct archive_context *ctx;
    struct writer *w;
};

// FIXME: error handling here is a mess
void item_handler(struct item_handler_args *args) {
    /* Handler thread need a separated context for temp data. */
    struct archive_context *ctx = args->ctx;
    struct writer *w = args->w;

    int read_count = 0;
    while (1) {
        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(ctx->ring, &cqe);
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            exit(1);
        }
        struct item *res = io_uring_cqe_get_data(cqe);
        if (cqe->res < 0) {
            fprintf(stderr, "async op failed: %d\n", cqe->res);
            exit(1);
        }
        if (cqe->res > 0)
            /* Returned from read(2). */
            res->bytes = cqe->res;
        io_uring_cqe_seen(ctx->ring, cqe);
        if (--res->cnt > 0)
            /* Not all operations have been processed. */
            continue;

        if (writer_prepare_statx(w, res->name, &res->sbuf)) {
            fprintf(stderr, "writer_prepare_statx failed\n");
            exit(1);
        }

        if (pthread_mutex_lock(ctx->lock)) {
            perror("pthread_mutex_lock");
            exit(1);
        }

        if (res->bytes <= 4096 && res->sbuf.stx_size == res->bytes) {
            if (writer_execute_buffer(w, res->buf, res->bytes)) {
                fprintf(stderr, "writer_execute_buffer failed\n");
                exit(1);
            }
        } else {
            if (writer_execute_fd(w, res->fd, res->sbuf.stx_size)) {
                fprintf(stderr, "writer_execute_fd failed\n");
                exit(1);
            }
        }

        if (pthread_mutex_unlock(ctx->lock)) {
            perror("pthread_mutex_unlock");
            exit(1);
        }

        close(res->fd);
        buf_pool_put(ctx->item_pool, res);
        read_count++;
        if (__atomic_load_n(&ctx->done, __ATOMIC_ACQUIRE) &&
            read_count == __atomic_load_n(&ctx->emitted, __ATOMIC_ACQUIRE)) {
            break;
        }
    }
}

int archive_path(struct writer *w, const char *path) {
    int ret = 0;

    pthread_mutex_t lock;
    if (pthread_mutex_init(&lock, NULL)) {
        perror("pthread_mutex_init");
        ret = 1;
        goto exit;
    }

    struct statx s;
    if (statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH, STATX_ALL, &s)) {
        perror("statx");
        ret = 1;
        goto destroy_and_exit;
    }

    int path_fd = 0;
    if (!is_symlink(&s)) {
        path_fd = open(path, O_RDONLY);
        if (path_fd < 0) {
            perror("open");
            ret = 1;
            goto close_and_exit;
        }
    }

    if (!S_ISDIR(s.stx_mode)) {
        /* It's not a directory. Just add it. */
        if (is_symlink(&s))
            if (writer_prepare_link(w, AT_FDCWD, path))
                goto close_and_exit;
        if ((ret = writer_prepare_statx(w, path, &s)))
            goto close_and_exit;
        if ((ret = writer_execute_fd(w, path_fd, s.stx_size)))
            goto close_and_exit;
        goto close_and_exit;
    }

    struct buf_pool dir_pool, item_pool;
    buf_pool_init(&dir_pool, DIR_BUF_SIZE, MAX_DIR_DEPTH);
    buf_pool_init(&item_pool, ITEM_BUF_SIZE, MAX_ITEM_COUNT);

    struct io_uring ring;
    io_uring_queue_init(RING_DEPTH, &ring, 0);

    struct archive_context ctx = {
            .lock = &lock,
            .ring = &ring,
            .item_pool = &item_pool,
            .emitted = 0,
            .done = 0,
    };

    struct item_handler_args args = {.ctx = &ctx, .w = w};
    pthread_t tid;
    if (pthread_create(&tid, NULL, (void *(*)(void *)) item_handler, &args)) {
        perror("pthread_create");
        goto close_and_exit;
    }

    /* walk_path needs a separated writer. */
    struct writer walk_writer;
    writer_init(&walk_writer, w->fd);

    ret = walk_path(&ctx, &walk_writer, path, path_fd, &dir_pool);
    __atomic_store_n(&ctx.done, 1, __ATOMIC_RELEASE);

    while (io_uring_sq_ready(&ring)) {
        if (io_uring_submit(&ring) < 0) {
            perror("io_uring_submit");
            goto close_and_exit;
        }
    }

    pthread_join(tid, NULL);

    writer_free(&walk_writer);
    buf_pool_free(&dir_pool);
    buf_pool_free(&item_pool);
    io_uring_queue_exit(&ring);

    close_and_exit:
    if (path_fd)
        if (close(path_fd)) {
            perror("close");
            ret = 1;
        }

    destroy_and_exit:
    if (pthread_mutex_destroy(&lock)) {
        perror("pthread_mutex_destroy");
        ret = 1;
    }

    exit:
    return ret;
}
