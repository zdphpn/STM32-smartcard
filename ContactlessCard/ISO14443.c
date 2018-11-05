
#include "ISO14443.h"


/*
文件用途:			ISO14443协议
作者:				张栋培
创建时间:			2018/04/20
更新时间:			2018/10/31
版本:				V1.2

历史版本:			V1.0:基于THM3070实现ISO14443协议
					V1.1:修复了块号导致的BUG,修复了检测卡TESTA/B函数工作异常的问题
					V1.2:去除某些函数内使用局部变量去接收不定长数据的操作

*/



#include "THM3070.h"
#include "string.h"


static const uint16_t TAB_MaximumFarmeSize[16] =  								//最大帧长度表
{
    16, 24, 32, 40, 48, 64, 96, 128, 256, 256, 256, 256, 256, 256, 256, 256
};


uint8_t ISO_PCB = 0x0A;															//通信数据块的第1字节,0x02/0x0A,默认为I块,不链接,CID跟随
uint8_t ISO_PICC_CIDSUP = 0x01;													//卡是否支持CID,默认支持
uint32_t ISO_PICC_FWT = 0x64;													//通信等待超时时间,默认100*330us=33ms
uint16_t ISO_PICC_MFSIZE = 16;													//卡能接收的最大帧长,默认16


uint8_t ISO_ATQB[16] = {0x00};													//ATQB数据
#define ISO_ATQB_PUPI				(ISO_ATQB+1)								//ATQB中的PUPI
#define ISO_ATQB_APPDATA			(ISO_ATQB+5)								//ATQB中的Application Data
#define ISO_ATQB_PROTOCOLINFO		(ISO_ATQB+9)								//ATQB中的参数信息
#define ISO_ATQB_PLINFO_SFGI		((ISO_ATQB_PROTOCOLINFO[3]&0xF0)>>4)		//SFGI,卡响应ATTRIB后读卡器应该延时的时间
#define ISO_ATQB_PLINFO_FO			((ISO_ATQB_PROTOCOLINFO[2]&0x03)>>0)		//F0参数
#define ISO_ATQB_PLINFO_FO_CIDEN	(ISO_ATQB_PLINFO_FO&0x01)					//是否支持CID
#define ISO_ATQB_PLINFO_ADC			((ISO_ATQB_PROTOCOLINFO[2]&0x0C)>>2)		//
#define ISO_ATQB_PLINFO_FWI			((ISO_ATQB_PROTOCOLINFO[2]&0xF0)>>4)		//通信超时时间
#define ISO_ATQB_PLINFO_PTYPE		((ISO_ATQB_PROTOCOLINFO[1]&0x0F)>>0)		//是否支持14443协议,TR2最小值
#define ISO_ATQB_PLINFO_MFSIZE		((ISO_ATQB_PROTOCOLINFO[1]&0xF0)>>4)		//最大帧长度(要查表)
#define ISO_ATQB_PLINFO_BAUD		ISO_ATQB_PROTOCOLINFO[0]					//通信速率

uint8_t ISO_UID[10] = {0x00};													//UID数据
uint8_t ISO_ATS[64] = {0x00};													//ATS数据
#define ISO_ATS_TL					(ISO_ATS[0])								//ATS长度TL
#define ISO_ATS_T0					(ISO_ATS[1])								//ATS格式T0
#define ISO_ATS_TCEN				((ISO_ATS_T0>>6)&0x01)						//ATS中是否包含TC
#define ISO_ATS_TBEN				((ISO_ATS_T0>>5)&0x01)						//ATS中是否包含TB
#define ISO_ATS_TAEN				((ISO_ATS_T0>>4)&0x01)						//ATS中是否包含TA
#define ISO_ATS_FSCI				((ISO_ATS_T0>>0)&0x0F)						//卡接收最大帧长(要查表)
#define ISO_ATS_TA					(ISO_ATS[2])								//TA参数
#define ISO_ATS_TB					(ISO_ATS[2+ISO_ATS_TAEN])					//TB参数
#define ISO_ATS_TB_FWI				((ISO_ATS_TB&0xF0)>>4)						//通信超时时间
#define ISO_ATS_TB_SFGI				((ISO_ATS_TB&0x0F)>>0)						//SFGI,卡响应ATS后读卡器应该延时的时间
#define ISO_ATS_TC					(ISO_ATS[2+ISO_ATS_TAEN+ISO_ATS_TBEN])		//TC参数
#define ISO_ATS_TC_CIDEN			((ISO_ATS_TC&0x02)>>1)						//是否支持CID
#define ISO_ATS_TK					(ISO_ATS+2+ISO_ATS_TAEN+ISO_ATS_TBEN+ISO_ATS_TCEN)		//TK历史字节
#define ISO_ATS_TKLen 				(ISO_ATS_TL-2-ISO_ATS_TCEN-ISO_ATS_TBEN-ISO_ATS_TAEN)	//TK历史字节长度



