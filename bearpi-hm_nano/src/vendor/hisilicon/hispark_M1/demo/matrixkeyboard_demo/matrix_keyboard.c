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

#include <stdlib.h>
#include <stdio.h>
#include <hi_io.h>
#include <hi_gpio.h>
#include <hi_task.h>
#include <hi_time.h>
#include <hi_early_debug.h>
#include <math.h>
#include <ohos_init.h>
#include <cmsis_os2.h>
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "iot_i2c.h"

#define ROW         (1)
#define COLUMN      (2)
#define MAX_SUM      (10)
#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define SSD1306_I2C_IDX 0

static unsigned int num_count = 0;
unsigned int  sum = 0;
unsigned int sum_count = 0;
unsigned int sum_count_2 = 0;
unsigned int cnt_sum_1 = 0;
char display_char[30] = { 0 }; // 30代表大小
unsigned int cnt_sum_2 = 0;
unsigned int cnt_sum_3 = 0;
unsigned char multiply = 0;
unsigned char divide = 0;
unsigned int addition = 0;

/* 行 */
IotGpioValue gpio_6_val = IOT_GPIO_VALUE0, gpio_7_val = IOT_GPIO_VALUE0;
IotGpioValue gpio_8_val = IOT_GPIO_VALUE0, gpio_9_val = IOT_GPIO_VALUE0;
/* 列 */
IotGpioValue gpio_0_val = IOT_GPIO_VALUE0, gpio_1_val = IOT_GPIO_VALUE0;
IotGpioValue gpio_2_val = IOT_GPIO_VALUE0, gpio_10_val = IOT_GPIO_VALUE0;

unsigned char l = 0, h = 0;

const unsigned char headSize[] = {64, 64};

/**
 * 汉字字模在线： https://www.23bei.com/tool-223.html
 * 数据排列：从左到右从上到下
 * 取模方式：横向8位左高位
 * 字体总类：[HZK1212宋体]
**/
void TempHumChinese(void)
{
    const uint32_t W = 16;
    uint8_t fonts[][32] = {
        {
            /* -- ID:0,字符:"计",ASCII编码:BCC6,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x00, 0x40, 0x20, 0x40, 0x10, 0x40, 0x10, 0x40, 0x00, 0x40, 0x00, 0x44, 0xF7, 0xFE, 0x10, 0x40,
            0x10, 0x40, 0x10, 0x40, 0x10, 0x40, 0x12, 0x40, 0x14, 0x40, 0x18, 0x40, 0x10, 0x40, 0x00, 0x40,
        }, {
            /* -- ID:1,字符:"算",ASCII编码:CBE3,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x20, 0x80, 0x3E, 0xFC, 0x49, 0x20, 0x9F, 0xF0, 0x10, 0x10, 0x1F, 0xF0, 0x10, 0x10, 0x1F, 0xF0,
            0x10, 0x10, 0x1F, 0xF0, 0x08, 0x24, 0xFF, 0xFE, 0x08, 0x20, 0x08, 0x20, 0x10, 0x20, 0x20, 0x20,
        }, {
            /* -- ID:2,字符:"器",ASCII编码:C6F7,对应字:宽x高=16x16,画布:宽W=16 高H=16,共32字节 */
            0x3E, 0xF8, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x3E, 0xF8, 0x01, 0x20, 0x01, 0x14, 0xFF, 0xFE,
            0x02, 0x80, 0x0C, 0x60, 0x30, 0x18, 0xFE, 0xFE, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x3E, 0xF8
        }
    };
    for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        ssd1306_DrawRegion((i + 1) * W, 0, W, fonts[i], sizeof(fonts[0])); // x轴坐标i + 2*w，y轴坐标3，宽度为16
    }
    ssd1306_UpdateScreen();
}

