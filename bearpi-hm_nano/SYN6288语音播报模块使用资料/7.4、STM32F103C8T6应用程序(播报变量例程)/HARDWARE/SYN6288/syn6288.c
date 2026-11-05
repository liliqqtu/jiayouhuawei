#include "syn6288.h"
#include "usart.h"
#include "string.h"
#include "delay.h"
#include "stdarg.h"

//Music:选择背景音乐。0:无背景音乐，1~15：选择背景音乐
void SYN_FrameInfo(u8 Music, u8 *HZdata)
{
  /****************需要发送的文本**********************************/
  unsigned  char  Frame_Info[50];
  unsigned  char  HZ_Length;
  unsigned  char  ecc  = 0;  			//定义校验字节
  unsigned  int i = 0;
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
  USART3_SendString(Frame_Info, 5 + HZ_Length + 1);
}


/***********************************************************
* 名    称： YS_SYN_Set(u8 *Info_data)
* 功    能： 主函数	程序入口
* 入口参数： *Info_data:固定的配置信息变量
* 出口参数：
* 说    明：本函数用于配置，停止合成、暂停合成等设置 ，默认波特率9600bps。
* 调用方法：通过调用已经定义的相关数组进行配置。
**********************************************************/
void YS_SYN_Set(u8 *Info_data)
{
  u8 Com_Len;
  Com_Len = strlen((char*)Info_data);
  USART3_SendString(Info_data, Com_Len);
}
/******************************************************************************
 * 函  数： SYN6288_Say
 * 功  能： 输出合成语音
 * 参  数： 格式化参数，如printf参数般一样的用法
 * 返回值： 无
 * 示  例： SYN6288_Say("你好吗?");
*******************************************************************************/
void SYN6288_Say(char *fmt, ...)
{
    static char  str_1[100];   // 缓存区1
    static char  str_2[200];   // 缓存区2

    va_list ap;
    va_start(ap, fmt);
    vsprintf(str_1, fmt, ap);
    va_end(ap);

    sprintf(str_2, "[d][V12][m15][t5]%s", str_1);  // [d]恢复默认状态，[V12]朗读音量0~16，[m15]背景音量0~16，[t5]语速0~5
	SYN_FrameInfo(0, (uint8_t *)str_2); // 无背景音乐
}

