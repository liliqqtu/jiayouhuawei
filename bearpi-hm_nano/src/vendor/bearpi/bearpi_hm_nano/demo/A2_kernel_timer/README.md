# BearPi-HM_Nano开发板OpenHarmony内核编程开发——定时器
本示例将演示如何在BearPi-HM_Nano开发板上使用cmsis 2.0 接口进行定时器开发。


## Timer API分析


### osTimerNew()

```c
osTimerId_t osTimerNew	(osTimerFunc_t func,osTimerType_t type,void *argument,const osTimerAttr_t *attr)
```
**描述：**

函数osTimerNew创建一个一次性或周期性计时器，并将其与一个带参数的回调函数相关联。计时器在osTimerStart启动之前一直处于停止状态。可以在RTOS启动(调用 osKernelStart)之前安全地调用该函数，但不能在内核初始化 (调用 osKernelInitialize)之前调用该函数。
> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| func | 函数指针指向回调函数。  |
| type | 定时器类型，osTimerOnce表示单次定时器，ostimer周期表示周期性定时器。  |
| argument |定时器回调函数的参数。|
| attr |计时器属性。|

### osTimerStart()

```c
osStatus_t osTimerStart	(osTimerId_t timer_id,uint32_t ticks)
```
**描述：**

函数osTimerStart启动或重新启动指定参数timer_id的计时器。参数ticks指定计时器的计数值。

> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| timer_id | 由osTimerNew获得的计时器ID。  |
| ticks | 时间滴答计时器的值。  |

## 软件设计


**主要代码分析**

在TimerExample函数中，通过osTimerNew()函数创建了回调函数为Timer1Callback的定时器1，并通过osTimerStart()函数将该定时器设置为100个tick,因为hi3861默认10ms为一个tick,所以100个tick正好为1S钟，1S计时到后会触发Timer1Callback()函数并打印日志。定时器2也同理为3S触发Timer2Callback()函数并打印日志。

```c
/**
 * @brief Main Entry of the Timer Example
 * 
 */
static void TimerExample(void)
{
    osTimerId_t id1, id2;
    uint32_t timerDelay;
    osStatus_t status;

    id1 = osTimerNew(Timer1Callback, osTimerPeriodic, NULL, NULL);
    if (id1 != NULL)
    {

        // Hi3861 1U=10ms,100U=1S
        timerDelay = 100U;

        status = osTimerStart(id1, timerDelay);
        if (status != osOK) {
            printf("Failed to start Timer1!\n");
        }
    }

    id2 = osTimerNew(Timer2Callback, osTimerPeriodic, NULL, NULL);
    if (id2 != NULL) {

        // Hi3861 1U=10ms,300U=3S
        timerDelay = 300U;

        status = osTimerStart(id2, timerDelay);
        if (status != osOK) {
            printf("Failed to start Timer2!\n");
        }
    }
}
```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/A2_kernel_timer文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加A2_kernel_timer:timer_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "A2_kernel_timer:timer_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，Timer1Callback会1S打印一次数据，Timer2Callback会3S打印一次数据。
```
This is BearPi Timer1_Callback!
This is BearPi Timer1_Callback!
This is BearPi Timer1_Callback!
This is BearPi Timer2_Callback!
This is BearPi Timer1_Callback!
This is BearPi Timer1_Callback!
This is BearPi Timer1_Callback!
This is BearPi Timer2_Callback!
```
