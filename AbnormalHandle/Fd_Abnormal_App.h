#ifndef _FD_ABNORMAL_APP_H_
#define _FD_ABNORMAL_APP_H_

#include "stdtype.h"
#include "FdCtrl.h" 

#define CONT_ALARM_NUM 5

// 初始化逻辑层
void Fd_App_Init(void);

// 核心判定逻辑：检查数据并更新状态
// 返回值：TRUE=需要备份, FALSE=正常
BOOL Fd_App_CheckAndUpdate(UINT8 fdid, STR_FD_DA *pFdData, STR_FD_INFO *pFdInfo);

// 查询某探测器是否处于“需要备份”的状态
BOOL Fd_App_IsBackupNeeded(UINT8 fdid);

#endif

