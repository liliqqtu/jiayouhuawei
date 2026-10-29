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

#include "cmsis_os2.h"
#include "hal_bsp_structAll.h"

#define event1_Flags 0x00000001U // 事件掩码 每一位代表一个事件

typedef struct {
    int top;  // 上边距
    int left; // 下边距
    int hight; // 高
    int width; // 宽
} margin_t;   // 边距类型

typedef struct message_data {
    unsigned short distance; // 距离传感器的值
    tn_pcf8574_io_t pcf8574_io;
} msg_data_t;

#endif
