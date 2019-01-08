
#include "STM7816.h"
#include "string.h"


/*
文件用途:           STM32驱动接触卡
作者:               张栋培
创建时间:           2018/07/04
更新时间:           2018/07/04
版本:               V1.0

历史版本:           V1.0:基于STM32 USART口实现7816 T=0协议


*/




/*复位引脚*/
#define STM_RST_H    GPIO_SetBits(STM_RST_GPIOx,STM_RST_Pinx)
#define STM_RST_L    GPIO_ResetBits(STM_RST_GPIOx,STM_RST_Pinx)



uint8_t STM_ATR[40];                                                            //存储一个ATR
uint8_t ATR_TA1 = 0x00;                                                         //卡ATR中TA1的值,TA1包含FD
uint8_t STM_T1 = 0;                                                             //卡是否为T=1

uint32_t STM_WT = 9600;                                                         //通信超时时间WT

uint8_t STM_F = 1;                                                              //F
uint8_t STM_D = 1;                                                              //D
uint32_t STM_ClkHz = 3600000;                                                   //频率3.6MHz

uint16_t STM_DelayMS = 0;                                                       //超时计数

                                                                                //FD表
static const uint16_t F_Table[16] = {372, 372, 558, 744, 1116, 1488, 1860, 372, 372, 512, 768, 1024, 1536, 2048, 372, 372};
static const uint8_t D_Table[16] = {1, 1, 2, 4, 8, 16, 32, 64, 12, 20, 1, 1, 1, 1, 1, 1};


/*
功能：  STM7816口初始化
参数：  无
返回：  无
*/
void STM7816_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    USART_ClockInitTypeDef USART_ClockInitStructure;

    STM_RCC_APBxPeriphClockCmd;

    GPIO_InitStructure.GPIO_Pin = STM_CLK_Pinx;                                 //CLK复用
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STM_CLK_GPIOx, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = STM_IO_Pinx;                                  //IO复用
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STM_IO_GPIOx, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = STM_RST_Pinx;                                 //复位输出
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STM_RST_GPIOx, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = STM_VCC_Pinx;                                 //供电输出
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(STM_VCC_GPIOx, &GPIO_InitStructure);
    STM7816_SetVCC(1);                                                          //上电


    USART_SetGuardTime(STM_USARTx, 12);                                         //保护时间

    USART_ClockInitStructure.USART_Clock = USART_Clock_Enable;
    USART_ClockInitStructure.USART_CPOL = USART_CPOL_Low;
    USART_ClockInitStructure.USART_CPHA = USART_CPHA_1Edge;
    USART_ClockInitStructure.USART_LastBit = USART_LastBit_Enable;
    USART_ClockInit(STM_USARTx, &USART_ClockInitStructure);

    STM7816_SetClkHz(STM_ClkHz);                                                //设置CLK频率

    USART_InitStructure.USART_BaudRate = 9677;
    USART_InitStructure.USART_WordLength = USART_WordLength_9b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1_5;
    USART_InitStructure.USART_Parity = USART_Parity_Even;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(STM_USARTx, &USART_InitStructure);

    STM7816_SetFD(STM_F, STM_D);                                                //设置FD

    USART_Cmd(STM_USARTx, ENABLE);
    USART_SmartCardNACKCmd(STM_USARTx, ENABLE);
    USART_SmartCardCmd(STM_USARTx, ENABLE);


}

/*
功能：  超时计数定时中断(1MS中断)
参数：  无
返回：  无
*/
void STM7816_TIMxInt(void)
{
    if(STM_DelayMS > 0)                                                         //计数时间>0
    {
        STM_DelayMS--;                                                          //
    }
}

/*
功能：  STM7816口设置频率
参数：  频率,=0为关时钟
返回：  无
*/
void STM7816_SetClkHz(uint32_t hz)
{
    uint32_t apbclock = 0x00;
    uint32_t usartxbase = 0;
    RCC_ClocksTypeDef RCC_ClocksStatus;

    if(hz == 0)
    {
        STM_USARTx->CR2 &= ~0x00000800;                                         //关时钟

        return;
    }

    usartxbase = (uint32_t)STM_USARTx;
    RCC_GetClocksFreq(&RCC_ClocksStatus);
    if (usartxbase == USART1_BASE)
    {
        apbclock = RCC_ClocksStatus.PCLK2_Frequency;
    }
    else
    {
        apbclock = RCC_ClocksStatus.PCLK1_Frequency;
    }

    apbclock /= hz;                                                             //根据串口的频率计算分频比
    apbclock /= 2;
    if(apbclock < 1)apbclock = 1;

    USART_SetPrescaler(STM_USARTx, apbclock);                                   //设置分频
    STM_USARTx->CR2 |= 0x00000800;                                              //开时钟

}


