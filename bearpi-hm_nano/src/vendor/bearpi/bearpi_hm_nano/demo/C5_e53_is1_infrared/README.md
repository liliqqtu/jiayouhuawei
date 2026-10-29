# BearPi-HM_Nano开发板传感器驱动开发——E53_IS1人体红外感应
本示例将演示如何在BearPi-HM_Nano开发板上使用E53_IS1实现人体红外感应，当检测到有人走动时，蜂鸣器发出报警，设备安装如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C5_e53_is1_infrared/E53_IS1安装.png "E53_IS1安装")
## E53_IS1 API分析
本案例主要使用了以下API完成人体红外感应。
### E53IS1Init()
```C
void E53IS1Init(void);
```
 **描述：**

初始化E53_IS1。

### E53IS1ReadData()
```C
void E53IS1ReadData(E53IS1CallbackFunc func);
```
 **描述：**
 
设置人体感应触发的回调函数。




## 硬件设计
本案例将用到 E53_IS1 红外感应扩展板与 BearPi-HM_Nano 开发板，其中E53_IS1扩展板原理图如下，当检测到人时，传感器会输出高电平，通过对GPIO_7的监测就能判断是否有人走动。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C5_e53_is1_infrared/E53_IS1接口.png "E53_IS1接口")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C5_e53_is1_infrared/E53接口电路.png "E53接口电路")

E53_IS1 红外感应扩展板与 BearPi-HM_Nano 开发板安装如下图所示

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C5_e53_is1_infrared/E53_IS1安装.png "E53_IS1安装")
## 软件设计

**主要代码分析**


首先调用 `E53IS1Init()` 函数初始化E53_SC1所接的引脚的功能，然后调用 `E53IS1ReadData()` BeepAlarm(),系统启动后会通过osEventFlagsWait()函数让ExampleTask任务一直等待事件标志位FLAGS_MSK1，当检测到人后，BeepAlarm()函数会发送事件标志位，ExampleTask任务继续运行开启蜂鸣器报警3秒钟 然后关闭蜂鸣器继续等待下一次触发事件。
```C
static void BeepAlarm(char *arg)
{
    (void)arg;
    osEventFlagsSet(g_eventFlagsId, FLAGS_MSK1);
}

static void ExampleTask(void)
{
    int ret;
    E53IS1Init();
    ret = E53IS1ReadData(BeepAlarm);
    if (ret != 0) {
        printf("E53_IS1 Read Data failed!\r\n");
        return;
    }
    while (1) {
        osEventFlagsWait(g_eventFlagsId, FLAGS_MSK1, osFlagsWaitAny, osWaitForever);
        BeepStatusSet(ON);
        osDelay(TASK_DELAY_3S);
        BeepStatusSet(OFF);
    }
}
```



## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/A1_kernal_thread文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加A1_kernal_thread:e53_is1_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "A1_kernal_thread:e53_is1_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，人员靠近开发板，蜂鸣器开始报警。


