
#ifndef _STM7816_H
#define _STM7816_H


/*
文件用途:			STM32驱动接触卡
作者:				张栋培
创建时间:			2018/07/04
更新时间:			2018/07/04
版本:				V1.0

*/


#include "stm32f10x.h"


/*引脚定义*/
#define STM_USARTx				USART3
#define STM_USARTx_IRQn			USART3_IRQn

#define STM_CLK_GPIOx			GPIOC
#define STM_CLK_Pinx			GPIO_Pin_12
#define STM_IO_GPIOx			GPIOC
#define STM_IO_Pinx				GPIO_Pin_10

#define STM_RST_GPIOx			GPIOB
#define STM_RST_Pinx			GPIO_Pin_12
#define STM_VCC_GPIOx			GPIOD
#define STM_VCC_Pinx			GPIO_Pin_2

#define STM_RCC_APBxPeriphClockCmd	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO|RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD,ENABLE);\
															GPIO_PinRemapConfig(GPIO_PartialRemap_USART3,ENABLE);\
															RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3,ENABLE)
																		
																		
																		

extern uint8_t ATR_TA1;										//卡ATR中TA1的值,TA1包含FD	
extern uint8_t STM_T1;										//卡是否为T=1,0为T=0,1为T=1
extern uint32_t STM_WT;										//通信超时时间WT=9600

extern uint8_t STM_F;										//F=1
extern uint8_t STM_D;										//D=1
extern uint32_t STM_ClkHz;									//频率=4.5MHz


void STM7816_Init(void);									//初始化
void STM7816_TIMxInt(void);									//1MS定时中断调用,为通信超时提供时间依据

void STM7816_SetClkHz(uint32_t hz);							//设置时钟频率
void STM7816_SetFD(uint8_t F,uint8_t D);					//设置FD
void STM7816_SetWT(uint32_t wt);							//设置通信超时时间
void STM7816_SetVCC(uint8_t on);							//设置VCC

uint8_t ColdReset(uint8_t* atr,uint16_t* len);				//冷复位
uint8_t WarmReset(uint8_t* atr,uint16_t* len);				//热复位
uint8_t PPS(uint8_t F,uint8_t D);							//PPS
															//发送APDU,并接收返回数据,没有出错重发机制
uint8_t ExchangeTPDU(uint8_t* sData,uint16_t len_sData,uint8_t* rData,uint16_t* len_rData);


#endif

