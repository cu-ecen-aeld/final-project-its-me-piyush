#ifndef LOGGER_H
#define LOGGER_H

#define LOG_MSG_MAX 256

typedef enum {
    LOG_SENSOR, LOG_ACTION, LOG_SAFETY,
    LOG_WARN, LOG_INFO, LOG_ERROR,
} log_level_t;

typedef struct {
    log_level_t level;
    char        msg[LOG_MSG_MAX];
    long        ts_sec;
    long        ts_usec;
} log_entry_t;

int  logger_init(const char *filepath);
void logger_shutdown(void);
void logger_write(log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define LOG_INFO(...)   logger_write(LOG_INFO,   __VA_ARGS__)
#define LOG_WARN(...)   logger_write(LOG_WARN,   __VA_ARGS__)
#define LOG_ERROR(...)  logger_write(LOG_ERROR,  __VA_ARGS__)
#define LOG_SENSOR(...) logger_write(LOG_SENSOR, __VA_ARGS__)
#define LOG_ACTION(...) logger_write(LOG_ACTION, __VA_ARGS__)
#define LOG_SAFETY(...) logger_write(LOG_SAFETY, __VA_ARGS__)

#endif
