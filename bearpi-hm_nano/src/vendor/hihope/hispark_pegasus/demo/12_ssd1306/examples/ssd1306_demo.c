/*
 * Copyright (C) 2021 HiHope Open Source Organization .
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
 *
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_i2c.h"
#include "iot_errno.h"
#include "iot_pwm.h"
#include "hi_io.h"

#include "ssd1306.h"
#include "ssd1306_tests.h"

#define OLED_I2C_BAUDRATE (400 * 1000)
#define STACK_SIZE        (10240)
#define DELAY_1S          (1)
#define DELAY_2S          (2)
#define DELAY_25MS        (25)
#define NUMBER_2          (2)
#define NUMBER_100        (100)
#define MAX_COUNT         (20)

void TestGetTick(void)
{
    uint32_t tick = NUMBER_100 * NUMBER_100;  // 10 * 1000;
    for (int i = 0; i < MAX_COUNT; i++) {
        usleep(tick);
        printf("HAL_GetTick(): %d\r\n", HAL_GetTick());
    }

    for (int i = 0; i < MAX_COUNT; i++) {
        HAL_Delay(DELAY_25MS);
        printf(" HAL_GetTick(): %d\r\n", HAL_GetTick());
    }
}


/**
 * 汉字字模在线： https://www.23bei.com/tool-223.html
 * 数据排列：从左到右从上到下
 * 取模方式：横向8位左高位
**/
void TestDrawChinese1(void)
{
    const uint32_t W = 16, H = 16;
    uint8_t fonts[][32] = {
        {
            /* -- ID:0,字符:"你",ASCII编码:C4E3,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x23, 0xFC, 0x22, 0x04, 0x64, 0x08, 0xA8, 0x40, 0x20, 0x40,
            0x21, 0x50, 0x21, 0x48, 0x22, 0x4C, 0x24, 0x44, 0x20, 0x40, 0x20, 0x40, 0x21, 0x40, 0x20, 0x80,
        }, {
            /* -- ID:1,字符:"好",ASCII编码:BAC3,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x10, 0x00, 0x11, 0xFC, 0x10, 0x04, 0x10, 0x08, 0xFC, 0x10, 0x24, 0x20, 0x24, 0x24, 0x27, 0xFE,
            0x24, 0x20, 0x44, 0x20, 0x28, 0x20, 0x10, 0x20, 0x28, 0x20, 0x44, 0x20, 0x84, 0xA0, 0x00, 0x40,
        }, {
            /* -- ID:2,字符:"鸿",ASCII编码:BAE8,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x40, 0x20, 0x30, 0x48, 0x10, 0xFC, 0x02, 0x88, 0x9F, 0xA8, 0x64, 0x88, 0x24, 0xA8, 0x04, 0x90,
            0x14, 0x84, 0x14, 0xFE, 0xE7, 0x04, 0x3C, 0x24, 0x29, 0xF4, 0x20, 0x04, 0x20, 0x14, 0x20, 0x08,
        }, {
            /* -- ID:3,字符:"蒙",ASCII编码:C3C9,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x04, 0x48, 0x7F, 0xFC, 0x04, 0x40, 0x7F, 0xFE, 0x40, 0x02, 0x8F, 0xE4, 0x00, 0x00, 0x7F, 0xFC,
            0x06, 0x10, 0x3B, 0x30, 0x05, 0xC0, 0x1A, 0xA0, 0x64, 0x90, 0x18, 0x8E, 0x62, 0x84, 0x01, 0x00,
        }
    };

    ssd1306_Fill(Black);
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        ssd1306_DrawRegion(i * W, 0, W, H, fonts[i], sizeof(fonts[0]), W);
    }
    ssd1306_UpdateScreen();
    sleep(DELAY_1S);
}

void TestDrawChinese2(void)
{
    const uint32_t W = 12, H = 12, S = 16;
    uint8_t fonts[][24] = {
        {
            /* -- ID:0,字符:"你",ASCII编码:C4E3,对应字:宽x高=12x12,画布:宽W=16 高H=12,共24字节 */
            0x12, 0x00, 0x12, 0x00, 0x27, 0xF0, 0x24, 0x20, 0x69, 0x40, 0xA1, 0x00, 0x25, 0x40, 0x25, 0x20,
            0x29, 0x10, 0x31, 0x10, 0x25, 0x00, 0x22, 0x00,
        }, {
            /* -- ID:1,字符:"好",ASCII编码:BAC3,对应字:宽x高=12x12,画布:宽W=16 高H=12,共24字节 */
            0x20, 0x00, 0x27, 0xE0, 0x20, 0x40, 0xF8, 0x80, 0x48, 0x80, 0x48, 0xA0, 0x57, 0xF0, 0x50, 0x80,
            0x30, 0x80, 0x28, 0x80, 0x4A, 0x80, 0x81, 0x00,
        }, {
            /* -- ID:2,字符:"鸿",ASCII编码:BAE8,对应字:宽x高=12x12,画布:宽W=16 高H=12,共24字节 */
            0x00, 0x40, 0x80, 0x80, 0x5D, 0xE0, 0x09, 0x20, 0xC9, 0xA0, 0x09, 0x60, 0x29, 0x00, 0xCD, 0xF0,
            0x58, 0x10, 0x43, 0xD0, 0x40, 0x10, 0x40, 0x60,
        }, {
            /* -- ID:3,字符:"蒙",ASCII编码:C3C9,对应字:宽x高=12x12,画布:宽W=16 高H=12,共24字节 */
            0x09, 0x00, 0x7F, 0xE0, 0x09, 0x00, 0x7F, 0xF0, 0x80, 0x10, 0x7F, 0xE0, 0x0C, 0x40, 0x32, 0x80,
            0xC7, 0x00, 0x0A, 0x80, 0x32, 0x70, 0xC6, 0x20
        }
    };

    ssd1306_Fill(Black);
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        ssd1306_DrawRegion(i * H, 0, W, H, fonts[i], sizeof(fonts[0]), S);
    }
    ssd1306_UpdateScreen();
    sleep(DELAY_1S);
}

void Ssd1306TestTask(void)
{
    IoTGpioInit(HI_IO_NAME_GPIO_13);
    IoTGpioInit(HI_IO_NAME_GPIO_14);

    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);
    
    IoTI2cInit(0, OLED_I2C_BAUDRATE);

    // WatchDogDisable();

    // usleep(20 * 1000);
    usleep(NUMBER_2 * NUMBER_100 * NUMBER_100);
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("Hello HiHope!", Font_7x10, White);

    // usleep(2000 * 1000);
    sleep(DELAY_2S);

    uint32_t start = HAL_GetTick();
    ssd1306_UpdateScreen();
    uint32_t end = HAL_GetTick();
    printf("ssd1306_UpdateScreen time cost: %u ms.\r\n", end - start);

    TestDrawChinese1();
    TestDrawChinese2();

    TestGetTick();
    int testCount = 100; // test 100 times
    while (testCount--) {
        ssd1306_TestAll();
        // usleep(10000);
        usleep(NUMBER_100 * NUMBER_100);
    }
}

void Ssd1306TestDemo(void)
{
    osThreadAttr_t attr;

    attr.name = "Ssd1306Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(Ssd1306TestTask, NULL, &attr) == NULL) {
        printf("[Ssd1306TestDemo] Falied to create Ssd1306TestTask!\n");
    }
}
APP_FEATURE_INIT(Ssd1306TestDemo);