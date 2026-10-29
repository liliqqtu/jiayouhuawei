# BearPi-HM_Nano开发板OpenHarmony内核编程开发——信号量
本示例将演示如何在BearPi-HM_Nano开发板上使用cmsis 2.0 接口通过信号量同时从不同的线程访问共享资源。


## Semaphore API分析

###  osSemaphoreNew()

```c
osSemaphoreId_t osSemaphoreNew(uint32_t max_count,uint32_t initial_count,const osSemaphoreAttr_t *attr)
```
**描述：**

osSemaphoreNew 函数创建并初始化一个信号量对象，该对象用于管理对共享资源的访问，并返回指向信号量对象标识符的指针或在发生错误时返回 NULL 。它可以在 RTOS 启动之前（调用 osKernelStart）安全地调用，但不能在它初始化之前（调用 osKernelInitialize）调用。
> **注意** :不能在中断服务调用该函数。


**参数：**

|参数名|描述|
|:--|:------| 
| max_count |可用令牌的最大数量。  |
| initial_count |可用令牌的初始数量。  |
| attr |信号量的属性;空:默认值。  |

###  osSemaphoreRelease()

```c
osStatus_t osSemaphoreRelease(osSemaphoreId_t semaphore_id)
```
**描述：**
函数osSemaphoreRelease释放由参数semaphore_id指定的信号量对象的标记。

> **注意** :该函数可以在中断服务例程调用。


**参数：**

|参数名|描述|
|:--|:------| 
| semaphore_id | 由osSemaphoreNew获得的信号量ID。  |


###  osSemaphoreAcquire()

```c
osStatus_t osSemaphoreAcquire(osSemaphoreId_t semaphore_id,uint32_t timeout)	
```
**描述：**
阻塞函数osSemaphoreAcquire一直等待，直到由参数semaphore_id指定的信号量对象的标记可用为止。如果一个令牌可用，该函数立即返回并递减令牌计数。

> **注意** :如果参数timeout设置为0，可以从中断服务例程调用。


**参数：**

|参数名|描述|
|:--|:------| 
| semaphore_id | 由osSemaphoreNew获得的信号量ID。  |
| timeout | 超时值。 |


## 软件设计

**主要代码分析**

在Semaphore_example函数中，通过osSemaphoreNew()函数创建了g_semaphoreId信号量，Semaphore1Thread()函数中通过osSemaphoreAcquire()函数获取两个信号量，Semaphore2Thread()和Semaphore3Thread()函数中，先开始阻塞等待g_semaphoreId信号量。只有当Semaphore1Thread()函数中增加两次信号量，Semaphore2Thread()和Semaphore3Thread()才能继续同步运行。若Semaphore1Thread()函数中只增加一次信号量，那Semaphore2Thread()和Semaphore3Thread()只能轮流执行。
```c
void Semaphore1Thread(void)
{
    while (1) {
        // release Semaphores twice so that Semaphore2Thread and Semaphore3Thread can execute synchronously
        osSemaphoreRelease(g_semaphoreId);

        // if the Semaphore is released only once, Semaphore2Thread and Semaphore3Thread will run alternately.
        osSemaphoreRelease(g_semaphoreId);

        printf("Semaphore1Thread Release  Semap \n");
        osDelay(THREAD_DELAY_1S);
    }
}
void Semaphore2Thread(void)
{
    while (1) {
        // wait Semaphore
        osSemaphoreAcquire(g_semaphoreId, osWaitForever);

        printf("Semaphore2Thread get Semap \n");
        osDelay(THREAD_DELAY_10MS);
    }
}

void Semaphore3Thread(void)
{
    while (1) {
        // wait Semaphore
        osSemaphoreAcquire(g_semaphoreId, osWaitForever);

        printf("Semaphore3Thread get Semap \n");
        osDelay(THREAD_DELAY_10MS);
    }
}

/**
 * @brief Main Entry of the Semaphore Example
 *
 */
void SemaphoreExample(void)
{
    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = THREAD_STACK_SIZE;
    attr.priority = THREAD_PRIO;

    attr.name = "Semaphore1Thread";
    if (osThreadNew((osThreadFunc_t)Semaphore1Thread, NULL, &attr) == NULL) {
        printf("Failed to create Semaphore1Thread!\n");
    }

    attr.name = "Semaphore2Thread";
    if (osThreadNew((osThreadFunc_t)Semaphore2Thread, NULL, &attr) == NULL) {
        printf("Failed to create Semaphore2Thread!\n");
    }

    attr.name = "Semaphore3Thread";
    if (osThreadNew((osThreadFunc_t)Semaphore3Thread, NULL, &attr) == NULL) {
        printf("Failed to create Semaphore3Thread!\n");
    }

    g_semaphoreId = osSemaphoreNew(SEM_MAX_COUNT, 0, NULL);
    if (g_semaphoreId == NULL) {
        printf("Failed to create Semaphore!\n");
    }
}
```

## 编译调试


* 步骤一：将hi3861_hdu_iot_application/src/vendor/bearpi/bearpi_hm_nano/demo/A5_kernel_semaphore文件夹复制到hi3861_hdu_iot_application/src/applications/sample/wifi-iot/app/目录下。

* 步骤二：修改applications/sample/wifi-iot/app/目录下的BUILD.gn，在features字段中添加A5_kernel_semaphore:semaphore_example.

    ```c
    import("//build/lite/config/component/lite_component.gni")

    lite_component("app") {
    features = [ "A5_kernel_semaphore:semaphore_example", ]
    }
    ```
* 步骤三：点击DevEco Device Tool工具“Rebuild”按键，编译程序。

    ![image-20230103154607638](/doc/pic/image-20230103154607638.png)

* 步骤四：点击DevEco Device Tool工具“Upload”按键，等待提示（出现Connecting，please reset device...），手动进行开发板复位（按下开发板RESET键），将程序烧录到开发板中。

    ![image-20230103154836005](/doc/pic/image-20230103154836005.png)    
    


## 运行结果

示例代码编译烧录代码后，按下开发板的RESET按键，通过串口助手查看日志，Semaphore1Thread一次释放两个信号量，Semaphore2Thread和Semaphore3Thread同步执行。
```
Semaphore1Thread Release  Semap 
Semaphore2Thread get Semap 
Semaphore3Thread get Semap 
Semaphore1Thread Release  Semap 
Semaphore2Thread get Semap 
Semaphore3Thread get Semap 
Semaphore1Thread Release  Semap 
Semaphore2Thread get Semap 
Semaphore3Thread get Semap 
```
