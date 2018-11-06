
#include "ISO15693.h"


/*
文件用途:           ISO15693协议
作者:               张栋培
创建时间:           2018/10/22
更新时间:           2018/10/22
版本:               V1.0

历史版本:           V1.0:基于THM3070实现ISO45693协议


*/


static uint8_t ISO_SelectFlag = 0;                                              //卡片是否处在选择态
static uint8_t ISO_UIDTemp[10];                                                 //暂存卡片UID
static uint8_t ISO_SendTemp[32];                                                //发送接收数据缓存

#include "string.h"


/*
功能：  防冲突
参数1： 已知的UID数据
参数2： 已知的UID位数
参数3： 保存UID的空间
参数4： 启用AFI(高8)|AFI(低8)
返回：  执行结果
*/
static uint8_t collision(uint8_t *maskData, uint8_t maskLen, uint8_t *DAT_UID, uint16_t AFIEN)
{
    uint8_t RSTST, select;
    uint16_t len;

    uint8_t maskDataT[8], maskLenT;

    if(maskLen > 64)                                                            //最大64位
    {
        return THM_RSTST_TMROVER;
    }

    maskLenT = maskLen;                                                         //暂存mask位长度

    if(AFIEN > 0x00FF)
    {
        ISO_SendTemp[0] = 0x36;
        ISO_SendTemp[1] = 0x01;
        ISO_SendTemp[2] = AFIEN & 0x00FF;
        ISO_SendTemp[3] = maskLen;
        len = maskLen / 8;
        if(maskLen % 8 > 0)
        {
            len += 1;                                                           //计算字节长度
        }
        memcpy(ISO_SendTemp + 4, maskData, len);
        if(len > 0)
        {
            memcpy(maskDataT, maskData, len);                                   //暂存mask数据
        }
        len += 4;

    }
    else
    {
        ISO_SendTemp[0] = 0x26;
        ISO_SendTemp[1] = 0x01;
        ISO_SendTemp[2] = maskLen;
        len = maskLen / 8;
        if(maskLen % 8 > 0)
        {
            len += 1;                                                           //计算字节长度
        }
        memcpy(ISO_SendTemp + 3, maskData, len);
        if(len > 0)
        {
            memcpy(maskDataT, maskData, len);                                   //暂存mask数据
        }
        len += 3;
    }

    THM3070_SendFrame_V(ISO_SendTemp, len);
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND)                                                 //成功
    {
        memcpy(DAT_UID, ISO_SendTemp + 2, 0x08);
        memcpy(ISO_UIDTemp, ISO_SendTemp + 2, 0x08);
    }
    else if(RSTST == THM_RSTST_CERR)                                            //冲突
    {
        delay_ms(6);
        if(len > 10)
        {
            len = 10;                                                           //限一下长度
        }
        if(len > 2)                                                             //UID冲突
        {
            memcpy(maskDataT, ISO_SendTemp + 2, len - 2);                       //保存已知的UID
            maskLenT = THM3070_ReadREG(THM_REG_BITPOS) + 1;                     //获取冲突位
            select = 0x80 >> (maskLenT - 1);

            //maskDataT[len-3]|=select;                                         //置1冲突位,向上选择UID
            //RSTST=collision(maskDataT,maskLenT+(len-3)*8,DAT_UID,AFIEN);

            maskDataT[len - 3] &= (~select);                                    //清0冲突位,向下选择UID
            RSTST = collision(maskDataT, maskLenT + (len - 3) * 8, DAT_UID, AFIEN);  //递归调用
        }
        else                                                                    //UID前冲突(此时UID还不知道,可以一位一位的增加已知的UID位数(置为0或置为1))
        {
            select = 0x80;
            maskLenT++;                                                         //mask位长度+1
            len = maskLenT / 8;
            if(maskLenT % 8 > 0)
            {
                len += 1;                                                       //计算字节长度
                select = 0x10 >> ((maskLenT % 8) - 1);
            }

            maskDataT[len - 1] &= (~select);                                    //已知位最后一位清0
            RSTST = collision(maskDataT, maskLenT, DAT_UID, AFIEN);             //递归调用
            if(RSTST == THM_RSTST_TMROVER && ISO_SelectFlag == 0)               //第一次出现没有响应
            {
                ISO_SelectFlag = 1;                                             //设置标识,变量复用了注意不是之前的意思了
                maskDataT[len - 1] |= select;                                   //已知位最后一位置1(0或1总有一个要响应)
                RSTST = collision(maskDataT, maskLenT, DAT_UID, AFIEN);         //递归调用
            }
        }
    }

    return RSTST;
}

