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

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "hal_bsp_pcf8574.h"

#define KEY HI_IO_NAME_GPIO_14 // WiFi模组的IO14引脚

osThreadId_t Task1_ID = 0;   //  任务1 ID
hi_gpio_value val, val_last; // GPIO的状态值
#define TASK_STACK_SIZE 1024
#define TASK_DELAY_TIME (200 * 1000)

/**
 * @brief 按键中断回调函数
 * @note   当按键按下的时候才会触发
 * @param  *arg:
 * @retval None
 */
hi_void gpio_callback(void)
{
    printf("key down...\r\n");
    hi_gpio_get_input_val(KEY, &val); // 获取GPIO引脚的状态
    printf("gpio_callback: %s\r\n", val ? "HI_GPIO_VALUE_1" : "HI_GPIO_VALUE_0");
}
/**
 * @description: 任务1
 * @param {*}
 * @return {*}
 */
void Task1(void)
{
    printf("enter Task 1.......\r\n");
    hi_gpio_init();                                            // GPIO初始化
    hi_io_set_pull(KEY, HI_IO_PULL_UP);                        // 设置GPIO上拉
    hi_io_set_func(KEY, HI_IO_FUNC_GPIO_14_GPIO);              // 设置IO14为GPIO功能
    hi_gpio_set_dir(KEY, HI_GPIO_DIR_IN);                      // 设置GPIO为输入模式
    hi_gpio_register_isr_function(KEY,                         // KEY按键引脚
                                  HI_INT_TYPE_EDGE,            // 下降沿检测
                                  HI_GPIO_EDGE_FALL_LEVEL_LOW, // 低电平时触发
                                  &gpio_callback,              // 触发后调用的回调函数
                                  NULL);                       // 回调函数的传参值
    while (1) {
        hi_gpio_get_input_val(KEY, &val); // 获取GPIO引脚的状态
        if (val != val_last) {
            // 当GPIO状态改变的时候, 打印输出
            printf("keyValue: %s\r\n", val ? "HI_GPIO_VALUE_1" : "HI_GPIO_VALUE_0");
            val_last = val;
        }
        // 200ms一检测
        usleep(TASK_DELAY_TIME); // 200ms sleep
    }
}

/**
 * @description: 初始化并创建任务
 * @param {*}
 * @return {*}
 */
static void base_key_demo(void)
{
    printf("[demo] Enter base_key_demo()!\r\n");

    PCF8574_Init(); // 初始化IO扩展芯片

    osThreadAttr_t taskOptions;
    taskOptions.name = "Task1";              // 任务的名字
    taskOptions.attr_bits = 0;               // 属性位
    taskOptions.cb_mem = NULL;               // 堆空间地址
    taskOptions.cb_size = 0;                 // 堆空间大小
    taskOptions.stack_mem = NULL;            // 栈空间地址
    taskOptions.stack_size = TASK_STACK_SIZE;           // 栈空间大小 单位:字节
    taskOptions.priority = osPriorityNormal; // 任务的优先级:wq

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &taskOptions); // 创建任务1
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!\r\n", Task1_ID);
    }
}
SYS_RUN(base_key_demo);
