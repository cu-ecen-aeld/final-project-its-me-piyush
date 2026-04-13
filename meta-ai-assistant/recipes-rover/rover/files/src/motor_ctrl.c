#include "motor.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <gpiod.h>

/* libgpiod v2 API */

static struct gpiod_chip        *g_chip     = NULL;
static struct gpiod_line_request *g_request  = NULL;
static int                       g_sim_mode = 0;

/* Pin index mapping within the request */
#define IDX_IN1  0
#define IDX_IN2  1
#define IDX_IN3  2
#define IDX_IN4  3
#define NUM_PINS 4

static const unsigned int g_pins[NUM_PINS] = {
    MOTOR_PIN_IN1,
    MOTOR_PIN_IN2,
    MOTOR_PIN_IN3,
    MOTOR_PIN_IN4,
};

static void set_motor_pins(int idx_a, int idx_b, int speed, const char *side)
{
    int va = (speed > 0) ? 1 : 0;
    int vb = (speed < 0) ? 1 : 0;

    if (g_sim_mode) {
        LOG_INFO("motor [SIM] %s: IN_A=%d IN_B=%d speed=%d",
                 side, va, vb, speed);
        return;
    }

    struct gpiod_line_request *req = g_request;
    gpiod_line_request_set_value(req, g_pins[idx_a],
        va ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
    gpiod_line_request_set_value(req, g_pins[idx_b],
        vb ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

int motor_init(void)
{
    g_chip = gpiod_chip_open(MOTOR_GPIO_CHIP);
    if (!g_chip) {
        LOG_WARN("motor: cannot open %s, simulation mode", MOTOR_GPIO_CHIP);
        g_sim_mode = 1;
        return 0;
    }

    struct gpiod_request_config *rcfg = gpiod_request_config_new();
    if (!rcfg) { g_sim_mode = 1; return 0; }
    gpiod_request_config_set_consumer(rcfg, "rover-motor");

    struct gpiod_line_config *lcfg = gpiod_line_config_new();
    if (!lcfg) {
        gpiod_request_config_free(rcfg);
        g_sim_mode = 1;
        return 0;
    }

    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings) {
        gpiod_line_config_free(lcfg);
        gpiod_request_config_free(rcfg);
        g_sim_mode = 1;
        return 0;
    }

    gpiod_line_settings_set_direction(settings,
                                      GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings,
                                         GPIOD_LINE_VALUE_INACTIVE);

    for (int i = 0; i < NUM_PINS; i++) {
        if (gpiod_line_config_add_line_settings(lcfg, &g_pins[i], 1,
                                                settings) < 0) {
            LOG_WARN("motor: failed to configure pin %u", g_pins[i]);
            g_sim_mode = 1;
            break;
        }
    }

    gpiod_line_settings_free(settings);

    if (!g_sim_mode) {
        g_request = gpiod_chip_request_lines(g_chip, rcfg, lcfg);
        if (!g_request) {
            LOG_WARN("motor: line request failed, simulation mode");
            g_sim_mode = 1;
        } else {
            LOG_INFO("motor: GPIO ready on %s (libgpiod v2)",
                     MOTOR_GPIO_CHIP);
        }
    }

    gpiod_line_config_free(lcfg);
    gpiod_request_config_free(rcfg);
    return 0;
}

void motor_free(void)
{
    motor_stop();
    if (g_request) {
        gpiod_line_request_release(g_request);
        g_request = NULL;
    }
    if (g_chip) {
        gpiod_chip_close(g_chip);
        g_chip = NULL;
    }
    LOG_INFO("motor: released");
}

int motor_set_left(int speed)
{
    if (speed < -100) speed = -100;
    if (speed >  100) speed =  100;
    set_motor_pins(IDX_IN1, IDX_IN2, speed, "LEFT");
    return 0;
}

int motor_set_right(int speed)
{
    if (speed < -100) speed = -100;
    if (speed >  100) speed =  100;
    set_motor_pins(IDX_IN3, IDX_IN4, speed, "RIGHT");
    return 0;
}

int motor_forward(int speed)
{
    LOG_ACTION("motor: FORWARD speed=%d", speed);
    motor_set_left(speed);
    motor_set_right(speed);
    return 0;
}

int motor_backward(int speed)
{
    LOG_ACTION("motor: BACKWARD speed=%d", speed);
    motor_set_left(-speed);
    motor_set_right(-speed);
    return 0;
}

int motor_turn_left(int speed)
{
    LOG_ACTION("motor: TURN_LEFT speed=%d", speed);
    motor_set_left(-speed);
    motor_set_right(speed);
    return 0;
}

int motor_turn_right(int speed)
{
    LOG_ACTION("motor: TURN_RIGHT speed=%d", speed);
    motor_set_left(speed);
    motor_set_right(-speed);
    return 0;
}

int motor_stop(void)
{
    LOG_ACTION("motor: STOP");
    motor_set_left(0);
    motor_set_right(0);
    return 0;
}
