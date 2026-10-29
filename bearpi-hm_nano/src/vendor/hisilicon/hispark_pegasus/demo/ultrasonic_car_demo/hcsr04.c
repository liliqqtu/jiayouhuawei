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
#include <stdlib.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "gyro.h"
#include "ssd1306.h"
#include "iot_errno.h"
#include "iot_gpio_ex.h"
#include "pca9555.h"
#include "motor_control.h"
#include "sg92r_control.h"
#include "hi_time.h"
#include "hcsr04.h"

CAR_DRIVE car_drive = { 0 };
ENUM_MODE g_mode = MODE_ON_OFF;

int g_CarStarted = 0;
float yaw_data = 0.0f;

#define MASK_BUTTON1        (0x10)
#define MASK_BUTTON2        (0x08)
#define MASK_BUTTON3        (0x04)
#define YAW                 (90.0)
#define DISTANCE            (15.0)
#define CAR_TURN_LEFT                     (0)
#define CAR_TURN_RIGHT                    (1)

void init_ctrl_algo(void)
{
    (void)memset(car_drive, 0, sizeof(CAR_DRIVE));
    car_drive.LeftForward = 13; // 13 左轮前进速度
    car_drive.RightForward = 10; // 10 右轮前进速度
    car_drive.TurnLeft = 30; // 30 左转弯右轮速度
    car_drive.TurnRight = 30; // 30 右转弯左轮速度
    car_drive.yaw = YAW;
    car_drive.distance = DISTANCE;
    car_drive.leftangle = 2500; // 2500 舵机左转90度
    car_drive.middangle = 1500; // 1500 舵机居中
    car_drive.rightangle = 500; // 500 舵机右转90度
}

void init_oled_mode(void)
{
    g_mode = MODE_ON_OFF;
    ssd1306_ClearOLED();
    ssd1306_printf("LF:%d, RF:%d", car_drive.LeftForward, car_drive.RightForward);
    ssd1306_printf("TL:%d, TR:%d", car_drive.TurnRight, car_drive.TurnLeft);
    ssd1306_printf("yaw:%.02f", car_drive.yaw);
    ssd1306_printf("distance:%.2f", car_drive.distance);
}

void ButtonDesplay(ENUM_MODE mode)
{
    switch (mode) {
        case MODE_ON_OFF:
            ssd1306_printf("LF:%d, RF:%d", car_drive.LeftForward, car_drive.RightForward);
            ssd1306_printf("TL:%d, TR:%d", car_drive.TurnRight, car_drive.TurnLeft);
            ssd1306_printf("yaw:%.2f", car_drive.yaw);
            ssd1306_printf("distance:%.2f", car_drive.distance);
            break;
        case MODE_SET_LEFT_FORWARD:
            ssd1306_printf("Set LForward=%d", car_drive.LeftForward);
            break;
        case MODE_SET_RIGHT_FORWARD:
            ssd1306_printf("Set RForward=%d", car_drive.RightForward);
            break;
        case MODE_SET_TURN_LEFT:
            ssd1306_printf("Set TurnLeft=%d", car_drive.TurnRight);
            break;
        case MODE_SET_TURN_RIGHT:
            ssd1306_printf("Set TurnRight=%d", car_drive.TurnLeft);
            break;
        case MODE_SET_YAW:
            ssd1306_printf("Set yaw = %.2f", car_drive.yaw);
            break;
        case MODE_SET_DISTANCE:
            ssd1306_printf("Set distance=%.2f", car_drive.distance);
            break;
        case MODE_SET_LEFTSG92R:
            ssd1306_printf("Set LSg92r = %u", car_drive.leftangle);
            break;
        case MODE_SET_MIDDERSG92R:
            ssd1306_printf("Set MSg92r = %u", car_drive.middangle);
            break;
        case MODE_SET_RIGHTSG92R:
            ssd1306_printf("Set RSg92r = %u", car_drive.rightangle);
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
            ssd1306_printf("TurnLeft=%d", car_drive.TurnRight);
            break;
        case MODE_SET_TURN_RIGHT:
            car_drive.TurnLeft += ((button_pressed) ? -1 : 1);
            ssd1306_printf("TurnRight=%d", car_drive.TurnLeft);
            break;
        case MODE_SET_YAW:
            car_drive.yaw += ((button_pressed) ? -0.1 : 0.1); // 航向角每次增加或者减少0.1
            ssd1306_printf("yaw =%.2f", car_drive.yaw);
            break;
        case MODE_SET_DISTANCE:
            car_drive.distance += (button_pressed ? -0.1 : 0.1); // 距离每次增加或者减少0.1
            ssd1306_printf("distance=%.2f", car_drive.distance);
            break;
        case MODE_SET_LEFTSG92R:
            car_drive.leftangle += (button_pressed ? -100 : 100); // 舵机左转每次增加或者减少100
            ssd1306_printf("MidderSg92r = %u", car_drive.leftangle);
            break;
        case MODE_SET_MIDDERSG92R:
            car_drive.middangle += (button_pressed ? -100 : 100); // 舵机居中每次增加或者减少100
            ssd1306_printf("MidderSg92r = %u", car_drive.middangle);
            break;
        case MODE_SET_RIGHTSG92R:
            car_drive.rightangle += (button_pressed ? -100 : 100); // 舵机右转每次增加或者减少100
            ssd1306_printf("MidderSg92r = %u", car_drive.rightangle);
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

void Hcsr04Init(void)
{
    // 设置超声波Echo为输入模式
    // 设置GPIO8功能（设置为GPIO功能）
    IoSetFunc(IOT_IO_NAME_GPIO_8, IOT_IO_FUNC_GPIO_8_GPIO);
    // 设置GPIO8为输入方向
    IoTGpioSetDir(IOT_IO_NAME_GPIO_8, IOT_GPIO_DIR_IN);

    // 设置GPIO7功能（设置为GPIO功能）
    IoSetFunc(IOT_IO_NAME_GPIO_7, IOT_IO_FUNC_GPIO_7_GPIO);
    // 设置GPIO7为输出方向
    IoTGpioSetDir(IOT_IO_NAME_GPIO_7, IOT_GPIO_DIR_OUT);
}

float GetDistance(void)
{
    // 定义变量
    static unsigned long start_time = 0, time = 0;
    float distance = 0.0;
    IotGpioValue value = IOT_GPIO_VALUE0;
    unsigned int flag = 0;

    // 设置GPIO7输出低电平
    /* 给trig发送至少10us的高电平脉冲，以触发传感器测距 */
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE1);
    // 20us延时函数（设置高电平持续时间）
    hi_udelay(20);
    // 设置GPIO7输出高电平
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_7, IOT_GPIO_VALUE0);
    /* 计算与障碍物之间的距离 */
    while (1) {
        // 获取GPIO8的输入电平状态
        IoTGpioGetInputVal(IOT_IO_NAME_GPIO_8, &value);
        // 判断GPIO8的输入电平是否为高电平并且flag为0
        if (value == IOT_GPIO_VALUE1 && flag == 0) {
            // 获取系统时间
            start_time = hi_get_us();
            // 将flag设置为1
            flag = 1;
        }
        // 判断GPIO8的输入电平是否为低电平并且flag为1
        if (value == IOT_GPIO_VALUE0 && flag == 1) {
            // 获取高电平持续时间
            time = hi_get_us() - start_time;
            break;
        }
    }
    // 计算距离障碍物距离（340米/秒 转换为 0.034厘米/微秒），一去一来2倍距离
    distance = time * 0.034 / 2;
    return distance;
}