void keyboard_gpio_config(unsigned char mode)
{
    IoSetFunc(IOT_IO_NAME_GPIO_6, IOT_IO_FUNC_GPIO_6_GPIO);
    IoSetFunc(IOT_IO_NAME_GPIO_7, IOT_IO_FUNC_GPIO_7_GPIO);
    IoSetFunc(IOT_IO_NAME_GPIO_8, IOT_IO_FUNC_GPIO_8_GPIO);
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_GPIO);

    IoSetFunc(IOT_IO_NAME_GPIO_0, IOT_IO_FUNC_GPIO_0_GPIO);
    IoSetFunc(IOT_IO_NAME_GPIO_1, IOT_IO_FUNC_GPIO_1_GPIO);
    IoSetFunc(IOT_IO_NAME_GPIO_2, IOT_IO_FUNC_GPIO_2_GPIO);
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_GPIO);
    /* 初始化行输入，列输出 */
    if (mode == HI_TRUE) {
        IoTGpioSetDir(IOT_IO_NAME_GPIO_6, IOT_GPIO_DIR_IN);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_7, IOT_GPIO_DIR_IN);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_8, IOT_GPIO_DIR_IN);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_IN);

        IoTGpioSetDir(IOT_IO_NAME_GPIO_0, HI_GPIO_DIR_OUT);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_1, HI_GPIO_DIR_OUT);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_2, HI_GPIO_DIR_OUT);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_10, HI_GPIO_DIR_OUT);
    } else {
        IoTGpioSetDir(IOT_IO_NAME_GPIO_6, HI_GPIO_DIR_OUT);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_7, HI_GPIO_DIR_OUT);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_8, HI_GPIO_DIR_OUT);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_9, HI_GPIO_DIR_OUT);

        IoTGpioSetDir(IOT_IO_NAME_GPIO_0, IOT_GPIO_DIR_IN);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_1, IOT_GPIO_DIR_IN);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_2, IOT_GPIO_DIR_IN);
        IoTGpioSetDir(IOT_IO_NAME_GPIO_10, IOT_GPIO_DIR_IN);
    }
}
void gpio_init(void)
{
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_6, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_8, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
    
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0,  IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_1,  IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_2,  IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE0);
}
/* 行扫描 */
void row_scan_output(void)
{
    // 行输出置低
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_6, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_8, IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
    
    // 列输出置高
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0,  IOT_GPIO_VALUE1);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_1,  IOT_GPIO_VALUE1);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_2,  IOT_GPIO_VALUE1);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE1);
}
/* 列扫描 */
void column_scan_output(void)
{
    // 行输出置高
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_6, IOT_GPIO_VALUE1);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE1);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_8, IOT_GPIO_VALUE1);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);

    // 二次扫描 列输出置低
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0,  IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_1,  IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_2,  IOT_GPIO_VALUE0);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_10, IOT_GPIO_VALUE0);
}
void get_gpio_input_value(IotGpioValue* val_1, IotGpioValue* val_2, IotGpioValue* val_3,
                          IotGpioValue* val_4, unsigned char row_column_flag)
{
    if (row_column_flag == ROW) {
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_6, val_1);
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_7, val_2);
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_8, val_3);
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_9, val_4);
    }
    if (row_column_flag == COLUMN) {
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_0, val_1);
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_1, val_2);
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_2, val_3);
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_10, val_4);
    }
}
/* 行扫描 */
void key_press_line_scan(void)
{
    if ((gpio_6_val == 1) || (gpio_7_val == 1) || (gpio_8_val == 1) || (gpio_9_val == 1)) { // 检测按下的按键所在行
        if (gpio_6_val && !gpio_7_val && !gpio_8_val && !gpio_9_val) { // 返回行值
            h = 1;
        }
        if (gpio_7_val && !gpio_6_val && !gpio_8_val && !gpio_9_val) {
            h = 2; // 代表行按键第2个被按下
        }
        if (gpio_8_val && !gpio_6_val && !gpio_7_val && !gpio_9_val) {
            h = 3; // 代表行按键第3个被按下
        }
        if (gpio_9_val && !gpio_6_val && !gpio_7_val && !gpio_8_val) {
            h = 4; // 代表行按键第4个被按下
        }
    } else if (gpio_6_val==0 && gpio_7_val==0 && gpio_8_val==0 && gpio_9_val==0) { // 无按键按下
        h = 0;
    }
}
/* 列扫描 */
void key_press_column_scan(void)
{
    if ((gpio_0_val==1) || (gpio_1_val==1) || (gpio_2_val==1) || (gpio_10_val==1)) { // 检测按下的按键所在列
        /* 第一行 */
        if ((gpio_0_val == 1) && (h == 1)) {
            l = '+';
        } else if ((gpio_1_val == 1) && (h == 1)) {
            l = '-';
        } else if ((gpio_2_val == 1) && (h == 1)) {
            l = '*';
        } else if ((gpio_10_val == 1) && (h == 1)) {
            l = '/';
        } else if ((gpio_0_val == 1) && (h == 2)) {  /* 第二行 2 */
            l = 1;
        } else if ((gpio_1_val == 1) && (h == 2)) { // 第2行
            l = 2; // 代表行按键第2个被按下
        } else if ((gpio_2_val == 1) && (h == 2)) { // 第2行
            l = 3; // 代表行按键3被按下
        } else if ((gpio_10_val == 1) && (h == 2)) { // 第2行
            l = 'C';
        } else if ((gpio_0_val == 1) && (h == 3)) { /* 第3行 */
            l = 4; // 代表行按键4被按下
        } else if ((gpio_1_val == 1) && (h == 3)) { /* 第3行 */
            l = 5; // 代表行按键5被按下
        } else if ((gpio_2_val == 1) && (h ==3)) { /* 第3行 */
            l = 6; // 代表行按键6被按下
        } else if ((gpio_10_val == 1) && (h == 3)) { /* 第3行 */
            l = '0';
        } else if ((gpio_0_val == 1) && (h == 4) && (gpio_6_val != 1)) { /* 第4行 */
            l = 7; // 代表行按键7被按下
        } else if ((gpio_1_val == 1) && (h == 4) && (gpio_7_val != 1)) { /* 第4行 */
            l = 8; // 代表行按键8被按下
        } else if ((gpio_2_val == 1) && (h == 4) && (gpio_8_val != 1)) { /* 第4行 */
            l = 9; // 代表行按键9被按下
        } else if ((gpio_10_val == 1) && (h == 4)) { /* 第4行 */
            l = '=';
        }
    }
}

