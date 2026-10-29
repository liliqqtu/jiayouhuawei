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

#include <math.h>
#include <stdlib.h>
#include <string.h>  // For memcpy

#include <hi_time.h>

#include <stdio.h>
#include <unistd.h>
#include "iot_gpio_ex.h"

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "iot_i2c.h"
#include "iot_errno.h"
#include "hi_errno.h"
#include "hi_i2c.h"
#include "iot_gpio.h"
#include "ssd1306_fonts.h"
#include "ssd1306.h"
#include "gyro.h"

#define LSM6DS_I2C_IDX 0
#define IOT_I2C_IDX_BAUDRATE         400000 // 400k
 /* 20.0f 比例增益支配率收敛到加速度计/磁强计 Proportional gain dominance converges to accelerometer/magnetometer */
#define Kp 20.0f
/* 0004f 积分增益支配率的陀螺仪偏见的衔接 The Connection of Gyroscope Bias with Integrated Gain Dominance */
#define Ki 0.0004f
/* 0.005f 采样周期的一半 Half of the sampling period */
#define halfT 0.005f
/* 1 0 0 0四元数的元素，代表估计方向 Element of quaternion, representing estimation direction */
float q0 = 1, q1 = 0, q2 = 0, q3 = 0;
/* 按比例缩小积分误差 Scale down integral error */
float exInt = 0, eyInt = 0, ezInt = 0;
/* 偏航角，俯仰角，翻滚角 Yaw angle */
float Yaw, Pitch, Roll;
/* 0.0f 偏航角初始值 0.0f initial yaw angle */
static float yaw_conv = 0.0f;

