# BearPi-HM_Nano开发板OpenHarmony内核编程开发——消息队列
本示例将演示如何在BearPi-HM_Nano开发板上使用cmsis 2.0 接口通过消息队列进行线程之间交换消息。


## MessageQueue API分析
### osMessageQueueNew()

```c
osMessageQueueId_t osMessageQueueNew(uint32_t msg_count,uint32_t msg_size,const osMessageQueueAttr_t *attr)
```
**描述：**

函数osMessageQueueNew创建并初始化一个消息队列对象。该函数返回消息队列对象标识符，如果出现错误则返回NULL，可以在RTOS启动(调用 osKernelStart)之前安全地调用该函数，也可以在内核初始化 (调用 osKernelInitialize)之前调用该函数。
> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| msg_count |队列中的最大消息数。  |
| msg_size |最大消息大小(以字节为单位)。  |
| attr |消息队列属性;空:默认值。 |

### osMessageQueuePut()

```c
osStatus_t osMessageQueuePut(osMessageQueueId_t mq_id,const void *msg_ptr,uint8_t msg_prio,uint32_t timeout)	
```
**描述：**
函数osMessageQueuePut将msg_ptr指向的消息放入参数mq_id指定的消息队列中。

> **注意** :如果参数timeout设置为0，可以从中断服务例程调用。


**参数：**

|参数名|描述|
|:--|:------| 
| mq_id | 由osMessageQueueNew获得的消息队列ID。  |
| msg_ptr | 要发送的消息。  |
| msg_prio | 指优先级。  |
| timeout | 超时值。  |

### osMessageQueueGet()

```c
osStatus_t osMessageQueueGet(osMessageQueueId_t mq_id,void *msg_ptr,uint8_t *msg_prio,uint32_t 	timeout)
```
**描述：**
函数osMessageQueueGet从参数mq_id指定的消息队列中检索消息，并将其保存到参数msg_ptr所指向的缓冲区中。

> **注意** :如果参数timeout设置为0，可以从中断服务例程调用。


**参数：**

|参数名|描述|
|:--|:------| 
| mq_id | 由osMessageQueueNew获得的消息队列ID。  |
| msg_ptr | 指针指向队列中获取消息的缓冲区指针。  |
| msg_prio | 指优先级。  |
| timeout | 超时值。  |


## 软件设计

**主要代码分析**

在MessageExample函数中，通过osMessageQueueNew()函数创建了消息队列ID，MsgQueue1Thread()函数中通过osMessageQueuePut()函数向消息队列中发送消息。在MsgQueue2Thread()函数中通过osMessageQueueGet()函数读取消息队列中的消息比打印出来。

```c

void MsgQueue1Thread(void)
{
    // do some work...
    msg.buf = "Hello BearPi-HM_Nano!";
    msg.idx = 0U;
    while (1) {
        osMessageQueuePut(g_msgQueueId, &msg, 0U, 0U);

        // suspend thread
        osThreadYield();
        osDelay(THREAD_DELAY_1S);
    }
}

void MsgQueue2Thread(void)
{
    osStatus_t status;

    while (1) {
        // wait for message
        status = osMessageQueueGet(g_msgQueueId, &msg, NULL, osWaitForever);
        if (status == osOK) {
            printf("Message Queue Get msg:%s\n", msg.buf);
        }
    }
}

/**
 * @brief Main Entry of the Message Example
 *
 */
static void MessageExample(void)
{
    g_msgQueueId = osMessageQueueNew(MSGQUEUE_COUNT, MSGQUEUE_SIZE, NULL);
    if (g_msgQueueId == NULL) {
        printf("Failed to create Message Queue!\n");
    }

    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    attr.name = "MsgQueue1Thread";
    if (osThreadNew(MsgQueue1Thread, NULL, &attr) == NULL) {
        printf("Failed to create MsgQueue1Thread!\n");
    }

    attr.name = "MsgQueue2Thread";
    if (osThreadNew(MsgQueue2Thread, NULL, &attr) == NULL) {
        printf("Failed to create MsgQueue2Thread!\n");
    }
}

```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/A6_kernel_message文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加A6_kernel_message:message_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "A6_kernel_message:message_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，会打印从消息队列中获取的消息。
```c
Message Queue Get msg:Hello BearPi-HM_Nano!
Message Queue Get msg:Hello BearPi-HM_Nano!
Message Queue Get msg:Hello BearPi-HM_Nano!
Message Queue Get msg:Hello BearPi-HM_Nano!
Message Queue Get msg:Hello BearPi-HM_Nano!
```