uint8_t ISO_SDataTemp[260];														//发送或接收缓存



/*
功能：	TYPEB寻卡
参数1：	发送的时间槽数=2^slotNum
参数2：	存放响应的ATQB数据,保证其空间>=12
参数3：	存放响应的ATQB数据长度
返回：	执行结果
*/
uint8_t REQB(uint8_t slotNum, uint8_t *DAT_ATQB, uint16_t *LEN_ATQB)
{
    uint8_t RSTST;

    uint8_t CMD[3] = {0x05, 0x00, 0x00};										//REQB命令
    CMD[2] |= (slotNum & 0x07);													//加入时间槽数

    ISO_PICC_FWT = 0x05;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为5*330us=1.65ms
    THM3070_SetTYPEB();															//TYPEB模式

    THM3070_SendFrame(CMD, 3);
    RSTST = THM3070_RecvFrame(DAT_ATQB, LEN_ATQB);

    memcpy(ISO_ATQB, DAT_ATQB, *LEN_ATQB);

    return RSTST;
}

/*
功能：	TYPEB唤醒
参数1：	发送的时间槽数=2^slotNum
参数2：	存放响应的ATQB数据,保证其空间>=12
参数3：	存放响应的ATQB数据长度
返回：	执行结果
*/
uint8_t WUPB(uint8_t slotNum, uint8_t *DAT_ATQB, uint16_t *LEN_ATQB)
{
    uint8_t RSTST;

    uint8_t CMD[3] = {0x05, 0x00, 0x08};										//WUPB命令
    CMD[2] |= (slotNum & 0x07);													//加入时间槽数

    ISO_PICC_FWT = 0x05;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为5*330us=1.65ms
    THM3070_SetTYPEB();															//TYPEB模式


    THM3070_SendFrame(CMD, 3);
    RSTST = THM3070_RecvFrame(DAT_ATQB, LEN_ATQB);

    memcpy(ISO_ATQB, DAT_ATQB, *LEN_ATQB);										//保存ATQB

    return RSTST;
}

/*
功能：	TYPEB发送时间槽
参数1：	发送的时间槽数编号:2-16
参数2：	存放响应的ATQB数据,保证其空间>=12
参数3：	存放响应的ATQB数据长度
返回：	执行结果
*/
uint8_t SlotMARKER(uint8_t slotIndex, uint8_t *DAT_ATQB, uint16_t *LEN_ATQB)
{
    uint8_t RSTST;

    uint8_t CMD[1] = {0x05};													//SlotMARKER命令
    slotIndex <<= 4;
    slotIndex -= 1;
    CMD[0] |= (slotIndex & 0xF0);												//加入时间槽编号

    ISO_PICC_FWT = 0x05;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为5*330us=1.65ms

    THM3070_SendFrame(CMD, 3);
    RSTST = THM3070_RecvFrame(DAT_ATQB, LEN_ATQB);

    memcpy(ISO_ATQB, DAT_ATQB, *LEN_ATQB);										//保存ATQB

    return RSTST;
}

