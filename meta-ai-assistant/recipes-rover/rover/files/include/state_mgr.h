#ifndef STATE_MGR_H
#define STATE_MGR_H

#include "state.h"

int  state_mgr_start(void);
void state_mgr_stop(void);
int  state_get(rover_state_t *out);
int  state_set(const rover_state_t *in);
int  state_set_sensors(const rover_state_t *in);
int  state_set_mode(int mode);
int  state_emergency_stop(void);

#endif
