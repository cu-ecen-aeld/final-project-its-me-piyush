#include "logger.h"
#include "cbuf.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#define LOG_BUF_CAPACITY 64

static cbuf_t       g_buf;
static pthread_t    g_thread;
static FILE        *g_file;
static volatile int g_running;

static const char *level_str(log_level_t l)
{
    switch (l) {
        case LOG_SENSOR: return "SENSOR";
        case LOG_ACTION: return "ACTION";
        case LOG_SAFETY: return "SAFETY";
        case LOG_WARN:   return "WARN  ";
        case LOG_INFO:   return "INFO  ";
        case LOG_ERROR:  return "ERROR ";
        default:         return "?     ";
    }
}

static void *flush_thread(void *arg)
{
    (void)arg;
    log_entry_t e;

    while (g_running || cbuf_count(&g_buf) > 0) {
        if (cbuf_read(&g_buf, &e) == 0) {
            char line[LOG_MSG_MAX + 64];
            snprintf(line, sizeof(line),
                     "[%ld.%06ld] [%s] %s\n",
                     e.ts_sec, e.ts_usec,
                     level_str(e.level), e.msg);
            fputs(line, stdout);
            if (g_file) {
                fputs(line, g_file);
                fflush(g_file);
            }
        } else {
            usleep(5000); /* 5ms sleep when idle */
        }
    }
    return NULL;
}

int logger_init(const char *filepath)
{
    if (cbuf_init(&g_buf, LOG_BUF_CAPACITY,
                  sizeof(log_entry_t), CBUF_OVERWRITE) != 0)
        return -1;

    g_file = NULL;
    if (filepath) {
        g_file = fopen(filepath, "a");
    }

    g_running = 1;
    pthread_create(&g_thread, NULL, flush_thread, NULL);
    return 0;
}

void logger_shutdown(void)
{
    g_running = 0;
    pthread_join(g_thread, NULL);
    if (g_file) {
        fclose(g_file);
        g_file = NULL;
    }
    cbuf_free(&g_buf);
}

void logger_write(log_level_t level, const char *fmt, ...)
{
    log_entry_t e;
    struct timeval tv;

    gettimeofday(&tv, NULL);
    e.level   = level;
    e.ts_sec  = (long)tv.tv_sec;
    e.ts_usec = (long)tv.tv_usec;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e.msg, LOG_MSG_MAX, fmt, ap);
    va_end(ap);

    cbuf_write(&g_buf, &e);
}
