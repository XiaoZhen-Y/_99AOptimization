#include "Fd_Abnormal_App.h"
#include "Fd_Abnormal_Store.h"
#include <math.h> 

// 探测器异常状态缓存
// 0=正常, 1=异常(需备份)
static UINT8 g_ucFdAbnormalState[C_FD_MAX] = {0};

void Fd_App_Init(void)
{
    // 重置所有状态
    for(int i=0; i<C_FD_MAX; i++) g_ucFdAbnormalState[i] = 0;
}

// 内部：温度异常判定
static BOOL Check_Temp(uint16_t Temp1, uint16_t Temp2, STR_FD_INFO *pFdInfo)
{   
    uint16_t diff = (Temp1 > Temp2) ? (Temp1 - Temp2) : (Temp2 - Temp1);
    
    if (diff >= 10) {
        if(pFdInfo->ucTempAbnmlCnt < 100) pFdInfo->ucTempAbnmlCnt++;
    } else {
        pFdInfo->ucTempAbnmlCnt = 0;
    }
    
    return (!(pFdInfo->ucTempAbnmlCnt >= 2));
}

// 内部：数据有效性判定
static BOOL Check_SensorData(STR_FD_DA *pFdData)
{
    if (pFdData->usTemp > 165) {
		return FALSE;
	}
    if (pFdData->usCo > 2000) {
		return FALSE;
	}
    if (pFdData->usVoc > 3300) {
		return FALSE;
    }
	USER_LOG(4, "Check_SensorData FAULT\r\n");
    return TRUE;
}

BOOL Fd_App_CheckAndUpdate(UINT8 fdid, STR_FD_DA *pFdData, STR_FD_INFO *pFdInfo)
{
    if (fdid == 0 || fdid > C_FD_MAX || !pFdData) return FALSE;
    UINT8 idx = fdid - 1;

    BOOL bIsAbnormal = FALSE;

    // 1. 执行各类判定
    if (!Check_SensorData(pFdData)) {
        USER_LOG(4, "Check_SensorData FAULT\r\n");
        bIsAbnormal = TRUE;
    }
    // 注意：Check_Temp 需要双温度源，此处假设调用者已处理或传入正确参数
    if (!Check_Temp(pFdData->usTemp, pFdData->usTempMin, pFdInfo)) {
        USER_LOG(4, "Check_Temp FAULT");
        bIsAbnormal = TRUE;
    }

    // 2. 更新故障码
    if (bIsAbnormal) {
//        if (pFdData->ucFault == 0) pFdData->ucFault = 0x02; // 传感器故障
        pFdData->ucAlarm = 0; // 故障时屏蔽报警
        g_ucFdAbnormalState[idx] = 1; // 标记需备份
    } else {
        g_ucFdAbnormalState[idx] = 0;
    }
    
    // 3. 连续报警计数处理 (保留原逻辑)
    if (pFdData->ucAlarm > 0) {
        pFdInfo->ucAlarmCount++;
        if (pFdInfo->ucAlarmCount >= CONT_ALARM_NUM) pFdInfo->ucAlarmCount = CONT_ALARM_NUM;
        else pFdData->ucAlarm = 0;
    } else {
        pFdInfo->ucAlarmCount = 0;
    }

    return (g_ucFdAbnormalState[idx] == 1);
}

BOOL Fd_App_IsBackupNeeded(UINT8 fdid)
{
    if (fdid == 0 || fdid > C_FD_MAX) return FALSE;
    return (g_ucFdAbnormalState[fdid-1] == 1);
}

