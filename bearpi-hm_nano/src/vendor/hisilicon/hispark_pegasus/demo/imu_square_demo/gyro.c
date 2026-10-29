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
#include "motor_control.h"
#include "pca9555.h"
#include "gyro.h"

float q0 = 1, q1 = 0, q2 = 0, q3 = 0;   // 四元数的元素，代表估计方向
float exInt = 0, eyInt = 0, ezInt = 0;  // 按比例缩小积分误差
float Yaw;                 // 偏航角，俯仰角，翻滚角

#define LSM6DS_I2C_IDX 0
#define IOT_I2C_IDX_BAUDRATE         400000 // 400k

#define Kp 20.0f                // 比例增益支配率收敛到加速度计/磁强计
#define Ki 0.0004f                // 积分增益支配率的陀螺仪偏见的衔接
#define halfT 0.005f             // 采样周期的一半

CAR_DRIVE car_drive = { 0 };
ENUM_MODE g_mode = MODE_ON_OFF;

int g_CarStarted = 0;
float yaw_data = 0.0f;

#define MASK_BUTTON1        (0x10)
#define MASK_BUTTON2        (0x08)
#define MASK_BUTTON3        (0x04)
#define YAW                 (90.0)

void init_ctrl_algo(void)
{
    memset(car_drive, 0, sizeof(CAR_DRIVE));
    car_drive.LeftForward = 13; // 13 左轮前进速度
    car_drive.RightForward = 10; // 10 右轮前进速度
    car_drive.TurnRight = 30; // 30 右转弯左轮速度
    car_drive.yaw1 = YAW;
    car_drive.yaw2 = 181.0; // 航向角181.0
    car_drive.yaw3 = 270.0; // 航向角270.0
    car_drive.yaw4 = 360.0; // 航向角360.0
    car_drive.time = 3000; // 行驶时间3000ms
}

void init_oled_mode(void)
{
    g_mode = MODE_ON_OFF;
    ssd1306_ClearOLED();
    ssd1306_printf("LF:%d, RF:%d", car_drive.LeftForward, car_drive.RightForward);
    ssd1306_printf("TL:%d, time:%d", car_drive.TurnRight, car_drive.time);
    ssd1306_printf("yaw1:%.1f,yaw2:%.1f", car_drive.yaw1, car_drive.yaw2);
    ssd1306_printf("yaw3:%.1f,yaw4:%.1f", car_drive.yaw3, car_drive.yaw4);
}

void ButtonDesplay(ENUM_MODE mode)
{
    switch (mode) {
        case MODE_ON_OFF:
            ssd1306_printf("LF:%d, RF:%d", car_drive.LeftForward, car_drive.RightForward);
            ssd1306_printf("TL:%d, time:%d", car_drive.TurnRight, car_drive.time);
            ssd1306_printf("yaw1:%.1f,yaw2:%.1f", car_drive.yaw1, car_drive.yaw2);
            ssd1306_printf("yaw3:%.1f,yaw4:%.1f", car_drive.yaw3, car_drive.yaw4);
            break;
        case MODE_SET_LEFT_FORWARD:
            ssd1306_printf("Set LForward=%d", car_drive.LeftForward);
            break;
        case MODE_SET_RIGHT_FORWARD:
            ssd1306_printf("Set RForward=%d", car_drive.RightForward);
            break;
        case MODE_SET_TURN_LEFT:
            ssd1306_printf("Set TurnRight=%d", car_drive.TurnRight);
            break;
        case MODE_SET_YAW1:
            ssd1306_printf("Set yaw1 = %.2f", car_drive.yaw1);
            break;
        case MODE_SET_YAW2:
            ssd1306_printf("Set yaw2 = %.2f", car_drive.yaw2);
            break;
        case MODE_SET_YAW3:
            ssd1306_printf("Set yaw3 = %.2f", car_drive.yaw3);
            break;
        case MODE_SET_YAW4:
            ssd1306_printf("Set yaw4 = %.2f", car_drive.yaw4);
            break;
        case MODE_SET_DRIVE_TIME:
            ssd1306_printf("Set time = %d", car_drive.time);
            break;
        default:
            init_oled_mode();
            break;
    }
}