/*
功能：  STM7816口设置FD
参数：  FD的表格索引值
返回：  无
*/
void STM7816_SetFD(uint8_t F, uint8_t D)
{
    uint32_t etudiv;

    etudiv = STM_USARTx->GTPR & 0x0000001F;                                     //获取时钟分频数
    etudiv = 2 * etudiv * F_Table[F] / D_Table[D];                              //波特比率=((时钟分频*2)*F)/D

    STM_USARTx->BRR = etudiv;
}

/*
功能：  STM7816接口设置通信超时时间
参数：  超时时间(单位ETU)
返回：  无
*/
void STM7816_SetWT(uint32_t wt)
{
    STM_WT = wt;
}

/*
功能：  串口接收一个字节数据
参数：  数据,超时时间(单位MS)
返回：  1超时错误,0成功
*/
static uint8_t USART_RecvByte(uint8_t *dat, uint16_t overMs)
{
    STM_DelayMS = overMs + 1;                                                   //设置超时时间,+1避免1ms误差
    while(STM_DelayMS)                                                          //时间内
    {
        //接收到数据
        if(RESET != USART_GetFlagStatus(STM_USARTx, USART_FLAG_RXNE))
        {
            *dat = (uint8_t)USART_ReceiveData(STM_USARTx);
            break;
        }
    }
    overMs = STM_DelayMS == 0;                                                  //是否超时
    STM_DelayMS = 0;

    return overMs;                                                              //返回
}

/*
功能：  串口发送一个字节数据
参数：  数据
返回：  无
*/
static void USART_SendByte(uint8_t dat)
{
    USART_ClearFlag(STM_USARTx, USART_FLAG_TC);                                 //清发送标识
    USART_SendData(STM_USARTx, dat);                                            //发送
    while(USART_GetFlagStatus(STM_USARTx, USART_FLAG_TC) == RESET);
    //while(USART_GetFlagStatus(STM_USARTx,USART_FLAG_RXNE)==RESET);            //没卡的时候会一直等待
    (void)USART_ReceiveData(STM_USARTx);                                        //为什么要接收?
}
/*
功能：  CLK到MS转换,内部函数
参数：  CLK,1进一/0舍去
返回：  转换后的MS值
*/
static uint16_t CLKToMS(uint32_t clk, uint8_t half)
{
    uint16_t temp;

    temp = clk / (STM_ClkHz / 1000);                                           //倍数
    clk = clk % (STM_ClkHz / 1000);                                            //余数

    if(half && clk)                                                            //有余数并且进一
    {
        temp += 1;                                                             //进一
    }
    return temp;                                                               //返回
}
/*
功能：  CLK到US转换,内部函数
参数：  CLK,1进一/0舍去
返回：  转换后的US值
*/
static uint16_t CLKToUS(uint32_t clk, uint8_t half)
{
    uint16_t temp;

    clk *= 1000;
    temp = clk / (STM_ClkHz / 1000);                                            //倍数
    clk = clk % (STM_ClkHz / 1000);                                             //余数

    if(half && clk)                                                             //有余数并且进一
    {
        temp += 1;                                                              //进一
    }
    return temp;                                                                //返回
}
/*
功能：  ETU到MS转换,内部函数
参数：  ETU,1进一/0舍去
返回：  转换后的MS值
*/
static uint16_t ETUToMS(uint32_t etu, uint8_t half)
{
    uint16_t temp;

    etu *= (F_Table[STM_F] / D_Table[STM_D]);
    temp = etu / (STM_ClkHz / 1000);                                            //倍数
    etu = etu % (STM_ClkHz / 1000);                                             //余数

    if(half && etu)                                                             //有余数并且进一
    {
        temp += 1;                                                              //进一
    }
    return temp;                                                                //返回
}

/*
功能：  计算一个数中1(2进制)的个数,内部函数
参数：  数据
返回：  1的个数
*/
static uint8_t NumberOf1_Solution1(uint32_t num)
{
    uint8_t count = 0;
    while(num)
    {
        if(num & 0x01)
        {
            count++;
        }
        num = num >> 1;
    }
    return count;
}

