#include "Fd_Abnormal.h"
#include "Fd_Abnormal_Store.h" // 引用存储层
#include "Fd_Abnormal_App.h"   // 引用逻辑层
#include "canip.h"
#include <string.h>

void Fd_Abnormal_Init(void)
{
    Fd_App_Init();
    Fd_Store_Init();
}

void Fd_Abnormal_Handler(uint8_t FdId, STR_FD_DA *pFdData, STR_FD_INFO *pFdInfo)
{
    // 调用逻辑层进行判定和状态更新
    // App层会修改 pFdData 中的 Fault/Alarm 字段
    BOOL result = Fd_App_CheckAndUpdate(FdId, pFdData, pFdInfo);
	
	if(result == TRUE)
	{
		pFdInfo->fd_isAbnormal = 1;
	}
    USER_LOG(4, "FD_AB CHECK result = %d\r\n", result);
}

void Fd_Abnormal_BkState(uint8_t FdId, STR_CAN_STD_TYPE *c_stCanMess)
{
    if(Fd_App_IsBackupNeeded(FdId) || Fd_Store_IsSessionActive(FdId)){
        USER_LOG(4, "bk,FdId = %d\r\n", FdId);
        Fd_Store_Write(FdId, c_stCanMess);
    }else{
        USER_LOG(4, "donot bk,FdId = %d\r\n", FdId);
    }
}

void Fd_Abnormal_RequestBkStatus(void)
{
    UINT8 usedCnt = 0;
    UINT16 bitmap = 0;
    
    // 从存储层获取状态
    Fd_Store_GetStatus(&usedCnt, &bitmap);
    
    STR_CAN_STD_TYPE stCanMsg;
    stCanMsg.exd = 1;
    stCanMsg.canid.exdid = 0x1A60F4F5;
    stCanMsg.len = 8;
    memset(stCanMsg.data, 0, 8);
    
    stCanMsg.data[0] = 0x00; // Status Response ID
    stCanMsg.data[1] = usedCnt;
    stCanMsg.data[2] = (bitmap >> 8) & 0xFF;
    stCanMsg.data[3] = bitmap & 0xFF;
    
    CanIPSend(&stCanMsg);
}

void Fd_Abnormal_RequestBkData(uint8_t chunk)
{
    // 调用存储层回放功能
    Fd_Store_PlaybackChunk(chunk);
}

void Fd_Abnormal_ClearAll(void)
{
    Fd_Store_ClearAll();
    Fd_App_Init();
}

