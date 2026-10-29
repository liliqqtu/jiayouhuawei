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

#ifndef HAL_BSP_STRUCTALL_H
#define HAL_BSP_STRUCTALL_H

// 使用位域定义IO扩展芯片的外部接口
typedef union _PCF8574_IO {
    unsigned char all;
    struct PCF8574_REG {
        unsigned char p0 : 1; // P0口
        unsigned char p1 : 1;
        unsigned char p2 : 1;
        unsigned char p3 : 1;

        unsigned char p4 : 1;
        unsigned char p5 : 1;
        unsigned char p6 : 1;
        unsigned char p7 : 1; // P7口
    } bit;
} tn_pcf8574_io_t;

#define NV_ID 0x0B
typedef struct {
    unsigned short smartControl_flag;
    unsigned short light_upper;
    unsigned short light_lower;
    unsigned char humi_upper;
    unsigned char humi_lower;
} hi_nv_save_sensor_threshold;

#endif
