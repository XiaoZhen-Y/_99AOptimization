#ifndef _FD_ABNORMAL_STORE_H_
#define _FD_ABNORMAL_STORE_H_

#include "stdtype.h"
#include "FdCtrl.h" // 包含 STR_CAN_STD_TYPE 定义

#define USER_DEBUG 1
// 调用USER_printf，自动附带函数名
#if USER_DEBUG
    #define USER_LOG(Terminal, fmt, ...)  USER_printf((Terminal), "[%s] " fmt, __FUNCTION__, ##__VA_ARGS__)
#else
    #define USER_LOG(Terminal, fmt, ...)
#endif

/* ================= 配置定义 ================= */
#define FD_STORE_MAX_CHUNKS     10      // Flash最大块数
#define FD_STORE_PAGES_PER_CHUNK 32     // 每块包含页数
#define FD_STORE_MSGS_PER_PAGE   64     // 每页包含报文数
#define FD_STORE_BUFFER_NUM      2      // 并发缓存数量
#define FD_STORE_CHUNK_MAX       2      // 每个探测器最大块数

#define FLASH_PAGE_SIZE          2048      // FLASH一页大小

/* ================= API 接口 ================= */

// 初始化存储模块
void Fd_Store_Init(void);

// 核心写入：将报文存入指定探测器的存储区
// 返回值：TRUE=成功(缓存或落盘), FALSE=失败(无空间)
BOOL Fd_Store_Write(UINT8 fdid, const STR_CAN_STD_TYPE *pMsg);

// 查询该探测器是否正在录制中
BOOL Fd_Store_IsSessionActive(UINT8 fdid);

// 状态查询：获取当前存储使用情况 (用于CAN查询)
// pUsedCnt: 已使用块数, pBitmap: 块占用位图
void Fd_Store_GetStatus(UINT8 *pUsedCnt, UINT16 *pBitmap);

// 回读请求：读取指定Chunk的数据并发送CAN
void Fd_Store_PlaybackChunk(UINT8 chunk_id);

// 清除所有备份数据
void Fd_Store_ClearAll(void);

INT8 Find_Buffer(UINT8 fdid);

#endif


