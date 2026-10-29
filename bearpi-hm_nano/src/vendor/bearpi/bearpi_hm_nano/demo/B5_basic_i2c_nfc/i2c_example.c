/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
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
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "iot_errno.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_i2c.h"
#include "nfc.h"
#include "ohos_init.h"

#define I2C_TASK_STACK_SIZE (1024 * 8)
#define I2C_TASK_PRIO 25

#define TEXT "Welcome to BearPi-HM_Nano!"
#define WEB "openharmony.cn"

#define WIFI_IOT_IO_FUNC_GPIO_0_I2C1_SDA 6
#define WIFI_IOT_IO_FUNC_GPIO_1_I2C1_SCL 6
#define WIFI_IOT_I2C_IDX 1
#define WIFI_IOT_I2C_BAUDRATE 400000
#define TASK_DELAY_1S 1000000

/**
 * @brief i2c task writes data to NFC tag
 *
 */
static void I2cTask(void)
{
    uint8_t ret;

    // GPIO_0 multiplexed to I2C1_SDA
    IoTGpioInit(0);
    IoTGpioSetFunc(0, WIFI_IOT_IO_FUNC_GPIO_0_I2C1_SDA);

    // GPIO_1 multiplexed to I2C1_SCL
    IoTGpioInit(1);
    IoTGpioSetFunc(1, WIFI_IOT_IO_FUNC_GPIO_1_I2C1_SCL);

    // baudrate: 400kbps
    IoTI2cInit(WIFI_IOT_I2C_IDX, WIFI_IOT_I2C_BAUDRATE);

    printf("I2C Test Start\n");

    ret = storeText(NDEFFirstPos, (uint8_t *)TEXT);
    if (ret != 1) {
        printf("NFC Write Data Falied :%d \n", ret);
    }
    ret = storeUrihttp(NDEFLastPos, (uint8_t *)WEB);
    if (ret != 1) {
        printf("NFC Write Data Falied :%d \n", ret);
    }
    while (1) {
        printf("=======================================\n");
        printf("***********I2C_NFC_example**********\n");
        printf("=======================================\n");
        printf("Please use the mobile phone with NFC function close to the development board!\n");
        usleep(TASK_DELAY_1S);
    }
}

/**
 * @brief Main Entry of the I2c Example
 *
 */
static void I2cExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "I2cTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = I2C_TASK_STACK_SIZE;
    attr.priority = I2C_TASK_PRIO;

    if (osThreadNew((osThreadFunc_t)I2cTask, NULL, &attr) == NULL) {
        printf("Falied to create I2cTask!\n");
    }
}

APP_FEATURE_INIT(I2cExampleEntry);