
#ifndef _THM3070_H
#define _THM3070_H

/*
文件用途:			THM3070驱动文件
作者:					张栋培
创建时间:			2018/04/20
更新时间:			2018/05/31
版本:					V1.1

*/


#include "stm32f10x.h"


#define THM_SPIMOD					1																	//1硬件SPI,0软件模拟SPI

#if THM_SPIMOD==1
#define THM_SPIx						SPI2
#endif
/*引脚定义*/
//SPI通信接口
#define THM_IO1_SCK_GPIOx		GPIOB
#define THM_IO1_SCK_Pinx		GPIO_Pin_13
#define THM_IO2_MOSI_GPIOx	GPIOB
#define THM_IO2_MOSI_Pinx		GPIO_Pin_15
#define THM_IO3_MISO_GPIOx	GPIOB
#define THM_IO3_MISO_Pinx		GPIO_Pin_14
#define THM_IO4_SS_N_GPIOx	GPIOC
#define THM_IO4_SS_N_Pinx		GPIO_Pin_6

//接口模式、低功耗、复位脚
#define THM_MOD0_GPIOx			GPIOC
#define THM_MOD0_Pinx				GPIO_Pin_7
#define THM_STANDBY_GPIOx		GPIOA
#define THM_STANDBY_Pinx		GPIO_Pin_8
#define THM_RESET_GPIOx			GPIOC
#define THM_RESET_Pinx			GPIO_Pin_8

//时钟,记得把使用的外设时钟都写上
#define THM_RCC_APBxPeriphClockCmd	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC|RCC_APB2Periph_AFIO,ENABLE);\
																		RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2,ENABLE)
/*引脚定义*/


/*寄存器定义*/
#define THM_REG_PSEL		0x01																	//协议选择
#define THM_REG_FCONB		0x02																	//TYPEB协议帧控制
#define THM_REG_EGT			0x03																	//TYPEB协议EGT控制
#define THM_REG_CRCSEL	0x04																	//CRC控制
#define THM_REG_INTCON	0x07																	//中断控制
#define THM_REG_SMOD		0x10																	//ISO/IEC15693发送模式设定
#define THM_REG_PWYH		0x11																	//脉冲宽度设置
#define THM_REG_STATCTRL 0x12																	//不知道是啥
#define THM_REG_FMCTRL	0x13																	//帧格式控制
#define THM_REG_EMVEN		0x20																	//噪声检测及处理使能
#define THM_REG_TXCON		0x40																	//射频发送电路设置
#define THM_REG_RXCON		0x45																	//射频接收电路设置

#define THM_REG_DATA		0x00																	//数据寄存器
#define THM_REG_RSTAT		0x05																	//接收状态寄存器
#define THM_REG_SCON		0x06																	//发送控制寄存器
#define THM_REG_RSCH		0x08																	//发送接收计数器高字节
#define THM_REG_RSCL		0x09																	//发送接收计数器低字节
#define THM_REG_CRCH		0x0A																	//CRC结果高字节
#define THM_REG_CRCL		0x0B																	//CRC结果低字节
#define THM_REG_BITPOS	0x0E																	//冲突比特位
#define THM_REG_EMVERR	0x25																	//噪声及FDT错误状态
#define THM_REG_TXFIN		0x26																	//发送完成状态

#define THM_REG_TMRH		0x0C																	//接收定时器高字节
#define THM_REG_TMRL		0x0D																	//接收定时器低字节
#define THM_REG_FWIHIGH	0x21																	//FWI高位
#define THM_REG_FWIMID	0x22																	//FWI中位
#define THM_REG_FWILOW	0x23																	//FWI低位

#define THM_REG_AFDTOFFSET	0x24															//TYPEA_FDT边界值
#define THM_REG_TR0MINH	0x2E																	//TR0min高位
#define THM_REG_TR0MINL	0x2F																	//TR0min低位
#define THM_REG_TR1MINH	0x33																	//TR1min高位
#define THM_REG_TR1MINL	0x34																	//TR1min低位
#define THM_REG_TR1MAXH	0x35																	//TR1max高位
#define THM_REG_TR1MAXL	0x36																	//TR1max低位
#define THM_REG_TXDP1		0x41																	//PMOS驱动高电平输出电阻
#define THM_REG_TXDP0		0x42																	//PMOS驱动低电平输出电阻
#define THM_REG_TXDN1		0x43																	//NMOS驱动高电平输出电阻
#define THM_REG_TXDN0		0x44																	//NMOS驱动低电平输出电阻
#define THM_REG_RNGCON	0x30																	//随机数控制
#define THM_REG_RNGSTS	0x31																	//随机数状态
#define THM_REG_RNGDATA	0x32																	//随机数数据
/*寄存器定义*/


/*接收错误码定义*/
#define THM_RSTST_OTHER					0x80													//其他错误
#define THM_RSTST_CERR					0x40													//数据有碰撞
#define THM_RSTST_PERR					0x20													//奇偶校验错误
#define THM_RSTST_FERR					0x10													//帧格式错误
#define THM_RSTST_DATOVER				0x08													//接收数据溢出错误
#define THM_RSTST_TMROVER				0x04													//接收超时
#define THM_RSTST_CRCERR				0x02													//CRC错误
#define THM_RSTST_FEND					0x00													//接收完成
/*接收错误码定义*/


#define THM_ERROR_EMVERR				0x25



/*外部函数定义*/
uint8_t	THM3070_Init(void);																		//芯片初始化

void		THM3070_PortMode(uint8_t mode);												//接口模式(SPI/IDR)
void		THM3070_SleepMode(uint8_t mode);											//运行或低功耗模式

void		THM3070_RFReset(void);																//关场再开场
void		THM3070_RFClose(void);																//关场

void    THM3070_SetFWT(uint32_t fwt);													//设置超时时间
void    THM3070_SetTYPEA(void);																//设置TYPEA模式
void    THM3070_SetTYPEB(void);																//设置TYPEB模式
void    THM3070_SetTxBaud(uint8_t baud);											//设置发送速率
void    THM3070_SetRxBaud(uint8_t baud);											//设置接收速率

void 		THM3070_WriteREG(uint8_t addr,uint8_t val);						//写寄存器
uint8_t THM3070_ReadREG(uint8_t addr);												//读寄存器

void		THM3070_SendFrame(uint8_t* p_buff,uint16_t len_buff);	//发送帧
uint8_t	THM3070_RecvFrame(uint8_t* p_buff,uint16_t* len_buff);//接收帧


void    THM3070_SetMIFARE(void);															//设置MIFARE模式
void    THM3070_SetFrameFormat(uint8_t format);									//设置帧格式
void		THM3070_SendFrame_M(uint8_t* p_buff,uint16_t len_buff);	//发送帧Mifare
uint8_t	THM3070_RecvFrame_M(uint8_t* p_buff,uint16_t* len_buff);//接收帧Mifare

/*外部函数定义*/


#endif


