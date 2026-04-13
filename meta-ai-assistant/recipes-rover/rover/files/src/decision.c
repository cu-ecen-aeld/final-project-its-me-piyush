#include "decision.h"
#include "logger.h"
#include "state_mgr.h"
#include <string.h>

/* ---- Thresholds ---- */

#define OBSTACLE_STOP_CM   15.0f
#define OBSTACLE_SLOW_CM   40.0f
#define OBSTACLE_TURN_CM   60.0f
#define DARK_THRESHOLD     150
#define CRUISE_SPEED       70
#define TURN_SPEED         60
#define SLOW_SPEED         30

/* ---- Internal helpers ---- */

static void do_stop(const rover_state_t *state, const char *reason)
{
    motor_cmd_t cmd = { .left = 0, .right = 0 };
    if (safety_check_motor(&cmd, state) != SAFE_BLOCKED || state->motor_stop)
        motor_stop();
    LOG_ACTION("decision: STOP | %s", reason);
}

static void do_forward(int speed, const rover_state_t *state,
                       const char *reason)
{
    motor_cmd_t cmd = { .left = speed, .right = speed };
    safety_result_t r = safety_check_motor(&cmd, state);
    if (r == SAFE_BLOCKED) return;
    motor_set_left(cmd.left);
    motor_set_right(cmd.right);
    LOG_ACTION("decision: FORWARD speed=%d | %s", cmd.left, reason);
}

static void __attribute__((unused)) do_turn_left(int speed, const rover_state_t *state,
                         const char *reason)
{
    motor_cmd_t cmd = { .left = -speed, .right = speed };
    safety_result_t r = safety_check_motor(&cmd, state);
    if (r == SAFE_BLOCKED) return;
    motor_set_left(cmd.left);
    motor_set_right(cmd.right);
    LOG_ACTION("decision: TURN_LEFT speed=%d | %s", speed, reason);
}

static void do_turn_right(int speed, const rover_state_t *state,
                          const char *reason)
{
    motor_cmd_t cmd = { .left = speed, .right = -speed };
    safety_result_t r = safety_check_motor(&cmd, state);
    if (r == SAFE_BLOCKED) return;
    motor_set_left(cmd.left);
    motor_set_right(cmd.right);
    LOG_ACTION("decision: TURN_RIGHT speed=%d | %s", speed, reason);
}

/* ---- Autonomous mode rules ---- */

static void run_auto(const rover_state_t *s)
{
    /* Priority 1: emergency stop */
    if (safety_is_emergency(s)) {
        do_stop(s, "emergency stop active");
        return;
    }

    float dist = s->distance;

    /* Priority 2: obstacle in stop zone */
    if (dist <= OBSTACLE_STOP_CM) {
        do_stop(s, "obstacle in stop zone");
        return;
    }

    /* Priority 3: obstacle in turn zone -- turn to avoid */
    if (dist <= OBSTACLE_TURN_CM) {
        /* Simple strategy: always turn right first.
           Vision module can override this with direction data later. */
        do_turn_right(TURN_SPEED, s, "obstacle ahead, avoiding right");
        return;
    }

    /* Priority 4: obstacle in slow zone -- reduce speed */
    if (dist <= OBSTACLE_SLOW_CM) {
        do_forward(SLOW_SPEED, s, "obstacle in slow zone");
        return;
    }

    /* Priority 5: clear path -- cruise */
    do_forward(CRUISE_SPEED, s, "path clear");
}

/* ---- Manual mode ---- */

static void run_manual(const rover_state_t *s)
{
    /* In manual mode the decision engine only enforces safety.
       Actual commands come from external input (web UI / gesture).
       Here we just validate the current motor state. */
    motor_cmd_t cmd = {
        .left  = s->motor_left,
        .right = s->motor_right,
    };
    safety_result_t r = safety_check_motor(&cmd, s);
    if (r == SAFE_BLOCKED) {
        motor_stop();
        LOG_SAFETY("decision: manual command blocked by safety");
        return;
    }
    if (r == SAFE_MODIFIED) {
        motor_set_left(cmd.left);
        motor_set_right(cmd.right);
    }
    /* SAFE_OK: hardware already set by caller, nothing to do */
}

/* ---- Gesture mode ---- */

static void run_gesture(const rover_state_t *s)
{
    /* Gesture recognition result stored in mode field for now.
       Vision module will write detected gesture to state.
       Placeholder until gesture.c is implemented. */
    LOG_INFO("decision: gesture mode -- waiting for vision input");
    (void)s;
}

/* ---- LED/buzzer rules (run in all modes) ---- */

static void run_indicators(const rover_state_t *s)
{
    /* Buzzer: beep when obstacle close */
    if (s->distance <= OBSTACLE_STOP_CM && !s->buzzer_on) {
        LOG_ACTION("decision: BUZZER ON | obstacle %.1fcm", s->distance);
        /* buzzer_set(1) -- will be wired when GPIO available */
    } else if (s->distance > OBSTACLE_SLOW_CM && s->buzzer_on) {
        LOG_ACTION("decision: BUZZER OFF | path clear");
        /* buzzer_set(0) */
    }

    /* LED: turn on in dark environment */
    if (s->light_level < DARK_THRESHOLD && !s->light_on) {
        LOG_ACTION("decision: LED ON | light=%d lux", s->light_level);
        /* led_set(1) */
    } else if (s->light_level >= DARK_THRESHOLD && s->light_on) {
        LOG_ACTION("decision: LED OFF | light=%d lux", s->light_level);
        /* led_set(0) */
    }
}

/* ---- Public API ---- */

void decision_run_cycle(const rover_state_t *state)
{
    run_indicators(state);

    switch (state->mode) {
    case MODE_AUTO:
        run_auto(state);
        break;
    case MODE_MANUAL:
        run_manual(state);
        break;
    case MODE_GESTURE:
        run_gesture(state);
        break;
    default:
        LOG_WARN("decision: unknown mode %d", state->mode);
        do_stop(state, "unknown mode");
        break;
    }
}

safety_result_t decision_apply_motor(motor_cmd_t *cmd,
                                     const rover_state_t *state)
{
    safety_result_t r = safety_check_motor(cmd, state);
    if (r != SAFE_BLOCKED) {
        motor_set_left(cmd->left);
        motor_set_right(cmd->right);
    }
    return r;
}
