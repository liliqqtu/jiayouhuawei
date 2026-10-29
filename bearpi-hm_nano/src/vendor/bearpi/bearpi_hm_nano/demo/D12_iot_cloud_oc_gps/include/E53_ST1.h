/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
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

#ifndef __E53_ST1_H__
#define __E53_ST1_H__

#define L80R_CONSTANT_10         10
#define L80R_CONSTANT_2          2
#define L80R_CONSTANT_3          3
#define L80R_CONSTANT_4          4
#define L80R_CONSTANT_5          5
#define L80R_CONSTANT_6          6
#define L80R_CONSTANT_60         60

#define L80R_COEFFICIENT         100000
#define L80R_DATA_LEN            100


/***************************************************************
* 名		称: GasStatus_ENUM
* 说    明：枚举状态结构体
***************************************************************/
typedef enum {
    OFF = 0,
    ON
} E53ST1Status;

/***************************************************\
* GPS NMEA-0183协议重要参数结构体定义
* 卫星信息
\***************************************************/
typedef struct {
    uint32_t latitude_bd;       // 纬度分扩大100000倍，实际要除以100000
    uint8_t nshemi_bd;          // 北纬 / 南纬,N:北纬;S:南纬
    uint32_t longitude_bd;      // 经度分扩大100000倍,实际要除以100000
    uint8_t ewhemi_bd;          // 东经 / 西经,E:东经;W:西经
}gps_msg;

/* E53_ST1传感器数据类型定义 ------------------------------------------------------------*/
typedef struct {
    float    Longitude;         // 经度
    float    Latitude;          // 纬度
} E53ST1Data;


void E53ST1Init(void);
void E53ST1ReadData(E53ST1Data *ReadData);
void BeepStatusSet(E53ST1Status status);

#endif /* __E53_ST1_H__ */

