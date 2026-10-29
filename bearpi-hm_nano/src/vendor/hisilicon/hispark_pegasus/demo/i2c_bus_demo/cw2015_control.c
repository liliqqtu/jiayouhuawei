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
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio_ex.h"
#include "iot_i2c.h"
#include "iot_gpio.h"
#include "iot_errno.h"
#include "iot_watchdog.h"
#include "hi_errno.h"
#include "hi_i2c.h"
#include "ssd1306.h"

#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define CW2015_I2C_IDX 0
#define IOT_PWM_PORT_PWM3   3
#define CW2015_READ_ADDR     (0xC5)
#define CW2015_WRITE_ADDR    (0xC4)
#define WRITELEN  2
#define CW2015_HIGHT_REGISTER 0x02
#define CW2015_LOW_REGISTER   0x03
#define CW2015_WAKE_REGISTER  0x0A
#define DELYA_US20            20

/*
 * @berf i2c read
 * @param hi_u8 reg_high_8bit_cmd:Transmit register value 8 bits high
 * @param hi_u8 reg_low_8bit_cmd:Transmit register value low 8 bits
 * @param hi_u8* recv_data:Receive data buff
 * @param hi_u8 send_len:Sending data length
 * @param hi_u8 read_len:Length of received data
 */
uint32_t Cw20_WriteRead(uint8_t reg_high_8bit_cmd, uint8_t send_len, uint8_t read_len)
{
    uint32_t status = 0;
    uint32_t ret = 0;
    uint8_t recvData[888] = { 0 };
    hi_i2c_data i2c_write_cmd_addr = { 0 };
    uint8_t send_user_cmd[1] = {reg_high_8bit_cmd};
    memset(recvData, 0x0, sizeof(recvData));
    i2c_write_cmd_addr.send_buf = send_user_cmd;
    i2c_write_cmd_addr.send_len = send_len;

    i2c_write_cmd_addr.receive_buf = recvData;
    i2c_write_cmd_addr.receive_len = read_len;

    status = hi_i2c_writeread(CW2015_I2C_IDX, CW2015_READ_ADDR, &i2c_write_cmd_addr);
    if (status != IOT_SUCCESS) {
        printf("I2cRead() failed, %0X!\n", status);
        return status;
    }
    ret = recvData[0];
    return ret;
}

uint32_t Cw20_Write(uint8_t addr, uint8_t writedata, uint32_t buffLen)
{
    uint8_t buffer[2] = {addr, writedata};
    uint32_t retval = IoTI2cWrite(CW2015_I2C_IDX, CW2015_WRITE_ADDR, buffer, buffLen);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    printf("IoTI2cWrite(%02X)\r\n", buffer[0]);
    return IOT_SUCCESS;
}

void CW2015Init(void)
{
    /* 使电量检测模块从sleep mode变为wake up mode,0x00代表唤醒,0x11代表沉睡,2代表2bit控制 */
    /* Change the power detection module from sleep mode to wake up mode. */
    /* 0x00 represents wake-up, 0x11 represents deep sleep, and 2 bit control */
    Cw20_Write(CW2015_WAKE_REGISTER, 0x00, 2);
}

float GetVoltage(void)
{
    uint8_t buff[WRITELEN] = {0};
    float voltage = 0;
    uint32_t temp = 0;
    // 读取电压的前6位 Read the first 6 bits of voltage
    buff[0] = Cw20_WriteRead(CW2015_HIGHT_REGISTER, 1, 1);
    // 读取电压的后8位 Read the last 8 bits of voltage
    buff[1] = Cw20_WriteRead(CW2015_LOW_REGISTER, 1, 1);
    /* 通过位运算最后得到14位的A/D测量值 */
    /* The final 14 bit A/D measurement value is obtained through bit operation */
    /* 将buf[0]左移8位与buf[1]组成最终电压值 */
    /* Move buf [0] to the left by 8 bits to form the final voltage value with buf [1] */
    temp = (buff[0] << 8) | buff[1];
    /* 通过计算得到最终的电压值 （CW2015的电压分辨率为305.0uV,转换1uv = 1 / 1000000） */
    /* The final voltage value is obtained through calculation */
    /* the voltage resolution of CW2015 is 305.0uV, and the conversion 1uv=1/1000000) */
    voltage = temp * 305.0 / 1000000;
    return voltage;
}