#include "AlmDataRecord.H"
#include "c_convert.H"


#define ALM_DATA_TEMP_MAX   10
static STR_ALMDATA_REC stAlmDataRec[ALM_DATA_ITEM_MAX];      //  申请一页 2K
static STR_ALMDATA_REC stAlmDataTemp[ALM_DATA_TEMP_MAX]; 
static uint8_t ucTpCount = 0;
typedef struct {
    uint16_t usCount;
    uint32_t uwCrc;
} STR_ALMDATAREC_PARA;

void AlmDataRec_Init(void)
{
    STR_ALMDATAREC_PARA stAlmDataRecPara;

    NVMRead(DINDEX_ALMDATA_RECORD, &stAlmDataRecPara, sizeof(stAlmDataRecPara));
    
    if (stAlmDataRecPara.usCount != (UINT32) ~(stAlmDataRecPara.uwCrc)) {
        stAlmDataRecPara.usCount = 0;
        stAlmDataRecPara.uwCrc = ~ stAlmDataRecPara.usCount; 
        
        NVMWrite(DINDEX_ALMDATA_RECORD, &stAlmDataRecPara, sizeof(stAlmDataRecPara));
    }     
}

void AlmDataRec_Store(const STR_CAN_STD_TYPE *ptrCan)
{
    stAlmDataTemp[ucTpCount].stCANData= *ptrCan;
    msb32_to_array(stAlmDataTemp[ucTpCount].ucTimers, tTimeStamp);
    ucTpCount++;
    if(ucTpCount >= ALM_DATA_TEMP_MAX)
    {
    AlmDataRec_Task();
    }
}

void AlmDataRec_Task()
{
    STR_ALMDATAREC_PARA stAlmDataRecPara;
    uint16_t usCount = 0;       //  记录总条数
    uint8_t ucOffset = 0;       //  块内第几页
    uint8_t ucItem = 0;         //  页内第几条
    uint8_t ucIndex = 0;        //  第几个存储单元
    uint8_t ucSurplus = 0;      //  差值    
    uint8_t ucUsable = 0;       //  块内可用空间
    if(ucTpCount > 0)
    {
        NVMRead(DINDEX_ALMDATA_RECORD, &stAlmDataRecPara, sizeof(STR_ALMDATAREC_PARA));
        
        //  ALMDATA1和ALMDATA2一组为ALM1，ALM1共存储128条
        //  ALMDATA3和ALMDATA4一组为ALM2，ALM2共存储128条
        //  找到当前存储至ALM1还是ALM2
        
        usCount = stAlmDataRecPara.usCount % ALM_DATA_COUNT_MAX;    
    //    USER_printf(11, COL_CYAN"paraCount = %d usCount = %d\r\n", stAlmDataRecPara.usCount, usCount);
        
        //  找到ALM_X中的ALMDATA_X块，一块存储64条
        ucOffset = usCount / ALM_DATA_ITEM_MAX;
    //    USER_printf(11, COL_CYAN"ucOffset = %d\r\n", ucOffset);
        //  找到ALMDATA_X块中的第几条
        ucItem = usCount % ALM_DATA_ITEM_MAX;
    //    USER_printf(11, COL_CYAN"ucItem = %d\r\n", ucItem);
        
        ucIndex = DINDEX_ALMDATA_PARA_1 + ucOffset;
    //    USER_printf(11, COL_CYAN"ucIndex = %d\r\n", ucIndex);
    //    USER_printf(11, COL_RED"******************\r\n");
        memset(stAlmDataRec, NULL, sizeof(stAlmDataRec));
        
        NVMReadByOffset(ucIndex, 0, &stAlmDataRec, sizeof(stAlmDataRec));
        ucUsable = ALM_DATA_ITEM_MAX - ucItem;

        if(ucTpCount > ucUsable)
        {
            ucSurplus = ucTpCount - ucUsable;    
            memcpy(&stAlmDataRec[ucItem], &stAlmDataTemp[0], sizeof(STR_ALMDATA_REC)*ucUsable);
            NVMWriteByOffset(ucIndex, 0, &stAlmDataRec, sizeof(stAlmDataRec));
            
            ucOffset = (usCount + ALM_DATA_ITEM_MAX) / ALM_DATA_ITEM_MAX;
            ucIndex = DINDEX_ALMDATA_PARA_1 + ucOffset;
            NVMReadByOffset(ucIndex, 0, &stAlmDataRec, sizeof(stAlmDataRec));
            memcpy(&stAlmDataRec[0], &stAlmDataTemp[ucUsable], sizeof(STR_ALMDATA_REC)*ucSurplus);
            NVMWriteByOffset(ucIndex, 0, &stAlmDataRec, sizeof(stAlmDataRec));
        }else{
            memcpy(&stAlmDataRec[ucItem], &stAlmDataTemp[0], sizeof(STR_ALMDATA_REC)*ucTpCount);
            NVMWriteByOffset(ucIndex, 0, &stAlmDataRec, sizeof(stAlmDataRec));
        }
    stAlmDataRecPara.usCount += ucTpCount;
    stAlmDataRecPara.uwCrc = ~stAlmDataRecPara.usCount;
    NVMWrite(DINDEX_ALMDATA_RECORD, &stAlmDataRecPara, sizeof(stAlmDataRecPara));  
    ucTpCount = 0;
    memset(&stAlmDataTemp[0], NULL, sizeof(stAlmDataTemp));
    }
}

