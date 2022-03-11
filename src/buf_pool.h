#ifndef VAAR_BUF_POOL_H
#define VAAR_BUF_POOL_H

#include <stdint.h>

/*
 * Thread-safe lockless buffer pool.
 * However, never get more than allocated items before putting them back (and all the put routines
 * have returned), or the world may be destroyed.
 */
struct buf_pool {
    void *buf, **blocks;
    uint32_t head, tail, mask;
};

/*
 * Initialize a buffer pool with the smallest power of 2 greater than or equal to item_cnt items.
 * The space of item count * item size is allocated at instant.
 */
int buf_pool_init(struct buf_pool *pool, uint32_t item_size, uint32_t item_cnt);

/*
 * Get a buffer of item_size. Thread-safe.
 */
static inline void *buf_pool_get(struct buf_pool *pool) {
    uint32_t offset = __atomic_fetch_add(&pool->head, 1, __ATOMIC_RELAXED);
    return pool->blocks[offset & pool->mask];
}

/*
 * Put a buffer back. Thread-safe.
 */
static inline void buf_pool_put(struct buf_pool *pool, void *item) {
    uint32_t offset = __atomic_fetch_add(&pool->tail, 1, __ATOMIC_RELAXED);
    pool->blocks[offset & pool->mask] = item;
}

/*
 * Destroy a buffer pool and free the space.
 */
void buf_pool_free(struct buf_pool *pool);

#endif //VAAR_BUF_POOL_H
