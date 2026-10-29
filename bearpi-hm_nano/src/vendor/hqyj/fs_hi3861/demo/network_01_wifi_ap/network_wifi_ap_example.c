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
#include <string.h>
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hal_bsp_wifi.h"
#include "wifi_device.h"
#include "hal_bsp_pcf8574.h"

osThreadId_t Task1_ID; // 任务1设置为低优先级任务
#define TASK_STACK_SIZE (1024 * 10)
#define TASK_DELAY_TIME 3 // s

#define ARRAY_INDEX_0 0
#define ARRAY_INDEX_1 1
#define ARRAY_INDEX_2 2
#define ARRAY_INDEX_3 3
#define ARRAY_INDEX_4 4
#define ARRAY_INDEX_5 5

// 当 有设备连接热点时 触发的回调函数
static void connectingWiFihotspotsCallback(StationInfo *info)
{
    if (info == NULL) {
        printf("HotspotStaJoin:info is null.\n");
    } else {
        printf("New Sta Join.\n");
    }
}
// 当 有设备断开热点时 触发的回调函数
static void disconnectWiFihotspotsCallback(StationInfo *info)
{
    if (info == NULL) {
        printf(" HotspotStaLeave:info is null.\n");
    } else {
        // 打印mac地址
        printf("HotspotStaLeave: macAddress=%02X:%02X:%02X:%02X:%02X:%02X, reason=%d.\n",
               info->macAddress[ARRAY_INDEX_0],
               info->macAddress[ARRAY_INDEX_1],
               info->macAddress[ARRAY_INDEX_2],
               info->macAddress[ARRAY_INDEX_3],
               info->macAddress[ARRAY_INDEX_4],
               info->macAddress[ARRAY_INDEX_5],
               info->disconnectedReason);
    }
}
// 当 热点的状态发生改变时的回调函数
static void changeWiFiHotspotStateCallback(int state)
{
    printf("HotspotStateChanged:state is %d.", state);
    if (state == WIFI_HOTSPOT_ACTIVE) {
        printf("wifi hotspot active.\n");
    } else {
        printf("wifi hotspot noactive.\n");
    }
}

void Task1(void)
{
    WifiErrorCode result;

    // 注册WiFi事件的回调函数
    WifiEvent wifiEvent = {0};
    wifiEvent.OnHotspotStaJoin = connectingWiFihotspotsCallback;
    wifiEvent.OnHotspotStaLeave = disconnectWiFihotspotsCallback;
    wifiEvent.OnHotspotStateChanged = changeWiFiHotspotStateCallback;
    result = RegisterWifiEvent(&wifiEvent);
    if (result != WIFI_SUCCESS) {
        printf(" Failed to register WiFi hotspot callback function.\n");
        return;
    }

    sleep(TASK_DELAY_TIME); // 1 s
    
    WiFi_createHotspots("FS_Hi3861_AP", "fs123456");
    while (1) {
        sleep(TASK_DELAY_TIME); // 1 s
    }
}
static void network_wifi_ap_example(void)
{
    printf("Enter network_wifi_ap_example()!.\n");

    PCF8574_Init();
    osThreadAttr_t options;
    options.name = "thread_1";
    options.attr_bits = 0;
    options.cb_mem = NULL;
    options.cb_size = 0;
    options.stack_mem = NULL;
    options.stack_size = TASK_STACK_SIZE;
    options.priority = osPriorityNormal;

    Task1_ID = osThreadNew((osThreadFunc_t)Task1, NULL, &options);
    if (Task1_ID != NULL) {
        printf("ID = %d, Create Task1_ID is OK!.\n", Task1_ID);
    }
}
SYS_RUN(network_wifi_ap_example);