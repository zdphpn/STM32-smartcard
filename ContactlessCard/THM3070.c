

#include "THM3070.h"


/*
文件用途:           THM3070驱动文件
作者:               张栋培
创建时间:           2018/04/20
更新时间:           2018/05/31
版本:               V1.1

历史版本:           V1.0:实现THM3070驱动,通信接口为软件模拟SPI方式
                    V1.1:增加硬件SPI,软件硬件可通过THM_SPIMOD选择
*/





#if THM_SPIMOD==0
/*模拟SPI引脚状态切换定义*/
#define THM_IO1_SCK_H       GPIO_SetBits(THM_IO1_SCK_GPIOx,THM_IO1_SCK_Pinx)
#define THM_IO1_SCK_L       GPIO_ResetBits(THM_IO1_SCK_GPIOx,THM_IO1_SCK_Pinx)
#define THM_IO2_MOSI_H      GPIO_SetBits(THM_IO2_MOSI_GPIOx,THM_IO2_MOSI_Pinx)
#define THM_IO2_MOSI_L      GPIO_ResetBits(THM_IO2_MOSI_GPIOx,THM_IO2_MOSI_Pinx)

#define THM_IO3_MISO        GPIO_ReadInputDataBit(THM_IO3_MISO_GPIOx,THM_IO3_MISO_Pinx)

/*模拟SPI引脚状态切换定义*/
#endif

#define THM_IO4_SS_N_H      GPIO_SetBits(THM_IO4_SS_N_GPIOx,THM_IO4_SS_N_Pinx)
#define THM_IO4_SS_N_L      GPIO_ResetBits(THM_IO4_SS_N_GPIOx,THM_IO4_SS_N_Pinx)


#define THM3070_TimeCountMax 0x00004400                                         //最大等待计数,不应过大,防止THM3070在收发帧数据时卡死
static uint32_t THM3070_TimeCount = 0;


static void THM3070_GPIO_Init(void);
static uint8_t THM3070_REG_Init(void);
/*
功能：  THM3070芯片初始化
参数：  无
返回：  1成功，0失败
*/
uint8_t THM3070_Init()
{
    uint8_t temp;

    THM3070_GPIO_Init();                                                        //引脚初始化

    GPIO_ResetBits(THM_RESET_GPIOx, THM_RESET_Pinx);
    delay_us(5000);                                                             //复位
    GPIO_SetBits(THM_RESET_GPIOx, THM_RESET_Pinx);

    temp = THM3070_REG_Init();                                                  //寄存器初始化

    //THM3070_RFClose();                                                         //默认把场关闭

    return temp;
}

/*
功能：  THM3070所用GPIO初始化，内部函数
参数：  无
返回：  无
*/
static void THM3070_GPIO_Init()
{
    GPIO_InitTypeDef GPIO_InitStructure;

    THM_RCC_APBxPeriphClockCmd;                                                 //开时钟

    GPIO_InitStructure.GPIO_Pin = THM_MOD0_Pinx;                                //MOD0引脚输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(THM_MOD0_GPIOx, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = THM_STANDBY_Pinx;                             //STANDBY引脚输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(THM_STANDBY_GPIOx, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = THM_RESET_Pinx;                               //RESET引脚输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(THM_RESET_GPIOx, &GPIO_InitStructure);

    THM3070_SleepMode(0);                                                       //运行
    THM3070_PortMode(0);                                                        //SPI通信接口

}
/*
功能：  THM3070寄存器初始化，内部函数
参数：  无
返回：  无
*/
static uint8_t THM3070_REG_Init()
{
    uint8_t temp;

    THM3070_WriteREG(THM_REG_TXDP1, 0xC8);                                      //场强大小,TYPEB 模式下保证DP0>0 AND DP0<DP1
    THM3070_WriteREG(THM_REG_TXDP0, 0xC0);                                      //
    THM3070_WriteREG(THM_REG_TXDN1, 0x60);                                      //
    THM3070_WriteREG(THM_REG_TXDN0, 0x17);                                      //TYPEB调制深度200/DP0:200/DN0,0x17 is default


    THM3070_WriteREG(THM_REG_PSEL, 0x00);                                       //TYPEB,106kbit/s
    THM3070_WriteREG(THM_REG_FCONB, 0x2A);                                      //EOF=10.5ETU,SOF_H=2.5ETU,SOF_L=10.5ETU
    THM3070_WriteREG(THM_REG_EGT, 0x01);                                        //EGT=2ETU
    THM3070_WriteREG(THM_REG_CRCSEL, 0xC1);                                     //发送CRC使能,接收CRC判断使能,接收超时计时使能
    THM3070_WriteREG(THM_REG_INTCON, 0x01);                                     //允许IRQ输出
    THM3070_WriteREG(THM_REG_SMOD, 0x00);                                       //编码1/4,调制深度10%-30%
    THM3070_WriteREG(THM_REG_PWYH, 0x27);                                       //(0x27+1)/Fc
    THM3070_WriteREG(THM_REG_EMVEN, 0xFD);                                      //使能各种噪声错误检测
    THM3070_WriteREG(THM_REG_RXCON, 0x42);                                      //接收电路工作,放大倍数40dB
    THM3070_WriteREG(THM_REG_TXCON, 0x62);                                      //10%ASK调制,打开发射电路

    THM3070_SetFWT(0x64);                                                       //设置一个默认的超时时间100*330us=33ms

    temp = THM3070_ReadREG(THM_REG_RXCON);
    if(temp != 0x42)                                                            //读寄存器比较看写入是否成功
    {
        return 0;
    }
    return 1;
}