void ButtonSet(ENUM_MODE mode, bool button_pressed)
{
    printf("mode = %d\r\n", mode);
    switch (mode) {
        case MODE_ON_OFF:
            g_CarStarted = !g_CarStarted;
            ssd1306_ClearOLED();
            printf("g_CarStarted = %d\r\n", g_CarStarted);
            ssd1306_printf(g_CarStarted ? "start" : "stop");
            break;
        case MODE_SET_LEFT_FORWARD:
            car_drive.LeftForward += ((button_pressed) ? -1 : 1);
            ssd1306_printf("LeftForward=%d", car_drive.LeftForward);
            break;
        case MODE_SET_RIGHT_FORWARD:
            car_drive.RightForward += (button_pressed ? -1 : 1);
            ssd1306_printf("RightForward=%d", car_drive.RightForward);
            break;
        case MODE_SET_TURN_LEFT:
            car_drive.TurnRight += ((button_pressed) ? -1 : 1);
            ssd1306_printf("TurnLEFT=%d", car_drive.TurnRight);
            break;
        case MODE_SET_YAW1:
            car_drive.yaw1 += ((button_pressed) ? -0.1 : 0.1); // 航向角每次增加或者减少0.1
            ssd1306_printf("yaw1 =%.2f", car_drive.yaw1);
            break;
        case MODE_SET_YAW2:
            car_drive.yaw2 += ((button_pressed) ? -0.1 : 0.1); // 航向角每次增加或者减少0.1
            ssd1306_printf("yaw2 =%.2f", car_drive.yaw2);
            break;
        case MODE_SET_YAW3:
            car_drive.yaw3 += ((button_pressed) ? -0.1 : 0.1); // 航向角每次增加或者减少0.1
            ssd1306_printf("yaw3 =%.2f", car_drive.yaw3);
            break;
        case MODE_SET_YAW4:
            car_drive.yaw4 += ((button_pressed) ? -0.1 : 0.1); // 航向角每次增加或者减少0.1
            ssd1306_printf("yaw4 =%.2f", car_drive.yaw4);
            break;
        case MODE_SET_DRIVE_TIME:
            car_drive.time += ((button_pressed) ? -10 : 10); // 时间增加减少10ms
            ssd1306_printf("time =%d", car_drive.time);
            break;
        default:
            break;
    }
}

void ButtonPressProc(uint8_t ext_io_val)
{
    static uint8_t ext_io_val_d = 0xFF;
    uint8_t diff;
    bool button1_pressed, button2_pressed, button3_pressed;
    diff = ext_io_val ^ ext_io_val_d;
    button1_pressed = ((diff & MASK_BUTTON1) && ((ext_io_val & MASK_BUTTON1) == 0)) ? true : false;
    button2_pressed = ((diff & MASK_BUTTON2) && ((ext_io_val & MASK_BUTTON2) == 0)) ? true : false;
    button3_pressed = ((diff & MASK_BUTTON3) && ((ext_io_val & MASK_BUTTON3) == 0)) ? true : false;
    ssd1306_ClearOLED();
    if (button1_pressed) {
        g_mode = (g_mode >= (MODE_END - 1)) ? 0 : (g_mode + 1);
        ButtonDesplay(g_mode);
    } else if (button2_pressed || button3_pressed) {
        ButtonSet(g_mode, button2_pressed);
    }
    ext_io_val_d = ext_io_val;
}


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

void CarSquar(void)
{
    if (g_CarStarted) {
        car_forward(car_drive.LeftForward, car_drive.RightForward);
        TaskMsleep(car_drive.time);
        while (fabs(Yaw) < car_drive.yaw1) {
            Lsm_Get_RawAcc();
            car_left(car_drive.TurnRight);
        }
        car_forward(car_drive.LeftForward, car_drive.RightForward);
        TaskMsleep(car_drive.time);
        while (fabs(Yaw) < car_drive.yaw2) {
            Lsm_Get_RawAcc();
            car_left(car_drive.TurnRight);
        }
        car_forward(car_drive.LeftForward, car_drive.RightForward);
        TaskMsleep(car_drive.time);
        while (fabs(Yaw) < car_drive.yaw3) {
            Lsm_Get_RawAcc();
            car_left(car_drive.TurnRight);
        }
        car_forward(car_drive.LeftForward, car_drive.RightForward);
        TaskMsleep(car_drive.time);
        while (fabs(Yaw) < car_drive.yaw4) {
            Lsm_Get_RawAcc();
            car_left(car_drive.TurnRight);
        }
        car_stop();
        g_CarStarted = !g_CarStarted;
        ssd1306_printf(g_CarStarted ? "start" : "stop");
    } else {
        car_stop();
    }
}

static void IMUSquarTask(void)
{
    uint32_t ret;
    InitPCA9555();
    GA12N20Init();
    LSM6DS_Init();
    TaskMsleep(100); // 等待100ms初始化完成
    ret = LSM6DS_WriteRead(LSM6DSL_WHO_AM_I, 1, 1);
    printf("who am i: %X\n", ret);
    init_ctrl_algo();
    init_oled_mode();
    PCA_RegisterEventProcFunc(ButtonPressProc);
    while (1) {
        CarSquar();
    }
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

void IMUSquarSampleEntry(void)
{
    osThreadAttr_t attr;
    attr.name = "IMUSquarTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 5; // 堆栈大小5*1024
    attr.priority = osPriorityNormal;

    if (osThreadNew(IMUSquarTask, NULL, &attr) == NULL) {
        printf("[RobotTask] Failed to create Aht20TestTask!\n");
    }
}
APP_FEATURE_INIT(IMUSquarSampleEntry);