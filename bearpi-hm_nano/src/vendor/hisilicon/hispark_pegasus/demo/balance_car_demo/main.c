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

/* 平衡车直立控制算法演示Demo, 这个Demo里包括了陀螺仪滤波, 电机驱动, AB编码器, PID控制算法,
 * OLED字符串显示, 扩展IO, 定时器中断等功能
 * 固件烧录成功后, 按OLED的提示, S3按键将启动平衡车功能, S1键将切换设置项
 * 烧录固件后, 根据不同的小车的个体差异, 可能需要微调参数. 目前, 单节电池放在下电池仓,
 * 并加装了金属卡扣后, target angle缺省设置为-82.8度:
 * 如果你的小车一直后仰, 试着增大target, 即向0度方向调整
 * 如果你的小车一直前倾, 试着减小target, 即向-90度方向调整
 *
 * Demo of the balance car vertical control algorithm. This Demo includes gyroscope filtering,
 * motor drive, AB encoder, PID control algorithm, OLED string display, extended IO, timer interrupt
 * and other functions. After the firmware is successfully burned, press the OLED prompt, S3 will
 * start the balance car function, and S1 will switch the settings. After the firmware is burned,
 * according to the individual differences of different cars, you may need to fine tune the parameters
 * At present, after a single battery is placed in the lower battery compartment and metal clips are
 * installed, the target angle is set to -82.8 degrees by default:
 * if your car keeps leaning back, try to increase the target, that is, adjust to the direction of 0 degrees
 * If your car keeps leaning forward, try to reduce the target, that is, adjust to the direction of -90 degrees
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>  // For memcpy
#include <stdio.h>
#include <unistd.h>
#include <ohos_init.h>
#include <cmsis_os2.h>
#include <hi_timer.h>
#include "gyro.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "wheel_codec.h"
#include "robot_l9110s.h"
#include "iot_watchdog.h"
#include "ctrl_algo.h"
#include "hi_task.h"
#include "pca9555.h"
#include "debug_util.h"

typedef enum {
    MODE_ON_OFF = 0,
    MODE_STAND_TARGET,
    MODE_STAND_KP,
    MODE_STAND_KD,
    MODE_VELO_TARGET,
    MODE_VELO_KP,
    MODE_VELO_KI,
    MODE_END,
    MODE_TURN,
    MODE_DEBUG_GYRO,
    MODE_KI_ADJ,
} ENUM_MODE;

#define MASK_BUTTON1        (0x10)
#define MASK_BUTTON2        (0x08)
#define MASK_BUTTON3        (0x04)
#define DEAD_ZONE           (1)   // pwm valid range: [-99, -DEAD_ZONE], [DEAD_ZONE, 99]

CTRL_PID_STRUCT ctrl_pid_velocity2 = {0};
CTRL_PID_STRUCT ctrl_pid_stand2 = {0};

ENUM_MODE g_mode = MODE_ON_OFF;
int g_car_started = 0;
static int g_TimerFlag = 0;
uint32_t g_timer_handle;
float g_target_angle = -81.8f;
int g_target_velo = 0;
int g_target_diff = 0;

void print_oled_mode(void)
{
    g_mode = MODE_ON_OFF;
    ssd1306_ClearOLED();
    ctrl_pid_velocity2 = GetVelocity();
    ctrl_pid_stand2 = GetStand();
    ssd1306_printf("kp:%.2f, kd:%.2f", ctrl_pid_stand2.kp, ctrl_pid_stand2.kd);
    ssd1306_printf("kp:%.2f, ki:%.4f", ctrl_pid_velocity2.kp, ctrl_pid_velocity2.ki);
    ssd1306_printf("press S1 to switch\n");
    ssd1306_printf("press S3 to start\n");
}

void timer_svr(hi_u32 data)
{
    data=data;
    g_TimerFlag = 1;
}

void init_gyro_timer(void)
{
    uint32_t ret;

    ret = hi_timer_create(&g_timer_handle);
    if (ret != HI_ERR_SUCCESS) {
        printf("hrtimer create fail\n");
        return;
    } else {
        printf("create timer success\n");
    }
    ret = hi_timer_start(g_timer_handle, HI_TIMER_TYPE_PERIOD, 10, timer_svr, 188); // 10ms, 188
}

void ButtonDesplay(ENUM_MODE mode)
{
    switch (mode) {
        case MODE_ON_OFF:
            ssd1306_printf("stand loop");
            ssd1306_printf("kp:%.2f, kd:%.2f", ctrl_pid_stand2.kp, ctrl_pid_stand2.kd);
            ssd1306_printf("target angle:%.2f", g_target_angle);
            ssd1306_printf("press btn2/3 to start");
            break;
        case MODE_STAND_TARGET:
            ssd1306_printf("target angle=%.1f", g_target_angle);
            break;
        case MODE_STAND_KP:
            ssd1306_printf("stand kp=%.2f", ctrl_pid_stand2.kp);
            break;
        case MODE_STAND_KD:
            ssd1306_printf("stand kd=%.2f", ctrl_pid_stand2.kd);
            break;
        case MODE_VELO_KP:
            ssd1306_printf("velocity kp=%.2f", ctrl_pid_velocity2.kp);
            break;
        case MODE_VELO_KI:
            ssd1306_printf("velocity ki=%.4f", ctrl_pid_velocity2.ki);
            break;
        case MODE_VELO_TARGET:
            ssd1306_printf("velocity target=%d", g_target_velo);
            break;
        case MODE_TURN:
            ssd1306_printf("turn:%d", g_target_diff);
            break;
        case MODE_DEBUG_GYRO:
            ssd1306_printf("dfx");
            ssd1306_printf("press 2 record");
            ssd1306_printf("press 3 report");
            break;
        default:
            print_oled_mode();
            break;
    }
}

void ButtonSet(ENUM_MODE mode, bool button_pressed, bool button4_pressed)
{
    switch (mode) {
        case MODE_ON_OFF:
            g_car_started = !g_car_started;
            ssd1306_ClearOLED();
            ssd1306_printf(g_car_started ? "start" : "stop");
            break;
        case MODE_DEBUG_GYRO:
            if (button_pressed) {
                reset_debug_points();
            }
            if (button4_pressed) {
                print_debug_points();
            }
            break;
        case MODE_STAND_KP:
            ctrl_pid_stand2.kp += ((button_pressed) ? -0.1 : 0.1); // 0.1 kp系数
            ssd1306_printf("stand kp=%.2f", ctrl_pid_stand2.kp);
            break;
        case MODE_STAND_KD:
            ctrl_pid_stand2.kd += (button_pressed ? -0.1 : 0.1); // 0.1 kd系数
            ssd1306_printf("stand kd=%.2f",  ctrl_pid_stand2.kd);
            break;
        case MODE_VELO_KP:
            ctrl_pid_velocity2.kp += ((button_pressed) ? -0.01 : 0.01); // 0.01 kp系数
            ssd1306_printf("velocity kp=%.2f", ctrl_pid_velocity2.kp);
            break;
        case MODE_VELO_KI:
            ctrl_pid_velocity2.ki += ((button_pressed) ? -0.001 : 0.001); // 0.001速度环系数
            ssd1306_printf("velocity ki=%.4f", ctrl_pid_velocity2.ki);
            break;
        case MODE_VELO_TARGET:
            g_target_velo += ((button_pressed) ? -1 : 1);
            ssd1306_printf("velo target =%d", g_target_velo);
            break;
        case MODE_STAND_TARGET:
            g_target_angle += (button_pressed ? -0.1 : 0.1); // 0.1 俯仰角系数
            ssd1306_printf("target_angle=%.1f", g_target_angle);
            break;
        case MODE_TURN:
            g_target_diff += (button_pressed ? -1 : 1);
            ssd1306_printf("turn diff=%d", g_target_diff);
            break;
        default:
            print_oled_mode();
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
        ButtonSet(g_mode, button2_pressed, button3_pressed);
    }
    ext_io_val_d = ext_io_val;
}

void car_state(int pwm)
{
    int pwm_mid = pwm;
/* execute */
    if (g_car_started) {
        if ((pwm_mid > -DEAD_ZONE) && (pwm_mid < DEAD_ZONE)) {
            car_stop();
        } else {
            car_drive(pwm_mid);
        }
    } else {
        car_stop();
    }
}

