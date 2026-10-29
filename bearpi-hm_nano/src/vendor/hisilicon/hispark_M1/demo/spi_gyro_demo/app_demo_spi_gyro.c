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
#include <hi_task.h>
#include <hi_types_base.h>
#include <hi_spi.h>
#include <hi_time.h>
#include <../platform/drivers/spi/spi.h>
#include <hi_gpio.h>
#include <time.h>
#include <hi_i2c.h>
#include "hi_io.h"
#include "hi_gpio.h"
#include "math.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "ssd1306.h"
#include "app_demo_spi_flash.h"
#include "iot_i2c.h"
#include "app_demo_spi_gyro.h"

#define IOT_I2C_IDX_BAUDRATE (400 * 1000)
#define SSD1306_I2C_IDX 0

#define gyroKp                          (20.0f)     // 比例增益支配率收敛到加速度计/磁强计
#define gyroKi                          (0.0004f)   // 积分增益支配率的陀螺仪偏见的衔接
#define gyroHalfT                       (0.005f)    // 采样周期的一半
#define PAI               3.14
#define DEGREES           180

float q0 = 1, q1 = 0, q2 = 0, q3 = 0;   // 四元数的元素，代表估计方向
float exInt = 0, eyInt = 0, ezInt = 0;  // 按比例缩小积分误差
float g_gyro_yaw, g_gyro_pitch, g_gyro_roll;                 // 偏航角，俯仰角，翻滚角
static char line[32] = { 0 };
static float yaw_conv = 0.0f;

void GyroGpioInit(void)
{
    IoSetFunc(IOT_IO_NAME_GPIO_10, IOT_IO_FUNC_GPIO_10_SPI0_CK);
    IoSetFunc(IOT_IO_NAME_GPIO_11, IOT_IO_FUNC_GPIO_11_SPI0_RXD); // SPI MISO 数据从从发到主
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_SPI0_TXD); // SPI MOSI 数据从主发到从

    IoSetFunc(IOT_IO_NAME_GPIO_12, IOT_IO_FUNC_GPIO_12_GPIO); // GYROcs片段
    IoTGpioSetDir(IOT_IO_NAME_GPIO_12, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE1);

    IoSetFunc(IOT_IO_NAME_GPIO_0, IOT_IO_FUNC_GPIO_0_GPIO); // FLASHcs片段
    IoTGpioSetDir(IOT_IO_NAME_GPIO_0, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_0, IOT_GPIO_VALUE1);

    hi_io_set_driver_strength(HI_IO_NAME_GPIO_10, HI_IO_DRIVER_STRENGTH_2);
}

void LSM6DSL_Write_Reg(hi_spi_idx id, unsigned char addrdata, unsigned char writedata, unsigned int writelen)
{
    unsigned char writebuff[2] = {addrdata, writedata};
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE0);  // 使能SPI传输
    int ret = hi_spi_host_write(id, &writebuff, 2);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi write[%02X] fail! %x ", writebuff[0], ret);
    }
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE1);  // 禁止SPI传输
}

void LSM6DSL_Write_Read_Reg(hi_spi_idx id, unsigned char addrdata, unsigned char writedata,
                            unsigned char *readdata, unsigned int readdatalen)
{
    int ret = 0;
    unsigned char writebuff[2] = {addrdata, writedata};
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE0);  // 使能SPI传输
    ret = memset_s(readdata, readdatalen + 1, 0x0, readdatalen);
    if (ret != EOK) {
        printf("memcpy_s failed, err = %d\n", ret);
    }
    ret = hi_spi_host_writeread(id, &writebuff, readdata, readdatalen);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi read[%02X] fail! %x ", readdata[0], ret);
    }
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE1);  // 禁止SPI传输
}

unsigned char LSM6DSL_Read_Reg(hi_spi_idx id, unsigned char writedata,
                               unsigned char *readdata, unsigned int readdatalen)
{
    int ret = 0;
    unsigned char resultdata;
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE0);  // 使能SPI传输
    ret = memset_s(readdata, readdatalen + 1, 0x0, readdatalen);
    if (ret != EOK) {
        printf("memcpy_s failed, err = %d\n", ret);
    }
    ret = hi_spi_host_writeread(id, &writedata, readdata, readdatalen);
    if (ret != HI_ERR_SUCCESS) {
        printf("spi read[%02X] fail! %x ", readdata[0], ret);
    }
    resultdata = readdata[1];
    // printf("resultdata = %02x]r]n", resultdata);
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_12, IOT_GPIO_VALUE1);  // 禁止SPI传输
    return resultdata;
}

