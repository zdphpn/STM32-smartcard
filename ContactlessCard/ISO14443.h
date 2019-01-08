
#ifndef _ISO14443_H
#define _ISO14443_H


/*
文件用途:           ISO14443协议
作者:               张栋培
创建时间:           2018/04/20
更新时间:           2018/05/31
版本:               V1.2

*/



#include "stm32f10x.h"
#include "THM3070.h"

/*命令执行状态*/
#define ISO_RSTST_CERR                  THM_RSTST_CERR                          //数据有碰撞
#define ISO_RSTST_PERR                  THM_RSTST_PERR                          //奇偶校验错误
#define ISO_RSTST_FERR                  THM_RSTST_FERR                          //帧格式错误
#define ISO_RSTST_DATOVER               THM_RSTST_DATOVER                       //接收数据溢出错误
#define ISO_RSTST_TMROVER               THM_RSTST_TMROVER                       //接收超时
#define ISO_RSTST_CRCERR                THM_RSTST_CRCERR                        //CRC错误
#define ISO_RSTST_FEND                  THM_RSTST_FEND                          //接收完成




extern uint8_t ISO_PICC_CIDSUP;                                                 //卡是否支持CID,默认支持
extern uint32_t ISO_PICC_FWT;                                                   //通信等待超时时间,默认5ms
extern uint16_t ISO_PICC_MFSIZE;                                                //卡能接收的最大帧长,默认16



uint8_t REQB(uint8_t slotNum, uint8_t *DAT_ATQB, uint16_t *LEN_ATQB);           //TYPEB寻卡
uint8_t WUPB(uint8_t slotNum, uint8_t *DAT_ATQB, uint16_t *LEN_ATQB);           //TYPEB唤醒
uint8_t SlotMARKER(uint8_t slotIndex, uint8_t *DAT_ATQB, uint16_t *LEN_ATQB);   //TYPEB发送时间槽
uint8_t ATTRIB(uint8_t *DAT_ATTRIBAnswer, uint16_t *LEN_ATTRIBAnswer);          //TYPEB激活
uint8_t HALTB(uint8_t *DAT_HALTBAnswer, uint16_t *LEN_HALTBAnswer);             //TYPEB休眠

uint8_t FINDB(uint8_t *DAT_ATQB, uint16_t *LEN_ATQB);                           //TYPEB唤醒+激活
uint8_t TESTB(void);                                                            //测试TYPEB卡是否还在场内


uint8_t REQA(uint8_t *DAT_ATQA, uint16_t *LEN_ATQA);                            //TYPEA寻卡
uint8_t WUPA(uint8_t *DAT_ATQA, uint16_t *LEN_ATQA);                            //TYPEA唤醒
uint8_t AnticollAndSelect(uint8_t *DAT_UID, uint16_t *LEN_UID);                 //TYPEA防冲突和选卡
uint8_t RATS(uint8_t *DAT_ATS, uint16_t *LEN_ATS);                              //TYPEA激活
uint8_t PPSS(uint8_t TxBuad, uint8_t RxBuad);                                   //TYPEA卡PPS

uint8_t FINDA(uint8_t *DAT_ATS, uint16_t *LEN_ATS);                             //TYPEA唤醒+防冲突+选择+激活
uint8_t TESTA(void);                                                            //测试TYPEA卡是否还在场内


uint8_t ExchangeAPDU(uint8_t *sData, uint16_t len_sData, uint8_t *rData, uint16_t *len_rData);  //发送APDU命令并获取响应,参照14443协议自动链接,出错只重发一次
uint8_t ExchangeData(uint8_t *sData, uint16_t len_sData, uint8_t *rData, uint16_t *len_rData);  //发送原始数据并获取响应



#endif