/*
功能：  预测ATR的真正长度,内部函数
参数：  已知的ATR数据及长度
返回：  ATR的真正长度
*/
uint8_t foreATRLen(uint8_t *atr, uint8_t len)
{
    uint8_t len1 = 2, len2 = 2, next = 1, temp = 1, TD2;

    STM_T1 = 0;                                                                 //默认T=0
    ATR_TA1 = 0;                                                                //TA1清零
    if(len < len2)                                                              //数据量太小
    {
        return 0xFF;
    }
    while(next)
    {
        next = 0;
        if((atr[len2 - 1] & 0x80) == 0x80)                                      //TDi存在
        {
            next = 1;                                                           //进入下轮循环
            len1++;
        }
        if((atr[len2 - 1] & 0x40) == 0x40)                                      //TCi存在
        {
            len1++;
        }
        if((atr[len2 - 1] & 0x20) == 0x20)                                      //TBi存在
        {
            len1++;
        }
        if((atr[len2 - 1] & 0x10) == 0x10)                                      //TAi存在
        {
            len1++;

            if(len2 == 2)                                                       //TA1存在
            {
                ATR_TA1 = atr[2];                                               //记录TA1
            }
        }
        len2 = len1;
        if(len < len2)                                                          //数据量太小
        {
            return 0xFF;
        }
    }
    len1 += (atr[1] & 0x0F);                                                    //加上历史字节长度

    if(atr[1] & 0x80)                                                           //TD1存在
    {
        temp += NumberOf1_Solution1(atr[1] & 0xF0);                             //通过判断1的位数,找到TD1的索引
        if((atr[temp] & 0x0F) == 0x01)                                          //TD1低位为1,则为T=1卡
        {
            STM_T1 = 1;
        }
        if(atr[temp] & 0x80)                                                    //判断TD2是否存在
        {
            STM_T1 = 1;                                                         //存在一定是T=1
            temp += NumberOf1_Solution1(atr[temp] & 0xF0);                      //通过判断1的位数,找到TD2的索引
            TD2 = temp;
            if((atr[TD2] & 0x30) == 0x30)
            {
                STM_T1 = 1;
            }
            else if((atr[TD2] & 0xF0) == 0x00 && (atr[TD2] & 0x0F) == 0x01)
            {
                STM_T1 = 1;
            }
        }
    }
    if(STM_T1)                                                                  //T=1ATR最后有1字节校验
    {
        len1++;
    }
    return len1;                                                                //返回预测的ATR长度值
}

/*
功能：  热复位
参数：  ATR,ATR长度
返回：  1超时,0成功
*/
uint8_t WarmReset(uint8_t *atr, uint16_t *len)
{
    uint8_t i, err;
    uint16_t overTim;

    USART_RecvByte(&i, 1);                                                      //把无用的数据清掉

    STM_WT = 9600;                                                              //恢复初始值
    STM_F = 1;
    STM_D = 1;
    *len = 0;

    STM7816_SetFD(STM_F, STM_D);                                                //恢复波特率

    STM_RST_L;                                                                  //复位脚拉低
    overTim = CLKToUS(500, 1);                                                  //CLK到US,进一保证大于
    delay_us(overTim);                                                          //等待至少400个时钟

    STM_RST_H;                                                                  //复位脚拉高
    overTim = CLKToUS(300, 0);                                                  //CLK到US,不进一保证小于
    delay_us(overTim);                                                          //等待最多400个时钟

    for(i = 0; i < sizeof(STM_ATR); i++)                                        //循环接收ATR
    {
        if(i == 0)
        {
            overTim = CLKToMS(40000, 1);                                        //第一个字节返回时间在400-40000个时钟内
        }
        else
        {
            overTim = ETUToMS(STM_WT * D_Table[STM_D], 1);                      //其余字节按超时时间
        }
        err = USART_RecvByte(STM_ATR + i, overTim);                             //在超时时间内接收一字节
        if(!err)                                                                //接收到
        {
            uint8_t atrLen;

            (*len)++;
            atrLen = foreATRLen(STM_ATR, *len);
            if(atrLen == *len)
            {
                break;
            }
        }
        else                                                                    //超时
        {
            err = i == 0;
            break;
        }
    }
    memcpy(atr, STM_ATR, *len);                                                 //拷贝ATR数据


    return err;
}

/*
功能：  设置IO口状态,内部函数
参数：  1开/0关
返回：  无
*/
static void setIOState(uint8_t on)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    if(on)
    {
        GPIO_InitStructure.GPIO_Pin = STM_IO_Pinx;                              //IO复用
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(STM_IO_GPIOx, &GPIO_InitStructure);
    }
    else
    {
        GPIO_InitStructure.GPIO_Pin = STM_IO_Pinx;                              //IO输出
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(STM_IO_GPIOx, &GPIO_InitStructure);

        GPIO_ResetBits(STM_IO_GPIOx, STM_IO_Pinx);                              //拉低
    }
}

