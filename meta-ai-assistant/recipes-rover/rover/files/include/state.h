#ifndef STATE_H
#define STATE_H

#define STATE_SOCK_PATH "/tmp/rover_state.sock"

typedef struct {
    float temperature;
    float humidity;
    int   light_level;
    int   motion;
    float distance;
    int   fan_on;
    int   light_on;
    int   heater_on;
    int   buzzer_on;
    int   motor_left;
    int   motor_right;
    int   motor_stop;
    int   mode;
    long  updated_sec;
    long  updated_usec;
} rover_state_t;

typedef enum {
    CMD_GET_STATE   = 1,
    CMD_SET_STATE   = 2,
    CMD_STOP        = 3,
    CMD_SET_SENSORS = 4,
    CMD_SET_MODE    = 5,
} state_cmd_t;

typedef struct {
    state_cmd_t   cmd;
    rover_state_t payload;
} state_msg_t;

#endif
