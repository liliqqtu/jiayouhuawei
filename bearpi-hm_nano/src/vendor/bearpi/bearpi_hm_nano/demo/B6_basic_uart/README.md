# BearPi-HM_Nano开发板基础外设开发——UART数据读写
本示例将演示如何在BearPi-HM_Nano开发板上使用UART进行数据的收发。

## UART API分析
本示例主要使用了以下API完成UART数据读写。
### IoTUartInit()
```c
unsigned int IoTUartInit(unsigned int id, const IotUartAttribute *param);

```
 **描述：**

配置一个UART设备。
**参数：**

|参数名|描述|
|:--|:------| 
| id | UART端口号。  |
| param |表示基本UART属性。|

### IoTUartWrite()
```c
int IoTUartWrite(unsigned int id, const unsigned char *data, unsigned int dataLen);

```
 **描述：**
将数据写入UART设备。


**参数：**

|参数名|描述|
|:--|:------| 
| id | UART端口号。  |
| data |表示指向要写入数据的起始地址的指针。|
| dataLen |表示读取数据的长度。|

### IoTUartRead()
```c
int IoTUartRead(unsigned int id, unsigned char *data, unsigned int dataLen);
```
 **描述：**

从UART设备读取数据。


**参数：**

|参数名|描述|
|:--|:------| 
| id | UART端口号。  |
| data |表示指向要读取数据的起始地址的指针。|
| dataLen |表示读取数据的长度。|




## 硬件设计
本案例将用 BearPi-HM_Nano 开发板 E53 接口的 UART 作为测试，如原理图所示第 18 和 19 脚分别为 TXD 和 RXD ，连接了主控芯片的 GPIO_6 和 GPIO_5 ，所以在编写软件的时候需要将 GPIO_6 和 GPIO_5 分别复用为 TXD 和 RXD 。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/B6_basic_uart/E53接口电路.png "E53接口电路")

## 软件设计

**主要代码分析**

这部分代码为UART初始化的代码，首先要在 `uart_attr` 结构体这配置波特率、数据位、停止位、奇偶检验位，然后通过 `IoTUartInit()` 函数对串口1进行配置。

```c
IotUartAttribute uart_attr = {

    //baud_rate: 9600
    .baudRate = 9600,

    //data_bits: 8bits
    .dataBits = 8,
    .stopBits = 1,
    .parity = 0,
};

//Initialize uart driver
ret = IoTUartInit(WIFI_IOT_UART_IDX_1, &uart_attr);
if (ret != IOT_SUCCESS) {
    printf("Failed to init uart! Err code = %d\n", ret);
    return;
}
```
这部分的代码主要实现通过 `IoTUartWrite()` 函数在串口1发送一串数据，然后通过 `IoTUartRead()` 函数将数据读回来，并通过 `debug` 串口打印出来。
```c
//send data through uart1
IoTUartWrite(WIFI_IOT_UART_IDX_1, (unsigned char *)data, strlen(data));

//receive data through uart1
IoTUartRead(WIFI_IOT_UART_IDX_1, uart_buff_ptr, UART_BUFF_SIZE);

printf("Uart1 read data:%s\n", uart_buff_ptr);
```


## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/B6_basic_uart文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加B6_basic_uart:uart_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "B6_basic_uart:uart_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键， `将开发板上E53接口的UART_TX和UART_RX用杜邦线短接` 通过串口助手查看日志，串口1实现自发自收。
```c
=======================================
*************UART_example**************
=======================================
Uart1 read data:Hello, BearPi!
=======================================
*************UART_example**************
=======================================
Uart1 read data:Hello, BearPi!
```