/*
功能：	TYPEB激活
参数1：	存放ATTRIB的响应,保证其空间>=1+n
参数2：	存放ATTRIB的响应长度
返回：	执行结果
*/
uint8_t ATTRIB(uint8_t *DAT_ATTRIBAnswer, uint16_t *LEN_ATTRIBAnswer)
{
    uint8_t RSTST;
    uint8_t TxBuad = 0, RxBuad = 0;

    uint8_t CMD[9] = {0x1D};													//ATTRIB命令
    memcpy(CMD + 1, ISO_ATQB_PUPI, 4);											//PUPI
    CMD[5] = 0x00;																//TR0,TR1,EOF,SOF
    CMD[6] = 0x08;																//PCD接收最大帧256
    /*调整速率
    if(ISO_ATQB_PLINFO_BAUD!=0x00)												//可调速率
    {
    	uint8_t temp=0x00;

    	if((ISO_ATQB_PLINFO_BAUD&0x40)==0x40)									//PICC TO PCD 848kbit/s
    	{
    		temp|=0xC0;
    		RxBuad=0x03;
    	}
    	else if((ISO_ATQB_PLINFO_BAUD&0x20)==0x20)								//PICC TO PCD 424kbit/s
    	{
    		temp|=0x80;
    		RxBuad=0x02;
    	}
    	else if((ISO_ATQB_PLINFO_BAUD&0x10)==0x10)								//PICC TO PCD 212kbit/s
    	{
    		temp|=0x40;
    		RxBuad=0x01;
    	}
    	CMD[6]|=temp;															//设置PICC TO PCD速率

    	if((ISO_ATQB_PLINFO_BAUD&0x80)==0x80)									//发送接收速率相同
    	{
    		CMD[6]|=temp>>2;
    	}
    	else
    	{
    		temp=0x00;
    		if((ISO_ATQB_PLINFO_BAUD&0x04)==0x04)								//PCD TO PICC 848kbit/s
    		{
    			temp|=0x30;
    			TxBuad=0x03;
    		}
    		else if((ISO_ATQB_PLINFO_BAUD&0x02)==0x02)							//PCD TO PICC 424kbit/s
    		{
    			temp|=0x20;
    			TxBuad=0x02;
    		}
    		else if((ISO_ATQB_PLINFO_BAUD&0x01)==0x01)							//PCD TO PICC 212kbit/s
    		{
    			temp|=0x10;
    			TxBuad=0x01;
    		}
    		CMD[6]|=temp;														//设置PCD TO PICC速率
    	}
    }
    调整速率*/
    CMD[7] = 0x01;																//支持14443,TR2
    CMD[8] = 0x00;																//为卡分配的CID,默认为0


    ISO_PICC_FWT = 0x10;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为16*330us=5ms

    THM3070_SendFrame(CMD, 9);
    RSTST = THM3070_RecvFrame(DAT_ATTRIBAnswer, LEN_ATTRIBAnswer);

    if(RSTST == THM_RSTST_FEND)
    {
        ISO_PICC_CIDSUP = ISO_ATQB_PLINFO_FO_CIDEN;								//设置是否支持CID
        ISO_PICC_FWT = 0x0001 << ISO_ATQB_PLINFO_FWI;							//设置通信超时时间
        if(ISO_PICC_FWT < 0x10)
        {
            ISO_PICC_FWT = 0x10;
        }
        if(ISO_PICC_FWT > 0x4000)
        {
            ISO_PICC_FWT = 0x4000;
        }
        ISO_PICC_MFSIZE = TAB_MaximumFarmeSize[ISO_ATQB_PLINFO_MFSIZE]; 		//设置最大帧长度
        if(TxBuad > 0)
        {
            THM3070_SetTxBaud(TxBuad);
        }
        if(RxBuad > 0)
        {
            THM3070_SetRxBaud(RxBuad);
        }

        delay_us(0x0001 << ISO_ATQB_PLINFO_SFGI);

    }

    if(ISO_PICC_CIDSUP)
    {
        ISO_PCB = 0x0A;															//初始PCB,支持CID,PCB=0x0B
    }
    else
    {
        ISO_PCB = 0x02;															//初始PCB,不支持CID,PCB=0x03
    }

    return RSTST;

}

/*
功能：	复位场+TYPEB唤醒+激活
参数1：	存放响应的ATQB数据,保证其空间>=12
参数2：	存放响应的ATQB数据长度
返回：	执行结果
*/
uint8_t FINDB(uint8_t *DAT_ATQB, uint16_t *LEN_ATQB)
{
    uint8_t RSTST = THM_RSTST_CERR, slotNum = 0;

    THM3070_RFReset();
    while(RSTST == THM_RSTST_CERR && slotNum < 4)
    {
        RSTST = WUPB(slotNum, DAT_ATQB, LEN_ATQB);
        if(RSTST == THM_RSTST_TMROVER)
        {
            uint8_t temp = 0;

            while(temp + 2 <= (0x01 << slotNum))
            {
                RSTST = SlotMARKER(temp + 2, DAT_ATQB, LEN_ATQB);
                if(RSTST == THM_RSTST_FEND)
                {
                    break;
                }
                temp++;
            }
        }

        slotNum++;
    }
    if(RSTST == THM_RSTST_FEND)
    {
        uint16_t len;

        RSTST = ATTRIB(ISO_SDataTemp, &len);
    }

    return RSTST;
}