/*
功能：  设置VCC
参数：  1开/0关
返回：  无
*/
void STM7816_SetVCC(uint8_t on)
{
    if(on)
    {
        GPIO_SetBits(STM_VCC_GPIOx, STM_VCC_Pinx);                              //拉高
    }
    else
    {
        GPIO_ResetBits(STM_VCC_GPIOx, STM_VCC_Pinx);                            //拉低
    }
}

/*
功能：  冷复位
参数：  ATR,ATR长度
返回：  1超时,0成功
*/
uint8_t ColdReset(uint8_t *atr, uint16_t *len)
{
    uint8_t i, err;
    uint16_t overTim;

    USART_RecvByte(&i, 1);                                                      //把无用的数据清掉

    STM_WT = 9600;                                                              //恢复初始值
    STM_F = 1;
    STM_D = 1;
    *len = 0;

    STM7816_SetFD(STM_F, STM_D);                                                //恢复波特率

    STM7816_SetClkHz(0);                                                        //CLK,IO,RST拉低
    setIOState(0);
    STM_RST_L;

    STM7816_SetVCC(0);                                                          //断电
    delay_ms(50);                                                               //延时

    STM7816_SetVCC(1);                                                          //上电
    delay_ms(50);                                                               //延时

    STM7816_SetClkHz(STM_ClkHz);                                                //给时钟
    setIOState(1);                                                              //200个时钟内IO拉高,这里在给时钟后直接拉高
    USART_RecvByte(&i, 1);                                                      //把无用的数据清掉

    overTim = CLKToUS(500, 1);                                                  //CLK到US,进一保证大于
    delay_us(overTim);                                                          //等待至少400个时钟
    STM_RST_H;                                                                  //复位脚拉高
    overTim = CLKToUS(300, 0);                                                  //CLK到US,不进一保证小于
    delay_us(overTim);                                                          //等待最多400个时钟,还是要等,有卡会在RST拉高后马上返回一个字节数据,真奇怪

    for(i = 0; i < sizeof(STM_ATR); i++)                                        //循环接收ATR
    {
        if(i == 0)
        {
            overTim = CLKToMS(40000, 1);                                        //第一个字节返回时间在400-40000个时钟内
        }
        else
        {
            overTim = ETUToMS(STM_WT * D_Table[STM_D], 1);                      //其余字节按超时时间
        }
        err = USART_RecvByte(STM_ATR + i, overTim);                             //在超时时间内接收一字节
        if(!err)                                                                //接收到
        {
            uint8_t atrLen;

            (*len)++;
            atrLen = foreATRLen(STM_ATR, *len);
            if(atrLen == *len)
            {
                break;
            }
        }
        else                                                                    //超时
        {
            err = i == 0;
            break;
        }
    }
    memcpy(atr, STM_ATR, *len);                                                 //拷贝ATR数据


    return err;

}

