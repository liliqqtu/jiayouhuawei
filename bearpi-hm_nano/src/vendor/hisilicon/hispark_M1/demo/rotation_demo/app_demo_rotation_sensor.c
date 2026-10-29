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

#include <stdio.h>
#include <hi_adc.h>
#include <hi_gpio.h>
#include <hi_io.h>
#include <hi_stdlib.h>
#include <hi_pwm.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio_ex.h"
#include "iot_gpio.h"
#include "ssd1306.h"
#include "iot_adc.h"
#include "iot_i2c.h"

#define ADC_LENGTH    (20)
#define VLT_MIN    (100)
#define ADC_READ_DATA    (110)
#define IOT_PWM_PORT_PWM1   1
#define IOT_FREQ            4000
#define IOT_DUTY            50
#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define SSD1306_I2C_IDX 0

void rotation_gpio_init(void)
{
    /*
     * 设置GPIO13的管脚复用关系为I2C0_SDA
     * Set the pin reuse relationship of GPIO13 to I2C0_ SDA
     */
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
    /*
     * 设置GPIO14的管脚复用关系为I2C0_SCL
     * Set the pin reuse relationship of GPIO14 to I2C0_ SCL
     */
        IoTI2cInit(SSD1306_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    /*
     * 设置I2C设备0的波特率为400k
     * Set the baud rate of I2C device 0 to 400k
     */
    IoTI2cSetBaudrate(SSD1306_I2C_IDX, IOT_I2C_IDX_BAUDRATE);
    IoTGpioInit(IOT_IO_NAME_GPIO_10);
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_PWM1_OUT);
    IoTGpioSetDir(IOT_IO_NAME_GPIO_10, IOT_GPIO_DIR_OUT);
    // 初始化PWM1 Initialize PWM1
    IoTPwmInit(IOT_PWM_PORT_PWM1);
    IoSetFunc(IOT_IO_NAME_GPIO_7, IOT_IO_FUNC_GPIO_7_GPIO);
    IoTGpioSetDir(IOT_IO_NAME_GPIO_7, IOT_GPIO_DIR_IN);
    IoSetPull(IOT_IO_NAME_GPIO_7, IOT_IO_PULL_UP);
}

void all_light_out(void)
{
    IoTPwmStart(IOT_PWM_PORT_PWM1, IOT_DUTY, IOT_FREQ);
}

/**
 * @bref The numerical change of the process from dark (weak light) to daytime (strong light) in LED environment,
 * 1920 is the maximum range
 * @param unsigned short data
*/
void sensor_all_light_dark_to_bright(unsigned short data)
{
    unsigned short pstr = data / 1875.0 * 100; // 1875.0为ADC读取最大值，*100为了保证数据在0-99之间
    IoTPwmStart(IOT_PWM_PORT_PWM1, pstr, IOT_FREQ);
}


/**
 * @berf Electrodeless dimming
 * @param void
*/
void colorful_light_stepless_dimming(void)
{
    int ret = 0;
    float voltage;
    unsigned short data = 0;
    unsigned char vstr[64] = {0}; // 64为大小
    unsigned char ratio[64] = {0}; // 64为大小
    ret = AdcRead(IOT_ADC_CHANNEL_3, &data, IOT_ADC_EQU_MODEL_4, IOT_ADC_CUR_BAIS_DEFAULT, 0xFF);
    if (ret != HI_ERR_SUCCESS) {
        printf("ADC Read Fail\n");
        return HI_NULL;
    }
    voltage = (float)data * 1.8 * 4 / 4096.0;  /* vlt * 1.8 * 4 / 4096.0 is to convert codeword to voltage */
    printf("data: %hu, voltage = %0.f\n", data, voltage);
    float pstr = data / 1875.0 * 100; // 1875.0为ADC读取最大值，*100为了保证数据在0-99之间
    ssd1306_SetCursor(10, 8); // 10为X轴坐标，8为Y轴坐标
    ret = snprintf_s(ratio, sizeof(ratio), sizeof(ratio), "voltage: %.1f V", voltage);
    if (ret == 0) {
        printf("voltage failed\r\n");
    }
    ssd1306_DrawString(ratio, Font_7x10, White);
    ssd1306_SetCursor(0, 40); // 横坐标为0，纵坐标为40
    ret = snprintf_s(vstr, sizeof(vstr), sizeof(vstr), "Duty cycle: %0.f", pstr);
    if (ret == 0) {
        printf("Duty cycle failed\r\n");
    }
    ssd1306_DrawString(vstr, Font_7x10, White);
    ssd1306_UpdateScreen();
    if (data > ADC_READ_DATA) {
        ssd1306_Fill(Black);
        sensor_all_light_dark_to_bright(data);
    }
}

/*
Use the key to control the brightness of the white light
*/
void brightness_control_sample(void)
{
    rotation_gpio_init();
    ssd1306_Init();
    ssd1306_Fill(Black);
    all_light_out();
    while (1) {
        colorful_light_stepless_dimming();
        TaskMsleep(100); // 延时100ms
    }
}

void app_demo_rotation_task(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "brightnessTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 堆栈大小5*1024，stack size 5*1024
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)brightness_control_sample, NULL, &attr) == NULL) {
        printf("[brightnessTask] Failed to create LSM6DSTask!\n");
    }
}

APP_FEATURE_INIT(app_demo_rotation_task);
