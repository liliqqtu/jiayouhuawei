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
#include <stdio.h>
#include <unistd.h>

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

#include "pca9555.h"
#include "debug_util.h"
#include "wheel_codec.h"
#include "ctrl_algo.h"
#include "gyro.h"

#define LSM6DS_I2C_IDX                  (0)
#define IOT_I2C_IDX_BAUDRATE            (400000)    // 400k

#define gyroKp                          (20.0f)     // 比例增益支配率收敛到加速度计/磁强计
#define gyroKi                          (0.0004f)   // 积分增益支配率的陀螺仪偏见的衔接
#define gyroHalfT                       (0.005f)    // 采样周期的一半

float q0 = 1, q1 = 0, q2 = 0, q3 = 0;   // 四元数的元素，代表估计方向
float exInt = 0, eyInt = 0, ezInt = 0;  // 按比例缩小积分误差
float g_gyro_yaw, g_gyro_pitch, g_gyro_roll;                 // 偏航角，俯仰角，翻滚角
static float yaw_conv = 0.0f;

/**
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
    uint8_t recvData[888] = {0};
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
    return status;
}


static uint32_t LSM6DS_Write(uint8_t addr, uint8_t writedata, uint32_t buffLen)
{
    uint8_t buffer[2] = {addr, writedata};
    uint32_t retval = IoTI2cWrite(LSM6DS_I2C_IDX, LSM6DS_WRITE_ADDR, buffer, buffLen);
    if (retval != IOT_SUCCESS) {
        printf("IoTI2cWrite(%02X) failed, %0X!\n", buffer[0], retval);
        return retval;
    }
    printf("IoTI2cWrite(%02X)\r\n", buffer[0]);
    return IOT_SUCCESS;
}

#define EXEUTION_FREQ 10  // 单位ms

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
    static float dt = 0.03; // 0.03代表300ms读取陀螺仪数据
    static float yaw = 0.0f, temp = 0.0f;

    // 除去零偏
    #if 0
    static int a = 0;
    a++;
    if (hi_get_seconds() <= 5) { // 5ms
        printf("---------times-----------:%d\n", a);
    }
    #endif

    if (fabs(gyroZ) < 0.04) { // 0.04标准值
        temp = 0;
    } else {
        temp = gyroZ * dt;
    }
    yaw += temp;
    yaw_conv = yaw * 57.32; // 57.32 初始值
    // 360°一个循环
    if (fabs(yaw_conv) > 360.0f) {
        if ((yaw_conv) < 0) {
            yaw_conv += 360.0f;
        } else {
            yaw_conv -= 360.0f;
        }
    }
}

void GetRoll(float atan2x, float atan2y)
{
    float atan2_x = atan2x;
    float atan2_y = atan2y;
    if (atan2_x > 0) {
        g_gyro_roll = atan(atan2_y / atan2_x) * DEGREES / PAI;
    } else if (atan2_x < 0 && atan2_y >= 0) {
        g_gyro_roll = atan(atan2_y / atan2_x) * DEGREES / PAI + DEGREES;
    } else if (atan2_x < 0 && atan2_y < 0) {
        g_gyro_roll = atan(atan2_y / atan2_x) * DEGREES / PAI - DEGREES;
    } else if (atan2_y > 0 && atan2_x == 0) {
        g_gyro_roll = 90; // 90°
    } else if (atan2_y < 0 && atan2_x == 0) {
        g_gyro_roll = -90; // -90°
    } else {
        printf("undefined\n");
    }
}

void GetPitch(float atan2x, float atan2y)
{
    float atan2_x = atan2x;
    float atan2_y_pitch = atan2y;
    if (atan2_x > 0) {
        g_gyro_pitch = atan(atan2_y_pitch / atan2_x) * DEGREES / PAI;
    } else if (atan2_x < 0 && atan2_y_pitch >= 0) {
        g_gyro_pitch = atan(atan2_y_pitch / atan2_x) * DEGREES / PAI + DEGREES;
    } else if (atan2_x < 0 && atan2_y_pitch < 0) {
        g_gyro_pitch = atan(atan2_y_pitch / atan2_x) * DEGREES / PAI - DEGREES;
    } else if (atan2_y_pitch > 0 && atan2_x == 0) {
        g_gyro_pitch = 90; // 90°
    } else if (atan2_y_pitch <  0 && atan2_x == 0) {
        g_gyro_pitch = -90; // -90°
    } else {
        printf("undefined\n");
    }
}

void IMU_Attitude_cal(float gcx, float gcy, float gcz, float acx, float acy, float acz)
{
    float norm;
    float vx, vy, vz;
    float ex, ey, ez;
    float atan2_x, atan2_y;
    float atan2_y_pitch;
    float ax = acx, ay = acy, az = acz;
    float gx = gcx, gy = gcy, gz = gcz;

    // 把采集到的三轴加速度转化为单位向量，即向量除以模
    norm = (float)sqrt((float)(ax * ax + ay * ay + az * az));
    ax = ax / norm;
    ay = ay / norm;
    az = az / norm;

    // 把四元素换算成方向余弦中的第三行的三个元素
    // vx、vy、vz其实就是上一次的欧拉角(四元数)机体参考坐标系换算出来的重力的单位向量
    vx = 2 * (q1 * q3 - q0 * q2); // 2计算系数
    vy = 2 * (q0 * q1 + q2 * q3); // 2计算系数
    vz = q0 * q0 - q1 * q1 - q2 * q2 + q3 * q3;

    // 对向量叉乘，求出姿态误差
    // ex、ey、ez为三轴误差元素
    ex = (ay * vz - az * vy);
    ey = (az * vx - ax * vz);
    ez = (ax * vy - ay * vx);

    // 叉乘向量仍旧是机体坐标系上的，而陀螺仪积分误差也是机体坐标系
    // 而且叉积的大小与陀螺仪误差成正比，正好拿来纠正陀螺
    exInt = exInt + ex * gyroKi;
    eyInt = eyInt + ey * gyroKi;
    ezInt = ezInt + ez * gyroKi;

    // 调整后的陀螺仪测量
    gx = gx + gyroKp * ex + exInt;
    gy = gy + gyroKp * ey + eyInt;
    gz = gz + gyroKp * ez + ezInt;

    // 使用一阶龙格库塔解四元数微分方程
    q0 = q0 + (-q1 * gx - q2 * gy - q3 * gz) * gyroHalfT;
    q1 = q1 + (q0 * gx + q2 * gz - q3 * gy) * gyroHalfT;
    q2 = q2 + (q0 * gy - q1 * gz + q3 * gx) * gyroHalfT;
    q3 = q3 + (q0 * gz + q1 * gy - q2 * gx) * gyroHalfT;

    // 四元数归一化
    norm = sqrt(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
    q0 = q0 / norm;
    q1 = q1 / norm;
    q2 = q2 / norm;
    q3 = q3 / norm;
    
    // 计算姿态角，本文Roll为横滚角，Pitch为俯仰角
    atan2_x = -2 * q1 * q1 - 2 * q2 * q2 + 1; // 2 计算参数
    atan2_y = 2 * q2 * q3 + 2 * q0 * q1; // 2 计算参数
    GetRoll(atan2_x, atan2_y);
    // 俯仰角
    atan2_y_pitch = -2 * q1 * q3 + 2 * q0 * q2; // 2 计算参数
    GetPitch(atan2_x, atan2_y_pitch);
}

void Lsm_Get_RawAcc(void)
{
    uint8_t buf[12] = {0};
    int16_t acc_x = 0, acc_y = 0, acc_z = 0;
    float acc_x_conv = 0, acc_y_conv = 0, acc_z_conv = 0;
    int16_t ang_rate_x = 0, ang_rate_y = 0, ang_rate_z = 0;
    float ang_rate_x_conv = 0, ang_rate_y_conv = 0, ang_rate_z_conv = 0;

    if ((LSM6DS_WriteRead(LSM6DSL_STATUS_REG, 1, 1) & 0x03) != 0) { // 0x03 开启读取数据
        if (IOT_SUCCESS != LSM6DS_ReadCont(LSM6DSL_OUTX_L_G, buf, 12)) { // BUF 12
            printf("i2c read error!\n");
        } else {
            ang_rate_x = (buf[1] << 8) + buf[0]; // 将buff1 右移8位在与上buff 0
            ang_rate_y = (buf[3] << 8) + buf[2]; // 将buff3 右移8位在与上buff 2
            ang_rate_z = (buf[5] << 8) + buf[4]; // 将buff5 右移8位在与上buff 4
            acc_x = (buf[7] << 8) + buf[6]; // 将buff7 右移8位在与上buff 6
            acc_y = (buf[9] << 8) + buf[8]; // 将buff9 右移8位在与上buff 8
            acc_z = (buf[11] << 8) + buf[10]; // 将buff11 右移8位在与上buff 10

            ang_rate_x_conv = PAI / 180.0 * ang_rate_x / 14.29; // 180.0代表度数 14.29量程
            ang_rate_y_conv = PAI / 180.0 * ang_rate_y / 14.29; // 180.0代表度数 14.29量程
            ang_rate_z_conv = PAI / 180.0 * ang_rate_z / 14.29; // 180.0代表度数 14.29量程

            acc_x_conv = acc_x / 4098.36; // 4098.36量程
            acc_y_conv = acc_y / 4098.36; // 4098.36量程
            acc_z_conv = acc_z / 4098.36; // 4098.36量程
            IMU_Attitude_cal(ang_rate_x_conv, ang_rate_y_conv, ang_rate_z_conv, acc_x_conv, acc_y_conv, acc_z_conv);
            IMU_YAW_CAL(ang_rate_z_conv);
        }
    }
}

void InitGyro(void)
{
    uint32_t ret;

    /* init i2c */
    IoTI2cInit(0, IOT_I2C_IDX_BAUDRATE); /* baudrate: 400000 */
    IoTI2cSetBaudrate(0, IOT_I2C_IDX_BAUDRATE);
    IoSetFunc(IOT_IO_NAME_GPIO_13, IOT_IO_FUNC_GPIO_13_I2C0_SDA);
    IoSetFunc(IOT_IO_NAME_GPIO_14, IOT_IO_FUNC_GPIO_14_I2C0_SCL);
    
    ret = LSM6DS_WriteRead(LSM6DSL_WHO_AM_I, 1, 1);
    printf("who am i: %X\n", ret);

    LSM6DS_Init();
}

float GetPitchValue(void)
{
    return g_gyro_pitch;
}