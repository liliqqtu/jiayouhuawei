/*
 * Copyright (C) 2021 HiHope Open Source Organization .
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
 *
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_pwm.h"
#include "iot_i2c.h"
#include "hi_io.h"
#include "iot_errno.h"
#include "iot_watchdog.h"
#include "wifi_device.h"

#include "lwip/netifapi.h"
#include "lwip/api_shell.h"
#include "lwip/sockets.h"

#include "ssd1306.h"
#include "net_params.h"
#include "wifi_connecter.h"

#define OLED_I2C_BAUDRATE (400 * 1000)
#define STACK_SIZE        (10240)
#define IOT_GPIO_13       (13)
#define IOT_GPIO_14       (14)
#define DELAY_TICKS_2     (2)
#define DELAY_TICKS_50    (50)

#define STATUS_OK 0

static int g_serverPort = PARAM_SERVER_PORT;
static const char* g_serverIp = PARAM_SERVER_ADDR;

static uint8_t g_streamBuffer[SSD1306_BUFFER_SIZE];

static uint32_t PlayStream(void)
{
    uint32_t frameIdMax = 1000000;  // for while loop condition
    uint32_t frameId = 0;
    int sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("lwip_socket failed!\r\n");
        return 0;
    }

    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = lwip_htons(g_serverPort);
    if (lwip_inet_pton(AF_INET, g_serverIp, &serverAddr.sin_addr) <= 0) {
        printf("lwip_inet_pton failed!\r\n");
        // goto do_close;
        lwip_close(sockfd);
        return frameId;
    }

    if (lwip_connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        printf("lwip_connect failed!\r\n");
        // goto do_close;
        lwip_close(sockfd);
        return frameId;
    }
    printf("connect to server %s success!\r\n", g_serverIp);

    frameId = 1;
    while (frameId < frameIdMax) {
        printf("send frameId request %u to server\r\n", frameId);
        uint32_t request = lwip_htonl(frameId); // to big endian
        ssize_t retval = lwip_send(sockfd, &request, sizeof(request), 0);
        if (retval < 0) {
            printf("lwip_send for frame %u failed!\r\n", frameId);
            break;
        }

        // printf("recving status from server...\r\n");
        uint32_t status = 0;
        retval = lwip_recv(sockfd, &status, sizeof(status), 0);
        if (retval != sizeof(status)) {
            printf("lwip_recv status for frame %u failed or done, %zd!\r\n", frameId, retval);
            break;
        }
        status = lwip_ntohl(status);
        if (status != STATUS_OK) {
            break;
        }

        // printf("recving bodyLen from server...\r\n");
        ssize_t bodyLen = 0;
        retval = lwip_recv(sockfd, &bodyLen, sizeof(bodyLen), 0);
        if (retval != sizeof(bodyLen)) {
            printf("lwip_recv bodyLen for frame %u failed or done, %zd!\r\n", frameId, retval);
            break;
        }
        bodyLen = lwip_ntohl(bodyLen);
        printf("status: %u, bodyLen: %zd\r\n", status, bodyLen);

        ssize_t bodyReceived = 0;
        while (bodyReceived < bodyLen) {
            retval = lwip_recv(sockfd, &g_streamBuffer[bodyReceived], bodyLen, 0);
            if (retval != bodyLen) {
                printf("lwip_recv for frame %u failed or done, %zd!\r\n", frameId, retval);
                break;
            }
            bodyReceived += retval;
            printf("recved body %zd/%zd...\r\n", bodyReceived, bodyLen);
        }

        ssd1306_Fill(Black);
        ssd1306_DrawBitmap(g_streamBuffer, sizeof(g_streamBuffer));
        ssd1306_UpdateScreen();
        frameId++;
    }
    printf("playing video done, played frames: %u!\r\n", frameId);

// do_close:
//    lwip_close(sockfd);
//    return frameId;
}

static void Ssd1306PlayTask(void)
{
    IoTGpioInit(IOT_GPIO_13);
    IoTGpioInit(IOT_GPIO_14);
    hi_io_set_func(IOT_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(IOT_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);

    IoTI2cInit(0, OLED_I2C_BAUDRATE);

    IoTWatchDogDisable();

    osDelay(DELAY_TICKS_2);
    ssd1306_Init();

    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString("Hello HarmonyOS!", Font_7x10, White);
    ssd1306_UpdateScreen();

    // prepare hotspot prarams
    WifiDeviceConfig apConfig = {};
    // strcpy(apConfig.ssid, PARAM_HOTSPOT_SSID);
    // strcpy(apConfig.preSharedKey, PARAM_HOTSPOT_PSK);
    strcpy_s(apConfig.ssid, WIFI_MAX_SSID_LEN, PARAM_HOTSPOT_SSID);
    strcpy_s(apConfig.preSharedKey, WIFI_MAX_KEY_LEN, PARAM_HOTSPOT_PSK);
    apConfig.securityType = PARAM_HOTSPOT_TYPE;

    int netId = ConnectToHotspot(&apConfig);
    if (netId < 0) {
        printf("connect to hotspot failed!\r\n");
    }

    uint32_t start = osKernelGetTickCount();
    uint32_t frames = PlayStream();
    uint32_t end = osKernelGetTickCount();

    printf("frames: %u, time cost: %.2f\r\n", frames, (end - start) / (float)osKernelGetTickFreq());
    osDelay(DELAY_TICKS_50);

    DisconnectWithHotspot(netId);
}

static void Ssd1306PlayDemo(void)
{
    osThreadAttr_t attr;

    attr.name = "Ssd1306Task";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(Ssd1306PlayTask, NULL, &attr) == NULL) {
        printf("[Ssd1306PlayDemo] Falied to create Ssd1306PlayTask!\n");
    }
}
APP_FEATURE_INIT(Ssd1306PlayDemo);