/*
功能：	TYPEA寻卡
参数1：	存放响应的ATQA数据,保证其空间>=2
参数2：	存放响应的ATQA数据长度
返回：	执行结果
*/
uint8_t REQA(uint8_t *DAT_ATQA, uint16_t *LEN_ATQA)
{
    uint8_t RSTST;

    uint8_t CMD[1] = {0x26};													//WUPA命令

    ISO_PICC_FWT = 0x05;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为5*330us=1.65ms
    THM3070_SetTYPEA();															//TYPEA模式


    THM3070_SendFrame(CMD, 1);													//协议要短帧,这样发送竟然也可以?
    RSTST = THM3070_RecvFrame(DAT_ATQA, LEN_ATQA);

    return RSTST;
}

/*
功能：	TYPEA唤醒
参数1：	存放响应的ATQA数据,保证其空间>=2
参数2：	存放响应的ATQA数据长度
返回：	执行结果
*/
uint8_t WUPA(uint8_t *DAT_ATQA, uint16_t *LEN_ATQA)
{
    uint8_t RSTST;

    uint8_t CMD[1] = {0x52};													//WUPA命令

    ISO_PICC_FWT = 0x05;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为5*330us=1.65ms
    THM3070_SetTYPEA();															//TYPEA模式


    THM3070_SendFrame(CMD, 1);													//协议要短帧,这样发送竟然也可以?
    RSTST = THM3070_RecvFrame(DAT_ATQA, LEN_ATQA);

    return RSTST;
}

/*
功能：	发送防冲突指令码,内部函数
参数1：	防冲突级别
参数2：	返回的选择指令数据
参数3：	返回的选择指令数据长度
返回：	执行结果
*/
static uint8_t SendAC(uint8_t casLevel, uint8_t *selCode, uint16_t *Len_selCode)
{
    uint8_t *temp = ISO_SDataTemp + 128;
    uint8_t curReceivePostion, lastPostion, RSTST;
    uint16_t len = 0;

    temp[0] = casLevel;															//SEL
    temp[1] = 0x20;																//NVB=0x20,没有已知的UID
    curReceivePostion = lastPostion = 0x00;

    while(1)
    {
        THM3070_SendFrame(temp, curReceivePostion + 2);							//协议要短帧,这样发送竟然也可以?
        RSTST = THM3070_RecvFrame(temp + lastPostion + 2, &len);
        if(len > 5)
        {
            len = 5;															//限制下长度
        }

        curReceivePostion = lastPostion + len;									//总共接收到的数据长度
        if(len != 0)
        {
            lastPostion += len - 1;												//去除最后1字节,因为它可能带有冲突
        }

        if(RSTST & THM_RSTST_CERR)												//有冲突
        {
            delay_ms(6);														//需要延时把冲突之后的字节过滤掉
            temp[1] = THM3070_ReadREG(THM_REG_BITPOS) + 1;						//接收到的比特位长度,NVB低4位
            temp[1] += (uint8_t)(len + 1) << 4;									//接收到的字节长度,NVB高4位
            if((temp[1] & 0x0f) == 0x08)										//比特位长度为8
            {
                temp[1] = ((temp[1] & 0xf0) + 0x10);							//比特位清零,字节+1
                lastPostion = (lastPostion + 1);								//+1
            }
        }
        else if(RSTST == THM_RSTST_FEND || RSTST == THM_RSTST_CRCERR)
        {
            if(lastPostion == 4)
            {
                memcpy(selCode + 2, temp + 2, 5);								//没有冲突,构造选择指令数据
                *Len_selCode = 7;												//长度为7

                return THM_RSTST_FEND;
            }
            else
            {
                return THM_RSTST_OTHER;
            }
        }
        else
        {
            return RSTST;														//返回
        }
    }
}