static void THM3070SPI_SendBuff(uint8_t *p_val, uint16_t len_val);
static void THM3070SPI_RecvBuff(uint8_t *p_val, uint16_t len_val);
/*
功能：  THM3070写寄存器
参数1： 寄存器地址
参数2： 寄存器值
返回：  无
*/
void THM3070_WriteREG(uint8_t addr, uint8_t val)
{
    uint8_t temp[2];

    temp[0] = addr | 0x80;                                                      //写寄存器时高位置1
    temp[1] = val;

    THM_IO4_SS_N_L;                                                             //片选拉低使能
    THM3070SPI_SendBuff(temp, 2);                                               //发送地址
    THM_IO4_SS_N_H;                                                             //片选拉高

}
/*
功能：  THM3070读寄存器
参数1： 寄存器地址
返回：  寄存器值
*/
uint8_t THM3070_ReadREG(uint8_t addr)
{
    uint8_t temp[1];

    THM_IO4_SS_N_L;                                                             //片选拉低使能

    temp[0] = addr & 0x7F;                                                      //读寄存器时高位清零
    THM3070SPI_SendBuff(temp, 1);                                               //发送地址
    THM3070SPI_RecvBuff(temp, 1);                                               //接收数据

    THM_IO4_SS_N_H;                                                             //片选拉高

    return temp[0];                                                             //返回
}



