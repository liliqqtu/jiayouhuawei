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

#ifndef HAL_BSP_MQTT_H
#define HAL_BSP_MQTT_H

/**
 * @brief MQTT  连接MQTT服务器
 * @return Returns {0} 成功;
 *         Returns {-1} 失败.
 */
int MQTTClient_connectServer(const char *ip_addr, int ip_port);
/**
 * @brief MQTT  断开连接MQTT服务器
 * @return Returns {0} 成功;
 *         Returns {-1} 失败.
 */
int MQTTClient_unConnectServer(void);
/**
 * @brief MQTT  订阅MQTT主题
 * @return Returns {0} 成功;
 *         Returns {-1} 失败.
 */
int MQTTClient_subscribe(char *subTopic);
/**
 * @brief MQTT 客户端的初始化
 * @param clientID  客户端ID
 * @param userName  用户名
 * @param password  密码
 * @return Returns {0} 成功;
 *         Returns {-1} 失败.
 */
int MQTTClient_init(char *clientID, char *userName, char *password);
/**
 * @brief MQTT 发布消息
 * @param pub_Topic 具有发布权限的主题名称
 * @param payloadData  发布数据
 * @param payloadLen 发布数据的长度
 * @return Returns {0} 成功;
 *         Returns {-1} 失败.
 */
int MQTTClient_pub(char *pub_Topic, unsigned char *payloadData, int payloadLen);
/**
 * @brief MQTT  接收消息
 * @param callback 当接收到消息之后，将消息传到到回调函数中
 * @return Returns {0} 成功;
 *         Returns {-1} 失败.
 */
int MQTTClient_sub(void);

extern int8_t(*p_MQTTClient_sub_callback)(unsigned char *topic, unsigned char *payload);

#endif // !__HAL_BSP_MQTT_H
