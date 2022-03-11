#include <malloc.h>

#include "buf_pool.h"

int buf_pool_init(struct buf_pool *pool, uint32_t item_size, uint32_t item_cnt) {
    item_cnt = 1 << (32 - __builtin_clz(item_cnt - 1));
    uint32_t tot_size = item_size * item_cnt;
    pool->head = pool->tail = 0;
    pool->mask = item_cnt - 1;
    pool->buf = malloc(tot_size);
    if (pool->buf == NULL) {
        perror("malloc");
        return 1;
    }
    pool->blocks = malloc(sizeof(size_t) * item_cnt);
    if (pool->blocks == NULL) {
        perror("malloc");
        return 1;
    }
    for (int i = 0; i < item_cnt; i++)
        pool->blocks[i] = pool->buf + (i * item_size);
    return 0;
}

void buf_pool_free(struct buf_pool *pool) {
    free(pool->buf);
    free(pool->blocks);
}
