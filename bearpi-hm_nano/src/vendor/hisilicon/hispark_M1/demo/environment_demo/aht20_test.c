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

#include <hi_early_debug.h>
#include <hi_i2c.h>
#include <hi_task.h>
#include <hi_time.h>
#include <hi_stdlib.h>
#include <hi_errno.h>
#include <hi_io.h>
#include <ohos_init.h>
#include <cmsis_os2.h>
#include <iot_gpio.h>
#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "iot_i2c.h"
#include "aht20.h"

#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define SSD1306_I2C_IDX 0

const unsigned char headSize[] = { 64, 64 };

/**
 * 汉字字模在线： https://www.23bei.com/tool-223.html
 * 数据排列：从左到右从上到下
 * 取模方式：横向8位左高位
 * 字体总类：[HZK1616宋体]
**/
void TempHumChinese(void)
{
    const uint32_t W = 16;
    uint8_t fonts[][32] = {
        {
           /* -- ID:0,字符:"温",ASCII编码:CEC2,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x00, 0x08, 0x43, 0xFC, 0x32, 0x08, 0x12, 0x08, 0x83, 0xF8, 0x62, 0x08, 0x22, 0x08, 0x0B, 0xF8,
            0x10, 0x00, 0x27, 0xFC, 0xE4, 0xA4, 0x24, 0xA4, 0x24, 0xA4, 0x24, 0xA4, 0x2F, 0xFE, 0x20, 0x00
        }, {
            /* -- ID:0,字符:"度",ASCII编码:B6C8,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x01, 0x00, 0x00, 0x84, 0x3F, 0xFE, 0x22, 0x20, 0x22, 0x28, 0x3F, 0xFC, 0x22, 0x20, 0x23, 0xE0,
            0x20, 0x00, 0x2F, 0xF0, 0x22, 0x20, 0x21, 0x40, 0x20, 0x80, 0x43, 0x60, 0x8C, 0x1E, 0x30, 0x04
        }
    };
    uint8_t fonts2[][32] = {
        {
           /* -- ID:0,字符:"湿",ASCII编码:CEC2,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x00, 0x08, 0x47, 0xFC, 0x34, 0x08, 0x14, 0x08, 0x87, 0xF8, 0x64, 0x08, 0x24, 0x08, 0x0F, 0xF8,
            0x11, 0x20, 0x21, 0x20, 0xE9, 0x24, 0x25, 0x28, 0x23, 0x30, 0x21, 0x24, 0x3F, 0xFE, 0x20, 0x00
        }, {
            /* -- ID:0,字符:"度",ASCII编码:B6C8,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x01, 0x00, 0x00, 0x84, 0x3F, 0xFE, 0x22, 0x20, 0x22, 0x28, 0x3F, 0xFC, 0x22, 0x20, 0x23, 0xE0,
            0x20, 0x00, 0x2F, 0xF0, 0x22, 0x20, 0x21, 0x40, 0x20, 0x80, 0x43, 0x60, 0x8C, 0x1E, 0x30, 0x04
        }
    };
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        ssd1306_DrawRegion(i * W, 3, W, fonts[i], sizeof(fonts[0])); // x轴坐标i*w，y轴坐标3，宽度为16
    }
    for (size_t j = 0; j < sizeof(fonts2) / sizeof(fonts2[0]); j++) {
        ssd1306_DrawRegion(j * W, 35, W, fonts2[j], sizeof(fonts2[0])); // x轴坐标i*w，y轴坐标35，宽度为16
    }
}


void Aht20TestTask(void)
{
    uint32_t retval = 0;
    float temp = 0.0f;
    float humi = 0.0f;
    static char line[32] = {0};

    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
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

    unsigned int time1 = 20000;
    usleep(time1);
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    while (AHT20_Calibrate() != HI_ERR_SUCCESS) {
        printf("AHT20 sensor init failed!\r\n");
        usleep(1000); // 1ms = 1000
    }
    while (1) {
        TempHumChinese();
        retval = AHT20_StartMeasure();
        printf("AHT20_StartMeasure: %d\r\n", retval);

        retval = AHT20_GetMeasureResult(&temp, &humi);
        if (retval != HI_ERR_SUCCESS) {
            printf("get humidity data failed!\r\n");
        }
        printf("AHT20_GetMeasureResult: %d, temp = %.2f, humi = %.2f\r\n", retval, temp, humi);
        ssd1306_SetCursor(32, 8); /* x坐标为32，y轴坐标为8 */

        int ret = snprintf_s(line, sizeof(line), sizeof(line), ": %.2f", temp);
        if (ret == 0) {
            printf("temp failed\r\n");
        }
        ssd1306_DrawString(line, Font_7x10, White);

        ret = snprintf_s(line, sizeof(line), sizeof(line), ": %.2f", humi);
        if (ret == 0) {
            printf("humi failed\r\n");
        }
        ssd1306_SetCursor(32, 40); /* x坐标为32，y轴坐标为40 */
        ssd1306_DrawString(line, Font_7x10, White);
        ssd1306_UpdateScreen();
        sleep(1);
    }
}

void Aht20Test(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "Aht20TestTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 任务栈大小为5 *1024
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)Aht20TestTask, NULL, &attr) == NULL) {
        printf("[Aht20TestTask] Failed to create BalanceTask!\n");
    }
}

APP_FEATURE_INIT(Aht20Test);