void BalanceTask(void)
{
    int pwm_mid;
    float bias, exec;
    int16_t pos_right, pos_left;
    static int16_t pos_right_d = 0, pos_left_d = 0;
    int16_t velo_left, velo_right;
    int16_t velo = 0;

    printf("last compile:%s,%s\n", __DATE__, __TIME__);
    hi_sleep(200); // 200ms
    InitGyro();
    init_ctrl_algo();
    ssd1306_Init();
    InitPCA9555();
    init_car_drive();
    car_stop();
    init_test_pin();
    init_gyro_timer();
    init_wheel_codec();
    /* button function */
    print_oled_mode();
    PCA_RegisterEventProcFunc(ButtonPressProc);

    while (1) {
        usleep(1);
        if (g_TimerFlag == 1) {
            g_TimerFlag = 0;
            /* get pitch feedback */
            Lsm_Get_RawAcc();
            /* call ctrl algorithm */
            get_wheel_cnt(&pos_left, &pos_right);
            velo_left = pos_left - pos_left_d;
            velo_right = pos_right - pos_right_d;
            pos_left_d = pos_left;
            pos_right_d = pos_right;
            velo = velo_left + velo_right;
            bias = ctrl_pid_algo(g_target_velo, velo, &ctrl_pid_velocity2);
            float pitch = GetPitchValue();
            exec = ctrl_pid_algo(g_target_angle + bias, pitch, &ctrl_pid_stand2);
            pwm_mid = (int)(exec);
            /* dfx */
            append_debug_point(velo_left);
            append_debug_point(velo_right);
            append_debug_point((int16_t)(bias * 100)); // 100
            append_debug_point((int16_t)((g_target_angle + bias - pitch) * 100)); // 100
            append_debug_point(pwm_mid);
            car_state(pwm_mid);
        }
    }
}


void BalanceDemo(void)
{
    osThreadAttr_t attr;
    init_car_drive();
    IoTWatchDogDisable();
    attr.name = "BalanceDemo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 5 * 1024; // 任务栈大小为5 *1024
    attr.priority = osPriorityNormal;
    if (osThreadNew((osThreadFunc_t)BalanceTask, NULL, &attr) == NULL) {
        printf("[BalanceTask] Failed to create BalanceTask!\n");
    }
}

APP_FEATURE_INIT(BalanceDemo);