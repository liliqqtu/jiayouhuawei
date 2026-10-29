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
#include <string.h>
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifi_starter.h"

static void WifiHotspotTask(void)
{
    WifiErrorCode errCode;
    HotspotConfig config = { 0 };

    // 设置AP的配置参数 set configuration parameters for AP
    strcpy(config.ssid, "HiSpark-AP"); // AP :HiSpark-AP
    strcpy(config.preSharedKey, "12345678"); // Password：12345678
    config.securityType = WIFI_SEC_TYPE_PSK;
    config.band = HOTSPOT_BAND_TYPE_2G;
    config.channelNum = 7; /* 通道7 Channel 7 */

    osDelay(10); /* 10 = 100ms */

    printf("starting AP ...\r\n");
    errCode = StartHotspot(&config);
    printf("StartHotspot: %d\r\n", errCode);

    int timeout = 60; /* 60 = 1 minute */
    while (timeout--) {
        printf("After %d seconds Ap will turn off!\r\n", timeout);
        osDelay(100); /* 100 = 1s */
    }

    printf("stop AP ...\r\n");
    StopHotspot();
    printf("stop AP ...\r\n");
    osDelay(10); /* 10 = 100ms */
}

static void WifiHotspotDemo(void)
{
    osThreadAttr_t attr;

    attr.name = "WifiHotspotTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240; // 任务栈10240, stack size 10240
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)WifiHotspotTask, NULL, &attr) == NULL) {
        printf("[WifiHotspotDemo] Falied to create WifiHotspotTask!\n");
    }
}

SYS_RUN(WifiHotspotDemo);