/* Judge steering gear */
unsigned int engine_go_where(void)
{
    unsigned int temp;
    float left_distance = 0.0;
    float right_distance = 0.0;
    /* 舵机往左转动测量左边障碍物的距离 */

    EngineTurnLeft(car_drive.leftangle);
    TaskMsleep(200); // 200ms
    left_distance = GetDistance();
    TaskMsleep(200); // 200ms
    /* 归中 */
    RegressMiddle(car_drive.middangle);
    TaskMsleep(200); // 200ms

    /* 舵机往右转动测量右边障碍物的距离 */
    EngineTurnRight(car_drive.rightangle);
    TaskMsleep(200); // 200ms
    right_distance = GetDistance();
    TaskMsleep(200); // 200ms
    /* 归中 */
    RegressMiddle(car_drive.middangle);

    if (left_distance > right_distance) {
        temp =  CAR_TURN_LEFT;
    } else {
        temp =  CAR_TURN_RIGHT;
    }
    return temp;
}

/*
 * 根据障碍物的距离来判断小车的行走方向
 * 1、距离大于等于15cm继续前进
 * 2、距离小于15cm，先停止再后退0.1s,继续进行测距,再进行判断
 */
/* Judge the direction of the car */
void car_where_to_go(float distance)
{
    if (distance < car_drive.distance) {
        car_backward(car_drive.LeftForward, car_drive.RightForward);
        TaskMsleep(500); // 后退500ms
        car_stop();
        unsigned int ret = engine_go_where();
        if (ret == CAR_TURN_LEFT) {
            while ((GetYaw() - yaw_data) < car_drive.yaw) {
                Lsm_Get_RawAcc();
                car_left(car_drive.TurnRight);
            }
        } else if (ret == CAR_TURN_RIGHT) {
            while ((yaw_data - GetYaw()) < car_drive.yaw) {
                Lsm_Get_RawAcc();
                car_right(car_drive.TurnLeft);
            }
        }
    } else {
        car_forward(car_drive.LeftForward, car_drive.RightForward);
    }
    yaw_data = GetYaw();
}

/* 超声波避障 */
void ultrasonic_demo(void)
{
    float m_distance = 0.0;
    /* 获取前方物体的距离 */
    m_distance = GetDistance();
    car_where_to_go(m_distance);
    TaskMsleep(20); // 20ms执行一次
}

void UltrasonicDemoTask(void)
{
    InitPCA9555();
    S92RInit();
    GA12N20Init();
    Hcsr04Init();
    LSM6DS_Init();
    TaskMsleep(100); // 等待100ms初始化完成
    init_ctrl_algo();
    init_oled_mode();
    PCA_RegisterEventProcFunc(ButtonPressProc);
    while (1) {
        if (g_CarStarted) {
            ultrasonic_demo();
        } else {
            car_stop();
        }
    }
}

void UltrasonicSampleEntry(void)
{
    osThreadAttr_t attr;
    attr.name = "UltrasonicDemoTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 5; /* 堆栈大小为1024*5 */
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)UltrasonicDemoTask, NULL, &attr) == NULL) {
        printf("[UltrasonicDemoTask] Failed to create UltrasonicDemoTask!\n");
    }
}
APP_FEATURE_INIT(UltrasonicSampleEntry);