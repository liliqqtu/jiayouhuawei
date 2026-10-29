# BearPi-HM_Nano开发板WiFi编程开发——Wifi AP 热点


本示例将演示如何在BearPi-HM_Nano开发板上编写一个创建Wifi热点程序。


## Wifi API分析
本案例主要使用了以下几个API完成Wifi热点创建。

### RegisterWifiEvent()
```c
WifiErrorCode RegisterWifiEvent (WifiEvent * event)
```
 **描述：**

为指定的Wi-Fi事件注册回调函数。当WifiEvent中定义的Wi-Fi事件发生时，将调用已注册的回调函数。

**参数：**

|参数名|描述|
|:--|:------| 
| event | 表示要注册回调的事件。  |


### EnableHotspot()
```c
WifiErrorCode EnableHotspot (void )
```

**描述：**

启用Wifi热点模式。

### SetHotspotConfig()
```c
WifiErrorCode SetHotspotConfig(const HotspotConfig* config)
```
**描述：**

设置指定的热点配置。

### IsHotspotActive()
```c
int IsHotspotActive(void);
```
**描述：**

检查AP热点模式是否启用。

### GetStationList()
```c
WifiErrorCode GetStationList(StationInfo* result, unsigned int* size)
```
**描述：**

获取连接到该热点的一系列STA。

**参数：**

|参数名|描述|
|:--|:------| 
| result | 表示连接到该热点的STA列表。  |
| size | 表示连接到该热点的STA数量。  |



## 软件设计

**主要代码分析**

完成Wifi热点的扫描需要以下几步。

1. 通过 `RegisterWifiEvent` 接口向系统注册热点状态改变事件、STA站点加入事件、STA站点退出事件。
    
    * `OnHotspotStateChangedHandler` 用于绑定热点状态改变事件，该回调函数有一个参数 `state` 。

        * state表示是否开启AP模式，取值为0和1，0表示已启用Wifi AP模式，1表示已禁用Wifi AP模式。

    * `OnHotspotStaLeaveHandler` 用于绑定STA站点退出事件,当有STA站点退出，该回调函数会打印出退出站点的MAC地址。
    * `OnHotspotStaJoinHandler` 用于绑定STA站点加入事件，当有新的STA站点加入时，该回调函数会创建 `HotspotStaJoinTask`，在该任务中会调用 `GetStationList` 函数获取当前接入到该AP的所有STA站点信息，并打印出每个STA站点的MAC地址。
2. 调用 `SetHotspotConfig` 接口，设置指定的热点配置。
3. 调用 `EnableHotspot` 接口，使能 Wifi AP 模式。

    
    ```c
    static BOOL WifiAPTask(void)
    {
        //延时2S便于查看日志
        osDelay(TASK_DELAY_2S);

        //注册wifi事件的回调函数
        g_wifiEventHandler.OnHotspotStaJoin = OnHotspotStaJoinHandler;
        g_wifiEventHandler.OnHotspotStaLeave = OnHotspotStaLeaveHandler;
        g_wifiEventHandler.OnHotspotStateChanged = OnHotspotStateChangedHandler;
        error = RegisterWifiEvent(&g_wifiEventHandler);
        if (error != WIFI_SUCCESS) {
            printf("RegisterWifiEvent failed, error = %d.\r\n", error);
            return -1;
        }
        printf("RegisterWifiEvent succeed!\r\n");
            //检查热点模式是否使能
        if (IsHotspotActive() == WIFI_HOTSPOT_ACTIVE) {
            printf("Wifi station is  actived.\r\n");
            return -1;
        }
        //设置指定的热点配置
        HotspotConfig config = { 0 };

        strcpy_s(config.ssid, strlen(AP_SSID) + 1, AP_SSID);
        strcpy_s(config.preSharedKey, strlen(AP_PSK) + 1, AP_PSK);
        config.securityType = WIFI_SEC_TYPE_PSK;
        config.band = HOTSPOT_BAND_TYPE_2G;
        config.channelNum = CHANNEL_NUM;

        error = SetHotspotConfig(&config);
        if (error != WIFI_SUCCESS) {
            printf("SetHotspotConfig failed, error = %d.\r\n", error);
            return -1;
        }
        printf("SetHotspotConfig succeed!\r\n");

        //启动wifi热点模式
        error = EnableHotspot();
        if (error != WIFI_SUCCESS) {
            printf("EnableHotspot failed, error = %d.\r\n", error);
            return -1;
        }
        printf("EnableHotspot succeed!\r\n");

        StartUdpServer();
    }
    ```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/D1_iot_wifi_ap文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加D1_iot_wifi_ap:wifi_ap.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "D1_iot_wifi_ap:wifi_ap", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，使用手机去连接该热点，会打印出一下信息。

```
RegisterWifiEvent succeed!

SetHotspotConfig succeed!

HotspotStateChanged:state is 1.

EnableHotspot succeed!

Wifi station is actived!

+NOTICE:STA CONNECTED
New Sta Join
HotspotSta[0]: macAddress=EC:5C:68:5B:2D:3D.

netifapi_netif_set_addr succeed!

netifapi_dhcps_start succeed!

Waiting to receive data...
```


