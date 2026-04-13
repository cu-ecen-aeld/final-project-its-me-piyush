#ifndef MOTOR_H
#define MOTOR_H

#define MOTOR_GPIO_CHIP  "/dev/gpiochip4"
#define MOTOR_PIN_IN1    17
#define MOTOR_PIN_IN2    27
#define MOTOR_PIN_IN3    22
#define MOTOR_PIN_IN4    23

int  motor_init(void);
void motor_free(void);
int  motor_set_left(int speed);
int  motor_set_right(int speed);
int  motor_forward(int speed);
int  motor_backward(int speed);
int  motor_turn_left(int speed);
int  motor_turn_right(int speed);
int  motor_stop(void);

#endif
