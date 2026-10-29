

# 源仓库

./demo/ 来源于：[润和满天星系列Pegasus智能家居OpenHarmony开发套件资料汇总: 润和满天星系列Pegasus资料汇总 - Gitee.com](https://gitee.com/hihope_iot/HiHope_Pegasus_Doc/tree/master/[3]智能家居套件完整例程/OpenHarmony master版本例程)

./docs/ 来源于：[【4】硬件原理图和其他海思官方资料 · HiHope开源社区/润和满天星系列Pegasus智能家居OpenHarmony开发套件资料汇总 - 码云 - 开源中国 (gitee.com)](https://gitee.com/hihope_iot/HiHope_Pegasus_Doc/tree/master/[4]硬件原理图和其他海思官方资料)

在迁移到[本仓库：hi3861_hdu_iot_application](https://gitee.com/HiSpark/hi3861_hdu_iot_application)时，相关的代码和编译配置有少量调整，并在OpenHarmony的 3.1Release 版本上编译通过并确认基本功能无异常；用户在使用这些示例程序时，如发现异常，建议参考和对比上述两个原始仓库的代码后再做进一步确认。

# 编译说明

在[本仓库：hi3861_hdu_iot_application](https://gitee.com/HiSpark/hi3861_hdu_iot_application)代码中编译./demo/目录下的示例程序，可以参考如下修改：

1. 修改  hi3861_hdu_iot_application\src\applications\sample\wifi-iot\app\BUILD.gn 文件，增加 demo 编译目标：

   ```
   lite_component("app") {
     features = [
       "startup",
       "//vendor/hihope/hispark_pegasus/demo:demo",
     ]
   }
   ```

2. 修改./demo/BUILD.gn，根据需要编译对应的示例程序：

   ```
   lite_component("demo") {
     features = [
   #   "00_thread:thread_demo",
   #   "01_timer:timer_demo",
   #   "02_delay:delay_demo",
   #   "03_mutex:mutex_demo",
   #   "04_semaphore:semp_demo",
   #   "05_message:message_demo",
   #   "06_gpioled:led_example",
   #   "07_gpiobutton:button_example",
   #   "08_pwmled:pwm_led_demo",
   #   "09_adc:adc_demo",
   #   "10_i2caht20:app",
   #   "11_uart:uart_demo",
   #   "12_ssd1306:app",
   #   "13_oledplayer:app",
   #   "14_pwmbeer:pwm_beer_demo",
   #   "15_pwmbeermusic:beeper_music_demo",
   #   "16_trafficlight:traffic_light_demo",
   #   "17_colorfullight:colorful_light_demo",
   #   "18_environment:environment",
   #   "19_wificonnect:wifi_demo",
   #   "20_wifihotspot:wifi_demo",
   #   "21_tcpclient:net_demo",
   #   "22_tcpserver:net_demo",
   #   "23_udpclient:net_demo",
   #   "24_udpserver:net_demo",
   #   "25_sntp:app",
   #   "26_paho.mqtt.embedded-c:app",
   #   "27_httpd:app",
   #   "28_easy_wifi:app",
     ]
   }
   ```

