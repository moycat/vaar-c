#ifndef VAAR_PATH_H
#define VAAR_PATH_H

static inline int clean_path(const char *path, char *new_path) {
    int read_cur = 0, write_cur = 0;
    while (path[read_cur] == '/')
        read_cur++;
    if (path[read_cur] == '\0')
        goto exit;
    while (path[read_cur] != '\0')
        new_path[write_cur++] = path[read_cur++];

    if (strstr(new_path, "/../") != NULL || !strcmp(new_path, "..") || !strncmp(new_path, "../", 3))
        return -1;
    if (write_cur >= 3 && !strncmp(new_path + write_cur - 3, "/..", 3))
        return -1;

    exit:
    new_path[write_cur] = '\0';
    return write_cur;
}

static inline int join_path(const char *path, char *filename, char *buf) {
    strcpy(buf, path);
    int n = (int) strlen(buf);
    if (n)
        buf[n] = '/';
    strcpy(buf + n + 1, filename);
    return (int) strlen(buf);
}

#endif //VAAR_PATH_H
