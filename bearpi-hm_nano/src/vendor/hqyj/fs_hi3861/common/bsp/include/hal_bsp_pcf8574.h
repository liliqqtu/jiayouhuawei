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

#ifndef HAL_BSP_PCF8574_H
#define HAL_BSP_PCF8574_H

#include <stdbool.h>
#include "cmsis_os2.h"

#define PCF8574_I2C_ADDR (0x42) // 器件的I2C从机地址
#define PCF8574_I2C_ID 0        // 模块的I2C总线号
#define PCF8574_SPEED 100000    // 100KHz

/**
 * @brief  设置风扇的状态
 * @param  status: true or false
 * @retval None
 */
void set_fan(uint8_t status);

/**
 * @brief  设置蜂鸣器的状态
 * @param  status: true or false
 * @retval None
 */
void set_buzzer(uint8_t status);

/**
 * @brief  设置LED灯的状态
 * @param  status: true or false
 * @retval None
 */
void set_led(uint8_t status);


/**
 * @brief PCF8574 写数据
 *
 * 向PCF8574芯片写1个字节的数据，只能写1个字节的数据。
 *
 * @param data 数据中的每一位代表引脚 高位->低位，P7->P0
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_Write(uint8_t data);

/**
 * @brief PCF8574 读数据
 *
 * 向PCF8574芯片读1个字节的数据，只能读1个字节的数据。
 *
 * @param data 数据中的每一位代表引脚 高位->低位，P7->P0
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_Read(uint8_t *recv_data);

/**
 * @brief 初始化 PCF8574
 *
 * @param
 * @return Returns {@link IOT_SUCCESS} 成功;
 *         Returns {@link IOT_FAILURE} 失败.
 */
uint32_t PCF8574_Init(void);

#endif
