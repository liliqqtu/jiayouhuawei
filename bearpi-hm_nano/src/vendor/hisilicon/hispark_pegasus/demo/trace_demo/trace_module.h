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

#ifndef TRACE_MODULE_H
#define TRACE_MODULE_H

#include <stdint.h>

typedef enum {
    MODE_ON_OFF = 0,
    MODE_SET_LEFT_FORWARD,
    MODE_SET_RIGHT_FORWARD,
    MODE_SET_TURN_LEFT,
    MODE_SET_TURN_RIGHT,
    MODE_SET_LEFT_ADC,
    MODE_SET_RIGHT_ADC,
    MODE_END,
} ENUM_MODE;

typedef struct {
    uint32_t LeftForward;
    uint32_t RightForward;
    uint32_t TurnLeft;
    uint32_t TurnRight;
    unsigned short leftadcdata;
    unsigned short rightadcdata;
} CAR_DRIVE;

#define     ADC_TEST_LENGTH             (20)
#endif