STR_ALMDATA_REC *AlmDataRec_ReadStore(uint16_t usReadIndex)
{
    STR_ALMDATAREC_PARA stAlmDataRecPara;
    uint16_t usCount = 0;
    uint8_t ucOffset = 0;
    uint8_t ucItem = 0;
    uint8_t ucIndex = 0;
    
    memset(stAlmDataRec, NULL, sizeof(stAlmDataRec));
    NVMRead(DINDEX_ALMDATA_RECORD, &stAlmDataRecPara, sizeof(stAlmDataRecPara));
    
    if (usReadIndex > stAlmDataRecPara.usCount) {
        return NULL;
    }
    
    usCount = usReadIndex % ALM_DATA_COUNT_MAX;
//    USER_printf(12, COL_CYAN"usCount = %d\r\n", usCount);
    ucOffset = usCount / ALM_DATA_ITEM_MAX;       //  确定第几页
//    USER_printf(12, COL_CYAN"ucOffset = %d\r\n", ucOffset);
    ucItem = usReadIndex % ALM_DATA_ITEM_MAX;         //  确定第几条
//    USER_printf(12, COL_CYAN"ucItem = %d\r\n", ucItem);
    ucIndex = DINDEX_ALMDATA_PARA_1 + ucOffset;
//    USER_printf(12, COL_CYAN"ucIndex = %d\r\n", ucIndex);
//    USER_printf(12, COL_RED"******************\r\n");
 
    NVMReadByOffset(ucIndex, 0, &stAlmDataRec, sizeof(stAlmDataRec));
    
    return (&stAlmDataRec[ucItem]);
}

uint16_t AlmDataRec_AlmDataNum(void)
{
    STR_ALMDATAREC_PARA stAlmDataRecPara;     //  用于获取存储参数
    
    NVMRead(DINDEX_ALMDATA_RECORD, &stAlmDataRecPara, sizeof(stAlmDataRecPara));        //  获取故障存储参数
    
    return (stAlmDataRecPara.usCount);
}

void AlmDataRec_ClearRecord(void)
{
    STR_ALMDATAREC_PARA stActRecPara;     //  用于获取存储参数
    NVMRead(DINDEX_ALMDATA_RECORD, &stActRecPara, sizeof(stActRecPara));        //  获取故障存储参数
    
    stActRecPara.usCount = 0;
    stActRecPara.uwCrc = ~stActRecPara.usCount;
    NVMWrite(DINDEX_ALMDATA_RECORD, &stActRecPara, sizeof(stActRecPara));
    
    SPI_FLASH_SectorErase(DINDEX_ALMDATA_PARA_1);
    SPI_FLASH_SectorErase(DINDEX_ALMDATA_PARA_2);
    SPI_FLASH_SectorErase(DINDEX_ALMDATA_PARA_3);
    SPI_FLASH_SectorErase(DINDEX_ALMDATA_PARA_4);
}



