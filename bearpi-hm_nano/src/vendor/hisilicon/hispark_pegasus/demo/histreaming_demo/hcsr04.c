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

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "hi_time.h"
#include "iot_gpio_ex.h"
#include "sg92r_control.h"
#include "motor_control.h"
#include "hcsr04.h"

#define CAR_TURN_LEFT                     (0)
#define CAR_TURN_RIGHT                    (1)
#define DISTANCE_BETWEEN_CAR_AND_OBSTACLE (15.0)

void Hcsr04Init(void)
{
    // 设置超声波Echo为输入模式
    // 设置GPIO8功能（设置为GPIO功能）
    IoSetFunc(IOT_IO_NAME_GPIO_8, IOT_IO_FUNC_GPIO_8_GPIO);
    // 设置GPIO8为输入方向
    IoTGpioSetDir(IOT_IO_NAME_GPIO_8, IOT_GPIO_DIR_IN);

    // 设置GPIO7功能（设置为GPIO功能）
    IoSetFunc(IOT_IO_NAME_GPIO_7, IOT_IO_FUNC_GPIO_7_GPIO);
    // 设置GPIO7为输出方向
    IoTGpioSetDir(IOT_IO_NAME_GPIO_7, IOT_GPIO_DIR_OUT);
}

float GetDistance(void)
{
    // 定义变量
    static unsigned long start_time = 0, time = 0;
    float distance = 0.0;
    IotGpioValue value = IOT_GPIO_VALUE0;
    unsigned int flag = 0;

    // 设置GPIO7输出低电平
    /* 给trig发送至少10us的高电平脉冲，以触发传感器测距 */
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE1);
    // 延时函数20us（设置高电平持续时间）
    hi_udelay(20);
    // 设置GPIO7输出高电平
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE0);
    /* 计算与障碍物之间的距离 */
    while (1) {
        // 获取GPIO8的输入电平状态
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_8, &value);
        // 判断GPIO8的输入电平是否为高电平并且flag为0
        if (value == IOT_GPIO_VALUE1 && flag == 0) {
            // 获取系统时间
            start_time = hi_get_us();
            // 将flag设置为1
            flag = 1;
        }
        // 判断GPIO8的输入电平是否为低电平并且flag为1
        if (value == IOT_GPIO_VALUE0 && flag == 1) {
            // 获取高电平持续时间
            time = hi_get_us() - start_time;
            break;
        }
    }
    // 计算距离障碍物距离（340米/秒 转换为 0.034厘米/微秒,一去一回2倍距离）
    distance = time * 0.034 / 2;
    printf("distance is %0.2f cm\r\n", distance);
    return distance;
}

/* Judge steering gear */
unsigned int engine_go_where(void)
{
    float left_distance = 0.0;
    float right_distance = 0.0;
    /* 舵机往左转动测量左边障碍物的距离 */

    EngineTurnLeft();
    TaskMsleep(200); // 200ms
    left_distance = GetDistance();
    TaskMsleep(200); // 200ms
    /* 归中 */
    RegressMiddle();
    TaskMsleep(200); // 200ms

    /* 舵机往右转动测量右边障碍物的距离 */
    EngineTurnRight();
    TaskMsleep(200); // 200ms
    right_distance = GetDistance();
    TaskMsleep(200); // 200ms
    /* 归中 */
    RegressMiddle();

    if (left_distance > right_distance) {
        return CAR_TURN_LEFT;
    } else {
        return CAR_TURN_RIGHT;
    }
}

/*
 * 根据障碍物的距离来判断小车的行走方向
 * 1、距离大于等于15cm继续前进
 * 2、距离小于15cm，先停止再后退0.1s,继续进行测距,再进行判断
 */
/* Judge the direction of the car */
unsigned int car_where_to_go(float distance)
{
    if (distance < DISTANCE_BETWEEN_CAR_AND_OBSTACLE) {
        car_stop();
        car_backward();
        TaskMsleep(100);  // 100ms
        car_stop();
        unsigned int ret = engine_go_where();
        if (ret == CAR_TURN_LEFT) {
            car_left();
            TaskMsleep(500); // 500ms
        } else if (ret == CAR_TURN_RIGHT) {
            car_right();
            TaskMsleep(500); // 500ms
        }
        car_stop();
    } else {
        car_forward();
    }
    return 1;
}

/* 超声波避障 */
void ultrasonic_demo(void)
{
    float m_distance = 0.0;
    // regress_middle();
    /* 获取前方物体的距离 */
    m_distance = GetDistance();
    car_where_to_go(m_distance);
    TaskMsleep(20); // 20ms
}