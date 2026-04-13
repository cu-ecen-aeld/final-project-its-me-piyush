#include "cbuf.h"
#include <stdlib.h>
#include <string.h>

/* helpers called with lock held */

static inline void *elem_ptr(cbuf_t *cb, size_t idx)
{
    return (char *)cb->buf + idx * cb->elem_size;
}

static inline void write_locked(cbuf_t *cb, const void *elem)
{
    memcpy(elem_ptr(cb, cb->head), elem, cb->elem_size);
    cb->head = (cb->head + 1) % cb->capacity;
    if (cb->count < cb->capacity)
        cb->count++;
    else
        cb->tail = (cb->tail + 1) % cb->capacity;
}

static inline void read_locked(cbuf_t *cb, void *elem)
{
    memcpy(elem, elem_ptr(cb, cb->tail), cb->elem_size);
    cb->tail = (cb->tail + 1) % cb->capacity;
    cb->count--;
}

/* public API */

int cbuf_init(cbuf_t *cb, size_t capacity, size_t elem_size, cbuf_mode_t mode)
{
    if (!cb || capacity == 0 || elem_size == 0)
        return -1;

    cb->buf = malloc(capacity * elem_size);
    if (!cb->buf)
        return -1;

    cb->capacity  = capacity;
    cb->elem_size = elem_size;
    cb->head      = 0;
    cb->tail      = 0;
    cb->count     = 0;
    cb->mode      = mode;

    pthread_mutex_init(&cb->lock, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
    pthread_cond_init(&cb->not_full,  NULL);

    return 0;
}

void cbuf_free(cbuf_t *cb)
{
    if (!cb) return;
    pthread_mutex_destroy(&cb->lock);
    pthread_cond_destroy(&cb->not_empty);
    pthread_cond_destroy(&cb->not_full);
    free(cb->buf);
    cb->buf = NULL;
}

int cbuf_write(cbuf_t *cb, const void *elem)
{
    if (!cb || !elem) return -1;

    pthread_mutex_lock(&cb->lock);

    if (cb->mode == CBUF_BLOCKING) {
        while (cb->count == cb->capacity)
            pthread_cond_wait(&cb->not_full, &cb->lock);
    }

    write_locked(cb, elem);
    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

int cbuf_read(cbuf_t *cb, void *elem)
{
    if (!cb || !elem) return -1;

    pthread_mutex_lock(&cb->lock);

    if (cb->mode == CBUF_BLOCKING) {
        while (cb->count == 0)
            pthread_cond_wait(&cb->not_empty, &cb->lock);
    } else {
        if (cb->count == 0) {
            pthread_mutex_unlock(&cb->lock);
            return -1;
        }
    }

    read_locked(cb, elem);
    pthread_cond_signal(&cb->not_full);
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

int cbuf_peek(cbuf_t *cb, void *elem)
{
    if (!cb || !elem) return -1;

    pthread_mutex_lock(&cb->lock);
    if (cb->count == 0) {
        pthread_mutex_unlock(&cb->lock);
        return -1;
    }
    size_t latest = (cb->head + cb->capacity - 1) % cb->capacity;
    memcpy(elem, elem_ptr(cb, latest), cb->elem_size);
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

size_t cbuf_snapshot(cbuf_t *cb, void *dst, size_t max_elems)
{
    if (!cb || !dst) return 0;

    pthread_mutex_lock(&cb->lock);
    size_t n = cb->count < max_elems ? cb->count : max_elems;
    for (size_t i = 0; i < n; i++) {
        size_t idx = (cb->tail + i) % cb->capacity;
        memcpy((char *)dst + i * cb->elem_size,
               elem_ptr(cb, idx), cb->elem_size);
    }
    pthread_mutex_unlock(&cb->lock);
    return n;
}

size_t cbuf_count(cbuf_t *cb)
{
    pthread_mutex_lock(&cb->lock);
    size_t n = cb->count;
    pthread_mutex_unlock(&cb->lock);
    return n;
}

bool cbuf_full(cbuf_t *cb)
{
    pthread_mutex_lock(&cb->lock);
    bool f = (cb->count == cb->capacity);
    pthread_mutex_unlock(&cb->lock);
    return f;
}

bool cbuf_empty(cbuf_t *cb)
{
    pthread_mutex_lock(&cb->lock);
    bool e = (cb->count == 0);
    pthread_mutex_unlock(&cb->lock);
    return e;
}