/*
功能：	SPI发送一个字节
参数：	待发送的值
返回：	无
*/
static void THM3070SPI_SendByte(uint8_t val)
{
#if THM_SPIMOD==0

    uint8_t i;

    for(i = 0; i < 8; i++)                                                      //循环按位发送
    {
        THM_IO1_SCK_L;                                                          //时钟拉低
        if(val & 0x80)                                                          //先发高位
        {
            THM_IO2_MOSI_H;
        }
        else                                                                    //数据拉高或拉低
        {
            THM_IO2_MOSI_L;
        }
        val = val << 1;                                                         //移位
        THM_IO1_SCK_H;                                                          //时钟拉高
    }
    THM_IO1_SCK_L;                                                              //时钟拉低

#else

    while(SPI_I2S_GetFlagStatus(THM_SPIx, SPI_I2S_FLAG_TXE) == RESET) {;}
    SPI_I2S_SendData(THM_SPIx, val);
    while(SPI_I2S_GetFlagStatus(THM_SPIx, SPI_I2S_FLAG_RXNE) == RESET) {;}
    SPI_I2S_ReceiveData(THM_SPIx);

#endif
}
/*
功能：  SPI发送数据段
参数1： 待发送的数据指针
参数2： 待发送的数据长度
返回：  无
*/
static void THM3070SPI_SendBuff(uint8_t *p_val, uint16_t len_val)
{
    uint16_t i;

    for(i = 0; i < len_val; i++)
    {
        THM3070SPI_SendByte(p_val[i]);                                          //发送字节
    }
}
/*
功能：  SPI接收一个字节
参数：  无
返回：  接收到的值
*/
static uint8_t THM3070SPI_RecvByte()
{
#if THM_SPIMOD==0
    uint8_t i, dat, temp;

    THM_IO1_SCK_L;                                                              //时钟拉低
    dat = 0;
    temp = 0x80;
    for(i = 0; i < 8; i++)                                                      //循环按位接收
    {
        THM_IO1_SCK_H;                                                          //时钟拉高
        if(THM_IO3_MISO & 0x01)                                                 //读取信号电平
        {
            dat |= temp;
        }
        THM_IO1_SCK_L;                                                          //时钟拉低
        temp = temp >> 1;                                                       //移位
    }
    return dat;                                                                 //返回

#else

    while(SPI_I2S_GetFlagStatus(THM_SPIx, SPI_I2S_FLAG_TXE) == RESET) {;}
    SPI_I2S_SendData(THM_SPIx, 0xFF);
    while(SPI_I2S_GetFlagStatus(THM_SPIx, SPI_I2S_FLAG_RXNE) == RESET) {;}
    return SPI_I2S_ReceiveData(THM_SPIx);

#endif
}
/*
功能：  SPI接收数据段
参数1： 存放数据的数据指针
参数2： 要接收的数据长度
返回：  无
*/
static void THM3070SPI_RecvBuff(uint8_t *p_val, uint16_t len_val)
{
    uint16_t i;

    if(len_val == 0)                                                            //数据长度为0
    {
        len_val = 0x0100;                                                       //长度为256
    }
    for(i = 0; i < len_val; i++)
    {
        p_val[i] = THM3070SPI_RecvByte();                                       //接收字节
    }
}


/*
功能：  发送帧
参数1： 存放数据的数据指针
参数2： 要发送的数据长度
返回：  无
*/
void THM3070_SendFrame(uint8_t *p_buff, uint16_t len_buff)
{
    uint8_t temp[1];

    THM3070_WriteREG(THM_REG_SCON, 0x05);                                       //清数据缓冲区
    THM3070_WriteREG(THM_REG_SCON, 0x01);                                       //正常状态
    THM3070_WriteREG(THM_REG_EMVERR, 0xFF);                                     //

    temp[0] = THM_REG_DATA | 0x80;                                              //写寄存器高位置1

    THM_IO4_SS_N_L;                                                             //片选拉低使能

    THM3070SPI_SendBuff(temp, 1);                                               //发送地址
    THM3070SPI_SendBuff(p_buff, len_buff);                                      //发送数据

    THM_IO4_SS_N_H;                                                             //片选拉高

    THM3070_WriteREG(THM_REG_SCON, 0x03);                                       //启动发送
    THM3070_TimeCount = 0;
    while(!THM3070_ReadREG(THM_REG_TXFIN))                                      //等待发送完成
    {
        THM3070_TimeCount++;
        if(THM3070_TimeCount > THM3070_TimeCountMax)                            //加入超时判断,避免死等
        {
            THM3070_TimeCount = 0;
            break;
        }
    }

}

