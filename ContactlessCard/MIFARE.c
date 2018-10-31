
#include "MIFARE.h"


/*
文件用途:			MIFARE协议
作者:					张栋培
创建时间:			2018/10/22
更新时间:			2018/10/22
版本:					V1.1

历史版本:			V1.0:基于THM3070实现MIFARE协议


*/



#include "string.h"


static uint8_t M1_RecvData[64];
static uint8_t M1_UID[10];

/*
功能：	TYPEA寻卡
参数1：	存放响应的ATQA数据,保证其空间>=2
参数2：	存放响应的ATQA数据长度
返回：	执行结果
*/
static uint8_t M_REQA(uint8_t* DAT_ATQA,uint16_t* LEN_ATQA)
{
	uint8_t RSTST;
	
	uint8_t CMD[1]={0x26};														//WUPA命令
	
	THM3070_SetMIFARE();
	THM3070_SetFWT(0x05);															//超时时间为5*330us=1.65ms	
	
	THM3070_SendFrame_M(CMD,1);													//协议要短帧,这样发送竟然也可以?
	RSTST=THM3070_RecvFrame_M(DAT_ATQA,LEN_ATQA);

	return RSTST;
}

/*
功能：	TYPEA唤醒
参数1：	存放响应的ATQA数据,保证其空间>=2
参数2：	存放响应的ATQA数据长度
返回：	执行结果
*/
static uint8_t M_WUPA(uint8_t* DAT_ATQA,uint16_t* LEN_ATQA)
{
	uint8_t RSTST;
	
	uint8_t CMD[1]={0x52};														//WUPA命令
	

	THM3070_SetMIFARE();
	THM3070_SetFWT(0x05);															//超时时间为5*330us=1.65ms
	
	
	THM3070_SendFrame_M(CMD,1);													//协议要短帧,这样发送竟然也可以?
	RSTST=THM3070_RecvFrame_M(DAT_ATQA,LEN_ATQA);

	return RSTST;
}

/*
功能：	发送防冲突指令码,内部函数
参数1：	防冲突级别
参数2：	返回的选择指令数据
参数3：	返回的选择指令数据长度
返回：	执行结果
*/
static uint8_t M_SendAC(uint8_t casLevel,uint8_t* selCode,uint16_t* Len_selCode)
{
	uint8_t* temp=M1_RecvData+48;
	uint8_t curReceivePostion,lastPostion,RSTST;
	uint16_t len;
	
	temp[0]=casLevel;																					//SEL
	temp[1]=0x20;																							//NVB=0x20,没有已知的UID
	curReceivePostion=lastPostion=0x00;
	
	while(1)
	{
		THM3070_SendFrame_M(temp,curReceivePostion+2);					//
		RSTST=THM3070_RecvFrame_M(temp+lastPostion+2,&len);
		
		curReceivePostion=lastPostion+len;											//总共接收到的数据长度
		if(len!=0)
		{
			lastPostion+=len-1;																		//去除最后1字节,因为它可能带有冲突
		}
		
		if(RSTST&THM_RSTST_CERR)																//有冲突
		{
			temp[1]=THM3070_ReadREG(THM_REG_BITPOS)+1;						//接收到的比特位长度,NVB低4位
			temp[1]+=(uint8_t)(len+1)<<4;													//接收到的字节长度,NVB高4位
			if((temp[1]&0x0f)==0x08)															//比特位长度为8
			{
				temp[1]=((temp[1]&0xf0)+0x10);											//比特位清零,字节+1
				lastPostion=(lastPostion+1);												//+1
			}
		}
		else if(RSTST==THM_RSTST_FEND||RSTST==THM_RSTST_CRCERR)
		{
			if(lastPostion==4)
			{
				memcpy(selCode+2,temp+2,5);													//没有冲突,构造选择指令数据
				*Len_selCode=7;																			//长度为7
				
				return THM_RSTST_FEND;
			}
			else
			{
				return THM_RSTST_OTHER;
			}
		}
		else
		{
			return RSTST;																					//返回
		}
	}
}

