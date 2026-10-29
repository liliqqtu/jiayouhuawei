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

#ifndef HISTREAMING_H
#define HISTREAMING_H

#include <link_platform.h>
#include <link_service.h>

typedef enum {
    CAR_STOP_STATUS = 0,
    CAR_RUNNING_STATUS,
    CAR_TRACE_STATUS
} hi_car_status;

typedef enum {
    CAR_DIRECTION_CONTROL_MODE = 1, // 控制小车方向
    CAR_MODULE_CONTROL_MODE, // 控制小车的扩展模块
    CAR_SPEED_CONTROL_MODE // 速度控制
} hi_car_control_mode;

typedef enum {
    CAR_SPEED_UP = 1,
    CAR_SPEED_REDUCTION,
} hi_car_speed_type;

typedef enum {
    CAR_KEEP_GOING_TYPE = 1,
    CAR_KEEP_GOING_BACK_TYPE,
    CAR_KEEP_TURN_LEFT_TYPE,
    CAR_KEEP_TURN_RIGHT_TYPE,
    CAR_GO_FORWARD_TYPE,
    CAR_TURN_LEFT_TYPE,
    CAR_STOP_TYPE,
    CAR_TURN_RIGHT_TYPE,
    CAT_TURN_BACK_TYPE,
} hi_car_direction_control_type;

/* 小车扩展模块的几种类型 */
typedef enum {
    CAR_CONTROL_ENGINE_LEFT_TYPE = 1,
    CAR_CONTROL_ENGINE_RIGHT_TYPE,
    CAR_CONTROL_ENGINE_MIDDLE_TYPE,
    CAR_CONTROL_TRACE_TYPE,
    CAR_CONTROL_ULTRASONIC_TYPE,
    CAR_CONTROL_STEER_ENGINE_TYPE,
} hi_car_module_control_type;

void histreaming_open(void);
void histreaming_close(LinkPlatform *link);
#endif /* __HISTREAMING_DEMO_H__ */