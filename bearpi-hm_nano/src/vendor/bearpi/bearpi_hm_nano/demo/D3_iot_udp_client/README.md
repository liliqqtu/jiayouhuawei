# BearPi-HM_Nano开发板WiFi编程开发——UDP客户端
本示例将演示如何在BearPi-HM_Nano开发板上使用socket编程创建UDP客户端，就收客户端消息并回复固定消息。


## socket API分析
本案例主要使用了以下几个API完socket编程实验。
### socket()

```c
sock_fd = socket(AF_INET, SOCK_STREAM, 0)) //AF_INT:ipv4, SOCK_STREAM:tcp协议
```
**描述：**

在网络编程中所需要进行的第一件事情就是创建一个socket，无论是客户端还是服务器端，都需要创建一个socket，该函数返回socket文件描述符，类似于文件描述符。socket是一个结构体，被创建在内核中。
### sendto()
```c
int sendto ( socket s , const void * msg, int len, unsigned int flags,
const struct sockaddr * to , int tolen ) ;
```
**描述：**

sendto() 用来将数据由指定的socket传给对方主机。参数s为已建好连线的socket。参数msg指向欲连线的数据内容，参数flags 一般设0。

### recvfrom()
```c
int recvfrom(int s, void *buf, int len, unsigned int flags, struct sockaddr *from, int *fromlen);
```
**描述：**
从指定地址接收UDP数据报。


**参数：**

|参数名|描述|
|:--|:------| 
| s | socket描述符。  |
| buf | UDP数据报缓存地址。  |
| len | UDP数据报长度。  |
| flags | 该参数一般为0。  |
| from | 对方地址。  |
| fromlen | 对方地址长度。  |



## 软件设计

**主要代码分析**

完成Wifi热点的连接需要以下几步。

1. 通过 `socket` 接口创建一个socket,`AF_INT`表示ipv4,`SOCK_STREAM`表示使用tcp协议。
2. 调用 `sendto` 接口发送数据到服务端。
3. 调用 `recvfrom` 接口接收服务端发来的数据。


```c
static void UDPClientTask(void)
{
    //在sock_fd 进行监听，在 new_fd 接收新的链接
    int sock_fd;

    //服务器的地址信息
    struct sockaddr_in send_addr;
    socklen_t addr_length = sizeof(send_addr);
    char recvBuf[512];

    //连接Wifi
    WifiConnect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);

    //创建socket
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("create socket failed!\r\n");
        exit(1);
    }

    //初始化预连接的服务端地址
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = htons(CONFIG_SERVER_PORT);
    send_addr.sin_addr.s_addr = inet_addr(CONFIG_SERVER_IP);
    addr_length = sizeof(send_addr);

    //总计发送 count 次数据
    while (1) {
        bzero(recvBuf, sizeof(recvBuf));

        //发送数据到服务远端
        sendto(sock_fd, send_data, strlen(send_data), 0, (struct sockaddr *)&send_addr, addr_length);

        //线程休眠一段时间
        sleep(TASK_DELAY_10S);

        //接收服务端返回的字符串
        recvfrom(sock_fd, recvBuf, sizeof(recvBuf), 0, (struct sockaddr *)&send_addr, &addr_length);
        printf("%s:%d=>%s\n", inet_ntoa(send_addr.sin_addr), ntohs(send_addr.sin_port), recvBuf);
    }

    //关闭这个 socket
    closesocket(sock_fd);
}
```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/D3_iot_udp_client文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加D3_iot_udp_client:udp_client.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "D3_iot_udp_client:udp_client", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

使用 Socket tool 创建UDP服务端用于测试，如下图所示。

![创建UDP服务端](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D3_iot_udp_client/创建UDP服务端.png)

示例代码编译烧录代码后，按下开发板的RESET按键，在数据发送窗口输入要发送的数据，点击发送后开发板会回复固定消息，如下图所示，且开发板收到消息后会通过日志打印出来。

```
192.168.0.175:8888=>Hello! BearPi-HM_nano UDP Client!
```

![UDP发送数据](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D3_iot_udp_client/UDP发送数据.png)