/*
功能：	TYPEA防冲突+选择
参数1：	卡UID,保证其空间>=10
参数2：	卡UID长度
返回：	执行结果
*/
uint8_t AnticollAndSelect(uint8_t *DAT_UID, uint16_t *LEN_UID)
{
    uint8_t *UIDTemp = ISO_SDataTemp;											//暂存收到的UID,可能包含连接字符CT=0x88

    uint8_t RSTST, CASLEVEL = 0x93;
    uint8_t *selCode = ISO_SDataTemp + 64;
    uint16_t len;
    uint8_t i, count;

    *LEN_UID = 0x00;
    ISO_PICC_FWT = 0x10;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为16*330us=5.28ms

    for(i = 0; i < 3; i++)
    {
        count = 3;
        while(count--)
        {
            RSTST = SendAC(CASLEVEL, selCode, &len);							//发送SEL=0x93/0x95/0x97
            if(RSTST == THM_RSTST_FEND)
            {
                break;
            }
        }
        if(RSTST == THM_RSTST_FEND)
        {
            memcpy(UIDTemp + i * 5, selCode + 2, 5);							//截取出UID,可能包含CT
        }
        else
        {
            return RSTST;
        }

        count = 3;
        while(count--)
        {
            selCode[0] = CASLEVEL;
            selCode[1] = 0x70;													//选择
            THM3070_SendFrame(selCode, 7);										//
            selCode[0] = 0;
            RSTST = THM3070_RecvFrame(selCode, &len);							//响应SAK
            if(RSTST == THM_RSTST_FEND)
            {
                break;
            }
        }
        if(RSTST == THM_RSTST_FEND)
        {
            if((selCode[0] & 0x04) != 0x00)										//SAK第3位为1表明UID不完整
            {
                CASLEVEL += 2;													//进入下一级
                memcpy(DAT_UID + i * 3, UIDTemp + i * 5 + 1, 3);				//截取出真正UID
            }
            else
            {
                memcpy(DAT_UID + i * 3, UIDTemp + i * 5, 4);					//UID完整,截取出UID
                break;
            }
        }
        else
        {
            return RSTST;
        }
    }
    *LEN_UID = 4 + i * 3;														//UID长度4/7/10
    memcpy(ISO_UID, DAT_UID, *LEN_UID);											//保存UID

    return RSTST;
}

/*
功能：	TYPEA激活
参数1：	卡返回的ATS,保证其空间>=5+n
参数2：	ATS的长度
返回：	执行结果
*/
uint8_t RATS(uint8_t *DAT_ATS, uint16_t *LEN_ATS)
{
    uint8_t RSTST;

    uint8_t CMD[2] = {0xE0, 0x80};												//RATS命令,最大帧长256,CID=0

    ISO_PICC_FWT = 0x10;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为16*330us=5ms


    THM3070_SendFrame(CMD, 2);													//
    RSTST = THM3070_RecvFrame(DAT_ATS, LEN_ATS);

    if(RSTST == THM_RSTST_FEND)
    {
        memcpy(ISO_ATS, DAT_ATS, *LEN_ATS);										//保存ATS

        ISO_PICC_MFSIZE = TAB_MaximumFarmeSize[ISO_ATS_FSCI]; 					//设置最大帧长度
        if(ISO_ATS_TAEN)
        {
            ;
        }
        if(ISO_ATS_TBEN)
        {
            ISO_PICC_FWT = 0x0001 << ISO_ATS_TB_FWI;							//设置通信超时时间
            if(ISO_PICC_FWT < 0x10)
            {
                ISO_PICC_FWT = 0x10;
            }
            if(ISO_PICC_FWT > 0x4000)
            {
                ISO_PICC_FWT = 0x4000;
            }
            delay_us(0x0001 << ISO_ATS_TB_SFGI);								//延时
        }
        if(ISO_ATS_TCEN)
        {
            ISO_PICC_CIDSUP = ISO_ATS_TC_CIDEN;									//设置是否支持CID
        }

        if(ISO_PICC_CIDSUP)
        {
            ISO_PCB = 0x0A;														//初始PCB,支持CID,PCB=0x0B
        }
        else
        {
            ISO_PCB = 0x02;														//初始PCB,不支持CID,PCB=0x03
        }

    }

    return RSTST;
}

/*
功能：	TYPEAPPS
参数1：	发送速率(读卡器发送速率,0x00:106kbit/s,0x01:212,0x02:424,0x03:848kbit/s)
参数2：	接收速率(读卡器接收速率,0x00:106kbit/s,0x01:212,0x02:424,0x03:848kbit/s)
返回：	执行结果
*/
uint8_t PPSS(uint8_t TxBuad, uint8_t RxBuad)
{
    uint8_t RSTST;
    uint16_t len;

    uint8_t CMD[3] = {0xD0, 0x11, 0x00};										//PPSS命令,CID=0
    CMD[2] |= (TxBuad & 0x03);
    CMD[2] |= ((RxBuad & 0x03) << 2);

    ISO_PICC_FWT = 0x05;
    THM3070_SetFWT(ISO_PICC_FWT);												//超时时间为5*330us=1.65ms

    THM3070_SendFrame(CMD, 3);													//
    RSTST = THM3070_RecvFrame(ISO_SDataTemp, &len);

    if(RSTST == THM_RSTST_FEND)
    {
        THM3070_SetTxBaud(TxBuad);
        THM3070_SetRxBaud(RxBuad);
    }

    return RSTST;
}

