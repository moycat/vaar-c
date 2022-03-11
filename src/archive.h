#ifndef VAAR_ARCHIVE_H
#define VAAR_ARCHIVE_H

#include "format.h"
#include "writer.h"

/*
 * Add a path to the writer.
 */
int archive_path(struct writer *w, const char *path);

#endif //VAAR_ARCHIVE_H
