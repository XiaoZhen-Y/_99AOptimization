#ifndef _FD_ABNORMAL_H_
#define _FD_ABNORMAL_H_

#include "stdtype.h"
#include "FdCtrl.h"

// 对外初始化
void Fd_Abnormal_Init(void);

// 异常处理句柄 (在数据处理任务中调用)
// 负责：判定异常、修改数据包、更新状态
void Fd_Abnormal_Handler(uint8_t FdId, STR_FD_DA *pFdData, STR_FD_INFO *pFdInfo);

// 备份状态机入口 (在CAN接收/数据流任务中调用)
// 负责：如果处于异常状态，将报文送入存储层
void Fd_Abnormal_BkState(uint8_t FdId, STR_CAN_STD_TYPE *c_stCanMess);

// CAN协议：请求备份状态
void Fd_Abnormal_RequestBkStatus(void);

// CAN协议：请求备份数据
void Fd_Abnormal_RequestBkData(uint8_t chunk);

// CAN协议：清除备份
void Fd_Abnormal_ClearAll(void);

#endif

