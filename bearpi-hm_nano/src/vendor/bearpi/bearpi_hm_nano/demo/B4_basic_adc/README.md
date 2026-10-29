# BearPi-HM_Nano开发板基础外设开发——ADC采样
本示例将演示如何在BearPi-HM_Nano开发板上通过按下按键改变GPIO口的电压，并使用ADC读取GPIO的电压值。

## ADC API分析
本案例主要使用了以下API完成ADC采样的功能。
### IoTAdcRead()
```c
unsigned int IoTAdcRead(unsigned int channel, unsigned short *data, IotAdcEquModelSel equModel,
                     IotAdcCurBais curBais, unsigned short rstCnt);
```
 **描述：**

根据输入参数从指定的ADC通道读取一段采样数据。


**参数：**

|参数名|描述|
|:--|:------| 
| channel | 表示ADC通道。  |
| data |表示指向存储读取数据的地址的指针。 |
| equModel | 表示平均算法的次数。 |
| curBais | 表示模拟功率控制模式。 |
| rstCnt | 指示从重置到转换开始的时间计数。一次计数等于334纳秒。值的范围必须从0到0xFF。|



## 硬件设计
本案例将使用板载用户按键F1来模拟GPIO口电压的变化。通过查看芯片手册可知GPIO_11对应的是 ADC Channel 5 ,所以需要编写软件去读取ADC Channel 5的电压,程序设计时先将GPIO_11上拉，使GPIO_11的电压一直处于高电平，当按键按下时GPIO_11接地，此时GPIO_11的电压变为 0 V。

![按键电路](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/B4_basic_adc/按键电路.png "按键电路")

## 软件设计

**主要代码分析**
 
该函数通过使用AdcRead()函数来读取 `ADC_CHANNEL_5` 的数值存储在data中， `IOT_ADC_EQU_MODEL_8` 表示8次平均算法模式，`IOT_ADC_CUR_BAIS_DEFAULT` 表示默认的自动识别模式，最后通过 `data * 1.8 * 4 / 4096.0` 计算出实际的电压值。
```c
/**
 * @brief get ADC sampling value and convert it to voltage
 * 
 */
static float GetVoltage(void)
{
    unsigned int ret;
    unsigned short data;

    ret = IoTAdcRead(ADC_CHANNEL, &data, IOT_ADC_EQU_MODEL_8, IOT_ADC_CUR_BAIS_DEFAULT, 0xff);
    if (ret != IOT_SUCCESS) {
        printf("ADC Read Fail\n");
    }

    return (float)data * ADC_VREF_VOL * ADC_COEFFICIENT / ADC_RATIO;
}
```



## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/B4_basic_adc文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加B4_basic_adc:adc_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "B4_basic_adc:adc_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，当F1按键未按下时采集到的电压为3.3V左右，当按键按下时，电压变为0.2V左右。
```c
=======================================
*************ADC_example***********
=======================================
vlt:3.371V
=======================================
*************ADC_example***********
=======================================
vlt:3.371V
=======================================
*************ADC_example***********
=======================================
vlt:3.373V
=======================================
*************ADC_example***********
=======================================
vlt:0.248V
=======================================
*************ADC_example***********
=======================================
vlt:0.244V
```