/*
功能：	TYPEA防冲突+选择
参数1：	卡UID,保证其空间>=10
参数2：	卡UID长度
返回：	执行结果
*/
static uint8_t M_AnticollAndSelect(uint8_t* DAT_UID,uint16_t* LEN_UID)
{
	uint8_t* UIDTemp=M1_RecvData;												//暂存收到的UID,可能包含连接字符CT=0x88
	
	uint8_t RSTST,CASLEVEL=0x93;
	uint8_t* selCode=M1_RecvData+32;
	uint16_t len;
	uint8_t i,count;
		
	THM3070_SetFrameFormat(1);													//标准帧不带CRC
	*LEN_UID=0x00;
	
	for(i=0;i<3;i++)
	{
		count=3;
		while(count--)
		{
			RSTST=M_SendAC(CASLEVEL,selCode,&len);					//发送SEL=0x93/0x95/0x97
			if(RSTST==THM_RSTST_FEND)
			{
				break;
			}
		}
		if(RSTST==THM_RSTST_FEND)
		{
			memcpy(UIDTemp+i*5,selCode+2,5);								//截取出UID,可能包含CT
			if((selCode[0]&0x04)==0x00)											//
			{
				THM3070_SetFrameFormat(2);										//标准帧带CRC
			}
		}
		else
		{
			return RSTST;
		}
		
		count=3;
		while(count--)
		{
			selCode[0]=CASLEVEL;
			selCode[1]=0x70;																//选择
			THM3070_SendFrame_M(selCode,7);									//
			selCode[0]=0;
			RSTST=THM3070_RecvFrame_M(selCode,&len);				//响应SAK
			if(RSTST==THM_RSTST_FEND)
			{
				break;
			}
		}
		if(RSTST==THM_RSTST_FEND)
		{
			if((selCode[0]&0x04)!=0x00)											//SAK第3位为1表明UID不完整
			{
				CASLEVEL+=2;																	//进入下一级
				memcpy(DAT_UID+i*3,UIDTemp+i*5+1,3);					//截取出真正UID
			}
			else
			{
				memcpy(DAT_UID+i*3,UIDTemp+i*5,4);						//UID完整,截取出UID
				break;
			}
		}
		else
		{
			return RSTST;
		}
	}
	*LEN_UID=4+i*3;																			//UID长度4/7/10
	memcpy(M1_UID,DAT_UID,*LEN_UID);										//保存UID
	
	return RSTST;
}


/*
功能：	复位场+TYPEA唤醒+防冲突+选择
参数1：	卡返回的UID,保证其空间>=4*n
参数2：	UID的长度
返回：	执行结果
*/
uint8_t FINDM(uint8_t* DAT_UID,uint16_t* LEN_UID)
{
	uint8_t RSTST;
	uint16_t len;
	
	THM3070_RFReset();
		
	RSTST=M_WUPA(M1_RecvData,&len);
	if(RSTST==THM_RSTST_FEND)
	{
		RSTST=M_AnticollAndSelect(M1_RecvData,&len);
		if(RSTST==THM_RSTST_FEND)
		{
			*LEN_UID=len;
			memcpy(DAT_UID,M1_RecvData,len);
		}
	}
	return RSTST;
}

/*
功能：	测试卡片是否还在天线场内,暂未实现
参数1：	无
返回：	执行结果
*/
uint8_t TESTM()
{

	return THM_RSTST_FEND;
}

/*
功能：	认证KEY
参数1：	KEYA/B,0x60/0x61
参数2：	KEY
参数3：	块号
返回：	执行结果
*/
static uint8_t AuthKey(uint8_t AB,uint8_t* Key,uint8_t BlockNum)
{
	uint8_t i,RSTST;
	uint16_t len;
	uint8_t CMD[2]={0x60,0x00};														//
	CMD[0]=AB;																						//
	CMD[1]=BlockNum;																			//赋值块号
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	
	THM3070_WriteREG(0x1D,0x00);													//单次认证
	
	THM3070_SendFrame_M(CMD,2);
	THM3070_RecvFrame_M(M1_RecvData,&len);
	if(len!=4)																						//返回4字节随机数
	{
		return 2;
	}
	
	THM3070_WriteREG(0x15,0x08);													//
	
	THM3070_WriteREG(0x17,M1_RecvData[0]);								//保存随机数到DATA1-DATA4
	THM3070_WriteREG(0x18,M1_RecvData[1]);
	THM3070_WriteREG(0x19,M1_RecvData[2]);
	THM3070_WriteREG(0x1A,M1_RecvData[3]);
	for(i=0;i<6;i++)																			//保存KEYA到DATA0
	{
		THM3070_WriteREG(0x16,Key[i]);
	}
	for(i=0;i<4;i++)																			//保存UID到DATA0
	{
		THM3070_WriteREG(0x16,M1_UID[i]);
	}
	THM3070_WriteREG(0x30,0x01);													//产生随机数
	i=0;
	while(1)
	{
		RSTST=THM3070_ReadREG(0x31);
		if(RSTST==1)
		{
			M1_RecvData[i++]=THM3070_ReadREG(0x32);						//读取随机数
			if(i>=4)
			{
				break;
			}
		}	
	}
	THM3070_WriteREG(0x17,M1_RecvData[0]);								//随机数保存到DATA1-4
	THM3070_WriteREG(0x18,M1_RecvData[1]);
	THM3070_WriteREG(0x19,M1_RecvData[2]);
	THM3070_WriteREG(0x1A,M1_RecvData[3]);
	THM3070_WriteREG(0x30,0x00);													//关闭随机数

	THM3070_WriteREG(0x15,0x0C);													//启动认证
	while(1)
	{
		RSTST=THM3070_ReadREG(0x14);												//读取认证结果
		if(RSTST&0xFF)
		{
			break;
		}
	}
	if((RSTST&0xEF)==0x00)																//认证成功
	{
		THM3070_WriteREG(0x15,0x08);												//
		THM3070_WriteREG(0x12,0x01);												//之后通信采用加密方式
		return 0;
	}
	else if(RSTST&0x80)
	{
		return 3;
	}
	else
	{
		return 4;
	}
}