/*
功能：	接收帧
参数1：	存放数据的数据指针
参数2：	要接收的数据长度
返回：	执行结果
*/
uint8_t THM3070_RecvFrame(uint8_t *p_buff, uint16_t *len_buff)
{
    uint8_t EMVError, RStatus = 0;

    THM3070_TimeCount = 0;
    while(1)                                                                    //等待接收完毕或出错
    {
        EMVError = THM3070_ReadREG(THM_REG_EMVERR);
        RStatus = THM3070_ReadREG(THM_REG_RSTAT);
        if(RStatus)
        {
            if(EMVError & 0x02)
            {
                return THM_ERROR_EMVERR;
            }
            break;
        }
        THM3070_TimeCount++;
        if(THM3070_TimeCount > THM3070_TimeCountMax)                            //加入超时判断,避免死等
        {
            THM3070_TimeCount = 0;
            RStatus = THM_RSTST_TMROVER;
            break;
        }
    }
    if(RStatus & 0x40)                                                          //判断错误类型
    {
        RStatus = THM_RSTST_CERR;
        //		delay_ms(6);                                                    //冲突之后在上层应用中延时
    }
    else if(RStatus & 0x20)
    {
        RStatus = THM_RSTST_PERR;
    }
    else if(RStatus & 0x10)
    {
        RStatus = THM_RSTST_FERR;
    }
    else if(RStatus & 0x08)
    {
        RStatus = THM_RSTST_DATOVER;
    }
    else if(RStatus & 0x04)
    {
        RStatus = THM_RSTST_TMROVER;
    }
    else if(RStatus & 0x02)
    {
        RStatus = THM_RSTST_CRCERR;
    }
    else
    {
        RStatus = THM_RSTST_FEND;                                               //无错误
    }

    *len_buff = THM3070_ReadREG(THM_REG_RSCH);
    *len_buff = *len_buff << 8;
    *len_buff |= THM3070_ReadREG(THM_REG_RSCL);                                 //读取数据长度
    if(*len_buff > 256)
    {
        return THM_RSTST_DATOVER;
    }

    if(*len_buff > 0)
    {
        uint8_t temp[1];

        THM_IO4_SS_N_L;                                                         //片选拉低使能

        temp[0] = THM_REG_DATA;                                                 //写寄存器高位置1

        THM3070SPI_SendBuff(temp, 1);                                           //发送地址
        THM3070SPI_RecvBuff(p_buff, *len_buff);                                 //接收数据

        THM_IO4_SS_N_H;                                                         //片选拉高
    }
    THM3070_WriteREG(THM_REG_RSTAT, 0x00);                                      //清除状态

    return RStatus;
}



/*
功能：  关场再开场
参数：  无
返回：  无
*/
void THM3070_RFReset()
{
    uint8_t temp;

    temp = THM3070_ReadREG(THM_REG_TXCON);                                      //先读取

    THM3070_WriteREG(THM_REG_TXCON, temp | 0x01);                               //只将最低位置1，关闭
    delay_ms(2);                                                                //延时
    THM3070_WriteREG(THM_REG_TXCON, temp & 0xFE);                               //只将最低位清0，打开
    delay_ms(8);                                                                //延时,10ms是最稳妥的
}

/*
功能：	关场再开场
参数：	无
返回：	无
*/
void THM3070_RFClose()
{
    uint8_t temp;

    temp = THM3070_ReadREG(THM_REG_TXCON);                                      //先读取

    THM3070_WriteREG(THM_REG_TXCON, temp | 0x01);                               //只将最低位置1，关闭
}

