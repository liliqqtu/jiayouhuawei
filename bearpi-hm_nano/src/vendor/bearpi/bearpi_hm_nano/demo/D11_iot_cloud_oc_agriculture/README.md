# BearPi-HM_Nano开发板智慧农业案例开发
本示例将演示如何在BearPi-HM_Nano开发板上使用MQTT协议连接华为IoT平台,使用的是E53_IA1 智慧农业扩展板与 BearPi-HM_Nano 开发板，设备安装如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/E53_IA1安装.png "E53_IA1安装")

## 软件设计


### 连接平台
在连接平台前需要设置获取CONFIG_APP_DEVICEID、CONFIG_APP_DEVICEPWD、CONFIG_APP_SERVERIP、CONFIG_APP_SERVERPORT，通过oc_mqtt_profile_connect()函数连接平台。
```c
    WifiConnect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);
    dtls_al_init();
    mqtt_al_init();
    oc_mqtt_init();
    
    g_app_cb.app_msg = queue_create("queue_rcvmsg",10,1);
    if(g_app_cb.app_msg == NULL){
        printf("Create receive msg queue failed");
        
    }
    oc_mqtt_profile_connect_t  connect_para;
    (void) memset( &connect_para, 0, sizeof(connect_para));

    connect_para.boostrap =      0;
    connect_para.device_id =     CONFIG_APP_DEVICEID;
    connect_para.device_passwd = CONFIG_APP_DEVICEPWD;
    connect_para.server_addr =   CONFIG_APP_SERVERIP;
    connect_para.server_port =   CONFIG_APP_SERVERPORT;
    connect_para.life_time =     CONFIG_APP_LIFETIME;
    connect_para.rcvfunc =       msg_rcv_callback;
    connect_para.security.type = EN_DTLS_AL_SECURITY_TYPE_NONE;
    //连接平台
    ret = oc_mqtt_profile_connect(&connect_para);
    if((ret == (int)en_oc_mqtt_err_ok)){
        g_app_cb.connected = 1;
        printf("oc_mqtt_profile_connect succed!\r\n");
    }
    else
    {
        printf("oc_mqtt_profile_connect faild!\r\n");
    }
```

### 推送数据

当需要上传数据时，需要先拼装数据，让后通过oc_mqtt_profile_propertyreport上报数据。代码示例如下： 

```c
static void deal_report_msg(report_t *report)
{
    oc_mqtt_profile_service_t    service;
    oc_mqtt_profile_kv_t         temperature;
    oc_mqtt_profile_kv_t         humidity;
    oc_mqtt_profile_kv_t         luminance;
    oc_mqtt_profile_kv_t         led;
    oc_mqtt_profile_kv_t         motor;

    if (g_app_cb.connected != 1) {
        return;
    }

    service.event_time = NULL;
    service.service_id = "Agriculture";
    service.service_property = &temperature;
    service.nxt = NULL;

    temperature.key = "Temperature";
    temperature.value = &report->temp;
    temperature.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    temperature.nxt = &humidity;

    humidity.key = "Humidity";
    humidity.value = &report->hum;
    humidity.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    humidity.nxt = &luminance;

    luminance.key = "Luminance";
    luminance.value = &report->lum;
    luminance.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    luminance.nxt = &led;

    led.key = "LightStatus";
    led.value = g_app_cb.led?"ON":"OFF";
    led.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    led.nxt = &motor;

    motor.key = "MotorStatus";
    motor.value = g_app_cb.motor?"ON":"OFF";
    motor.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    motor.nxt = NULL;
    //发送数据
    oc_mqtt_profile_propertyreport(NULL,&service);
    return;
}
```




### 命令接收

华为IoT平台支持下发命令，命令是用户自定义的。接收到命令后会将命令数据发送到队列中，task_main_entry函数中读取队列数据并调用deal_cmd_msg函数进行处理，代码示例如下： 

```c

static int msg_rcv_callback(oc_mqtt_profile_msgrcv_t *msg)
{
    int ret = 0;
    char *buf;
    int buf_len;
    app_msg_t *app_msg;

    if ((msg == NULL) || (msg->request_id == NULL) || (msg->type != EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_COMMANDS)) {
        return ret;
    }

    buf_len = sizeof(app_msg_t) + strlen(msg->request_id) + 1 + msg->msg_len + 1;
    buf = malloc(buf_len);
    if (buf == NULL) {
        return ret;
    }
    app_msg = (app_msg_t *)buf;
    buf += sizeof(app_msg_t);

    app_msg->msg_type = en_msg_cmd;
    app_msg->msg.cmd.request_id = buf;
    buf_len = strlen(msg->request_id);
    buf += buf_len + 1;
    memcpy_s(app_msg->msg.cmd.request_id, buf_len, msg->request_id, buf_len);
    app_msg->msg.cmd.request_id[buf_len] = '\0';

    buf_len = msg->msg_len;
    app_msg->msg.cmd.payload = buf;
    memcpy_s(app_msg->msg.cmd.payload, buf_len, msg->msg, buf_len);
    app_msg->msg.cmd.payload[buf_len] = '\0';

    ret = osMessageQueuePut(g_app_cb.app_msg, &app_msg, 0U, CONFIG_QUEUE_TIMEOUT);
    if (ret != 0) {
        free(app_msg);
    }

    return ret;
}

static void oc_cmdresp(cmd_t *cmd, int cmdret)
{
    oc_mqtt_profile_cmdresp_t cmdresp;
    ///< do the response
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = NULL;
    (void)oc_mqtt_profile_cmdresp(NULL, &cmdresp);
}

///< COMMAND DEAL
#include <cJSON.h>
static void deal_light_cmd(cmd_t *cmd, cJSON *obj_root)
{
    cJSON *obj_paras;
    cJSON *obj_para;
    int cmdret;

    obj_paras = cJSON_GetObjectItem(obj_root, "paras");
    if (obj_paras == NULL) {
        cJSON_Delete(obj_root);
    }
    obj_para = cJSON_GetObjectItem(obj_paras, "Light");
    if (obj_paras == NULL) {
        cJSON_Delete(obj_root);
    }
    ///< operate the LED here
    if (strcmp(cJSON_GetStringValue(obj_para), "ON") == 0) {
        g_app_cb.led = 1;
        LightStatusSet(ON);
        printf("Light On!\r\n");
    } else {
        g_app_cb.led = 0;
        LightStatusSet(OFF);
        printf("Light Off!\r\n");
    }
    cmdret = 0;
    oc_cmdresp(cmd, cmdret);

    cJSON_Delete(obj_root);
    return;
}

static void deal_motor_cmd(cmd_t *cmd, cJSON *obj_root)
{
    cJSON *obj_paras;
    cJSON *obj_para;
    int cmdret;

    obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
    if (obj_paras == NULL) {
        cJSON_Delete(obj_root);
    }
    obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
    if (obj_para == NULL) {
        cJSON_Delete(obj_root);
    }
    ///< operate the Motor here
    if (strcmp(cJSON_GetStringValue(obj_para), "ON") == 0) {
        g_app_cb.motor = 1;
        MotorStatusSet(ON);
        printf("Motor On!\r\n");
    } else {
        g_app_cb.motor = 0;
        MotorStatusSet(OFF);
        printf("Motor Off!\r\n");
    }
    cmdret = 0;
    oc_cmdresp(cmd, cmdret);

    cJSON_Delete(obj_root);
    return;
}
```


