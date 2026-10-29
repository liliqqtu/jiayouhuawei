/*
 * Copyright (c) 2023 Beijing HuaQing YuanJian Education Technology Co., Ltd
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H
#include <stdint.h>

#include "hal_bsp_structAll.h"

#define MOTOR_LOW_SPEED 40    // 低等速度
#define MOTOR_MIDDLE_SPEED 70 // 中等速度
#define MOTOR_HIGH_SPEED 100  // 高等速度

#define UDP_PORT 7788

// 小车的当前状态值
typedef enum {
    CAR_STATUS_RUN = 0x01, // 前进
    CAR_STATUS_BACK,       // 后退
    CAR_STATUS_LEFT,       // 左转
    CAR_STATUS_RIGHT,      // 右转
    CAR_STATUS_STOP,       // 停止
    CAR_STATUS_ON,         // 开启电机
    CAR_STATUS_OFF,        // 关闭电机
    CAR_STATUS_L_SPEED,    // 低速行驶
    CAR_STATUS_M_SPEED,    // 中速行驶
    CAR_STATUS_H_SPEED,    // 高速行驶
} te_car_status_t;

/*********************************** 系统的全局变量 ***********************************/
typedef struct _system_value {
    te_car_status_t car_status; // 小车的状态
    uint16_t left_motor_speed;       // 左电机的编码器值
    uint16_t right_motor_speed;      // 右电机的编码器值
    uint16_t battery_voltage;        // 电池当前电压值
    uint16_t distance;               // 距离传感器
    uint8_t auto_abstacle_flag;      // 是否开启避障功能
    int udp_socket_fd;          // UDP通信的套接字
} system_value_t;
extern system_value_t systemValue; // 系统全局变量

extern tn_pcf8574_io_t pcf8574_io;
#define IO_FAN pcf8574_io.bit.p0
#define IO_BUZZER pcf8574_io.bit.p1
#define IO_LED pcf8574_io.bit.p2

#endif