/*
功能：  选择接口
参数：  0=SPI,1=IDR
返回：  无
*/
void THM3070_PortMode(uint8_t mode)
{
#if THM_SPIMOD==1
    SPI_InitTypeDef SPI_InitStructure;
#endif
    GPIO_InitTypeDef GPIO_InitStructure;

    if(mode)
    {
        GPIO_InitStructure.GPIO_Pin = THM_IO1_SCK_Pinx;                         //输入
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(THM_IO1_SCK_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO2_MOSI_Pinx;                        //输入
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(THM_IO2_MOSI_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO3_MISO_Pinx;                        //输入
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(THM_IO3_MISO_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO4_SS_N_Pinx;                        //输入
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(THM_IO4_SS_N_GPIOx, &GPIO_InitStructure);

        GPIO_SetBits(THM_MOD0_GPIOx, THM_MOD0_Pinx);                            //拉高，选择IDR接口模式
    }
    else
    {
#if THM_SPIMOD==0
        GPIO_InitStructure.GPIO_Pin = THM_IO1_SCK_Pinx;                         //SCK引脚输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(THM_IO1_SCK_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO2_MOSI_Pinx;                        //MOSI引脚输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(THM_IO2_MOSI_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO3_MISO_Pinx;                        //MISO引脚输入
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(THM_IO3_MISO_GPIOx, &GPIO_InitStructure);

#else
        GPIO_InitStructure.GPIO_Pin = THM_IO1_SCK_Pinx;                         //SCK引脚复用
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(THM_IO1_SCK_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO2_MOSI_Pinx;                        //MOSI引脚复用
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(THM_IO2_MOSI_GPIOx, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = THM_IO3_MISO_Pinx;                        //MISO引脚复用
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(THM_IO3_MISO_GPIOx, &GPIO_InitStructure);

        SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
        SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
        SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
        SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
        SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
        SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
        SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_16;
        SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
        SPI_InitStructure.SPI_CRCPolynomial = 7;
        SPI_Init(THM_SPIx, &SPI_InitStructure);

        SPI_Cmd(THM_SPIx, ENABLE);
#endif
        GPIO_InitStructure.GPIO_Pin = THM_IO4_SS_N_Pinx;                        //SS_N引脚输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_Init(THM_IO4_SS_N_GPIOx, &GPIO_InitStructure);

        THM_IO4_SS_N_H;                                                         //片选拉高，默认不使能读写

        GPIO_ResetBits(THM_MOD0_GPIOx, THM_MOD0_Pinx);                          //拉低，选择SPI通信
    }
}
/*
功能：  运行或者低功耗
参数：  0=运行,1=低功耗
返回：  无
*/
void THM3070_SleepMode(uint8_t mode)
{
    if(mode)
    {
        GPIO_SetBits(THM_STANDBY_GPIOx, THM_STANDBY_Pinx);                      //拉高进入低功耗
    }
    else
    {
        GPIO_ResetBits(THM_STANDBY_GPIOx, THM_STANDBY_Pinx);                    //拉低正常运行
    }
}
/*
功能：  设置超时时间
参数：  *330us(0-0x00FFFFFF)
返回：  无
*/
void THM3070_SetFWT(uint32_t fwt)
{
    THM3070_WriteREG(THM_REG_FWIHIGH, (fwt >> 16) & 0xFF);                      //设置定时器计数值
    THM3070_WriteREG(THM_REG_FWIMID, (fwt >> 8) & 0xFF);
    THM3070_WriteREG(THM_REG_FWILOW, fwt & 0xFF);
}

/*
功能：  切换到TYPEA模式
参数1： 无
返回：  无
*/
void THM3070_SetTYPEA(void)
{
    THM3070_WriteREG(THM_REG_TXCON, 0x72);                                      //调制100%
    THM3070_WriteREG(THM_REG_PSEL, 0x10);                                       //通信协议置为TYPEA
    //THM3070_SetFrameFormat(2);                                                 //和B卡不同,这个可以不要
}

/*
功能：  切换到TYPEB模式
参数1： 无
返回：  无
*/
void THM3070_SetTYPEB(void)
{
    THM3070_WriteREG(THM_REG_TXCON, 0x62);                                      //调制10%
    THM3070_WriteREG(THM_REG_PSEL, 0x00);                                       //通信协议置为TYPEB
    THM3070_SetFrameFormat(2);                                                  //不知道为啥,没有的话切换到MIFARE再切回来B卡会受影响
}
/*
功能：  设置发送速率
参数1： 发送速率(读卡器发送速率,0x00:106kbit/s,0x01:212,0x02:424,0x03:848kbit/s)
返回：  无
*/
void THM3070_SetTxBaud(uint8_t baud)
{
    uint8_t temp;

    temp = THM3070_ReadREG(THM_REG_PSEL);                                       //先读取

    baud &= 0x03;                                                               //只取低2位
    baud <<= 2;                                                                 //左移2位

    temp &= 0xF3;                                                               //34位置0
    temp |= baud;                                                               //34位设置

    THM3070_WriteREG(THM_REG_PSEL, temp);                                       //只改变34位
}
/*
功能：  设置接收速率
参数1： 接收速率(读卡器接收速率,0x00:106kbit/s,0x01:212,0x02:424,0x03:848kbit/s)
返回：  无
*/
void THM3070_SetRxBaud(uint8_t baud)
{
    uint8_t temp;

    temp = THM3070_ReadREG(THM_REG_PSEL);                                       //先读取

    baud &= 0x03;                                                               //只取低2位

    temp &= 0xFC;                                                               //12位置0
    temp |= baud;                                                               //12位设置

    THM3070_WriteREG(THM_REG_PSEL, temp);                                       //只改变12位
}



/*
功能：  切换到MIFARE模式
参数1： 无
返回：  无
*/
void THM3070_SetMIFARE(void)
{
    THM3070_WriteREG(THM_REG_TXCON, 0x72);                                      //调制100%
    THM3070_WriteREG(THM_REG_PSEL, 0x50);                                       //通信协议置为MIFARE
    THM3070_SetFrameFormat(0);                                                  //短帧
}

/*
功能：  设置帧格式
参数：  帧格式，0为短帧,1防冲突帧不带CRC,2为标准帧
返回：  无
*/
void THM3070_SetFrameFormat(uint8_t format)
{
    if(format == 0)
    {
        THM3070_WriteREG(THM_REG_FMCTRL, 0xC0);
        THM3070_WriteREG(THM_REG_STATCTRL, 0x00);
        THM3070_WriteREG(THM_REG_FMCTRL, 0x40);
        THM3070_WriteREG(THM_REG_CRCSEL, 0x01);                                 //CRC关闭
    }
    else if(format == 1)
    {
        THM3070_WriteREG(THM_REG_FMCTRL, 0x46);
        THM3070_WriteREG(THM_REG_CRCSEL, 0x01);                                 //CRC关闭
    }
    else
    {
        THM3070_WriteREG(THM_REG_FMCTRL, 0x42);
        THM3070_WriteREG(THM_REG_CRCSEL, 0xC1);                                 //发送CRC使能,接收CRC判断使能,接收超时计时使能
    }
}

/*
功能：  发送帧MIFARE
参数1： 存放数据的数据指针
参数2： 要发送的数据长度
返回：  无
*/
void THM3070_SendFrame_M(uint8_t *p_buff, uint16_t len_buff)
{
    uint8_t temp[1];

    THM3070_WriteREG(THM_REG_SCON, 0x05);                                       //清数据缓冲区
    THM3070_WriteREG(THM_REG_SCON, 0x01);                                       //正常状态

    temp[0] = THM_REG_DATA | 0x80;                                              //写寄存器高位置1

    THM_IO4_SS_N_L;                                                             //片选拉低使能

    THM3070SPI_SendBuff(temp, 1);                                               //发送地址
    THM3070SPI_SendBuff(p_buff, len_buff);                                      //发送数据

    THM_IO4_SS_N_H;                                                             //片选拉高

    THM3070_WriteREG(0x1C, 0x01);                                               //启动发送

}

/*
功能：  接收帧MIFARE
参数1： 存放数据的数据指针
参数2： 要接收的数据长度
返回：  执行结果
*/
uint8_t THM3070_RecvFrame_M(uint8_t *p_buff, uint16_t *len_buff)
{
    uint8_t RStatus = 0;

    THM3070_TimeCount = 0;
    while(1)                                                                    //等待接收完毕或出错
    {
        RStatus = THM3070_ReadREG(0x14);
        if(RStatus & 0xFF)
        {
            break;
        }
        THM3070_TimeCount++;
        if(THM3070_TimeCount > THM3070_TimeCountMax)                            //加入超时判断,避免死等
        {
            THM3070_TimeCount = 0;
            RStatus = THM_RSTST_TMROVER;
            break;
        }
    }
    if(RStatus & 0xEF)                                                          //判断错误类型
    {
        THM3070_WriteREG(0x14, 0x00);
        return RStatus & 0xEF;
    }

    *len_buff = THM3070_ReadREG(THM_REG_RSCH);
    *len_buff = *len_buff << 8;
    *len_buff |= THM3070_ReadREG(THM_REG_RSCL);                                 //读取数据长度
    if(*len_buff > 256)
    {
        return THM_RSTST_DATOVER;
    }

    if(*len_buff > 0)
    {
        uint8_t temp[1];

        THM_IO4_SS_N_L;                                                         //片选拉低使能

        temp[0] = THM_REG_DATA;                                                 //写寄存器高位置1

        THM3070SPI_SendBuff(temp, 1);                                           //发送地址
        THM3070SPI_RecvBuff(p_buff, *len_buff);                                 //接收数据

        THM_IO4_SS_N_H;                                                         //片选拉高
    }
    if(RStatus == 0x10)
    {
        return THM_RSTST_FEND;
    }
    else
    {
        return THM_RSTST_OTHER;
    }
}


/*
功能：  切换到ISO15693模式
参数1： 无
返回：  无
*/
void THM3070_SetTYPEV(void)
{
    THM3070_WriteREG(THM_REG_PSEL, 0x20);                                       //通信协议置为ISO15693
    THM3070_WriteREG(THM_REG_SMOD, 0x01);                                       //编码1/4,调制深度100%
}

/*
功能：  发送帧15693
参数1： 存放数据的数据指针
参数2： 要发送的数据长度
返回：  无
*/
void THM3070_SendFrame_V(uint8_t *p_buff, uint16_t len_buff)
{
    uint8_t temp[1];

    THM3070_WriteREG(THM_REG_SCON, 0x05);                                       //清数据缓冲区
    THM3070_WriteREG(THM_REG_SCON, 0x01);                                       //正常状态
    THM3070_WriteREG(THM_REG_EMVERR, 0xFF);                                     //

    temp[0] = THM_REG_DATA | 0x80;                                              //写寄存器高位置1

    THM_IO4_SS_N_L;                                                             //片选拉低使能

    THM3070SPI_SendBuff(temp, 1);                                               //发送地址
    THM3070SPI_SendBuff(p_buff, len_buff);                                      //发送数据

    THM_IO4_SS_N_H;                                                             //片选拉高

    THM3070_WriteREG(THM_REG_SCON, 0x03);                                       //启动发送
    //THM3070_TimeCount=0;
    //while(!THM3070_ReadREG(THM_REG_TXFIN))                                    //等待发送完成,15693模式下此寄存器不起作用
    //{
        //THM3070_TimeCount++;
        //if(THM3070_TimeCount>THM3070_TimeCountMax)                            //加入超时判断,避免死等
        //{
            //THM3070_TimeCount=0;
            //break;
        //}
    //}
}


/*
功能：  接收帧15693
参数1： 存放数据的数据指针
参数2： 要接收的数据长度
返回：  执行结果
*/
uint8_t THM3070_RecvFrame_V(uint8_t *p_buff, uint16_t *len_buff)
{
    uint8_t EMVError, RStatus = 0;

    THM3070_TimeCount = 0;
    while(1)                                                                    //等待接收完毕或出错
    {
        EMVError = THM3070_ReadREG(THM_REG_EMVERR);
        RStatus = THM3070_ReadREG(THM_REG_RSTAT);
        if(RStatus)
        {
            if(EMVError & 0x02)
            {
                return THM_ERROR_EMVERR;
            }
            break;
        }
        THM3070_TimeCount++;
        if(THM3070_TimeCount > THM3070_TimeCountMax)                            //加入超时判断,避免死等
        {
            THM3070_TimeCount = 0;
            RStatus = THM_RSTST_TMROVER;
            break;
        }
    }
    if(RStatus & 0x40)                                                          //判断错误类型
    {
        RStatus = THM_RSTST_CERR;
        //delay_ms(6);
    }
    else if(RStatus & 0x20)
    {
        RStatus = THM_RSTST_PERR;
    }
    else if(RStatus & 0x10)
    {
        RStatus = THM_RSTST_FERR;
    }
    else if(RStatus & 0x08)
    {
        RStatus = THM_RSTST_DATOVER;
    }
    else if(RStatus & 0x04)
    {
        RStatus = THM_RSTST_TMROVER;
    }
    else if(RStatus & 0x02)
    {
        RStatus = THM_RSTST_CRCERR;
    }
    else
    {
        RStatus = THM_RSTST_FEND;                                               //无错误
    }

    *len_buff = THM3070_ReadREG(THM_REG_RSCH);
    *len_buff = *len_buff << 8;
    *len_buff |= THM3070_ReadREG(THM_REG_RSCL);                                 //读取数据长度
    if(*len_buff > 256)
    {
        return THM_RSTST_DATOVER;
    }

    if(*len_buff > 0)
    {
        uint8_t temp[1];

        THM_IO4_SS_N_L;                                                         //片选拉低使能

        temp[0] = THM_REG_DATA;                                                 //写寄存器高位置1

        THM3070SPI_SendBuff(temp, 1);                                           //发送地址
        THM3070SPI_RecvBuff(p_buff, *len_buff);                                 //接收数据

        THM_IO4_SS_N_H;                                                         //片选拉高
    }
    THM3070_WriteREG(THM_REG_RSTAT, 0x00);                                      //清除状态

    return RStatus;
}



