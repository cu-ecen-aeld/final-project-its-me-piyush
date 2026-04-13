#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "cbuf.h"
#include "logger.h"
#include "state.h"
#include "state_mgr.h"
#include "motor.h"
#include "safety.h"
#include "decision.h"

#define LOG_FILE          "/var/log/rover.log"
#define LOOP_INTERVAL_US  100000
#define SENSOR_INTERVAL   5
#define DECISION_INTERVAL 3

static volatile int g_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
    LOG_INFO("rover: shutdown signal received");
}

static void poll_sensors(rover_state_t *s)
{
    static float sim_dist = 100.0f;
    static int   sim_dir  = -1;

    sim_dist += sim_dir * 2.0f;
    if (sim_dist < 10.0f)  { sim_dist = 10.0f;  sim_dir =  1; }
    if (sim_dist > 120.0f) { sim_dist = 120.0f; sim_dir = -1; }

    memset(s, 0, sizeof(*s));
    s->temperature = 25.0f;
    s->humidity    = 50.0f;
    s->light_level = 300;
    s->motion      = 0;
    s->distance    = sim_dist;

    LOG_SENSOR("dist=%.1fcm temp=%.1fC light=%d",
               s->distance, s->temperature, s->light_level);
}

int main(void)
{
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    printf("=== Rover starting ===\n");

    if (logger_init(LOG_FILE) != 0) {
        fprintf(stderr, "Failed to init logger\n");
        return 1;
    }
    LOG_INFO("rover: logger started");

    if (state_mgr_start() != 0) {
        LOG_ERROR("rover: state_mgr failed to start");
        logger_shutdown();
        return 1;
    }
    LOG_INFO("rover: state_mgr started");

    if (motor_init() != 0)
        LOG_ERROR("rover: motor init failed");
    LOG_INFO("rover: motor ready");

    LOG_INFO("rover: entering main loop (interval=%dms)",
             LOOP_INTERVAL_US / 1000);

    int loop_count = 0;

    while (g_running) {
        loop_count++;

        /* Poll sensors -- use CMD_SET_SENSORS to never touch mode/motors */
        if (loop_count % SENSOR_INTERVAL == 0) {
            rover_state_t sensors;
            poll_sensors(&sensors);
            state_set_sensors(&sensors);  /* safe -- preserves mode */
        }

        /* Run decision engine */
        if (loop_count % DECISION_INTERVAL == 0) {
            rover_state_t current;
            if (state_get(&current) == 0) {
                if (current.mode == MODE_AUTO && !current.motor_stop) {
                    decision_run_cycle(&current);
                } else if (current.mode == MODE_MANUAL) {
                    motor_cmd_t cmd = {
                        .left  = current.motor_left,
                        .right = current.motor_right,
                    };
                    safety_check_motor(&cmd, &current);
                }
            }
        }

        if (loop_count >= 10000)
            loop_count = 0;

        usleep(LOOP_INTERVAL_US);
    }

    LOG_INFO("rover: shutting down");
    motor_stop();
    motor_free();
    state_mgr_stop();
    logger_shutdown();

    printf("=== Rover stopped ===\n");
    return 0;
}
