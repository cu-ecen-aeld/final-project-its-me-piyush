#include "safety.h"
#include "logger.h"
#include <string.h>

#define OBSTACLE_STOP_CM     15.0f
#define OBSTACLE_SLOW_CM     40.0f
#define OBSTACLE_SLOW_FACTOR  0.4f
#define TEMP_MAX_C           40.0f

safety_result_t safety_check_motor(motor_cmd_t *cmd,
                                   const rover_state_t *state)
{
    if (state->motor_stop) {
        LOG_SAFETY("motor BLOCKED -- emergency stop active");
        cmd->left = cmd->right = 0;
        return SAFE_BLOCKED;
    }

    if (state->temperature >= TEMP_MAX_C) {
        LOG_SAFETY("motor BLOCKED -- critical temp %.1fC", state->temperature);
        cmd->left = cmd->right = 0;
        return SAFE_BLOCKED;
    }

    int moving_forward = (cmd->left > 0 && cmd->right > 0);

    if (moving_forward && state->distance <= OBSTACLE_STOP_CM) {
        LOG_SAFETY("motor BLOCKED -- obstacle %.1fcm", state->distance);
        cmd->left = cmd->right = 0;
        return SAFE_BLOCKED;
    }

    if (moving_forward && state->distance <= OBSTACLE_SLOW_CM) {
        int new_left  = (int)(cmd->left  * OBSTACLE_SLOW_FACTOR);
        int new_right = (int)(cmd->right * OBSTACLE_SLOW_FACTOR);
        LOG_SAFETY("motor MODIFIED -- obstacle %.1fcm speed %d->%d",
                   state->distance, cmd->left, new_left);
        cmd->left  = new_left;
        cmd->right = new_right;
        return SAFE_MODIFIED;
    }

    return SAFE_OK;
}

safety_result_t safety_check_actuator(const char *actuator,
                                      int value,
                                      const rover_state_t *state)
{
    (void)state;
    if (!actuator) return SAFE_BLOCKED;

    /* Buzzer: always allowed */
    /* LED:    always allowed */
    /* Add rover-specific rules here as hardware is added */

    (void)value;
    return SAFE_OK;
}

int safety_is_emergency(const rover_state_t *state)
{
    return state->motor_stop;
}
