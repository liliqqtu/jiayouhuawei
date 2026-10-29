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
#include <memory.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
/* Link Header Files */
#include "link_service.h"
#include "link_platform.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_pwm.h"
#include "hi_task.h"
#include "iot_watchdog.h"
#include "hi_types_base.h"
#include "hi_time.h"
#include "wifi_starter.h"
#include "hcsr04.h"
#include "sg92r_control.h"
#include "motor_control.h"
#include "iot_gpio_ex.h"
#include "histreaming.h"

unsigned char g_car_control_mode = 0;
unsigned char g_car_speed_control = 0;
unsigned char g_car_status = CAR_STOP_STATUS;
unsigned short g_car_modular_control_module = 0;
unsigned short g_car_direction_control_module = 0;

static void car_direction_control_func(void)
{
    switch (g_car_direction_control_module) {
        case CAR_KEEP_GOING_TYPE: // 一直往前
            car_forward();
            break;
        case CAR_KEEP_GOING_BACK_TYPE: // 一直往后
            car_backward();
            break;
        case CAR_KEEP_TURN_LEFT_TYPE:  // 一直往左
            car_left();
            break;
        case CAR_KEEP_TURN_RIGHT_TYPE: // 一直往右
            car_right();
            break;
        case CAR_STOP_TYPE:            // 车子停止
            car_stop();
        default:
            break;
    }
    g_car_direction_control_module = 0;
}

/* car mode control func */
void car_mode_control_func(void)
{
    unsigned short current_car_modular_control_module = g_car_modular_control_module;
    unsigned short current_car_control_mode = g_car_control_mode;
    RegressMiddle();
    switch (g_car_modular_control_module) {
        case CAR_CONTROL_STEER_ENGINE_TYPE: // 有舵机
            while (1) {
                if ((current_car_modular_control_module != g_car_modular_control_module) ||
                    (current_car_control_mode != g_car_control_mode)) {
                    printf("car_mode_control_func 1 module changed\n");
                    RegressMiddle();
                    break;
                }
                /* 获取前方物体的距离 */
                ultrasonic_demo();
            }
            break;
        default:
            break;
    }
}

void car_module_control_func(void)
{
    switch (g_car_modular_control_module) {
        case CAR_CONTROL_ENGINE_LEFT_TYPE:
            EngineTurnLeft();
            break;
        case CAR_CONTROL_ENGINE_RIGHT_TYPE:
            EngineTurnRight();
            break;
        case CAR_CONTROL_ENGINE_MIDDLE_TYPE:
            RegressMiddle();
            break;
        case CAR_CONTROL_STEER_ENGINE_TYPE:
            car_mode_control_func();
            break;
        default:
            break;
    }
    g_car_modular_control_module = 0;
}

/* modular control func */
static void car_modular_control(char* value)
{
    printf("car_modular_control\n");
    if (strcmp(value, "engine_left") == 0) { // 舵机向左
        g_car_modular_control_module = CAR_CONTROL_ENGINE_LEFT_TYPE;
    } else if (strcmp(value, "engine_right") == 0) { // 舵机向右
        g_car_modular_control_module = CAR_CONTROL_ENGINE_RIGHT_TYPE;
    } else if (strcmp(value, "engine_middle") == 0) { // 舵机居中
        g_car_modular_control_module = CAR_CONTROL_ENGINE_MIDDLE_TYPE;
    } else if (strcmp(value, "steer_engine") == 0) { // 启动舵机超声波避障模块
        g_car_modular_control_module = CAR_CONTROL_STEER_ENGINE_TYPE;
    }
}

/* car direction control */
static void car_direction_control(char* value)
{
    printf("car_direction_control\n");
    if (strcmp(value, "going") == 0) {
        g_car_direction_control_module = CAR_KEEP_GOING_TYPE;
    } else if (strcmp(value, "backing") == 0) {
        g_car_direction_control_module = CAR_KEEP_GOING_BACK_TYPE;
    } else if (strcmp(value, "lefting") == 0) {
        g_car_direction_control_module = CAR_KEEP_TURN_LEFT_TYPE;
    } else if (strcmp(value, "righting") == 0) {
        g_car_direction_control_module = CAR_KEEP_TURN_RIGHT_TYPE;
    } else if (strcmp(value, "stop") == 0) {
        g_car_direction_control_module = CAR_STOP_TYPE;
    }
}

static int GetStatusValue(struct LinkService* ar, const char* property, char* value, int len)
{
    (void)(ar);

    printf("Receive property: %s(value=%s[%d])\n", property, value, len);
    if (strcmp(property, "Status") == 0) {
    }

    /*
     * if Ok return 0,
     * Otherwise, any error, return StatusFailure
     */
    return 0;
}
/* recv from app cmd */
static int ModifyStatus(struct LinkService* ar, const char* property, char* value, int len)
{
    (void)(ar);

    if (property == NULL || value == NULL) {
        return -1;
    }

    printf("Receive property: %s(value=%s[%d])\r\n", property, value, len);
    if (strcmp(property, "CarControl") == 0) {
        g_car_control_mode = CAR_DIRECTION_CONTROL_MODE;
        car_direction_control(value);
    } else if (strcmp(property, "ModularControl") == 0) {
        g_car_control_mode = CAR_MODULE_CONTROL_MODE;
        car_modular_control(value);
    }
    /*
     * if Ok return 0,
     * Otherwise, any error, return StatusFailure
     */
    return 0;
}

/*
 * It is a Wifi IoT device
 */
static const char* g_wifista_type = "Light";
static const char* GetDeviceType(struct LinkService* ar)
{
    (void)(ar);

    return g_wifista_type;
}

static void *g_link_platform = NULL;

void histreaming_open(void)
{
    LinkService* wifiIot = 0;
    LinkPlatform* link = 0;

    wifiIot = (LinkService*)malloc(sizeof(LinkService));
    if (!wifiIot) {
        printf("malloc wifiIot failure\n");
    }

    wifiIot -> get    = GetStatusValue;
    wifiIot -> modify = ModifyStatus;
    wifiIot -> type = GetDeviceType;
    
    link = LinkPlatformGet();
    if (!link) {
        printf("get link failure\n");
    }

    if (link -> addLinkService(link, wifiIot, 1) != 0) {
        histreaming_close(link);
    }

    if (link->open(link) != 0) {
        histreaming_close(link);
    }

    /* cache link ptr */
    g_link_platform = (void*)(link);
}

void histreaming_close(LinkPlatform *link)
{
    LinkPlatform *linkPlatform = (LinkPlatform*)(link);
    if (!linkPlatform) {
        printf("failed\r\n");
    }

    linkPlatform->close(linkPlatform);

    if (linkPlatform != NULL) {
        LinkPlatformFree(linkPlatform);
    }
}

void WifiRobotTask(void)
{
    S92RInit();
    GA12N20Init();
    Hcsr04Init();
    StartHotspot();
    histreaming_open();
    while (1) {
        switch (g_car_control_mode) {
            case CAR_DIRECTION_CONTROL_MODE:
                car_direction_control_func();
                break;
            case CAR_MODULE_CONTROL_MODE:
                car_module_control_func();
                break;
            default:
                break;
        }
        TaskMsleep(200); // 200ms
    }
}

static void StartWifiRobotSampleEntry(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "WifiRobotTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 1024 * 5; /* 堆栈大小为1024*5 */
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)WifiRobotTask, NULL, &attr) == NULL) {
        printf("[WifiRobotTask] Failed to create WifiRobotTask!\n");
    }
}
APP_FEATURE_INIT(StartWifiRobotSampleEntry);