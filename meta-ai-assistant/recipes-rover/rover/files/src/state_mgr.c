#include "state_mgr.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <errno.h>

static rover_state_t   g_state;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int             g_server_fd = -1;
static volatile int    g_running   = 0;

static void stamp_state(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    g_state.updated_sec  = (long)tv.tv_sec;
    g_state.updated_usec = (long)tv.tv_usec;
}

static void init_state(void)
{
    memset(&g_state, 0, sizeof(g_state));
    g_state.temperature = 25.0f;
    g_state.humidity    = 50.0f;
    g_state.light_level = 500;
    g_state.distance    = 100.0f;
    g_state.mode        = 0;
    stamp_state();
}

static void *handle_client(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    state_msg_t msg;
    ssize_t n = recv(fd, &msg, sizeof(msg), MSG_WAITALL);
    if (n != (ssize_t)sizeof(msg)) {
        close(fd);
        return NULL;
    }

    switch (msg.cmd) {

    case CMD_GET_STATE: {
        pthread_mutex_lock(&g_lock);
        rover_state_t snap = g_state;
        pthread_mutex_unlock(&g_lock);
        send(fd, &snap, sizeof(snap), 0);
        break;
    }

    case CMD_SET_SENSORS: {
        rover_state_t *p = &msg.payload;
        pthread_mutex_lock(&g_lock);
        if (p->temperature != 0) g_state.temperature = p->temperature;
        if (p->humidity    != 0) g_state.humidity    = p->humidity;
        if (p->light_level != 0) g_state.light_level = p->light_level;
        if (p->distance    != 0) g_state.distance    = p->distance;
        g_state.motion = p->motion;
        stamp_state();
        pthread_mutex_unlock(&g_lock);
        break;
    }

    case CMD_SET_STATE: {
        rover_state_t *p = &msg.payload;
        pthread_mutex_lock(&g_lock);
        if (!g_state.motor_stop) {
            g_state.motor_left  = p->motor_left;
            g_state.motor_right = p->motor_right;
        }
        if (p->motor_stop)
            g_state.motor_stop = 1;
        g_state.fan_on    = p->fan_on;
        g_state.light_on  = p->light_on;
        g_state.heater_on = p->heater_on;
        g_state.buzzer_on = p->buzzer_on;
        stamp_state();
        pthread_mutex_unlock(&g_lock);
        LOG_INFO("state updated by client");
        break;
    }

    case CMD_SET_MODE: {
        int mode = msg.payload.mode;
        pthread_mutex_lock(&g_lock);
        g_state.mode = mode;
        /* Switching to AUTO clears motor_stop */
        if (mode == 0)
            g_state.motor_stop = 0;
        stamp_state();
        pthread_mutex_unlock(&g_lock);
        LOG_INFO("mode set to %d", mode);
        break;
    }

    case CMD_STOP:
        pthread_mutex_lock(&g_lock);
        g_state.motor_left  = 0;
        g_state.motor_right = 0;
        g_state.motor_stop  = 1;
        g_state.fan_on      = 0;
        stamp_state();
        pthread_mutex_unlock(&g_lock);
        LOG_SAFETY("EMERGENCY STOP received via state socket");
        break;

    default:
        LOG_WARN("state_mgr: unknown command %d", msg.cmd);
        break;
    }

    close(fd);
    return NULL;
}

static void *server_thread(void *arg)
{
    (void)arg;
    while (g_running) {
        int client = accept(g_server_fd, NULL, NULL);
        if (client < 0) {
            if (g_running)
                LOG_WARN("state_mgr: accept failed: %s", strerror(errno));
            break;
        }
        int *fd_copy = malloc(sizeof(int));
        *fd_copy = client;
        pthread_t t;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&t, &attr, handle_client, fd_copy);
        pthread_attr_destroy(&attr);
    }
    return NULL;
}

int state_mgr_start(void)
{
    init_state();
    unlink(STATE_SOCK_PATH);

    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, STATE_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        return -1;
    if (listen(g_server_fd, 8) < 0)
        return -1;

    g_running = 1;
    pthread_t t;
    pthread_create(&t, NULL, server_thread, NULL);
    pthread_detach(t);

    LOG_INFO("state_mgr listening on %s", STATE_SOCK_PATH);
    return 0;
}

void state_mgr_stop(void)
{
    g_running = 0;
    shutdown(g_server_fd, SHUT_RDWR);
    close(g_server_fd);
    unlink(STATE_SOCK_PATH);
}

static int sock_connect(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, STATE_SOCK_PATH, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }
    return fd;
}

int state_get(rover_state_t *out)
{
    int fd = sock_connect();
    if (fd < 0) return -1;
    state_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = CMD_GET_STATE;
    send(fd, &msg, sizeof(msg), 0);
    ssize_t n = recv(fd, out, sizeof(*out), MSG_WAITALL);
    close(fd);
    return (n == (ssize_t)sizeof(*out)) ? 0 : -1;
}

int state_set(const rover_state_t *in)
{
    int fd = sock_connect();
    if (fd < 0) return -1;
    state_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd     = CMD_SET_STATE;
    msg.payload = *in;
    send(fd, &msg, sizeof(msg), 0);
    close(fd);
    return 0;
}

int state_set_sensors(const rover_state_t *in)
{
    int fd = sock_connect();
    if (fd < 0) return -1;
    state_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd     = CMD_SET_SENSORS;
    msg.payload = *in;
    send(fd, &msg, sizeof(msg), 0);
    close(fd);
    return 0;
}

int state_set_mode(int mode)
{
    int fd = sock_connect();
    if (fd < 0) return -1;
    state_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd          = CMD_SET_MODE;
    msg.payload.mode = mode;
    send(fd, &msg, sizeof(msg), 0);
    close(fd);
    return 0;
}

int state_emergency_stop(void)
{
    int fd = sock_connect();
    if (fd < 0) return -1;
    state_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.cmd = CMD_STOP;
    send(fd, &msg, sizeof(msg), 0);
    close(fd);
    return 0;
}