## 编译调试


### 登录

设备接入华为云平台之前，需要在平台注册用户账号，华为云地址：<https://www.huaweicloud.com/>

在华为云首页单击产品，找到IoT物联网，单击设备接入IoTDA 并单击立即使用，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/登录平台01.png "登录平台")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/登录平台02.png "登录平台")

### 创建产品

在设备接入页面可看到总览界面，展示了华为云平台接入的协议与域名信息，根据需要选取MQTT通讯必要的信息备用，如下图所示。

接入协议（端口号）：MQTT 1883

域名：iot-mqtts.cn-north-4.myhuaweicloud.com
![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D10_iot_cloud_oc_infrared/查看平台信息.png "查看平台信息")

选中侧边栏产品页，单击右上角“创建产品”，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品01.png "创建产品")

在页面中选中所属资源空间，并且按要求填写产品名称，选中MQTT协议，数据格式为JSON，并填写厂商名称，在下方模型定义栏中选择所属行业以及添加设备类型，并单击右下角“立即创建”，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品02.png "创建产品")

创建完成后，在产品页会自动生成刚刚创建的产品，单击“查看”可查看创建的具体信息，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品03.png "创建产品")


单击产品详情页的自定义模型，在弹出页面中新增服务，如下图所示。

服务ID：`Agriculture`(必须一致)

服务类型：`Senser`(可自定义)
![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品04.png "创建产品")

在“Agriculture”的下拉菜单下点击“添加属性”填写相关信息“Temperature”，
“Humidity”，“Luminance”，“LightStatus”，“MotorStatus”，如下图所示。


![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品05.png "创建产品")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品06.png "创建产品")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品07.png "创建产品")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品08.png "创建产品")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品09.png "创建产品")

在“Agriculture”的下拉菜单下点击“添加命令”填写相关信息。

命令名称：`Agriculture_Control_light`

参数名称：`Light`

数据类型：`string`

长度：`3`

枚举值：`ON,OFF`

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品10.png "创建产品")

命令名称：`Agriculture_Control_Motor`

参数名称：`Motor`

数据类型：`string`

长度：`3`

枚举值：`ON,OFF`

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/创建产品11.png "创建产品")

#### 注册设备

在侧边栏中单击“设备”，进入设备页面，单击右上角“注册设备”，勾选对应所属资源空间并选中刚刚创建的产品，注意设备认证类型选择“秘钥”，按要求填写秘钥，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/注册设备01.png "注册设备")

记录下设备ID和设备密钥，如下图所示。
![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/注册设备02.png "注册设备")

注册完成后，在设备页面单击“所有设备”，即可看到新建的设备，同时设备处于未激活状态，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/注册设备03.png "注册设备")


### 修改代码中设备信息
修改`iot_cloud_oc_sample.c`中第31行附近的wifi的ssid和pwd，以及设备的DEVICEID和DEVICEPWD（这两个参数是在平台注册设备时产生的），如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/修改设备信息.png "修改设备信息")
### 修改BUILD.gn
将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/D11_iot_cloud_oc_agriculture文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加D11_iot_cloud_oc_agriculture:cloud_oc_agriculture.

```c
import("//build/lite/config/component/lite_component.gni")

lite_component("app") {
features = [ "D11_iot_cloud_oc_agriculture:cloud_oc_agriculture", ]
}
```
### 编译烧录
点击DevEco Device Tool工具“Rebuild”按键，编译程序。

![image-20230103154607638](/doc/pic/image-20230103154607638.png)

点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

![image-20230103154836005](/doc/pic/image-20230103154836005.png)

### 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，会打印温湿度及光照强度信息，平台上的设备显示为在线状态，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/设备在线.png "设备在线")
    
点击设备右侧的“查看”，进入设备详情页面，可看到上报的数据，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/查看设备信息.png "查看设备信息")

在华为云平台设备详情页，单击“命令”，选择同步命令下发，选中创建的命令属性，单击“确定”，即可发送下发命令控制设备，如下图所示。
![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D11_iot_cloud_oc_agriculture/命令下发.png "命令下发")