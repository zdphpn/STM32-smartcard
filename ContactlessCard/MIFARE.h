
#ifndef _MIFARE_H
#define _MIFARE_H


/*
文件用途:           MIFARE协议
作者:               张栋培
创建时间:           2018/10/22
更新时间:           2018/10/22
版本:               V1.0

*/



#include "stm32f10x.h"
#include "THM3070.h"


/*函数返回值含义对照表

    //所有函数0x00代表成功,非0x00代表失败
    0x00    执行成功
    0x04    接收数据超时

    //其他类型错误
    0x01    操作块的块号错误,如:试图将一个控制块作为了数值块
    0x02    数值块数据格式错误
    0x03    认证KEY错误
    0x0F    其他错误,如:接收到不应该接收到的错误


*/


uint8_t FINDM(uint8_t *DAT_UID, uint16_t *LEN_UID);                             //TYPEA唤醒+防冲突+选择,用于M1卡

uint8_t AuthKeyA(uint8_t BlockNum, uint8_t *KeyA);                              //认证KeyA
uint8_t AuthKeyB(uint8_t BlockNum, uint8_t *KeyB);                              //认证KeyB

uint8_t ReadBlock(uint8_t BlockNum, uint8_t *BlockData);                        //读取块数据
uint8_t WriteBlock(uint8_t BlockNum, uint8_t *BlockData);                       //写入块数据

uint8_t ReadValue(uint8_t BlockNum, uint8_t *Value);                            //读取值
uint8_t WriteValue(uint8_t BlockNum, uint8_t *Value);                           //写入值
uint8_t AddValue(uint8_t BlockNum, uint8_t *Value);                             //加值,自动转存到当前块
uint8_t SubValue(uint8_t BlockNum, uint8_t *Value);                             //减值,自动转存到当前块

uint8_t TESTM(void);                                                            //测试卡片是否还在天线场内,暂未实现


uint8_t Decrement(uint8_t BlockNum, uint8_t *Value);                            //减值,一般紧跟一条转存,一般不用
uint8_t Increment(uint8_t BlockNum, uint8_t *Value);                            //增值,一般紧跟一条转存,一般不用
uint8_t Transfre(uint8_t BlockNum);                                             //转存,一般不用

uint8_t Restore(uint8_t BlockNum);                                              //恢复,很少用



#endif

