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

#ifndef HAL_BSP_WIFI_H
#define HAL_BSP_WIFI_H

#include "cmsis_os2.h"
#include "wifi_error_code.h"

/**
 * @brief WIFI  创建wifi热点
 * @param ssid WiFi名称
 * @param psk  WiFi密码
 * @return Returns {WIFI_SUCCESS} 成功;
 *         Returns {other} 失败.
 */
WifiErrorCode WiFi_createHotspots(const char *ssid, const char *psk);
/**
 * @brief WIFI  连接附近的WiFi
 * @param ssid WiFi名称
 * @param psk  WiFi密码
 * @return Returns {WIFI_SUCCESS} 成功;
 *         Returns {other} 失败.
 */
WifiErrorCode WiFi_connectHotspots(const char *ssid, const char *psk);
/**
 * @brief  获取连接WiFi后的本地IP地址
 * @retval IP地址-字符串
 */
char* WiFi_GetLocalIP(void);

#endif