/*
 * @berf i2c read
 * @param hi_u8 reg_high_8bit_cmd:Transmit register value 8 bits high
 * @param hi_u8 reg_low_8bit_cmd:Transmit register value low 8 bits
 * @param hi_u8* recv_data:Receive data buff
 * @param hi_u8 send_len:Sending data length
 * @param hi_u8 read_len:Length of received data
*/
uint32_t LSM6DS_WriteRead(uint8_t reg_high_8bit_cmd, uint8_t send_len, uint8_t read_len)
{
    uint32_t status = 0;
    uint8_t recvData[888] = { 0 };
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

uint32_t LSM6DS_ReadCont(uint8_t reg_addr, uint8_t* buffer, uint16_t read_len)
{
    uint32_t status;
    hi_i2c_data i2c_attr;

    i2c_attr.send_buf = &reg_addr;
    i2c_attr.send_len = 1;
    i2c_attr.receive_buf = buffer;
    i2c_attr.receive_len = read_len;

    status = hi_i2c_writeread(LSM6DS_I2C_IDX, LSM6DS_READ_ADDR, &i2c_attr);
    for (int i = 0; i < read_len; i++) {
        printf("0x%x ", buffer[i]);
    }
    printf("\r\n");
    return status;
}

static uint32_t LSM6DS_Write(uint8_t addr, uint8_t writedata, uint32_t buffLen)
{
    uint8_t buffer[2] = {addr, writedata}; // 2代表buff长度 2 represents buff length
    uint32_t retval = IoTI2cWrite(LSM6DS_I2C_IDX, LSM6DS_WRITE_ADDR, buffer, buffLen);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    printf("IoTI2cWrite(%02X)\r\n", buffer[0]);
    return IOT_SUCCESS;
}

void GetLSM6DS(void)
{
    while (1) {
        TaskMsleep(5); // 5ms
        Lsm_Get_RawAcc();
    }
}

void LSM6DS_Init(void)
{
    /* 0x34 2初始化陀螺仪 0x34 2 Initialize gyroscope */
    LSM6DS_Write(LSM6DSL_CTRL3_C, 0x34, 2);
    /* 0X4C 2 配置陀螺仪 0X4C 2 Configure gyroscope */
    LSM6DS_Write(LSM6DSL_CTRL2_G, 0X4C, 2); // 角速度陀螺仪配置2000dps ,104Hz
    /* 0x38 2  timer en, pedo en, tilt en */
    LSM6DS_Write(LSM6DSL_CTRL10_C, 0x38, 2);
    /* 0x4F 2 加速度配置量程为8g,104Hz, lpf1_bw_sel=1, bw0_xl=1; */
    /* 0x4F 2 The acceleration configuration range is 8g, 104Hz, lpf1_ bw_ sel=1, bw0_ xl=1; */
    LSM6DS_Write(LSM6DSL_CTRL1_XL, 0x4F, 2);
    /* 0x10 2  */
    LSM6DS_Write(LSM6DSL_TAP_CFG, 0x10, 2);
    /* 0x00 2  */
    LSM6DS_Write(LSM6DSL_WAKE_UP_DUR, 0x00, 2);
    /* 0x02 2  */
    LSM6DS_Write(LSM6DSL_WAKE_UP_THS, 0x02, 2);
    /* 0x40 2  */
    LSM6DS_Write(LSM6DSL_TAP_THS_6D, 0x40, 2);
    /* 0x01 2  */
    LSM6DS_Write(LSM6DSL_CTRL8_XL, 0x01, 2);
}

void IMU_YAW_CAL(float gyroZ)
{
    static float dt = 0.03;  // 0.03
    static float yaw = 0.0f, temp = 0.0f;

    // 除去零偏
    #if 0
    static int a = 0;
    a++;
    if (hi_get_seconds() <= 5) { // 5s
        printf("---------times-----------:%d\n", a);
    }
    #endif

    if (fabs(gyroZ) < 0.04) { // 0.04初始值
        temp = 0;
    } else {
        temp = gyroZ * dt;
    }
    yaw += temp;
    /* 57.32 */
    yaw_conv = yaw * 57.32;
    // 360°一个循环
    if (fabs(yaw_conv) > 360.0f) {
        if ((yaw_conv) < 0) {
            yaw_conv += 360.0f; // 360°一个循环
        } else {
            yaw_conv -= 360.0f; // 360°一个循环
        }
    }
    printf("yaw_conv:%.02f\n", yaw_conv);
    static char line[32] = {0};
    ssd1306_SetCursor(0, 30); // 30行0列开始
    int ret = snprintf(line, sizeof(line), "yaw_conv:%.2f", yaw_conv);
    if (ret != 14) { // 字符串长度为14
        printf("ret = %d\r\n", ret);
    }
    ssd1306_DrawString(line, Font_7x10, White);
}

void Lsm_Get_RawAcc(void)
{
    uint8_t buf[12] = { 0 };
    int16_t acc_x = 0, acc_y = 0, acc_z = 0;
    float acc_x_conv = 0, acc_y_conv = 0, acc_z_conv = 0;
    int16_t ang_rate_x = 0, ang_rate_y = 0, ang_rate_z = 0;
    float ang_rate_x_conv = 0, ang_rate_y_conv = 0, ang_rate_z_conv = 0;
    float ang_rate_x_cal = 0, ang_rate_y_cal = 0, ang_rate_z_cal = 0;
    if ((LSM6DS_WriteRead(LSM6DSL_STATUS_REG, 1, 1) & 0x03) != 0) {
        if (IOT_SUCCESS != LSM6DS_ReadCont(LSM6DSL_OUTX_L_G, buf, 12)) { // 12buf长度
            printf("i2c read error!\n");
        } else {
            ang_rate_x = (buf[1] << 8) + buf[0]; // buf第1位左移8位与buff第0位
            ang_rate_y = (buf[3] << 8) + buf[2]; // buf第3位左移8位与buff第2位
            ang_rate_z = (buf[5] << 8) + buf[4]; // buf第5位左移8位与buff第4位
            acc_x = (buf[7] << 8) + buf[6]; // buf第7位左移8位与buff第6位
            acc_y = (buf[9] << 8) + buf[8]; // buf第9位左移8位与buff第8位
            acc_z = (buf[11] << 8) + buf[10]; // buf第11位左移8位与buff第10位

            ang_rate_x_conv = (3.14 * ang_rate_x) / 180; // 3.14= π 180 °
            ang_rate_y_conv = (3.14 * ang_rate_y) / 180; // 3.14= π 180 °
            ang_rate_z_conv = (3.14 * ang_rate_z) / 180; // 3.14= π 180 °
            ang_rate_x_cal = ang_rate_x_conv / 14.29; // 14.29量程
            ang_rate_y_cal = ang_rate_y_conv / 14.29; // 14.29量程
            ang_rate_z_cal = ang_rate_z_conv / 14.29; // 14.29量程
            acc_x_conv = acc_x / 4098.36; // 4098.36量程
            acc_y_conv = acc_y / 4098.36; // 4098.36量程
            acc_z_conv = acc_z / 4098.36; // 4098.36量程
            printf("lsm trans acc: %.2f, %.2f, %.2f \n ang: %.2f, %.2f, %.2f, ang_cal: %.2f, %.2f, %.2f\n ",
                   acc_x_conv, acc_y_conv, acc_z_conv, ang_rate_x_conv, ang_rate_y_conv, ang_rate_z_conv,
                   ang_rate_x_cal, ang_rate_y_cal, ang_rate_z_cal);
            IMU_YAW_CAL(ang_rate_z_cal);
        }
    }
}