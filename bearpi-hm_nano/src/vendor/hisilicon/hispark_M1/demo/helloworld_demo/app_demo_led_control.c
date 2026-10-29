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

#include <hi_gpio.h>
#include <hi_early_debug.h>
#include <hi_io.h>
#include <hi_time.h>
#include <hi_watchdog.h>
#include <hi_task.h>
#include <ohos_init.h>
#include <cmsis_os2.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "iot_i2c.h"

#define LED_LIGHT_DELAY_1S       (1000)
#define LED_CONTROL_TASK_SLEEP_20MS (20)
#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define SSD1306_I2C_IDX 0

void gpioInit(void)
{
    /*
     * 初始化I2C设备0，并指定波特率为400k
     * Initialize I2C device 0 and specify the baud rate as 400k
     */
    IoTI2cInit(SSD1306_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /*
     * 设置I2C设备0的波特率为400k
     * Set the baud rate of I2C device 0 to 400k
     */
    IoTI2cSetBaudrate(SSD1306_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /*
     * 设置GPIO13的管脚复用关系为I2C0_SDA
     * Set the pin reuse relationship of GPIO13 to I2C0_ SDA
     */
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    /*
     * 设置GPIO14的管脚复用关系为I2C0_SCL
     * Set the pin reuse relationship of GPIO14 to I2C0_ SCL
     */
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
}

/*
 * @berf led control
 * @param hi_void
 * @return null
*/
void led_control(void)
{
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_GPIO);
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
    TaskMsleep(LED_LIGHT_DELAY_1S);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
    TaskMsleep(LED_LIGHT_DELAY_1S);
}

void led_control_demo(void)
{
    gpioInit();
    ssd1306_Init();
    ssd1306_ClearOLED();
    // ssd1306_printf("Hello World");
    ssd1306_SetCursor(25, 10); // x轴坐标为25，y轴坐标为10
    ssd1306_DrawString("Hello World", Font_7x10, White);
    ssd1306_UpdateScreen();
    while (1) {
        led_control();
        /* Release CPU resources for 20ms */
        hi_sleep(LED_CONTROL_TASK_SLEEP_20MS);
    }
}

void app_demo_led_control_task(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "ledTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 任务栈大小为5 *1024
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)led_control_demo, NULL, &attr) == NULL) {
        printf("[ledTask] Failed to create BalanceTask!\n");
    }
}

APP_FEATURE_INIT(app_demo_led_control_task);