/*
功能：  查找
参数1： AFI使能(高8)|AFI(低8)
参数1： 保存UID的空间
返回：  执行结果
*/
uint8_t FINDV(uint16_t ENAndAFI, uint8_t *DAT_UID)
{
    uint8_t RSTST;

    THM3070_RFReset();                                                          //复位场

    RSTST = Inventory(ENAndAFI, DAT_UID);

    return RSTST;
}

/*
功能：  清查
参数1： AFI使能(高8)|AFI(低8)
参数2： 保存UID的空间
返回：  执行结果
*/
uint8_t Inventory(uint16_t ENAndAFI, uint8_t *DAT_UID)
{
    uint8_t RSTST;

    THM3070_SetTYPEV();
    THM3070_SetFWT(0x05);                                                       //超时时间为5*330us=1.65ms

    ISO_SelectFlag = 0;                                                         //默认地址模式
    RSTST = collision(0x00, 0x00, DAT_UID, ENAndAFI);
    ISO_SelectFlag = 0;                                                         //默认地址模式

    return RSTST;
}



/*
功能：	静默
参数1：	无
返回：	执行结果
*/
uint8_t Stayquiet()
{
    ISO_SendTemp[0x00] = 0x22;
    ISO_SendTemp[0x01] = 0x02;
    memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);                                //UID是强制的

    THM3070_SendFrame_V(ISO_SendTemp, 0x0A);

    return THM_RSTST_FEND;
}

/*
功能：  选择
参数1： 无
返回：  执行结果
*/
uint8_t Select()
{
    uint8_t RSTST;
    uint16_t len;

    ISO_SendTemp[0x00] = 0x22;
    ISO_SendTemp[0x01] = 0x25;
    memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);                                //UID是强制的

    THM3070_SendFrame_V(ISO_SendTemp, 0x0A);
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    if(RSTST == THM_RSTST_FEND)
    {
        ISO_SelectFlag = 1;                                                     //设置为选择模式
    }
    return RSTST;
}

/*
功能：	复位到准备
参数1：	无
返回：	执行结果
*/
uint8_t ResetToReady()
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                     //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x26;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);

        THM3070_SendFrame_V(ISO_SendTemp, 0x0A);
    }
    else                                                                        //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x26;

        THM3070_SendFrame_V(ISO_SendTemp, 0x02);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }

    return RSTST;
}

/*
功能：  读取块
参数1： 块号
参数2： 存放块内容
参数3： 存放块内容的长度
返回：  执行结果
*/
uint8_t ReadBlocks(uint8_t BlockNum, uint8_t *BlockData, uint16_t *BlockDataLen)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                     //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x20;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = BlockNum;

        THM3070_SendFrame_V(ISO_SendTemp, 0x0B);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x20;
        ISO_SendTemp[0x02] = BlockNum;

        THM3070_SendFrame_V(ISO_SendTemp, 0x03);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    if(RSTST == THM_RSTST_FEND)
    {
        memcpy(BlockData, ISO_SendTemp + 1, len - 1);
        *BlockDataLen = len - 1;
    }
    return RSTST;
}

