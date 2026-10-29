# BearPi-HM_Nano开发板基础外设开发——PWM输出
本示例将演示如何在BearPi-HM_Nano开发板上使用GPIO的PWM功能实现呼吸灯的效果。


## PWM API分析
本案例主要使用了以下几个API完成PWM功能实现呼吸灯功能。
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


### IoTPwmInit()
```c
unsigned int IoTPwmInit(unsigned int port);
```
**描述：**
初始化PWM功能。

**参数：**

|参数名|描述|
|:--|:------| 
| port | 表示PWM设备端口号。  |



## IoTPwmStart()
```c
unsigned int IoTPwmStart(unsigned int port, unsigned short duty, unsigned int freq);
```
**描述：**

根据输入参数输出PWM信号。

**参数：**

|参数名|描述|
|:--|:------| 
| port | PWM端口号。  |
| duty| 占空比。  |
| freq| 分频倍数。  |


## 硬件设计
本案例将使用板载的LED来验证GPIO的PWM功能，在BearPi-HM_Nano开发板上LED的连接电路图如下图所示，LED的控制引脚与主控芯片的GPIO_2连接，所以需要编写软件去控制GPIO_2输出PWM波实现呼吸灯的效果。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/B3_basic_pwm_led/LED灯电路.png "LED灯电路")

## 软件设计

**主要代码分析**

PWMTask()为PWM测试主任务，该任务先调用 IoTGpioInit()初始化GPIO，因为LED灯的控制引脚接在GPIO_2上，所以通过调用IoTGpioSetFunc()将GPIO_2复用为PWM功能，并通过IoTPwmInit()初始化PWM2端口，最后在死循环里面间隔10us输出不同占空比的PWM波，实现呼吸灯的效果。
```c
/**
 * @brief pwm task output PWM with different duty cycle
 * 
 */
static void PwmTask(void)
{
    unsigned int i;

    // init gpio of LED
    IoTGpioInit(LED_GPIO);

    // set the GPIO_2 multiplexing function to PWM
    IoTGpioSetFunc(LED_GPIO, IOT_GPIO_FUNC_GPIO_2_PWM2_OUT);

    // set GPIO_2 is output mode
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);

    // init PWM2
    IoTPwmInit(LED_GPIO);

    while (1) {
        for (i = 0; i < PWM_CHANGE_TIMES; i++) {
            // output PWM with different duty cycle
            IoTPwmStart(LED_GPIO, i, PWM_FREQ);
            usleep(PWM_DELAY_10US);
        }
        i = 0;
    }
}
```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/B3_basic_pwm_led文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加B3_basic_pwm_led:pwm_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "B3_basic_pwm_led:pwm_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，开发板开始正常工作，LED开始不断变化亮度，实现呼吸灯的效果。

