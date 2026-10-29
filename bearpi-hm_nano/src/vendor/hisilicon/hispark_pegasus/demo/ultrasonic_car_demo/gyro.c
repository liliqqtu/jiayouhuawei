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
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#include <hi_time.h>
#include <hi_timer.h>

#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "iot_i2c.h"
#include "iot_errno.h"
#include "hi_errno.h"
#include "hi_i2c.h"
#include "iot_gpio.h"
#include "gyro.h"

float q0 = 1, q1 = 0, q2 = 0, q3 = 0;   // 四元数的元素，代表估计方向
float exInt = 0, eyInt = 0, ezInt = 0;  // 按比例缩小积分误差
float Yaw;                 // 偏航角，俯仰角，翻滚角

#define LSM6DS_I2C_IDX 0
#define IOT_I2C_IDX_BAUDRATE         400000 // 400k

#define Kp 20.0f                // 比例增益支配率收敛到加速度计/磁强计
#define Ki 0.0004f                // 积分增益支配率的陀螺仪偏见的衔接
#define halfT 0.005f             // 采样周期的一半

/**
 * @berf i2c read
 * @param hi_u8 reg_high_8bit_cmd:Transmit register value 8 bits high
 * @param hi_u8 reg_low_8bit_cmd:Transmit register value low 8 bits
 * @param hi_u8* recv_data:Receive data buff
 * @param hi_u8 send_len:Sending data length
 * @param hi_u8 read_len:Length of received data
*/
static uint32_t LSM6DS_WriteRead(uint8_t reg_high_8bit_cmd, uint8_t send_len, uint8_t read_len)
{
    uint32_t status = 0;
    uint8_t recvData[888] = { 0 }; // 888长度
    uint32_t ret = 0;
    hi_i2c_data c081nfc_i2c_write_cmd_addr = { 0 };
    uint8_t send_user_cmd[1] = {reg_high_8bit_cmd};

    memset(recvData, 0x0, sizeof(recvData));
    c081nfc_i2c_write_cmd_addr.send_buf = send_user_cmd;
    c081nfc_i2c_write_cmd_addr.send_len = send_len;

    c081nfc_i2c_write_cmd_addr.receive_buf = recvData;
    c081nfc_i2c_write_cmd_addr.receive_len = read_len;

    status = hi_i2c_writeread(LSM6DS_I2C_IDX, LSM6DS_READ_ADDR, &c081nfc_i2c_write_cmd_addr);
    if (status != IOT_SUCCESS) {
        printf("I2cRead() failed, %0X!\n", status);
        return status;
    }

    ret = recvData[0];
    return ret;
}

static uint32_t LSM6DS_ReadCont(uint8_t reg_addr, uint8_t* buffer, uint16_t read_len)
{
    uint32_t status;
    hi_i2c_data i2c_attr;

    i2c_attr.send_buf = &reg_addr;
    i2c_attr.send_len = 1;
    i2c_attr.receive_buf = buffer;
    i2c_attr.receive_len = read_len;

    status = hi_i2c_writeread(LSM6DS_I2C_IDX, LSM6DS_READ_ADDR, &i2c_attr);
    return status;
}

static uint32_t LSM6DS_Write(uint8_t addr, uint8_t writedata, uint32_t buffLen)
{
    uint8_t buffer[2] = {addr, writedata}; // 2长度
    uint32_t retval = IoTI2cWrite(LSM6DS_I2C_IDX, LSM6DS_WRITE_ADDR, buffer, buffLen);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    printf("IoTI2cWrite(%02X)\r\n", buffer[0]);
    return IOT_SUCCESS;
}

void LSM6DS_Init(void)
{
    LSM6DS_Write(LSM6DSL_CTRL3_C, 0x34, 2); // 0x34 2 初始化陀螺仪
    LSM6DS_Write(LSM6DSL_CTRL2_G, 0X4C, 2); // 0x4c 2 角速度陀螺仪配置2000dps,104Hz
    LSM6DS_Write(LSM6DSL_CTRL10_C, 0x38, 2); // 0x38 2 timer en, pedo en, tilt en ??
    LSM6DS_Write(LSM6DSL_CTRL1_XL, 0x4F, 2); // 0x4F 2 加速度配置量程为8g,104Hz, lpf1_bw_sel=1, bw0_xl=1;
    
    LSM6DS_Write(LSM6DSL_TAP_CFG, 0x10, 2); // 0x10 2长度 LSM6DSL_TAP_CFG
    LSM6DS_Write(LSM6DSL_WAKE_UP_DUR, 0x00, 2); // 0x00 2长度 LSM6DSL_WAKE_UP_DUR
    LSM6DS_Write(LSM6DSL_WAKE_UP_THS, 0x02, 2); // 0x02 2长度 LSM6DSL_WAKE_UP_THS
    LSM6DS_Write(LSM6DSL_TAP_THS_6D, 0x40, 2); // 0x40 2长度 LSM6DSL_TAP_THS_6D
    LSM6DS_Write(LSM6DSL_CTRL8_XL, 0x01, 2); // 0x01 2长度 LSM6DSL_CTRL8_XL
}

void IMU_YAW_CAL(float gyroZ)
{
    static float dt = 0.01; // 0.01代表100ms读取一次陀螺仪数据
    static float yaw_conv = 0.0f, temp = 0.0f;

    // 除去零偏
#if 0
    static int a = 0;
    a++;
    if (hi_get_seconds() <= 5) { // 5ms
        printf("---------times-----------:%d\n", a);
    }
#endif

    if (fabs(gyroZ) < 0.04) { // 0.04 gyroZ标准值
        temp = 0;
    } else {
        temp = gyroZ * dt;
    }
    yaw_conv += temp;
    Yaw = yaw_conv * 57.32; // 57.32标准值
    // 360°一个循环
    if (fabs(Yaw) > 360.0f) {
        if ((Yaw) < 0) {
            Yaw += 360.0f;
        } else {
            Yaw -= 360.0f;
        }
    }
}


void Lsm_Get_RawAcc(void)
{
    uint8_t buf[12] = {0}; // 12位陀螺仪数据
    int16_t ang_rate_z = 0;
    float ang_rate_z_conv = 0;

    if ((LSM6DS_WriteRead(LSM6DSL_STATUS_REG, 1, 1) & 0x03) != 0) {
        if (IOT_SUCCESS != LSM6DS_ReadCont(LSM6DSL_OUTX_L_G, buf, 12)) { // 12个buff长度
            printf("i2c read error!\n");
        } else {
            ang_rate_z = (buf[5] << 8) + buf[4]; // 将buff5 右移8位在与上buff 4
            ang_rate_z_conv = 3.14 / 180.0 * ang_rate_z / 14.29; // 3.14= π， 180.0° 14.29为陀螺仪量程
            IMU_YAW_CAL(ang_rate_z_conv);
        }
    }
}

float GetYaw(void)
{
    return Yaw;
}