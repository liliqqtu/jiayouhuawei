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


#ifndef CTRL_ALGO_H
#define CTRL_ALGO_H

#include "stdint.h"

typedef struct {
    uint8_t type;
    float kp;
    float ki;
    float kd;
    float err_last;
    float err_sum;
    float limit_err;
    float limit_sum;
    float limit_exec;
} CTRL_PID_STRUCT;

#define CTRL_PID_TYPE_BIT_K     (0x00)
#define CTRL_PID_TYPE_BIT_I     (0x01)
#define CTRL_PID_TYPE_BIT_D     (0x02)
#define CTRL_PID_TYPE_MASK_K    (0x01 << CTRL_PID_TYPE_BIT_K)
#define CTRL_PID_TYPE_MASK_I    (0x01 << CTRL_PID_TYPE_BIT_I)
#define CTRL_PID_TYPE_MASK_D    (0x01 << CTRL_PID_TYPE_BIT_D)

void init_ctrl_algo(void);
float ctrl_pid_algo(float target, float feedback, CTRL_PID_STRUCT *param);
CTRL_PID_STRUCT GetVelocity(void);
CTRL_PID_STRUCT GetStand(void);

#endif

