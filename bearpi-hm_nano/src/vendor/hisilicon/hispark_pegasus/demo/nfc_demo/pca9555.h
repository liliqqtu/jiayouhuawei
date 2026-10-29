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

/*
 * 定义Part0、Part1引脚的输出类型
 * Define the output type of Part0 and Part1 pins
 */
#define PCA9555_READ                  (0X4B) // 读指令代码，Read instruction code
#define PCA9555_WRITE                 (0X4A) // 写指令代码，Write instruction code

/*
 * 定义PCA9555内部寄存器地址
 * Define PCA9555 internal register address
 */
#define PCA9555_PART0_INPUT           0x00
#define PCA9555_PART1_INPUT           0x01
#define PCA9555_PART0_OUTPUT          0x02
#define PCA9555_PART1_OUTPUT          0x03
#define PCA9555_PART0_IPOL            0x04
#define PCA9555_PART1_IPOL            0x05
#define PCA9555_PART0_IODIR           0x06
#define PCA9555_PART1_IODIR           0x07

#define RED_LED                       0x09
#define GREEN_LED                     0x12
#define BLUE_LED                      0x24
#define WHITE_LED                     0xff
#define LED_OFF                       0x00

#define PCA9555_I2C_IDX              0
#define IOT_I2C_IDX_BAUDRATE         400000 // 400k

typedef struct {
    /* Pointer to the buffer storing data to send */
    unsigned char *sendBuf;
    /* Length of data to send */
    unsigned int sendLen;
    /* Pointer to the buffer for storing data to receive */
    unsigned char *receiveBuf;
    /* Length of data received */
    unsigned int receiveLen;
} IotI2cData;

void PCA9555Init(void);
uint32_t PCA9555I2CReadByte(uint8_t *rec_byte);
uint32_t PCA9555I2CWriteByte(uint8_t* buffer, uint32_t buffLen);
void GetFunKeyState(void);
uint32_t SetPCA9555GpioValue(uint8_t addr, uint8_t buffer);

#endif // __PCA9555_H__