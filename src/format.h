#ifndef VAAR_FORMAT_H
#define VAAR_FORMAT_H

#include <endian.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/stat.h>

#define VAAR_ARCHIVE_MAGIC "\xf0\x9f\x90\xb3\xf0\x9f\x93\xa6\x00\x00"
#define VAAR_ARCHIVE_MAGIC_LEN 10

/*
 * File types used in Vaar file headers.
 */
enum {
    VAAR_DIR, /* directory */
    VAAR_REG, /* regular file */
    VAAR_SYM, /* symlink */
    VAAR_LNK, /* hard link */
};

/*
 * The timestamp struct used in Vaar file headers.
 * It has the same meaning of timespec.
 */
struct file_ts {
    int64_t sec;
    int64_t nsec;
} __attribute__((packed));

/*
 * The Vaar file header struct.
 * The total length can be calculated with file_header_length().
 * The numbers are stored and transferred in little endian.
 */
struct file_header {
    char name[256];
    uint64_t size;
    uint8_t type;
    uint8_t _reserved;
    uint16_t mode;
    struct file_ts mtime;

    /*
     * Owner and group fields. Names are preferred to IDs.
     * The IDs are used only on explicit request or when names don't exist.
     */
    uint32_t uid;
    uint32_t gid;
    char uname[32];
    char gname[32];

    /*
     * Link fields. Both for symlinks and hard links.
     */
    uint32_t link_anchor; /* non-zero for hard links or regular files with hard links */
    uint16_t link_len; /* non-zero for symlinks; length of linkname */
    char linkname[]; /* null-terminated if symlink; empty otherwise */
} __attribute__((packed));

/*
 * Get the size of a file_header for a file with its symlink target length being link_len.
 */
static inline int file_header_length(int link_len) {
    if (link_len > 0) {
        return (int) sizeof(struct file_header) + link_len + 1;
    }
    return (int) sizeof(struct file_header);
}

/*
 * Convert the number fields of file_header to little endian for writing.
 */
static inline void file_header_encode(struct file_header *hdr) {
    hdr->size = htole64(hdr->size);
    hdr->mode = htole16(hdr->mode);
    hdr->uid = htole32(hdr->uid);
    hdr->gid = htole32(hdr->gid);
    hdr->link_anchor = htole32(hdr->link_anchor);
    hdr->link_len = htole16(hdr->link_len);
    hdr->mtime.sec = htole64(hdr->mtime.sec);
    hdr->mtime.nsec = htole64(hdr->mtime.nsec);
}

/*
 * Convert the number fields of file_header back to host endian for reading.
 */
static inline void file_header_decode(struct file_header *hdr) {
    hdr->size = le64toh(hdr->size);
    hdr->mode = le16toh(hdr->mode);
    hdr->uid = le32toh(hdr->uid);
    hdr->gid = le32toh(hdr->gid);
    hdr->link_anchor = le32toh(hdr->link_anchor);
    hdr->link_len = le16toh(hdr->link_len);
    hdr->mtime.sec = le64toh(hdr->mtime.sec);
    hdr->mtime.nsec = le64toh(hdr->mtime.nsec);
}

#endif //VAAR_FORMAT_H