// 数字结果
void num_scan_results(void)
{
    int ret = 0;
    if (num_count < 9) { // 最多9位数字
        if (l == '0') {
            l = l - 0x30; // 0x30代表变为字符0
        }
        if (sum_count == 0) {
            sum = sum * 10 + l; // 10代表转换为数字
            cnt_sum_1 = sum;
            ret = snprintf_s(display_char, sizeof(display_char), sizeof(display_char), "%u", cnt_sum_1);
            if (ret == 0) {
                printf("cnt_sum_1 failed\r\n");
            }
            ssd1306_SetCursor(120 - (7 * strlen(display_char)), 20); // x轴坐标为120 - 7 * i，y轴坐标为20
            ssd1306_DrawString(display_char, Font_7x10, White);
        } else {
            cnt_sum_2 = cnt_sum_2 * 10 + l; // 10代表转换为数字
            ret = snprintf_s(display_char, sizeof(display_char), sizeof(display_char), "%u", cnt_sum_2);
            if (ret == 0) {
                printf("cnt_sum_2 failed\r\n");
            }
            ssd1306_SetCursor(120 - (7 * strlen(display_char)), 40); // x轴坐标为120 - 7 * i，y轴坐标为40
            ssd1306_DrawString(display_char, Font_7x10, White);
        }
        ssd1306_UpdateScreen();
        num_count++;
        if (num_count > MAX_SUM) {
            num_count = 0;
        }
    }
}

// 运算符
void operator_scan_results(void)
{
    /* 加法 */
    if (l == '+') {
        ssd1306_SetCursor(110, 30); // x轴坐标为110，y轴坐标为30
        ssd1306_DrawString("+", Font_7x10, White);
        num_count = 0;
        sum_count = 1;
        addition = 1;
    } else if ((l == '-')) {  /* 减法 */
        ssd1306_SetCursor(110, 30); //  x轴坐标为110，y轴坐标为30
        ssd1306_DrawString("-", Font_7x10, White);
        num_count = 0;
        sum_count = 1;
        sum_count_2 = 1;
    } else if (l == '*') { /* 乘法 */
        ssd1306_SetCursor(110, 30); // x轴坐标为110，y轴坐标为30
        ssd1306_DrawString("*", Font_7x10, White);
        num_count = 0;
        sum_count = 1;
        multiply = 1;
    } else if (l == '/') { /* 除法 */
        ssd1306_SetCursor(110, 30); // x轴坐标为110，y轴坐标为30
        ssd1306_DrawString("/", Font_7x10, White);
        num_count = 0;
        sum_count = 1;
        divide = 1;
    }
}

