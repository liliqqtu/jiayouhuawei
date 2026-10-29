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

#include "hal_bsp_key.h"
#include "hi_io.h"
#include "hi_gpio.h"

/**
 * @brief  按键初始化函数
 * @note   并且有中断回调函数
 * @retval None
 */
void KEY_Init(void)
{
    hi_gpio_init();                               // GPIO初始化
    hi_io_set_pull(KEY, HI_IO_PULL_UP);           // 设置GPIO上拉
    hi_io_set_func(KEY, HI_IO_FUNC_GPIO_14_GPIO); // 设置IO14为GPIO功能
    hi_gpio_set_dir(KEY, HI_GPIO_DIR_IN);         // 设置GPIO为输入模式
}

/**
 * @brief  获取按键此时的状态值
 * @note
 * @retval
 */
uint8_t KEY_Get_Input_Value(void)
{
    hi_gpio_value val;
    hi_gpio_get_input_val(KEY, &val);
    return val;
}
