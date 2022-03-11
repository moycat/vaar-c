#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>

#include "dir_entry.h"
#include "archive.h"
#include "writer.h"

int main(int argc, char *argv[]) {
    struct rlimit lmt;
    getrlimit(RLIMIT_NOFILE, &lmt);
    lmt.rlim_cur = lmt.rlim_max;
    setrlimit(RLIMIT_NOFILE, &lmt);

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <archive> <path 1> [path 2] ...\n", argv[0]);
        return 1;
    }

    printf("creating archive at [%s]\n", argv[1]);
    int fd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    struct writer w;
    if (writer_init(&w, fd)) {
        exit(1);
    }
    if (writer_magic(&w)) {
        exit(1);
    }

    for (int i = 2; i < argc; i++) {
        printf("adding [%s]...\n", argv[i]);
        if (archive_path(&w, argv[i])) {
            exit(1);
        }
    }

    writer_free(&w);

    printf("done, closing archive\n");
    if (close(fd)) {
        perror("close");
        exit(1);
    }
    return 0;
}
