# BearPi-HM_Nano开发板基础外设开发——GPIO输出
本示例将演示如何在BearPi-HM_Nano开发板上使用GPIO输出功能去点亮LED灯。

## GPIO API分析
本案例主要使用了以下几个API完成GPIO输出功能。
### IoTGpioInit()
```c
unsigned int IoTGpioInit(unsigned int id);
```
 **描述：**

初始化GPIO外设。
### IoTGpioSetFunc()
```c
unsigned int IoTGpioSetFunc(unsigned int id, unsigned char val);
```
**描述：**

设置GPIO引脚复用功能。

**参数：**

|参数名|描述|
|:--|:------| 
| id | 表示GPIO引脚号。  |
| val | 表示GPIO复用功能。 |

### IoTGpioSetDir()
```c
unsigned int IoTGpioSetDir(unsigned int id, IotGpioDir dir);
```
**描述：**

设置GPIO输出方向。

**参数：**

|参数名|描述|
|:--|:------| 
| id | 表示GPIO引脚号。  |
| dir | 表示GPIO输出方向。  |


## 硬件设计
本案例将使用板载的LED来验证GPIO的输出功能，在BearPi-HM_Nano开发板上LED的连接电路图如下图所示，LED的控制引脚与主控芯片的GPIO_2连接，所以需要编写软件去控制GPIO_2输出高低电平实现LED灯的亮灭。

![LED灯电路](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/B1_basic_led_blink/LED灯电路.png )

## 软件设计

**主要代码分析**

LedTask()为LED灯测试主任务，该任务先调用 IoTGpioInit()初始化GPIO，因为LED灯的控制引脚接在GPIO_2上，所以通过IoTGpioSetDir()将GPIO_2设置为普通GPIO的输出模式。最后在死循环里面间隔 1s 输出GPIO_2的高低电平，实现LED灯闪烁的现象。
```c
/**
 * @brief led task output high and low levels to turn on and off LED
 * 
 */
static void LedTask(void)
{
    //init gpio of LED
    IoTGpioInit(LED_GPIO);

    //set GPIO_2 is output mode
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);

    while (1) {
        //set GPIO_2 output high levels to turn on LED
        IoTGpioSetOutputVal(LED_GPIO, 1);

        //delay 1s
        sleep(1);

        //set GPIO_2 output low levels to turn off LED
        IoTGpioSetOutputVal(LED_GPIO, 0);

        //delay 1s
        sleep(1);
    }
}
```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/B1_basic_led_blink文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加B1_basic_led_blink:led_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "B1_basic_led_blink:led_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，开发板的LED灯开始闪烁。


