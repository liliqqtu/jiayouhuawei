/*
1.项目名称：绿深旗舰店-51综合测试板SYN6288语音合成模块测试程序
2.显示模块：无
3.使用软件：keil4 for 51 或 keil5 for 51
4.配套上位机：无
5.项目组成：SYN6288语音合成模块、绿深51综合测试板
6.项目功能：将文字转化成语音朗读出来，出厂波特率默认9600。
7.主要原理：具体参考SYN6288数据手册
8.购买地址：https://lssz.tmall.com 或淘宝上搜索“绿深旗舰店”
9.版权声明：绿深旗舰店所有程序都申请软件著作权。均与本店产品配套出售，请不要传播，以免追究其法律责任！
接线定义：（此例程使用的晶振为11.0592M）
		VCC--5V
		RXD--TX/P1.2
		TXD--RX/P1.3
		GND--GND
*/
#include "reg51.h"
#include <string.h>


#define uchar unsigned char
#define uint unsigned int

#define S2RI  0x01          //S2CON.0
#define S2TI  0x02          //S2CON.1
#define S2RB8 0x04          //S2CON.2
#define S2TB8 0x08          //S2CON.3

sfr AUXR  = 0x8e;           //Auxiliary register
sfr S2CON = 0x9a;           //UART2 control register
sfr S2BUF = 0x9b;           //UART2 data buffer
sfr BRT   = 0x9c;           //Baudrate generator
sfr IE2   = 0xaf;           //Interrupt control

/**************芯片设置命令*********************/
uchar SYN_StopCom[] = {0xFD, 0X00, 0X02, 0X02, 0XFD}; //停止合成
uchar SYN_SuspendCom[] = {0XFD, 0X00, 0X02, 0X03, 0XFC}; //暂停合成
uchar SYN_RecoverCom[] = {0XFD, 0X00, 0X02, 0X04, 0XFB}; //恢复合成
uchar SYN_ChackCom[] = {0XFD, 0X00, 0X02, 0X21, 0XDE}; //状态查询
uchar SYN_PowerDownCom[] = {0XFD, 0X00, 0X02, 0X88, 0X77}; //进入POWER DOWN 状态命令

//串口2初始化
void Uart2_Init(void)
{
  AUXR &= 0xF7;		//波特率不倍速
  S2CON = 0x50;		//8位数据,可变波特率
  AUXR &= 0xFB;		//独立波特率发生器时钟为Fosc/12,即12T
  BRT = 0xFD;		//设定独立波特率发生器重装值
  AUXR |= 0x10;		//启动独立波特率发生器
  ES = 1;
  EA = 1;
}

//串口2发送一字节数据
void UART2_SendByte(uchar DAT)
{
  S2BUF = DAT;
  while((S2CON & 0X02) == 0);
  S2CON &= ~0X02;
}

//串口2发送字符串数据
void UART2_SendString(uchar *DAT, uchar len)
{
  uchar i;
  for(i = 0; i < len; i++)
  {
    UART2_SendByte(*DAT++);
  }
}

//串口2中断
void Uart2_Isr() interrupt 8
{
  if(S2CON & S2RI)
  {
    S2CON &= ~S2RI;
  }

}


//Music:  0:无背景音乐  1~15:选择背景音乐
void SYN_FrameInfo(uchar Music, uchar *HZdata)
{
  /****************需要发送的文本**********************************/
  uchar  Frame_Info[50];
  uchar  HZ_Length;
  uchar  ecc  = 0;  			//定义校验字节
  uint i = 0;
  HZ_Length = strlen((char*)HZdata); 			//需要发送文本的长度

  /*****************帧固定配置信息**************************************/
  Frame_Info[0] = 0xFD ; 			//构造帧头FD
  Frame_Info[1] = 0x00 ; 			//构造数据区长度的高字节
  Frame_Info[2] = HZ_Length + 3; 		//构造数据区长度的低字节
  Frame_Info[3] = 0x01 ; 			//构造命令字：合成播放命令
  Frame_Info[4] = 0x01 | Music << 4 ; //构造命令参数：背景音乐设定

  /*******************校验码计算***************************************/
  for(i = 0; i < 5; i++)   				//依次发送构造好的5个帧头字节
  {
    ecc = ecc ^ (Frame_Info[i]);		//对发送的字节进行异或校验
  }

  for(i = 0; i < HZ_Length; i++)   		//依次发送待合成的文本数据
  {
    ecc = ecc ^ (HZdata[i]); 				//对发送的字节进行异或校验
  }
  /*******************发送帧信息***************************************/
  memcpy(&Frame_Info[5], HZdata, HZ_Length);
  Frame_Info[5 + HZ_Length] = ecc;
  UART2_SendString(Frame_Info, 5 + HZ_Length + 1);
}


/***********************************************************
* 名    称： YS_SYN_Set(uchar *Info_data)
* 功    能： 主函数	程序入口
* 入口参数： *Info_data:固定的配置信息变量
* 出口参数：
* 说    明：本函数用于配置，停止合成、暂停合成等设置 ，默认波特率9600bps。
* 调用方法：通过调用已经定义的相关数组进行配置。
**********************************************************/
void YS_SYN_Set(uchar *Info_data)
{
  uchar Com_Len;
  Com_Len = strlen((char*)Info_data);
  UART2_SendString(Info_data, Com_Len);
}

//软件延时1ms
void  delay(uint uldata)
{
  uint j  =  0;
  uint g  =  0;
  for (j = 0; j < uldata; j++)
    for (g = 0; g < 850; g++);
}


void  main(void)
{
  Uart2_Init();
  while(1)
  {
    //选择背景音乐2。(0：无背景音乐  1-15：背景音乐可选)
    //m[0~16]:0背景音乐为静音，16背景音乐音量最大
    //v[0~16]:0朗读音量为静音，16朗读音量最大
    //t[0~5]:0朗读语速最慢，5朗读语速最快
    //其他不常用功能请参考数据手册
    SYN_FrameInfo(2, "[v5][t5]欢迎使用绿深旗舰店SYN6288语音合成模块");
    //暂停合成，此时没有用到，用于展示函数用法
    //YS_SYN_Set(SYN_SuspendCom);
    delay(10000);
  }

}