/*
功能：	认证KEYA
参数1：	KEY
参数2：	块号
返回：	执行结果
*/
uint8_t AuthKeyA(uint8_t* KeyA,uint8_t BlockNum)
{
	uint8_t RSTST;
	
	RSTST=AuthKey(0x60,KeyA,BlockNum);
	
	return RSTST;
}

/*
功能：	认证KEYB
参数1：	KEY
参数2：	块号
返回：	执行结果
*/
uint8_t AuthKeyB(uint8_t* KeyB,uint8_t BlockNum)
{
	uint8_t RSTST;
	
	RSTST=AuthKey(0x61,KeyB,BlockNum);
	
	return RSTST;
}

/*
功能：	读取块内容
参数1：	块号
参数2：	数据(16字节)
返回：	执行结果
*/
uint8_t ReadBlock(uint8_t BlockNum,uint8_t* BlockData)
{
	uint16_t len;
	uint8_t CMD[2]={0x30,0x00};														//读命令
	CMD[1]=BlockNum;																			//赋值块号
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	
	THM3070_SendFrame_M(CMD,2);														//发送命令
	THM3070_RecvFrame_M(M1_RecvData,&len);								//接收
	if(len==0x12)
	{
		memcpy(BlockData,M1_RecvData,0x10);									//复制
		return 0;
	}
	
	return 1;
}

/*
功能：	写入块内容
参数1：	块号
参数2：	数据(16字节)
返回：	执行结果
*/
uint8_t WriteBlock(uint8_t BlockNum,uint8_t* BlockData)
{
	uint16_t len;
	uint8_t CMD[2]={0xA0,0x00};														//写命令
	CMD[1]=BlockNum;
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	
	THM3070_SendFrame_M(CMD,2);														//发送命令
	THM3070_RecvFrame_M(M1_RecvData,&len);
	if(len!=0x01||M1_RecvData[0]!=0xA0)										//接收判断
	{
		return 1;
	}
	THM3070_SendFrame_M(BlockData,0x10);									//发送数据
	THM3070_RecvFrame_M(M1_RecvData,&len);								//接收
	if(len!=0x01||M1_RecvData[0]!=0xA0)										//接收判断
	{
		return 1;
	}
	
	return 0;
}