/*
功能：	复位场+TYPEA唤醒+防冲突+选择+激活
参数1：	卡返回的ATS,保证其空间>=5+n
参数2：	ATS的长度
返回：	执行结果
*/
uint8_t FINDA(uint8_t *DAT_ATS, uint16_t *LEN_ATS)
{
    uint8_t RSTST;
    uint16_t len;

    THM3070_RFReset();
    RSTST = WUPA(ISO_SDataTemp, &len);
    if(RSTST == THM_RSTST_FEND || RSTST == THM_RSTST_CERR)
    {
        RSTST = AnticollAndSelect(ISO_SDataTemp, &len);
        if(RSTST == THM_RSTST_FEND)
        {
            RSTST = RATS(ISO_SDataTemp, &len);
            *LEN_ATS = len;
            memcpy(DAT_ATS, ISO_SDataTemp, len);
        }
    }
    return RSTST;
}



/*
功能：	发送原始数据并获取响应
参数1：	待发送的数据
参数2：	待发送的数据长度
参数3：	接收到的数据,保证其空间足够
参数4：	接收到的数据长度
返回：	执行结果
*/
uint8_t ExchangeData(uint8_t *sData, uint16_t len_sData, uint8_t *rData, uint16_t *len_rData)
{
    uint8_t RSTST;

    THM3070_SetFWT(ISO_PICC_FWT);

    THM3070_SendFrame(sData, len_sData);
    RSTST = THM3070_RecvFrame(rData, len_rData);

    return RSTST;
}


/*
功能：	构造发送块,内部函数
参数1：	构造好的块,保证其空间>=len+2
参数2：	带处理的数据
参数3：	带处理的数据及构造好后的块长度
返回：	无
*/
static void formBlock(uint8_t *block, const uint8_t *sData, uint16_t *len)
{
    uint16_t i;

    if(ISO_PICC_CIDSUP)
    {
        for(i = *len + 2; i > 1; i--)											//循环移位
        {
            block[i] = sData[i - 2];											//从最后往前,防止同地址出错
        }
        block[0] = 0x00;														//PCD清0
        block[1] = 0x00;														//CID为0
        *len += 2;																//长度+PCB+CID
    }
    else
    {
        for(i = *len + 1; i > 0; i--)
        {
            block[i] = sData[i - 1];
        }
        block[0] = 0x00;														//PCD清0
        *len += 1;																//长度+PCB
    }
}

