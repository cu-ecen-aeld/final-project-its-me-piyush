#ifndef SAFETY_H
#define SAFETY_H

#include "state.h"

typedef enum {
    SAFE_OK       = 0,
    SAFE_BLOCKED  = 1,
    SAFE_MODIFIED = 2,
} safety_result_t;

typedef struct {
    int left;
    int right;
} motor_cmd_t;

safety_result_t safety_check_motor(motor_cmd_t *cmd,
                                   const rover_state_t *state);
safety_result_t safety_check_actuator(const char *actuator,
                                      int value,
                                      const rover_state_t *state);
int safety_is_emergency(const rover_state_t *state);

#endif
