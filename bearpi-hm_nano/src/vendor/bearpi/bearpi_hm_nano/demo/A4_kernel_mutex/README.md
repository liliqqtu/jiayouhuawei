# BearPi-HM_Nano开发板OpenHarmony内核编程开发——互斥锁
本示例将演示如何在BearPi-HM_Nano开发板上使用cmsis 2.0 接口使用互斥来同步任务。


## Mutex API分析


### osMutexNew()

```c
osMutexId_t osMutexNew(const osMutexAttr_t *attr)
```
**描述：**

函数osMutexNew创建并初始化一个新的互斥锁对象，并返回指向互斥锁对象标识符的指针，如果出现错误则返回NULL可以在RTOS启动(调用 osKernelStart)之前安全地调用该函数，但不能在内核初始化 (调用 osKernelInitialize)之前调用该函数。
> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| attr |互斥对象的属性。  |

### osMutexAcquire()

```c
osStatus_t osMutexAcquire(osMutexId_t mutex_id,uint32_t timeout)
```
**描述：**
函数osMutexAcquire一直等待，直到参数mutex_id指定的互斥对象可用为止。如果没有其余线程获得互斥锁，该函数立即返回并阻塞互斥锁对象。

> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| mutex_id | 通过osMutexNew获得互斥锁ID。  |
| timeout | 超时值。 |

###  osMutexRelease()

```c
osStatus_t osMutexRelease(osMutexId_t mutex_id)
```
**描述：**
函数osMutexRelease释放一个由参数mutex_id指定的互斥量。当前等待这个互斥锁的其余线程将被置于就绪状态。

> **注意** :不能从中断服务例程调用此函数。


**参数：**

|参数名|描述|
|:--|:------| 
| mutex_id | 通过osMutexNew获得互斥锁ID。  |


## 软件设计

**主要代码分析**

在MutexExample函数中，通过osMutexNew()函数创建了互斥锁ID，并创建的三个不同优先级的任务，在第一秒，高优先级和中优先级线程被延迟。因此，低优先级线程可以启动自己的工作，获得互斥锁并在持有它时延迟。在第一秒之后，高优先级和中优先级线程就准备好了。因此高优先级线程获得优先级并尝试获取互斥锁。因为互斥锁已经被低优先级线程所拥有，所以高优先级线程被阻塞，中间优先级线程被执行，并开始执行许多非阻塞的工作，3S后低优先级释放互斥锁，高优先级线程准备就绪并立即被调度。

```c
void HighPrioThread(void)
{
    // wait 1s until start actual work
    osDelay(THREAD_DELAY_1S);

    while (1) {
        // try to acquire mutex
        osMutexAcquire(g_mutexId, osWaitForever);

        printf("HighPrioThread is running.\n");
        osDelay(THREAD_DELAY_3S);
        osMutexRelease(g_mutexId);
    }
}

void MidPrioThread(void)
{
    // wait 1s until start actual work
    osDelay(THREAD_DELAY_1S);

    while (1) {
        printf("MidPrioThread is running.\n");
        osDelay(THREAD_DELAY_1S);
    }
}

void LowPrioThread(void)
{
    while (1) {
        osMutexAcquire(g_mutexId, osWaitForever);
        printf("LowPrioThread is running.\n");

        // block mutex for 3s
        osDelay(THREAD_DELAY_3S);
        osMutexRelease(g_mutexId);
    }
}

/**
 * @brief Main Entry of the Mutex Example
 *
 */
void MutexExample(void)
{
    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;

    attr.name = "HighPrioThread";
    attr.priority = HIGH_THREAD_PRIO;
    if (osThreadNew((osThreadFunc_t)HighPrioThread, NULL, &attr) == NULL) {
        printf("Failed to create HighPrioThread!\n");
    }

    attr.name = "MidPrioThread";
    attr.priority = MID_THREAD_PRIO;
    if (osThreadNew((osThreadFunc_t)MidPrioThread, NULL, &attr) == NULL) {
        printf("Failed to create MidPrioThread!\n");
    }

    attr.name = "LowPrioThread";
    attr.priority = LOW_THREAD_PRIO;
    if (osThreadNew((osThreadFunc_t)LowPrioThread, NULL, &attr) == NULL) {
        printf("Failed to create LowPrioThread!\n");
    }

    g_mutexId = osMutexNew(NULL);
    if (g_mutexId == NULL) {
        printf("Failed to create Mutex!\n");
    }
}
```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/A4_kernel_mutex文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加A4_kernel_mutex:mutex_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "A4_kernel_mutex:mutex_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，中优先级任务一直正常运行，而高优先级和低优先级的任务因为互相抢占互斥锁，交替运行。
```c
LowPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
HighPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
LowPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
HighPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing.
MidPrioThread is runing
```
