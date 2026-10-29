# BearPi-HM_Nano开发板传感器驱动开发——E53_IA1读取温度 、湿度、光照强度
本示例将演示如何在BearPi-HM_Nano开发板上使用E53_IA1读取温度 、湿度、光照强度，当温度 、湿度超标时开启电机通风，当光照强度过低时，开启补光灯补光，设备安装如下图所示。

![E53_IA1安装](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C2_e53_ia1_temp_humi_pls/E53_IA1安装.png "E53_IA1安装")

## E53_IA1 API分析
本案例主要使用了以下API完成温度 、湿度、光照强度读取。
### E53IA1Init()
```C
void E53IA1Init(void)
```
 **描述：**

初始化E53_IA1

### E53IA1ReadData()
```C
void E53IA1ReadData(E53IA1Data *ReadData)
```
 **描述：**

读取温度 、湿度、光照强度。
### Light_StatusSet()
```C
void LightStatusSet(E53IA1Status status)
```
 **描述：**
 
控制补光灯开关。

**参数：**

|参数名|描述|
|:--|:------| 
| status | ON开，OFF关闭。  |

### Motor_StatusSet()
```C
void MotorStatusSet(E53IA1Status status)
```
 **描述：**

控制电机开关。

**参数：**

|参数名|描述|
|:--|:------| 
| status | ON开，OFF关闭。  |

## 硬件设计
本案例将用到 E53_IA1 智慧农业扩展板与 BearPi-HM_Nano 开发板，其中E53_IA1扩展板接口原理图如下，温湿度传感器sht30和光照强度传感器BH1750都是通过I2C来驱动，电机和补光灯分别通过GPIO_8和GPIO_14来控制。

![E53_IA1接口](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C2_e53_ia1_temp_humi_pls/E53_IA1接口.png "E53_IA1接口")

![E53接口电路](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C2_e53_ia1_temp_humi_pls/E53接口电路.png "E53接口电路")

E53_IA1 智慧农业扩展板与 BearPi-HM_Nano 开发板安装如下图所示。

![E53_IA1安装](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/C2_e53_ia1_temp_humi_pls/E53_IA1安装.png "E53_IA1安装")

## 软件设计

**主要代码分析**


首先调用 `E53IA1Init()` 函数初始化E53_IA1所接的引脚的功能，然后循环调用 `E53IA1ReadData(E53IA1Data *ReadData)` 函数读取温度 、湿度、光照强度并通过串口打印出来，当光照强度过低时，开启补光灯补光，当温度 、湿度超标时开启电机通风。

```C
static void ExampleTask(void)
{
    int ret;
    E53IA1Data data;

    ret = E53IA1Init();
    if (ret != 0) {
        printf("E53_IA1 Init failed!\r\n");
        return;
    }

    while (1) {
        printf("\r\n=======================================\r\n");
        printf("\r\n*************E53_IA1_example***********\r\n");
        printf("\r\n=======================================\r\n");

        ret = E53IA1ReadData(&data);
        if (ret != 0) {
            printf("E53_IA1 Read Data failed!\r\n");
            return;
        }
        printf("\r\n******************************Lux Value is  %.2f\r\n", data.Lux);
        printf("\r\n******************************Humidity is  %.2f\r\n", data.Humidity);
        printf("\r\n******************************Temperature is  %.2f\r\n", data.Temperature);

        if ((int)data.Lux < MIN_LUX) {
            LightStatusSet(ON);
        } else {
            LightStatusSet(OFF);
        }

        if (((int)data.Humidity > MAX_HUM) | ((int)data.Temperature > MAX_TEMP)) {
            MotorStatusSet(ON);
        } else {
            MotorStatusSet(OFF);
        }
        usleep(TASK_DELAY_1S);
    }
}
```



## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/C2_e53_ia1_temp_humi_pls文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加C2_e53_ia1_temp_humi_pls:e53_ia1_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "C2_e53_ia1_temp_humi_pls:e53_ia1_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，会打印温湿度及光照强度信息。用手遮住扩展板，补光灯会自动开启，控制温度或者湿度超标，电机会自动开启。
```c
=======================================

*************E53_IA1_example***********

=======================================

******************************Lux Value is  53.33

******************************Humidity is  44.10

******************************Temperature is  28.13
=======================================

*************E53_IA1_example***********

=======================================

******************************Lux Value is  53.33

******************************Humidity is  44.10

******************************Temperature is  28.13
```

