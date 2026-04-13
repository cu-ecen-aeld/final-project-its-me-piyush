#ifndef DECISION_H
#define DECISION_H

#include "state.h"
#include "motor.h"
#include "safety.h"

#define MODE_AUTO    0
#define MODE_MANUAL  1
#define MODE_GESTURE 2

void            decision_run_cycle(const rover_state_t *state);
safety_result_t decision_apply_motor(motor_cmd_t *cmd,
                                     const rover_state_t *state);

#endif