void IMU_YAW_CAL(float gyroZ)
{
    int ret = 0;
    static float dt = 0.03; // 0.03代表300ms读取陀螺仪数据
    static float yaw = 0.0f, temp = 0.0f;
    // 除去零偏
    #if 0
    static int a = 0;
    a++;
    if (hi_get_seconds() <= 5) { // 5s
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
    // printf("Pitch:%.02f, Roll:%.02f, yaw:%.2f\n", g_gyro_pitch, g_gyro_roll, yaw_conv);
    ssd1306_SetCursor(0, 15); // 0为横坐标，15为纵坐标
    ret = snprintf_s(line, sizeof(line), sizeof(line), "Pitch:%.2f", g_gyro_pitch);
    if (ret == 0) {
        printf("Pitch failed\r\n");
    }
    ssd1306_DrawString(line, Font_7x10, White);
    ssd1306_SetCursor(0, 30); // 0为横坐标，30为纵坐标
    ret = snprintf_s(line, sizeof(line), sizeof(line), "Roll:%.2f", g_gyro_roll);
    if (ret == 0) {
        printf("Roll failed\r\n");
    }
    ssd1306_DrawString(line, Font_7x10, White);
    ssd1306_SetCursor(0, 0); // 0为横坐标，0为纵坐标
    ret = snprintf_s(line, sizeof(line), sizeof(line), "yaw:%.2f", yaw_conv);
    if (ret == 0) {
        printf("yaw failed\r\n");
    }
    ssd1306_DrawString(line, Font_7x10, White);
    ssd1306_UpdateScreen();
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
    if (norm == 0) {
        printf("209 norm = 0,failed\n");
    }
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
    if (norm == 0) {
        printf("247 norm = 0,failed\n");
    }
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

void Lsm_Get_RawAcc(hi_spi_idx id, int a)
{
    unsigned char buf[12] = {0};
    short acc_x = 0, acc_y = 0, acc_z = 0;
    float acc_x_conv = 0, acc_y_conv = 0, acc_z_conv = 0;
    short ang_rate_x = 0, ang_rate_y = 0, ang_rate_z = 0;
    float ang_rate_x_conv = 0, ang_rate_y_conv = 0, ang_rate_z_conv = 0;
    float ang_rate_x_cal = 0, ang_rate_y_cal = 0, ang_rate_z_cal = 0;
    unsigned char read_buff[2] = { 0 };
    
    if ((LSM6DSL_Read_Reg(id, LSM6DSL_STATUS_REG | SPI_READ, read_buff, READDATALEN) & 0x01) != 0) {
        buf[0]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTX_H_XL | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第0位
        buf[1]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTX_L_XL | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第1位
        buf[2]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTY_H_XL | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第2位
        buf[3]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTY_L_XL | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第3位
        buf[4]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTZ_H_XL | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第4位
        buf[5]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTZ_L_XL | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第5位

        buf[6]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTX_H_G | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第6位
        buf[7]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTX_L_G | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第7位
        buf[8]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTY_H_G | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第8位
        buf[9]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTY_L_G | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第9位
        buf[10]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTZ_H_G | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第10位
        buf[11]= LSM6DSL_Read_Reg(id, LSM6DSL_OUTZ_L_G | SPI_READ, read_buff, READDATALEN); // 陀螺仪数据第11位

        ang_rate_x = (buf[6] << 8) | buf[7];  // 将buff6 右移8位在与上buff 7
        ang_rate_y = (buf[8] << 8)  | buf[9]; // 将buff8 右移8位在与上buff 9
        ang_rate_z = (buf[10] << 8)  | buf[11];  // 将buff10 右移8位在与上buff 11

        acc_x = (buf[0] << 8) | buf[1];  // 将buff0 右移8位在与上buff 1
        acc_y = (buf[2] << 8) | buf[3];  // 将buff2 右移8位在与上buff 3
        acc_z = (buf[4] << 8) | buf[5];  // 将buff4 右移8位在与上buff 5

        ang_rate_x_conv = PAI / 180.0 * ang_rate_x / 14.29; // 180.0代表度数 14.29量程
        ang_rate_y_conv = PAI / 180.0 * ang_rate_y / 14.29; // 180.0代表度数 14.29量程
        ang_rate_z_conv = PAI / 180.0 * ang_rate_z / 14.29; // 180.0代表度数 14.29量程

        acc_x_conv = acc_x / 4098.36; // 4098.36量程
        acc_y_conv = acc_y / 4098.36; // 4098.36量程
        acc_z_conv = acc_z / 4098.36; // 4098.36量程

        if (a % 1000 == 0) { // 1000次保存一次
            GD25Q40C_SPIFLASH_EraseSector(id, 0x000000); // 擦除芯片
            TaskMsleep(10); // 延时10ms
            GD25Q_SPIFLASH_WritePage(id, 0x000000, buf);
            TaskMsleep(10); // 延时10ms
            GD25Q_SPIFLASH_ReadBuffer(id, 0x000000);
            TaskMsleep(20); // 延时20ms
            ssd1306_SetCursor(0, 45); // 0为横坐标，45为纵坐标
            ssd1306_DrawString("save data to flash", Font_7x10, White);
        }
        IMU_Attitude_cal(ang_rate_x_conv, ang_rate_y_conv, ang_rate_z_conv, acc_x_conv, acc_y_conv, acc_z_conv);
        IMU_YAW_CAL(ang_rate_z_conv);
    }
}

void Lsm6d3s_Init(hi_spi_idx id)
{
    hi_u8 readdata[1];
    hi_u8 read_buff[2] = { 0 };
    readdata[0]= LSM6DSL_Read_Reg(id, LSM6DSL_WHO_AM_I | SPI_READ, read_buff, READDATALEN);
    LSM6DSL_Write_Reg(id, LSM6DSL_CTRL3_C | SPI_WRITE, 0x34, READDATALEN); // 0x34 2 初始化陀螺仪
    LSM6DSL_Write_Reg(id, LSM6DSL_CTRL2_G | SPI_WRITE, 0X4C, READDATALEN); // 0x4c 2 角速度陀螺仪配置2000dps,104Hz
    LSM6DSL_Write_Reg(id, LSM6DSL_CTRL10_C | SPI_WRITE, 0x38, READDATALEN); // 0x38 2 timer en, pedo en, tilt en
    // 0x4F 2 加速度配置量程为8g,104Hz, lpf1_bw_sel=1, bw0_xl=1;
    LSM6DSL_Write_Reg(id, LSM6DSL_CTRL1_XL | SPI_WRITE, 0x4F, READDATALEN);
    LSM6DSL_Write_Reg(id, LSM6DSL_TAP_CFG | SPI_WRITE, 0x10, READDATALEN); // 0x10 2长度 LSM6DSL_TAP_CFG
    LSM6DSL_Write_Reg(id, LSM6DSL_WAKE_UP_DUR | SPI_WRITE, 0x00, READDATALEN); // 0x00 2长度 LSM6DSL_WAKE_UP_DUR
    LSM6DSL_Write_Reg(id, LSM6DSL_WAKE_UP_THS | SPI_WRITE, 0x02, READDATALEN); // 0x02 2长度 LSM6DSL_WAKE_UP_THS
    LSM6DSL_Write_Reg(id, LSM6DSL_TAP_THS_6D | SPI_WRITE, 0x40, READDATALEN); // 0x40 2长度 LSM6DSL_TAP_THS_6D
    LSM6DSL_Write_Reg(id, LSM6DSL_CTRL8_XL | SPI_WRITE, 0x01, READDATALEN); // 0x01 2长度 LSM6DSL_CTRL8_XL
    return HI_ERR_SUCCESS;
}

void Oled_Desplay(void)
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

void GyroTask(void)
{
    hi_u32 ret;
    int i = 1;
    hi_spi_idx id = HI_SPI_ID_0;
    hi_spi_deinit(id); /* if wake_up from deep sleep, should deinit first */
    hi_spi_cfg_basic_info spi_cfg_basic_info;
    spi_cfg_basic_info.cpha = 1;
    spi_cfg_basic_info.cpol = 1;
    spi_cfg_basic_info.data_width = HI_SPI_CFG_DATA_WIDTH_E_8BIT;
    spi_cfg_basic_info.endian = 0;
    spi_cfg_basic_info.fram_mode = 0;
    spi_cfg_basic_info.freq = SPI_FREQUENCY;
    hi_spi_cfg_init_param spi_init_param = {0};
    spi_init_param.is_slave = 0;
    ret = hi_spi_init(id, spi_init_param, &spi_cfg_basic_info); // 基本参数配置
    if (ret != HI_ERR_SUCCESS) {
        printf("SPI init fail! %x ", ret);
    }
    GyroGpioInit();
    Oled_Desplay();
    Lsm6d3s_Init(id);
    GD25Q40C_Init(id);
    while (1) {
        Lsm_Get_RawAcc(id, i);
        i++;
        TaskMsleep(10); // 延时10ms
    }
}

static void GyroControlTask(void)
{
    osThreadAttr_t attr;

    attr.name = "GyroCntrolDemo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 5; // 堆栈大小为1024 stack size 1024 * 5
    attr.priority = 25; // 优先级25
    if (osThreadNew((osThreadFunc_t)GyroTask, NULL, &attr) == NULL) {
        printf("[GyroExample] Failed to create GyroTask!\n");
    }
}

APP_FEATURE_INIT(GyroControlTask);