void Calculation_results(void)
{
    int ret = 0;
    /* 加法结果 */
    if ((l == '=') && (addition == 1)) {
        sum_count = 0;
        addition = 0;
        cnt_sum_3 = cnt_sum_1 + cnt_sum_2;
        ret = snprintf_s(display_char, sizeof(display_char), sizeof(display_char), "= %u", cnt_sum_3);
        if (ret == 0) {
            printf("cnt_sum_3 failed\r\n");
        }
        ssd1306_SetCursor(120 - (7 * strlen(display_char)), 50); // x轴坐标为120 - (7 * strlen(cnt_sum_3))，y轴坐标为50
        ssd1306_DrawString(display_char, Font_7x10, White);
    }
    /* 减法结果 */
    if ((l == '=') && (sum_count_2 == 1)) {
        sum_count = 0;
        sum_count_2 = 0;
        cnt_sum_3 = cnt_sum_1 - cnt_sum_2;
        ret = snprintf_s(display_char, sizeof(display_char), sizeof(display_char), "= %u", cnt_sum_3);
        if (ret == 0) {
            printf("cnt_sum_3 failed\r\n");
        }
        ssd1306_SetCursor(120 - (7 * strlen(display_char)), 50); // x轴坐标为120 - (7 * strlen(cnt_sum_3))，y轴坐标为50
        ssd1306_DrawString(display_char, Font_7x10, White);
    }
    /* 乘法结果 */
    if ((l == '=') && (multiply == 1)) {
        sum_count = 0;
        multiply = 0;
        cnt_sum_3 = cnt_sum_1 * cnt_sum_2;
        ret = snprintf_s(display_char, sizeof(display_char), sizeof(display_char), "= %u", cnt_sum_3);
        if (ret == 0) {
            printf("cnt_sum_3 failed\r\n");
        }
        ssd1306_SetCursor(120 - (7 * strlen(display_char)), 50); // x轴坐标为120 - (7 * strlen(cnt_sum_3))，y轴坐标为50
        ssd1306_DrawString(display_char, Font_7x10, White);
    }
    /* 除法结果 */
    if ((l == '=') && (divide == 1)) {
        sum_count = 0;
        divide = 0;
        float float_sum = (float)cnt_sum_1 / (float)cnt_sum_2;
        ret = snprintf_s(display_char, sizeof(display_char), sizeof(display_char), "= %f", float_sum);
        if (ret == 0) {
            printf("cnt_sum_3 failed\r\n");
        }
        ssd1306_SetCursor(120 - (7 * strlen(display_char)), 50); // x轴坐标为120 - (7 * strlen(cnt_sum_3))，y轴坐标为50
        ssd1306_DrawString(display_char, Font_7x10, White);
    }
}

/* 行列扫描结果 */
void row_column_scan_results(void)
{
    if (l > 0) {
        if ((l != '*') && (l != 'C') && (l != '+') && (l != '-') && (l != '/') && (l != '=')) {
            num_scan_results();
        } else {
            operator_scan_results();
            Calculation_results();
            if (l == 'C') {
                sum = 0;
                cnt_sum_1 = 0;
                cnt_sum_2 = 0;
                cnt_sum_3 = 0;
                num_count = 0;
                sum_count = 0;
                divide = 0;
                addition = 0;
                ssd1306_ClearOLED();
                TempHumChinese();
            }
        }
        ssd1306_UpdateScreen();
    }
}

void OledFGpioInit(void)
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
    ssd1306_Init();
    ssd1306_ClearOLED();
}

void key_scan(void)
{
    OledFGpioInit();
    TempHumChinese();
    while (1) {
        /* 行 */
        gpio_6_val = IOT_GPIO_VALUE0, gpio_7_val = IOT_GPIO_VALUE0;
        gpio_8_val = IOT_GPIO_VALUE0, gpio_9_val = IOT_GPIO_VALUE0;
        /* 列 */
        gpio_0_val = IOT_GPIO_VALUE0, gpio_1_val = IOT_GPIO_VALUE0;
        gpio_2_val = IOT_GPIO_VALUE0, gpio_10_val = IOT_GPIO_VALUE0;
        l = 0, h = 0;
        // 支持连按
        /* gpio init */
        gpio_init();
        /* row output/input init */
        keyboard_gpio_config(HI_TRUE);
        /* 行扫描 */
        TaskMsleep(20); // 延时20ms，去抖动
        row_scan_output();
        /* 获取按键按下的每行的值 */
        get_gpio_input_value(&gpio_6_val, &gpio_7_val, &gpio_8_val, &gpio_9_val, ROW);
        key_press_line_scan();
        gpio_init();
        /* row output/input init */
        keyboard_gpio_config(HI_FALSE);
        /* 列扫描 */
        TaskMsleep(20); // 延时20ms，去抖动
        column_scan_output();
        /* 获取按键按下的每列的值 */
        get_gpio_input_value(&gpio_0_val, &gpio_1_val, &gpio_2_val, &gpio_10_val, COLUMN);
        key_press_column_scan();
        row_column_scan_results();
        TaskMsleep(100); // 延时100ms,循环读取
    }
}

void keyboard_task(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "keyboardtask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 任务栈大小为5 *1024
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)key_scan, NULL, &attr) == NULL) {
        printf("[keyboardtask] Failed to create BalanceTask!\n");
    }
}

APP_FEATURE_INIT(keyboard_task);