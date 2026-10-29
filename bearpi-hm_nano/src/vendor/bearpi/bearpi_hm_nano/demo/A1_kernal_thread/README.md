# BearPi-HM_Nano开发板OpenHarmony内核编程开发——Thread多线程
本示例将演示如何在BearPi-HM_Nano开发板上使用cmsis 2.0 接口进行多线程开发。


## Thread API分析

### osThreadNew()

```c
osThreadId_t osThreadNew(osThreadFunc_t	func, void *argument,const osThreadAttr_t *attr )	
```
**描述：**

函数osThreadNew通过将线程添加到活动线程列表并将其设置为就绪状态来启动线程函数。线程函数的参数使用参数指针*argument传递。当创建的thread函数的优先级高于当前运行的线程时，创建的thread函数立即启动并成为新的运行线程。线程属性是用参数指针attr定义的。属性包括线程优先级、堆栈大小或内存分配的设置。可以在RTOS启动(调用 osKernelStart)之前安全地调用该函数，但不能在内核初始化 (调用 osKernelInitialize)之前调用该函数。
> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| func | 线程函数。  |
| argument |作为启动参数传递给线程函数的指针。|
| attr |线程属性。|

## 软件设计

**主要代码分析**

在ThreadExample函数中，通过osThreadNew()函数创建了Thread1和Thread2两个进程，Thread1和Thread2启动后会输出打印日志。

```c
/**
 * @brief Thread1 entry
 *
 */
void Thread1(void)
{
    int sum = 0;

    while (1) {
        printf("This is BearPi-HM_Nano Thread1----%d\n", sum++);
        usleep(THREAD_DELAY_1S);
    }
}

/**
 * @brief Thread2 entry
 *
 */
void Thread2(void)
{
    int sum = 0;

    while (1) {
        printf("This is BearPi-HM_Nano Thread2----%d\n", sum++);
        usleep(THREAD_DELAY_500MS);
    }
}

/**
 * @brief Main Entry of the Thread Example
 *
 */
static void ThreadExample(void)
{
    osThreadAttr_t attr;

    attr.name = "Thread1";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    // Create the Thread1 task
    if (osThreadNew((osThreadFunc_t)Thread1, NULL, &attr) == NULL) {
        printf("Failed to create Thread1!\n");
    }

    // Create the Thread2 task
    attr.name = "Thread2";
    if (osThreadNew((osThreadFunc_t)Thread2, NULL, &attr) == NULL) {
        printf("Failed to create Thread2!\n");
    }
}

```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/A1_kernal_thread文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加A1_kernal_thread:thread_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "A1_kernal_thread:thread_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，Thread1和Thread2会交替打印信息。
```c
This is BearPi-HM_Nano Thread1----2
This is BearPi-HM_Nano Thread2----4
This is BearPi-HM_Nano Thread2----5
This is BearPi-HM_Nano Thread1----3
This is BearPi-HM_Nano Thread2----6
This is BearPi-HM_Nano Thread2----7
```
