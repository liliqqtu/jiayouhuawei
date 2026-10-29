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
#include "iot_config.h"
#include "iot_main.h"
#include "iot_profile.h"
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "wifi_connecter.h"
#include "iot_gpio_ex.h"
#include "iot_gpio.h"
#include "iot_watchdog.h"
#include "cjson_init.h"
#include "hi_stdlib.h"
#include "app_demo_iot.h"

/* attribute initiative to report */
#define TAKE_THE_INITIATIVE_TO_REPORT
/* oc request id */
#define CN_COMMAND_INDEX                    "commands/request_id="
/* oc report HiSpark attribute */
#define TRAFFIC_LIGHT_CMD_CONTROL_MODE      "ControlModule"
#define TRAFFIC_LIGHT_RED_ON_PAYLOAD        "RED_LED_ON"
#define TRAFFIC_LIGHT_RED_OFF_PAYLOAD        "RED_LED_OFF"

#define TASK_SLEEP_1000MS (1000)
int g_lightStatus = -1;

void RedLedInit(void)
{
    IoTGpioInit(IOT_IO_NAME_GPIO_9);
    // 设置GPIO9的管脚复用关系为GPIO
    IoSetFunc(IOT_IO_NAME_GPIO_9, IOT_IO_FUNC_GPIO_9_GPIO);
    // GPIO方向设置为输出
    IoTGpioSetDir(IOT_IO_NAME_GPIO_9, IOT_GPIO_DIR_OUT);
}

void RedLight(void)
{
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE1);
    g_lightStatus = 0;
}

void RedOff(void)
{
    IoTGpioSetOutputVal(IOT_IO_NAME_GPIO_9, IOT_GPIO_VALUE0);
    g_lightStatus = 1;
}

static void TrafficLightMsgRcvCallBack(char *payload)
{
    printf("PAYLOAD:%s\r\n", payload);
    if (strstr(payload, TRAFFIC_LIGHT_CMD_CONTROL_MODE) != NULL) {
        if (strstr(payload, TRAFFIC_LIGHT_RED_ON_PAYLOAD) != NULL) { // RED LED
            RedLight();
        } else if (strstr(payload, TRAFFIC_LIGHT_RED_OFF_PAYLOAD) != NULL) {
            RedOff();
        }
    }
}

// /< this is the callback function, set to the mqtt, and if any messages come, it will be called
// /< The payload here is the json string
static void DemoMsgRcvCallBack(int qos, char *topic, char *payload)
{
    const char *requesID;
    char *tmp;
    IoTCmdResp resp;
    printf("RCVMSG:QOS:%d TOPIC:%s PAYLOAD:%s\r\n", qos, topic, payload);
    /* app 下发的操作 */
    TrafficLightMsgRcvCallBack(payload);
    tmp = strstr(topic, CN_COMMAND_INDEX);
    if (tmp != NULL) {
        // /< now you could deal your own works here --THE COMMAND FROM THE PLATFORM
        // /< now er roport the command execute result to the platform
        requesID = tmp + strlen(CN_COMMAND_INDEX);
        resp.requestID = requesID;
        resp.respName = NULL;
        resp.retCode = 0;   ////< which means 0 success and others failed
        resp.paras = NULL;
        (void)IoTProfileCmdResp(CONFIG_DEVICE_PWD, &resp);
    }
}

/* traffic light:1.control module */
void IotPublishSample(void)
{
    IoTProfileService service;
    IoTProfileKV property;
    printf("traffic light:control module\r\n");
    memset_s(&property, sizeof(property), 0, sizeof(property));
    property.type = EN_IOT_DATATYPE_STRING;
    property.key = "ControlModule";
    if (g_lightStatus == 0) {
        property.value = "RED_LED_ON";
    } else {
        property.value = "RED_LED_OFF";
    }
    memset_s(&service, sizeof(service), 0, sizeof(service));
    service.serviceID = "TrafficLight";
    service.serviceProperty = &property;
    IoTProfilePropertyReport(CONFIG_DEVICE_ID, &service);
}

// /< this is the demo main task entry,here we will set the wifi/cjson/mqtt ready ,and
// /< wait if any work to do in the while
static void DemoEntry(void)
{
    ConnectToHotspot();
    RedLedInit();
    CJsonInit();
    IoTMain();
    IoTSetMsgCallback(DemoMsgRcvCallBack);
    TaskMsleep(30000); // 30000 = 3s连接华为云
/* 主动上报 */
    while (1) {
        // here you could add your own works here--we report the data to the IoTplatform
        TaskMsleep(TASK_SLEEP_1000MS);
        // know we report the data to the iot platform
        IotPublishSample();
    }
}

// This is the demo entry, we create a task here, and all the works has been done in the demo_entry
#define CN_IOT_TASK_STACKSIZE  0x1000
#define CN_IOT_TASK_PRIOR 28
#define CN_IOT_TASK_NAME "IOTDEMO"
static void AppDemoIot(void)
{
    osThreadAttr_t attr;
    IoTWatchDogDisable();
    attr.name = "IOTDEMO";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = CN_IOT_TASK_STACKSIZE;
    attr.priority = CN_IOT_TASK_PRIOR;

    if (osThreadNew((osThreadFunc_t)DemoEntry, NULL, &attr) == NULL) {
        printf("[TrafficLight] Failed to create IOTDEMO!\n");
    }
}

SYS_RUN(AppDemoIot);