/*
功能：  写入块
参数1： 块号
参数2： 块内容
参数3： 块内容的长度
返回：  执行结果
*/
uint8_t WriteBlocks(uint8_t BlockNum, uint8_t *BlockData, uint16_t BlockDataLen)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)	                                                   //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x21;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = BlockNum;
        memcpy(ISO_SendTemp + 0x0B, BlockData, BlockDataLen);

        THM3070_SendFrame_V(ISO_SendTemp, BlockDataLen + 0x0B);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x21;
        ISO_SendTemp[0x02] = BlockNum;
        memcpy(ISO_SendTemp + 0x03, BlockData, BlockDataLen);

        THM3070_SendFrame_V(ISO_SendTemp, BlockDataLen + 0x03);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);

    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    return RSTST;
}

/*
功能：  读取多个块
参数1： 首个块号
参数2： 要读取的块个数(0x00为数量1)
参数3： 存放块内容
参数4： 存放块内容的长度
返回：  执行结果
*/
uint8_t ReadMultipleBlocks(uint8_t BlockNum, uint8_t BlockLen, uint8_t *BlockData, uint16_t *BlockDataLen)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x23;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = BlockNum;
        ISO_SendTemp[0x0B] = BlockLen;

        THM3070_SendFrame_V(ISO_SendTemp, 0x0C);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x23;
        ISO_SendTemp[0x02] = BlockNum;
        ISO_SendTemp[0x03] = BlockLen;

        THM3070_SendFrame_V(ISO_SendTemp, 0x04);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    if(RSTST == THM_RSTST_FEND)
    {
        memcpy(BlockData, ISO_SendTemp + 1, len - 1);
        *BlockDataLen = len - 1;
    }
    return RSTST;
}

/*
功能：  读取块
参数1： 首个块号
参数2： 要写入的块个数(0x00为数量1)
参数3： 块内容
参数4： 块内容的长度
返回：  执行结果
*/
uint8_t WriteMultipleBlocks(uint8_t BlockNum, uint8_t BlockLen, uint8_t *BlockData, uint16_t BlockDataLen)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x24;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = BlockNum;
        ISO_SendTemp[0x0B] = BlockLen;
        memcpy(ISO_SendTemp + 0x0C, BlockData, BlockDataLen);

        THM3070_SendFrame_V(ISO_SendTemp, BlockDataLen + 0x0C);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x24;
        ISO_SendTemp[0x02] = BlockNum;
        ISO_SendTemp[0x03] = BlockLen;
        memcpy(ISO_SendTemp + 0x04, BlockData, BlockDataLen);

        THM3070_SendFrame_V(ISO_SendTemp, BlockDataLen + 0x04);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);

    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    return RSTST;
}

/*
功能：  写AFI
参数1： AFI
返回：  执行结果
*/
uint8_t WriteAFI(uint8_t AFI)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x27;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = AFI;

        THM3070_SendFrame_V(ISO_SendTemp, 0x0B);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x27;
        ISO_SendTemp[0x02] = AFI;

        THM3070_SendFrame_V(ISO_SendTemp, 0x03);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }

    return RSTST;
}

/*
功能：  写DSFID
参数1： DSFID
返回：  执行结果
*/
uint8_t WriteDSFID(uint8_t DSFID)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x29;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = DSFID;

        THM3070_SendFrame_V(ISO_SendTemp, 0x0B);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x29;
        ISO_SendTemp[0x02] = DSFID;

        THM3070_SendFrame_V(ISO_SendTemp, 0x03);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }

    return RSTST;
}


/*
功能：  读系统信息
参数1： 系统信息
参数2： 长度
返回：  执行结果
*/
uint8_t ReadSysInfo(uint8_t *InfoData, uint16_t *InfoDataLen)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x2B;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);

        THM3070_SendFrame_V(ISO_SendTemp, 0x0A);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x2B;

        THM3070_SendFrame_V(ISO_SendTemp, 0x02);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    if(RSTST == THM_RSTST_FEND)
    {
        memcpy(InfoData, ISO_SendTemp + 1, len - 1);
        *InfoDataLen = len - 1;
    }

    return RSTST;
}

