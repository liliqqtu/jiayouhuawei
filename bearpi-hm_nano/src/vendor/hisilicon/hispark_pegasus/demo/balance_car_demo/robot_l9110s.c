/*
 * Copyright (c) 2022 HiSilicon (Shanghai) Technologies CO., LIMITED.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include "compile_define.h"
#include "iot_gpio.h"
#include "iot_pwm.h"
#include "robot_l9110s.h"

#define IOT_PWM_PORT_PWM0           (0)
#define IOT_PWM_PORT_PWM1           (1)
#define IOT_PWM_PORT_PWM2           (2)
#define IOT_PWM_PORT_PWM3           (3)
#define IOT_FREQ            100000

#define DRIVE_LEFT_FORWARD_PIN_NAME (IOT_IO_NAME_GPIO_5)
#define DRIVE_LEFT_FORWARD_PIN_GPIO (IOT_IO_FUNC_GPIO_5_GPIO)
#define DRIVE_LEFT_FORWARD_PWM      (IOT_PWM_PORT_PWM2)
#define DRIVE_LEFT_FORWARD_PWM_FUNC (IOT_IO_FUNC_GPIO_5_PWM2_OUT)
#define DRIVE_LEFT_BACKWARD_PIN_NAME (IOT_IO_NAME_GPIO_6)
#define DRIVE_LEFT_BACKWARD_PIN_GPIO (IOT_IO_FUNC_GPIO_6_GPIO)
#define DRIVE_LEFT_BACKWARD_PWM      (IOT_PWM_PORT_PWM3)
#define DRIVE_LEFT_BACKWARD_PWM_FUNC (IOT_IO_FUNC_GPIO_6_PWM3_OUT)
#define DRIVE_RIGHT_FORWARD_PIN_NAME (IOT_IO_NAME_GPIO_10)
#define DRIVE_RIGHT_FORWARD_PIN_GPIO (IOT_IO_FUNC_GPIO_10_GPIO)
#define DRIVE_RIGHT_FORWARD_PWM      (IOT_PWM_PORT_PWM1)
#define DRIVE_RIGHT_FORWARD_PWM_FUNC (IOT_IO_FUNC_GPIO_10_PWM1_OUT)
#define DRIVE_RIGHT_BACKWARD_PIN_NAME (IOT_IO_NAME_GPIO_9)
#define DRIVE_RIGHT_BACKWARD_PIN_GPIO (IOT_IO_FUNC_GPIO_9_GPIO)
#define DRIVE_RIGHT_BACKWARD_PWM      (IOT_PWM_PORT_PWM0)
#define DRIVE_RIGHT_BACKWARD_PWM_FUNC (IOT_IO_FUNC_GPIO_9_PWM0_OUT)

void init_car_drive(void)
{
    IoSetFunc(DRIVE_LEFT_FORWARD_PIN_NAME, DRIVE_LEFT_FORWARD_PWM_FUNC);
    IoTGpioSetDir(DRIVE_LEFT_FORWARD_PIN_NAME, IOT_GPIO_DIR_OUT);
    IoTPwmInit(DRIVE_LEFT_FORWARD_PWM);

    IoSetFunc(DRIVE_LEFT_BACKWARD_PIN_NAME, DRIVE_LEFT_BACKWARD_PWM_FUNC);
    IoTGpioSetDir(DRIVE_LEFT_BACKWARD_PIN_NAME, IOT_GPIO_DIR_OUT);
    IoTPwmInit(DRIVE_LEFT_BACKWARD_PWM);

    IoSetFunc(DRIVE_RIGHT_FORWARD_PIN_NAME, DRIVE_RIGHT_FORWARD_PWM_FUNC);
    IoTGpioSetDir(DRIVE_RIGHT_FORWARD_PIN_NAME, IOT_GPIO_DIR_OUT);
    IoTPwmInit(DRIVE_RIGHT_FORWARD_PWM);

    IoSetFunc(DRIVE_RIGHT_BACKWARD_PIN_NAME, DRIVE_RIGHT_BACKWARD_PWM_FUNC);
    IoTGpioSetDir(DRIVE_RIGHT_BACKWARD_PIN_NAME, IOT_GPIO_DIR_OUT);
    IoTPwmInit(DRIVE_RIGHT_BACKWARD_PWM);
}

void car_forward(uint32_t pwm_value)
{
    car_stop();
    IoTPwmStart(DRIVE_LEFT_FORWARD_PWM, pwm_value, IOT_FREQ);     // 左轮正转
    IoTPwmStart(DRIVE_RIGHT_FORWARD_PWM, pwm_value, IOT_FREQ);    // 右轮正转
}

void car_backward(uint32_t pwm_value)
{
    car_stop();
    IoTPwmStart(DRIVE_LEFT_BACKWARD_PWM, pwm_value, IOT_FREQ);    // 左轮反转
    IoTPwmStart(DRIVE_RIGHT_BACKWARD_PWM, pwm_value, IOT_FREQ);
}

void car_drive(int pwm_value)
{
    if (pwm_value > 0) {
        car_forward((uint32_t)(pwm_value));
    } else if (pwm_value < 0) {
        car_backward((uint32_t)(-pwm_value));
    } else {
        car_stop();
    }
}

void car_stop(void)
{
    IoTPwmStop(DRIVE_LEFT_FORWARD_PWM);
    IoTPwmStop(DRIVE_LEFT_BACKWARD_PWM);
    IoTPwmStop(DRIVE_RIGHT_FORWARD_PWM);
    IoTPwmStop(DRIVE_RIGHT_BACKWARD_PWM);
}
