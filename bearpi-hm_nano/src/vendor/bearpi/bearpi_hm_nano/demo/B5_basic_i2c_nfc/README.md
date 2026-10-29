# BearPi-HM_Nano开发板基础外设开发——I2C控制NFC芯片
本示例将演示如何在BearPi-HM_Nano开发板上使用I2C协议向NFC芯片写入数据。


## I2C API分析
本示例主要使用了以下API完成I2C采样的功能。
### IoTI2cInit()
```c
unsigned int IoTI2cInit(unsigned int id, unsigned int baudrate);
```
 **描述：**

用指定的频率初始化I2C设备。


**参数：**

|参数名|描述|
|:--|:------| 
| id | I2C设备ID。  |
| baudrate |I2C频率。|

## IoTI2cSetBaudrate()
```c
unsigned int IoTI2cSetBaudrate(unsigned int id, unsigned int baudrate);
```
 **描述：**

为I2C设备设置频率。

**参数：**

|参数名|描述|
|:--|:------| 
| id | I2C设备ID。  |
| baudrate |I2C频率。|

## IoTI2cWrite()
```c
unsigned int IoTI2cWrite(unsigned int id, unsigned short deviceAddr, const unsigned char *data, unsigned int dataLen);

```
 **描述：**

将数据写入I2C设备。


**参数：**

|参数名|描述|
|:--|:------| 
| id | I2C设备ID。  |
| deviceAddr |I2C设备地址。|
| data |表示写入的数据。|
| dataLen |表示要写入的数据长度。|

## IoTI2cRead()
```c
unsigned int IoTI2cRead(unsigned int id, unsigned short deviceAddr, unsigned char *data, unsigned int dataLen);
```
 **描述：**

从I2C设备读取数据。读取的数据将保存到i2cData指定的地址。


**参数：**

|参数名|描述|
|:--|:------| 
| id | I2C设备ID。  |
| deviceAddr |I2C设备地址。|
| data |表示要读取的数据指向的指针。|
| dataLen |表示要读取的数据长度。|



## 硬件设计
如下图所示，NFC芯片使用的是I2C协议，I2C_SCL与GPIO_0相连接，I2C_SDA与GPIO_1相连接，所以需要编写软件使用GPIO_0和GPIO_1产生I2C信号去控制NFC芯片。

![E53接口电路](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/B5_basic_i2c_nfc/NFC电路.png "E53接口电路")

## 软件设计

**主要代码分析**

这部分代码为I2C初始化的代码，首先用 `IoTGpioSetFunc()` 函数将GPIO_0复用为I2C1_SDA，GPIO_1复用为I2C1_SCL。然后调用IoTI2cInit()函数初始化I2C1端口，最后使用 `I2cSetBaudrate()` 函数设置I2C1的频率为400kbps。
```c
    //GPIO_0 multiplexed to I2C1_SDA
    IoTGpioInit(0);
    IoTGpioSetFunc(0, WIFI_IOT_IO_FUNC_GPIO_0_I2C1_SDA);

    //GPIO_1 multiplexed to I2C1_SCL
    IoTGpioInit(1);
    IoTGpioSetFunc(1, WIFI_IOT_IO_FUNC_GPIO_1_I2C1_SCL);

    //baudrate: 400kbps
    IoTI2cInit(WIFI_IOT_I2C_IDX_1, 400000);
```
这部分的代码是向NFC芯片写入数据，但需要写入2个记录时，第2个记录的位置需要用`NDEFLastPos`来定义;当需要写入3个记录时，第2个和第3个记录的位置分别需要用`NDEFMiddlePos`和`NDEFLastPos`来定义。
```c
ret=storeText(NDEFFirstPos, (uint8_t *)TEXT);
if(ret != 1)
{
    printf("NFC Write Data Falied :%d ",ret);
}
ret=storeUrihttp(NDEFLastPos, (uint8_t *)WEB);
if(ret != 1)
{
    printf("NFC Write Data Falied :%d ",ret);
}
```


## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/B5_basic_i2c_nfc文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加B5_basic_i2c_nfc:i2c_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "B5_basic_i2c_nfc:i2c_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，并请使用带有NFC功能的手机靠近开发板，能读取数据。
```c
=======================================

***********I2C_NFC_example**********

=======================================

Please use the mobile phone with NFC function close to the development board!

=======================================

***********I2C_NFC_example**********

=======================================

Please use the mobile phone with NFC function close to the development board!
```