/*
功能：	发送APDU命令,出错重发一次
参数1：	要发送的APDU数据
参数2：	要发送的数据长度
参数3：	接收到的数据,保证其空间足够
参数4：	接收到的数据长度
返回：	执行结果
*/
uint8_t ExchangeAPDU(uint8_t *sData, uint16_t len_sData, uint8_t *rData, uint16_t *len_rData)
{
    uint8_t e1, e2, e3;															//错误

    uint8_t RSTST = THM_RSTST_TMROVER;											//发送状态
    uint16_t SendLen1, SendLen2, RecvLen = 0, Temp, LenTemp;

    SendLen1 = len_sData;														//发送数据长度
    SendLen2 = SendLen1;
    while(SendLen1)																//判断长度循环发送
    {
        e1 = e2 = e3 = 0;

        if(ISO_PICC_CIDSUP)														//CID跟随
        {
            Temp = ISO_PICC_MFSIZE - 4;											//最大帧长-PCD-CID-2CRC
        }
        else
        {
            Temp = ISO_PICC_MFSIZE - 3;											//最大帧长-PCD-2CRC
        }
        if(SendLen1 > Temp)														//长度>最大帧长
        {
            SendLen1 -= Temp;													//长度-
            formBlock(ISO_SDataTemp, sData, &Temp);								//数据往后移位,将第1字节PCB让出来

            ISO_SDataTemp[0] = ISO_PCB;											//赋值PCB
            ISO_SDataTemp[0] |= 0x10;											//发送链接,因为数据长一次发送不完,所以链接发送

        }
        else
        {
            Temp = SendLen1;													//记录长度
            SendLen1 = 0;														//长度为0
            formBlock(ISO_SDataTemp, sData, &Temp);

            ISO_SDataTemp[0] = ISO_PCB;											//赋值PCB
            ISO_SDataTemp[0] &= 0xEF;											//不链接
        }
        RSTST = ExchangeData(ISO_SDataTemp, Temp, rData + RecvLen, len_rData);	//发送
sok:
        if(RSTST == THM_RSTST_FEND)												//成功
        {
            uint8_t chaining = 0;												//接收是否链接,默认不链接
            while(1)
            {
                if(RSTST == THM_RSTST_FEND)										//成功
                {
                    chaining = 0;												//不链接

                    Temp = RecvLen;												//暂存一下,接收到数据移位去掉PCB的时候要用
                    ISO_SDataTemp[0] = rData[RecvLen];							//获取接收到的PCB

                    if((ISO_SDataTemp[0] | 0x3F) == 0x3F)						//为I块,高2位为0
                    {
                        if(ISO_SDataTemp[0] & 0x01)
                        {
                            ISO_PCB &= 0xFE;									//收到I块,块号和收到的相反
                        }
                        else
                        {
                            ISO_PCB |= 0x01;
                        }
                        if((ISO_SDataTemp[0] & 0x10) == 0x10)					//链接,链接位为1
                        {
                            chaining = 1;										//链接
                        }
                    }
                    else if((ISO_SDataTemp[0] & 0xC0) == 0xC0)					//为S块,高2位为1
                    {
                        while((RSTST == THM_RSTST_FEND) && (ISO_SDataTemp[0] & 0xC0) == 0xC0)	//成功且返回仍为S块
                        {
                            uint8_t swtx[4] = {0xFA, 0x00, 0x00};
                            uint32_t ISO_PICC_FWT_Copy;

                            ISO_PICC_FWT_Copy = ISO_PICC_FWT;

                            if(ISO_PICC_CIDSUP)									//CID使能
                            {
                                swtx[0] = 0xFA;									//CID跟随时的S块PCB
                                swtx[1] = rData[RecvLen + 1];					//CID使用返回数据中的CID
                                swtx[2] = rData[RecvLen + 2] & 0x3F;			//构造返回S(SWT)返回
                                swtx[3] = 0x03;									//数据长度

                                ISO_PICC_FWT = ISO_PICC_FWT * swtx[2];			//计算等待时间
                                if(ISO_PICC_FWT == 0)
                                {
                                    ISO_PICC_FWT = ISO_PICC_FWT_Copy;
                                }
                                if(ISO_PICC_FWT > 0x4000)						//限制
                                {
                                    ISO_PICC_FWT = 0x4000;
                                }
                            }
                            else
                            {
                                swtx[0] = 0xF2;									//CID不跟随时的S块PCB
                                swtx[1] = rData[RecvLen + 1] & 0x3F;
                                swtx[3] = 0x02;

                                ISO_PICC_FWT = ISO_PICC_FWT * swtx[2];			//计算等待时间
                                if(ISO_PICC_FWT == 0)
                                {
                                    ISO_PICC_FWT = ISO_PICC_FWT_Copy;
                                }
                                if(ISO_PICC_FWT > 0x4000)						//限制
                                {
                                    ISO_PICC_FWT = 0x4000;
                                }
                            }

                            RSTST = ExchangeData(swtx, swtx[3], rData + RecvLen, len_rData);	//发送
                            ISO_PICC_FWT = ISO_PICC_FWT_Copy;					//等待时间恢复
                            ISO_SDataTemp[0] = rData[RecvLen];

                            if(RSTST != THM_RSTST_FEND && e2 == 0)				//不成功
                            {
                                e2 = 1;

                                LenTemp = 0;
                                formBlock(swtx, swtx, &LenTemp);				//构造块
                                swtx[0] = ISO_PCB;
                                if(ISO_PICC_CIDSUP)								//CID使能
                                {
                                    swtx[0] |= 0xB8;
                                    swtx[0] &= 0xBF;							//构造R块(NAK),CID位置1
                                }
                                else
                                {
                                    swtx[0] |= 0xB0;
                                    swtx[0] &= 0xB7;							//构造R块(NAK),CID位置0
                                }

                                RSTST = ExchangeData(swtx, LenTemp, rData + RecvLen, len_rData);	//发送
                                ISO_SDataTemp[0] = rData[RecvLen];
                            }
                        }
                        continue;
                    }
                    else														//为R块
                    {
                        if(ISO_SDataTemp[0] & 0x01)
                        {
                            ISO_PCB &= 0xFE;									//收到I块,块号和收到的相反
                        }
                        else
                        {
                            ISO_PCB |= 0x01;
                        }
                        if(e1 == 1)												//如果上次有错误,收到R块表示卡请求重新发送之前发送的数据
                        {
                            SendLen1 = SendLen2;								//恢复错误位置重新发送
                        }
                        break;													//上次无错误,跳出继续发送
                    }
                    RecvLen = RecvLen + *len_rData;
                    if(ISO_PICC_CIDSUP)
                    {
                        RecvLen -= 2;
                        for(; Temp < RecvLen; Temp++)
                        {
                            rData[Temp] = rData[Temp + 2];						//移位去掉块的头
                        }
                    }
                    else
                    {
                        RecvLen -= 1;
                        for(; Temp < RecvLen; Temp++)
                        {
                            rData[Temp] = rData[Temp + 1];
                        }
                    }
                    SendLen2 = SendLen1;										//记录发送位置,用于错误重发

                    if(!chaining)break;											//不链接,跳出接收

                    LenTemp = 0;
                    formBlock(ISO_SDataTemp, ISO_SDataTemp, &LenTemp);
                    ISO_SDataTemp[0] = ISO_PCB;									//链接需要回送ACK,构造R块(ACK)
                    if(ISO_PICC_CIDSUP)											//CID使能
                    {
                        ISO_SDataTemp[0] |= 0xA8;
                        ISO_SDataTemp[0] &= 0xAF;								//构造R块(ACK),CID位置1
                    }
                    else
                    {
                        ISO_SDataTemp[0] |= 0xA0;
                        ISO_SDataTemp[0] &= 0xA7;								//构造R块(ACK),CID位置0
                    }

                    RSTST = ExchangeData(ISO_SDataTemp, LenTemp, rData + RecvLen, len_rData);	//发送ACK,卡会接着发送下一个链接数据
                }//if(RSTST==THM_RSTST_FEND)
                else
                {
                    if(e3 == 0)
                    {
                        e3 = 1;

                        LenTemp = 0;
                        formBlock(ISO_SDataTemp, ISO_SDataTemp, &LenTemp);
                        ISO_SDataTemp[0] = ISO_PCB;								//构造R块(ACK)
                        if(ISO_PICC_CIDSUP)										//CID使能
                        {
                            ISO_SDataTemp[0] |= 0xA8;
                            ISO_SDataTemp[0] &= 0xAF;							//构造R块(ACK),CID位置1
                        }
                        else
                        {
                            ISO_SDataTemp[0] |= 0xA0;
                            ISO_SDataTemp[0] &= 0xA7;							//构造R块(ACK),CID位置0
                        }

                        RSTST = ExchangeData(ISO_SDataTemp, LenTemp, rData + RecvLen, len_rData);	//发送ACK,请求卡再次发送上次数据

                        continue;
                    }
                    else
                    {
                        break;													//接收结束
                    }
                }
            }//while(1)
        }//sok:if(RSTST==THM_RSTST_FEND)
        else																	//第一次发送,卡响应有问题
        {
            if(e1 == 0)															//e用来限制次数
            {
                e1 = 1;
                LenTemp = 0;
                formBlock(ISO_SDataTemp, ISO_SDataTemp, &LenTemp);
                ISO_SDataTemp[0] = ISO_PCB;										//构造R块(NAK)
                if(ISO_PICC_CIDSUP)												//CID使能
                {
                    ISO_SDataTemp[0] |= 0xB8;
                    ISO_SDataTemp[0] &= 0xBF;									//构造R块(NAK),CID位置1
                }
                else
                {
                    ISO_SDataTemp[0] |= 0xB0;
                    ISO_SDataTemp[0] &= 0xB7;									//构造R块(NAK),CID位置0
                }

                RSTST = ExchangeData(ISO_SDataTemp, LenTemp, rData + RecvLen, len_rData);	//发送NAK,卡会响应ACK或者上次回送的数据I块,取决于上次谁错误

                goto sok;														//跳到判断
            }
            else
            {
                break;															//发送结束
            }
        }

    }//while(SendLen1)
    *len_rData = RecvLen;

    return RSTST;
}

/*
功能：	测试TYPEA卡是否还在场内
参数1：	无
返回：	执行结果
*/
uint8_t TESTA()
{
    uint8_t RSTST = 0, swtx[4];
    uint16_t len;

    len = 0;
    formBlock(swtx, swtx, &len);												//构造块

    swtx[0] = ISO_PCB;															//使用2-a的检测方式 R(NAK)1 <-> R(ACK)0
    if(ISO_PICC_CIDSUP)															//CID使能
    {
        swtx[0] |= 0xB8;
        swtx[0] &= 0xBF;														//构造R块(NAK),CID位置1
    }
    else
    {
        swtx[0] |= 0xB0;
        swtx[0] &= 0xB7;														//构造R块(NAK),CID位置0
    }

    RSTST = ExchangeData(swtx, len, ISO_SDataTemp, &len);						//发送


    return RSTST;
}

/*
功能：	测试TYPEB卡是否还在场内
参数1：	无
返回：	执行结果
*/
uint8_t TESTB()
{
    uint8_t RSTST = TESTA();

    return RSTST;
}




