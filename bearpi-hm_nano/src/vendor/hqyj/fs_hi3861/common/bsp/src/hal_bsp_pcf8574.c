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

#include "hi_i2c.h"
#include "hi_io.h"
#include "hi_errno.h"

#include "hal_bsp_structAll.h"
#include "hal_bsp_pcf8574.h"

tn_pcf8574_io_t tmp_io = {0}; // IO扩展芯片的引脚

void set_fan(uint8_t status)
{
    if (status == true) {
        tmp_io.bit.p0 = 1; // 风扇开启
        PCF8574_Write(tmp_io.all);
    } else if (status == false) {
        tmp_io.bit.p0 = 0; // 风扇关闭
        PCF8574_Write(tmp_io.all);
    }
}

void set_buzzer(uint8_t status)
{
    if (status == true) {
        tmp_io.bit.p1 = 0; // 蜂鸣器开启
        PCF8574_Write(tmp_io.all);
    } else if (status == false) {
        tmp_io.bit.p1 = 1; // 蜂鸣器关闭
        PCF8574_Write(tmp_io.all);
    }
}

void set_led(uint8_t status)
{
    if (status == true) {
        tmp_io.bit.p2 = 0; // LED开启
        PCF8574_Write(tmp_io.all);
    } else if (status == false) {
        tmp_io.bit.p2 = 1; // LED关闭
        PCF8574_Write(tmp_io.all);
    }
}

uint32_t PCF8574_Write(const uint8_t send_data)
{
    uint32_t result = 0;
    uint8_t buffer[] = {send_data};

    hi_i2c_data i2c_data = {0};
    i2c_data.send_buf = buffer;
    i2c_data.send_len = sizeof(buffer);

    result = hi_i2c_write(PCF8574_I2C_ID, PCF8574_I2C_ADDR, &i2c_data);
    if (result != HI_ERR_SUCCESS) {
        printf("I2C PCF8574 Write result is 0x%x!!!\r\n", result);
        return result;
    }
    return HI_ERR_SUCCESS;
}

uint32_t PCF8574_Read(uint8_t *recv_data)
{
    uint32_t result = 0;
    hi_i2c_data i2c_data = {0};
    i2c_data.receive_buf = recv_data;
    i2c_data.receive_len = 1;

    result = hi_i2c_read(PCF8574_I2C_ID, PCF8574_I2C_ADDR, &i2c_data);
    if (result != HI_ERR_SUCCESS) {
        printf("I2C PCF8574 Read result is 0x%x!!!\r\n", result);
        return result;
    }

    return HI_ERR_SUCCESS;
}

uint32_t PCF8574_Init(void)
{
    uint32_t result = 0;

    // gpio_9 复用为 I2C_SCL
    hi_io_set_pull(HI_IO_NAME_GPIO_9, HI_IO_PULL_UP);
    hi_io_set_func(HI_IO_NAME_GPIO_9, HI_IO_FUNC_GPIO_9_I2C0_SCL);
    // gpio_10 复用为 I2C_SDA
    hi_io_set_pull(HI_IO_NAME_GPIO_10, HI_IO_PULL_UP);
    hi_io_set_func(HI_IO_NAME_GPIO_10, HI_IO_FUNC_GPIO_10_I2C0_SDA);

    result = hi_i2c_init(PCF8574_I2C_ID, PCF8574_SPEED);
    if (result != HI_ERR_SUCCESS) {
        printf("I2C PCF8574 Init status is 0x%x!!!\r\n", result);
        return result;
    }
    printf("I2C PCF8574 Init is succeeded!!!\r\n");

    tmp_io.bit.p0 = 0; // 风扇关闭
    tmp_io.bit.p1 = 1; // 蜂鸣器关闭
    tmp_io.bit.p2 = 1; // LED关闭
    PCF8574_Write(tmp_io.all); // v4.2版本开发板 关闭风扇，关闭蜂鸣器，关闭LED灯
    return HI_ERR_SUCCESS;
}