/*
功能：  发送APDU并接收返回数据
参数：  发送数据,长度,接收数据,长度
返回：  0成功,1超时,2APDU格式错,3通信错
*/
uint8_t ExchangeTPDU(uint8_t *sData, uint16_t len_sData, uint8_t *rData, uint16_t *len_rData)
{
    uint8_t err, wait, recvFlag;
    uint8_t i, lc, le;

    uint8_t pc;
    uint8_t INS = sData[1];                                                     //记录一下INS,防止接收到数据之后使用时被覆盖出错

    uint16_t overTim;
    overTim = ETUToMS(STM_WT * D_Table[STM_D], 1);                              //超时时间

    if(len_sData == 4)                                                          //长度为4,CASE1
    {
        sData[4] = 0x00;
        lc = le = 0;
    }
    else if(len_sData == 5)                                                     //长度为5,CASE2
    {
        lc = 0;
        le = sData[4];
    }
    else
    {
        if(len_sData == sData[4] + 5)                                           //Lc+5,CASE3
        {
            lc = sData[4];
            le = 0;
        }
        else if(len_sData == sData[4] + 6)                                      //Lc+5+1(Le),CASE4
        {
            lc = sData[4];
            le = sData[len_sData - 1];
        }
        else
        {
            return 2;                                                           //无法解析APDU
        }
    }

    USART_RecvByte(&i, 1);                                                      //清除无用的数据
    for(i = 0; i < 5; i++)                                                      //发送5个APDU头
    {
        USART_SendByte(sData[i]);
    }

    wait = 1;
    len_sData = 0;
    *len_rData = 0;
    recvFlag = 0;
    while(wait)                                                                 //需要继续等待
    {
        wait = 0;
        err = USART_RecvByte(&pc, overTim);                                     //接收一字节数据
        if(err)
        {
            return err;                                                         //错误
        }
        else
        {
            if((pc >= 0x90 && pc <= 0x9F) || (pc >= 0x60 && pc <= 0x6F))        //处于90-9F/60-6F之间
            {
                switch(pc)
                {
                case 0x60:                                                      //继续等待
                    wait = 1;
                    break;
                default:                                                        //状态字SW
                    rData[*len_rData] = pc;                                     //SW1
                    (*len_rData)++;
                    USART_RecvByte(&pc, overTim);
                    rData[*len_rData] = pc;										//SW2
                    (*len_rData)++;
                    break;
                }
            }
            else                                                                //ACK
            {
                pc ^= INS;                                                      //过程字异或INS
                if(pc == 0)                                                     //返回值=INS,标识接下来应该将所有要发送的数据发送,或准备接收全部数据
                {
                    if(recvFlag == 0 && lc > len_sData)                         //发送状态并且有数据要发送
                    {
                        for(i = 0; i < lc - len_sData; i++)                     //发送要发送的全部数据
                        {
                            USART_SendByte(sData[i + 5 + len_sData]);
                        }
                        len_sData = lc;
                        recvFlag = 1;                                           //接收状态
                    }
                    if((recvFlag == 1 || lc == 0) && le > *len_rData)           //(接收状态或没有数据要发送)并且有数据要接收
                    {
                        for(i = 0; i < le - *len_rData; i++)
                        {
                            err = USART_RecvByte(rData + *len_rData + i, overTim);
                            if(err && i < le - *len_rData)
                            {
                                *len_rData = i;
                                return err;                                    //错误
                            }
                        }
                        *len_rData = le;
                    }
                    wait = 1;
                }
                else if(pc == 0xFF)                                             //返回值=~INS,标识接下来发送一字节数据,或准备接收1字节数据
                {
                    if(recvFlag == 0 && lc > len_sData)                         //发送状态并且有数据要发送
                    {
                        USART_SendByte(sData[5 + len_sData]);                   //发送接下来的一字节数据
                        len_sData++;
                        if(len_sData == lc)                                     //发送完毕
                        {
                            recvFlag = 1;                                       //接收状态
                        }
                    }
                    if((recvFlag == 1 || lc == 0) && le > *len_rData)           //(接收状态或没有数据要发送)并且有数据要接收
                    {
                        err = USART_RecvByte(rData + *len_rData, overTim);      //接收一字节数据
                        if(err)
                        {
                            break;
                        }
                        (*len_rData)++;                                         //接收长度累加
                    }
                    wait = 1;
                }
                else                                                            //其他
                {
                    return 3;
                }
            }
        }
    }
    return 0;
}

/*
功能：  PPS
参数：  F,D的表格索引值
返回：  0成功,1失败
*/
uint8_t PPS(uint8_t F, uint8_t D)
{
    uint8_t i, err;
    uint16_t overTim;
    uint8_t pps_cmd[4] = {0xFF, 0x10, 0xFD, 0x00};
    uint8_t pps_res[4] = {0x00, 0x00, 0x00, 0x00};

    overTim = ETUToMS(STM_WT * D_Table[STM_D], 1);                              //超时时间

    pps_cmd[2] = ((F << 4) & 0xF0) | (D & 0x0F);                                //赋值FD
    for(i = 0; i < 3; i++)
    {
        pps_cmd[3] ^= pps_cmd[i];                                               //异或校验
    }

    for(i = 0; i < 4; i++)                                                      //发送PPS
    {
        USART_SendByte(pps_cmd[i]);
    }

    for(i = 0; i < 4; i++)
    {
        err = USART_RecvByte(&pps_res[i], overTim);
        if(err)
        {
            break;                                                              //错误
        }
    }
    if(i == 4)                                                                  //按协议应返回四字节
    {
        if(pps_res[0] == 0xFF && (pps_res[1] & 0x10) == 0x10 && pps_res[2] == pps_cmd[2])
        {
            STM7816_SetFD(F, D);
        }
        else
        {
            return 1;
        }
    }
    else                                                                        //不按协议
    {
        //
        //if(((pps_res[0]&0x10)==0x10&&pps_res[1]==pps_cmd[2]))
        //{
            //STM7816_SetFD(F,D);
        //}
        //else
        //{
            //return 1;
        //}
    }

    return 1;
}