/*
功能：  读块状态
参数1： 块状态
参数2： 长度
返回：  执行结果
*/
uint8_t ReadMultipleStatus(uint8_t BlockNum, uint8_t BlockLen, uint8_t *Status, uint16_t *StatusLen)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x2C;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = BlockNum;
        ISO_SendTemp[0x0B] = BlockLen;

        THM3070_SendFrame_V(ISO_SendTemp, 0x0C);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x2C;
        ISO_SendTemp[0x02] = BlockNum;
        ISO_SendTemp[0x03] = BlockLen;

        THM3070_SendFrame_V(ISO_SendTemp, 0x04);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }
    if(RSTST == THM_RSTST_FEND)
    {
        memcpy(Status, ISO_SendTemp + 1, len - 1);
        *StatusLen = len - 1;
    }

    return RSTST;
}

/*
功能：  透传数据
参数1： 发送数据
参数2： 长度
参数3： 接收数据
参数4： 长度
返回：  执行结果
*/
uint8_t SendRFUCMD(uint8_t *SendData, uint16_t SendDataLen, uint8_t *RecvData, uint16_t *RecvDataLen)
{
    uint8_t RSTST;

    THM3070_WriteREG(THM_REG_PSEL, 0x20);                                      //通信协议置为ISO15693
    THM3070_WriteREG(THM_REG_SMOD, 0x01);                                      //

    THM3070_SendFrame_V(SendData, SendDataLen);
    RSTST = THM3070_RecvFrame_V(RecvData, RecvDataLen);

    return RSTST;
}



/*
功能：	永久锁定块,慎用
参数1：	块号
返回：	执行结果
*/
uint8_t LockBlocks(uint8_t BlockNum)
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x22;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);
        ISO_SendTemp[0x0A] = BlockNum;

        THM3070_SendFrame_V(ISO_SendTemp, 0x0B);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x22;
        ISO_SendTemp[0x02] = BlockNum;

        THM3070_SendFrame_V(ISO_SendTemp, 0x03);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }

    return RSTST;
}

/*
功能：  永久锁定AFI,慎用
参数1： 块号
返回：  执行结果
*/
uint8_t LockAFI()
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x28;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);

        THM3070_SendFrame_V(ISO_SendTemp, 0x0A);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x28;

        THM3070_SendFrame_V(ISO_SendTemp, 0x02);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }

    return RSTST;
}

/*
功能：  永久锁定DSFID,慎用
参数1： 块号
返回：  执行结果
*/
uint8_t LockDSFID()
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x2A;
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);

        THM3070_SendFrame_V(ISO_SendTemp, 0x0A);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x2A;

        THM3070_SendFrame_V(ISO_SendTemp, 0x02);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);
    if(RSTST == THM_RSTST_FEND && ISO_SendTemp[0x00] != 0x00)
    {
        RSTST = ISO_SendTemp[1];
    }

    return RSTST;
}

/*
功能：	测试卡片是否仍在天线场内
参数1：	无
返回：	执行结果
*/
uint8_t TESTV()
{
    uint8_t RSTST;
    uint16_t len;

    if(ISO_SelectFlag == 0)                                                    //地址模式,带UID
    {
        ISO_SendTemp[0x00] = 0x22;
        ISO_SendTemp[0x01] = 0x03;                                             //发送一个其他未定义的命令
        memcpy(ISO_SendTemp + 2, ISO_UIDTemp, 0x08);

        THM3070_SendFrame_V(ISO_SendTemp, 0x0A);
    }
    else                                                                       //选择模式,不带UID
    {
        ISO_SendTemp[0x00] = 0x12;
        ISO_SendTemp[0x01] = 0x03;

        THM3070_SendFrame_V(ISO_SendTemp, 0x02);
    }
    RSTST = THM3070_RecvFrame_V(ISO_SendTemp, &len);                           //接收响应

    return RSTST;
}
