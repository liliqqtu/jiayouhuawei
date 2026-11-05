/*
1.项目名称：绿深旗舰店-Arduino Leonardo综合测试板SYN6288语音合成模块测试程序
2.显示模块：无
3.配套APP：无
4.配套上位机：无
5.项目组成：绿深SYN6288语音合成模块、绿深Arduino综合测试板
6.项目功能：将文字转换为语音朗读出来。
7.主要原理：具体参考SYN6288数据手册
8.购买地址：https://lssz.tmall.com 或淘宝上搜索“绿深旗舰店”！
9.版权声明：绿深旗舰店所有程序都申请软件著作权。均与本店产品配套出售，请不要传播，以免追究其法律责任！
接线定义:
      VCC--5V
      RX--1
      TX--0
      GND--GND
*/



//由于Arduino IDE编码问题，在Arduino上使用时需要将要发送的文字转化成16进制发送，请使用Arduino函数生成器转换
//此处发送内容为：[m4][v16][t5]欢迎使用绿深旗舰店SYN6288语音合成模块
//m[0~16]:0背景音乐为静音，16背景音乐音量最大
//v[0~16]:0朗读音量为静音，16朗读音量最大
//t[0~5]:0朗读语速最慢，5朗读语速最快
//其他不常用功能请参考数据手册
void speech(){
  unsigned char i = 0;
  unsigned char head[56];

  head[0] = 0xFD;
  head[1] = 0x00;
  head[2] = 0x35;
  head[3] = 0x01;
  head[4] = 0x00;
  head[5] = 0x5B;
  head[6] = 0x6D;
  head[7] = 0x34;
  head[8] = 0x5D;
  head[9] = 0x5B;
  head[10] = 0x76;
  head[11] = 0x31;
  head[12] = 0x36;
  head[13] = 0x5D;
  head[14] = 0x5B;
  head[15] = 0x74;
  head[16] = 0x35;
  head[17] = 0x5D;
  head[18] = 0xBB;
  head[19] = 0xB6;
  head[20] = 0xD3;
  head[21] = 0xAD;
  head[22] = 0xCA;
  head[23] = 0xB9;
  head[24] = 0xD3;
  head[25] = 0xC3;
  head[26] = 0xC2;
  head[27] = 0xCC;
  head[28] = 0xC9;
  head[29] = 0xEE;
  head[30] = 0xC6;
  head[31] = 0xEC;
  head[32] = 0xBD;
  head[33] = 0xA2;
  head[34] = 0xB5;
  head[35] = 0xEA;
  head[36] = 0x53;
  head[37] = 0x59;
  head[38] = 0x4E;
  head[39] = 0x36;
  head[40] = 0x32;
  head[41] = 0x38;
  head[42] = 0x38;
  head[43] = 0xD3;
  head[44] = 0xEF;
  head[45] = 0xD2;
  head[46] = 0xF4;
  head[47] = 0xBA;
  head[48] = 0xCF;
  head[49] = 0xB3;
  head[50] = 0xC9;
  head[51] = 0xC4;
  head[52] = 0xA3;
  head[53] = 0xBF;
  head[54] = 0xE9;
  head[55] = 0x91;

  for(i=0; i<56; i++){
    Serial1.write(head[i]);
  }
}


void setup() {
  Serial1.begin(9600);
}

void loop() {
  speech();
  delay(10000);

}