/*
功能：	读取块值
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t ReadValue(uint8_t BlockNum,uint8_t* Value)
{
	uint8_t res;
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	res=ReadBlock(BlockNum,M1_RecvData);
	if(res==0)
	{
		uint8_t i;
		
		for(i=0;i<4;i++)
		{
			M1_RecvData[i+4]=~M1_RecvData[i+4];								//4-7=~4-7
		}
		for(i=0;i<4;i++)
		{
			if(M1_RecvData[i]!=M1_RecvData[i+4]||M1_RecvData[i]!=M1_RecvData[i+8])//0-3?=~4-7?=8-11
			{
				return 2;
			}
		}
		if(M1_RecvData[12]!=BlockNum||M1_RecvData[14]!=BlockNum)//12=14=BlockNum
		{
			return 2;
		}
		BlockNum=~BlockNum;
		if(M1_RecvData[13]!=BlockNum||M1_RecvData[15]!=BlockNum)//13=15=~BlockNum
		{
			return 2;
		}
		Value[0]=M1_RecvData[3];														//倒序
		Value[1]=M1_RecvData[2];
		Value[2]=M1_RecvData[1];
		Value[3]=M1_RecvData[0];
		return 0;
	}
	
	return 1;
}

/*
功能：	写入块值
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t WriteValue(uint8_t BlockNum,uint8_t* Value)
{
	uint8_t i;
	uint8_t* temp=M1_RecvData+16;
	uint8_t* VTemp=M1_RecvData+40;
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	VTemp[0]=Value[3];																	//倒序存放
	VTemp[1]=Value[2];
	VTemp[2]=Value[1];
	VTemp[3]=Value[0];

	memcpy(temp,VTemp,0x04);														//0-3=Value
	memcpy(temp+8,VTemp,0x04);													//8-11=value
	for(i=0;i<4;i++)
	{
		temp[i+4]=~VTemp[i];															//4-7=~value
	}
	temp[12]=temp[14]=BlockNum;													//12|14=BlockNum
	temp[13]=temp[15]=~BlockNum;												//13|15=~BlockNum
	
	i=WriteBlock(BlockNum,temp);
	
	return i;
	
}

/*
功能：	加值结果保存到当前块
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t AddValue(uint8_t BlockNum,uint8_t* Value)
{
	uint8_t res;
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	res=Increment(BlockNum,Value);
	if(res!=0)
	{
		return res;
	}
	res=Transfre(BlockNum);
	return res;
}

/*
功能：	减值结果保存到当前块
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t SubValue(uint8_t BlockNum,uint8_t* Value)
{
	uint8_t res;
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	res=Decrement(BlockNum,Value);
	if(res!=0)
	{
		return res;
	}
	res=Transfre(BlockNum);
	return res;
}




/*
功能：	减值(减少块的值,并将结果存入内部寄存器中)
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t Decrement(uint8_t BlockNum,uint8_t* Value)
{
	uint16_t len;
	uint8_t CMD[2]={0xC0,0x00};														//写命令
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	CMD[1]=BlockNum;
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	
	THM3070_SendFrame_M(CMD,2);														//发送命令
	THM3070_RecvFrame_M(M1_RecvData,&len);
	if(len!=0x01||M1_RecvData[0]!=0xA0)										//接收判断
	{
		return 1;
	}
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	THM3070_SendFrame_M(Value,0x04);											//发送数据
	len=0;
	THM3070_RecvFrame_M(M1_RecvData,&len);								//接收
	if(len>0)																							//应该没有响应
	{
		return 1;
	}
	
	return 0;
}

/*
功能：	增值(增加块的值,并将结果存入内部寄存器中)
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t Increment(uint8_t BlockNum,uint8_t* Value)
{
	uint16_t len;
	uint8_t CMD[2]={0xC1,0x00};														//写命令
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	CMD[1]=BlockNum;
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	
	THM3070_SendFrame_M(CMD,2);														//发送命令
	THM3070_RecvFrame_M(M1_RecvData,&len);
	if(len!=0x01||M1_RecvData[0]!=0xA0)										//接收判断
	{
		return 1;
	}
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	THM3070_SendFrame_M(Value,0x04);											//发送数据
	len=0;
	THM3070_RecvFrame_M(M1_RecvData,&len);								//接收
	if(len>0)																							//应该没有响应
	{
		return 1;
	}
	
	return 0;
}

/*
功能：	转存(将内部寄存器内容写入块中)
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t Transfre(uint8_t BlockNum)
{
	uint16_t len;
	uint8_t CMD[2]={0xB0,0x00};														//写命令
	uint16_t num;
	
	num=BlockNum+1;
	if(num%4==0)return 3;
	
	CMD[1]=BlockNum;
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
		
	THM3070_SendFrame_M(CMD,2);														//发送命令
	THM3070_RecvFrame_M(M1_RecvData,&len);
	if(len!=0x01||M1_RecvData[0]!=0xA0)										//接收判断
	{
		return 1;
	}
	return 0;
}

/*
功能：	恢复(将块中内容写入内部寄存器)
参数1：	块号
参数2：	值(4字节)
返回：	执行结果
*/
uint8_t Restore(uint8_t BlockNum)
{
	uint16_t len;
	uint8_t CMD[2]={0xC2,0x00};														//写命令
	CMD[1]=BlockNum;
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	THM3070_SendFrame_M(CMD,2);														//发送命令
	THM3070_RecvFrame_M(M1_RecvData,&len);
	if(len!=0x01||M1_RecvData[0]!=0xA0)										//接收判断
	{
		return 1;
	}
	
	THM3070_SetFWT(0x64);																	//超时时间为100*330us=33ms
	THM3070_SendFrame_M(M1_RecvData,0x04);								//发送数据,数据值无所谓
	len=0;
	THM3070_RecvFrame_M(M1_RecvData,&len);								//接收
	if(len>0)																							//应该没有响应
	{
		return 1;
	}
	
	return 0;
}


