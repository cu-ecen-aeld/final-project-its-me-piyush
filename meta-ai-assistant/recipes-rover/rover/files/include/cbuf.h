#ifndef CBUF_H
#define CBUF_H

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>

typedef enum {
    CBUF_OVERWRITE,
    CBUF_BLOCKING,
    CBUF_SNAPSHOT,
} cbuf_mode_t;

typedef struct {
    void            *buf;
    size_t           elem_size;
    size_t           capacity;
    size_t           head;
    size_t           tail;
    size_t           count;
    cbuf_mode_t      mode;
    pthread_mutex_t  lock;
    pthread_cond_t   not_empty;
    pthread_cond_t   not_full;
} cbuf_t;

int    cbuf_init(cbuf_t *cb, size_t capacity, size_t elem_size, cbuf_mode_t mode);
void   cbuf_free(cbuf_t *cb);
int    cbuf_write(cbuf_t *cb, const void *elem);
int    cbuf_read(cbuf_t *cb, void *elem);
int    cbuf_peek(cbuf_t *cb, void *elem);
size_t cbuf_snapshot(cbuf_t *cb, void *dst, size_t max_elems);
size_t cbuf_count(cbuf_t *cb);
bool   cbuf_full(cbuf_t *cb);
bool   cbuf_empty(cbuf_t *cb);

#endif
