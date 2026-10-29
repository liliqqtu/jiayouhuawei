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

#ifndef PCA9555_H
#define PCA9555_H

#include <stdint.h>

// 定义Part0、Part1引脚的输出类型
#define PCA9555_READ                  (0X4B)      // 读指令代码
#define PCA9555_WRITE                 (0X4A)      // 写指令代码

// 定义PCA9555内部寄存器地址
#define PCA9555_PART0_INPUT           0x00
#define PCA9555_PART1_INPUT           0x01
#define PCA9555_PART0_OUTPUT          0x02
#define PCA9555_PART1_OUTPUT          0x03
#define PCA9555_PART0_IPOL            0x04
#define PCA9555_PART1_IPOL            0x05
#define PCA9555_PART0_IODIR           0x06
#define PCA9555_PART1_IODIR           0x07

#define RED_LED                       0x09 // 右红灯：OUT0_EXTIO,左红灯：OUT3_EXTIO ==>  0000 1001 ==> 0x09
#define GREEN_LED                     0x12 // 右绿灯：OUT1_EXTIO,左绿灯：OUT4_EXTIO ==>  0001 0010 ==> 0x12
#define BLUE_LED                      0x24 // 右蓝灯：OUT2_EXTIO,左蓝灯：OUT5_EXTIO == > 0010 0100 ==> 0x24
#define WHITE_LED                     0xff // 三灯全亮 ==>  0011 1111 ==> 0x3f
#define LED_OFF                       0x00 // 三灯全灭 ==> 0000 0000  ==> 0x00

// 初始化指定地址的PCA9555器件
void PCA9555Init(void);

// 从指定地址的PCA9555器件的指定寄存器读一字节数据
uint32_t PCA9555I2CReadByte(uint8_t *rec_byte);

// 向指定地址的PCA9555器件的指定寄存器写一字节数据
uint32_t PCA9555I2CWriteByte(uint8_t* buffer, uint32_t buffLen);

// 获取三个功能按键 S3、S4、S5的状态
void GetFunKeyState(void);
uint32_t SetPCA9555GpioValue(uint8_t addr, uint8_t buffer);

#endif // __PCA9555_H__