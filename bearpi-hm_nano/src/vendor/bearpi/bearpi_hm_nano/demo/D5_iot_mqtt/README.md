# BearPi-HM_Nano开发板WiFi编程开发——MQTT协议开发
本示例将演示如何在BearPi-HM_Nano开发板上使用MQTT协议



## 编译调试

### 下载MQTT消息代理工具Mosquitto

点击[下载](https://mosquitto.org/download/)Mosquitto 工具，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/消息代理.png "消息代理")

下载后双击安装包，安装工具，安装完毕后，打开电脑设备管理器，在“服务”中开启mosquitto服务，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/启动mosquitto服务.png "启动mosquitto服务")

修改安装路径下的mosquitto.conf文件，修改515行附近代码，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/添加allow.png "添加allow")

修改216行附近代码，如下图所示，其中`192.168.0.173`为自己的电脑的IP地址。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/添加listener.png "添加listener")

### 下载Eclipse Paho MQTT 工具
点击[下载](https://repo.eclipse.org/content/repositories/paho-releases/org/eclipse/paho/org.eclipse.paho.ui.app/1.1.1/)Eclipse Paho MQTT 工具，如下图所示。


![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/下载客户端.png "下载客户端")

解压缩后，双击paho.exe，打开后的Eclipse Paho UI开始界面，点击上图中的 十字图标，就能新建一个MQTT的客户端的连接，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/新建客户端.png "新建客户端")

输入正确的MQTT服务端的连接地址，比如，本例中的连接地址是tcp://localhost:1883,然后点击“连接”按钮，这个时候，如果MQTT服务端没有设置密码（默认情况是没有密码的）的话，这个时候就能看到连接得到状态是“已连接”，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/填写客户端信息.png "填写客户端信息")


这个时候就能订阅消息了。选择“订阅”下方的绿色十字图标，就可以输入订阅的主题（topic）的名字，比如设置主题名称为“pubtopic”，并点击 “订阅”按钮，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/订阅.png "订阅")

### 修改对接IP
将代码中对接的IP修改为电脑在命令行窗口里输入 `ipconfig` 查询的电脑的本地IP，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/查询对接IP.png "查询对接IP")

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/修改对接IP.png "修改对接I")


### 修改BUILD.gn
将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/D5_iot_mqtt文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加D5_iot_mqtt:iot_mqtt.

```c
import("//build/lite/config/component/lite_component.gni")

lite_component("app") {
features = [ "D5_iot_mqtt:iot_mqtt", ]
}
```
### 编译烧录
点击DevEco Device Tool工具“Rebuild”按键，编译程序。

![image-20230103154607638](/doc/pic/image-20230103154607638.png)

点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


### 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，Eclipse Paho MQTT 工具上会接收到开发板发布的消息，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/接收消息.png "订阅")


往开发板发送一条消息主题为“substopic”，内容为“Hello”的MQTT消息。然后点击“发布”按钮，这个时候就能看到消息已经发送成功，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/发布消息.png "发布消息")

且开发板的串口上也打印出接收消息成功的信息，如下图所示。

![](/src/vendor/bearpi/bearpi_hm_nano/doc/figures/D5_iot_mqtt/接收消息成功.png "接收消息成功")

