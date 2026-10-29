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

#ifndef HAL_BSP_KEY_H
#define HAL_BSP_KEY_H

#include "cmsis_os2.h"

#define KEY HI_IO_NAME_GPIO_14 // WiFi模组的IO14引脚

/**
 * @brief  按键初始化函数
 * @note
 * @retval None
 */
void KEY_Init(void);

/**
 * @brief  获取按键此时的状态值
 * @note
 * @retval
 */
uint8_t KEY_Get_Input_Value(void);

#endif
