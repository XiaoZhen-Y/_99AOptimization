#include "Fd_Abnormal_Store.h"
#include "nvm.h"
#include "delay.h"
#include "w25n01.h"
#include "canip.h"
#include <string.h>

/* ================= 内部数据结构 ================= */

// 1. 单条存储记录 (时间戳+报文)
typedef struct __attribute__((packed)) {
    UINT32 timestamp;      // 4
    STR_CAN_STD_TYPE can_msg;  // 22
} stFd_StoreRecord_t;

// 2. 页结构 (Flash写入最小单位,需固定为2048字节)
typedef struct __attribute__((packed)) {
    UINT8 valid_count;      // 本页有效条数  // 1
    UINT8 reserved[35];     
    stFd_StoreRecord_t records[FD_STORE_MSGS_PER_PAGE]; 
    UINT8 reserved2[348]; 
} stFd_FlashPage_t;


// 3. Chunk元数据 (需持久化)
typedef struct __attribute__((packed)) {
    UINT8 fdid;             // 归属探测器ID
    UINT8 curr_page_idx;    // 当前写到了第几页 (0-31)
    UINT16 total_msgs;      // 总存储条数
    UINT32 start_time;      // 备份开始时间
} stFd_ChunkInfo_t;

// 4. 全局管理信息 (需持久化)
typedef struct __attribute__((packed)) {
    UINT16 chunk_bitmap;    // 位图 (bit0->chunk1)
    UINT8 chunk_count;      // 已用数量
    stFd_ChunkInfo_t chunks[FD_STORE_MAX_CHUNKS];
} stFd_StoreMgt_t;

// 5. RAM缓存控制块
typedef struct {
    BOOL is_busy;
    UINT8 owner_fdid;
    UINT8 target_chunk_idx; // 对应Flash中的哪个Chunk
    stFd_FlashPage_t page_cache; 
} stFd_RamBuffer_t;

/* ================= 全局变量 ================= */
static stFd_StoreMgt_t g_stStoreMgt = {0};
static stFd_RamBuffer_t g_stBuffers[FD_STORE_BUFFER_NUM] = {0};
extern volatile T_TIME tTimeStamp; // 引用外部系统时间

/* ================= 内部函数声明 ================= */
static void Fd_Store_SaveMgt(void);
static INT8 Alloc_Buffer(UINT8 fdid, UINT8 chunk_idx);
static void Flush_Buffer(UINT8 buf_idx);
static INT8 Alloc_NewChunk(UINT8 fdid);
static void Free_Buffer(UINT8 buf_idx);
BOOL HasFreeBuffer(void);

/* ================= 接口实现 ================= */

void Fd_Store_Init(void)
{
    // 读取管理信息
    NVMRead(DINDEX_FDABNORMAL_MGT, &g_stStoreMgt, sizeof(g_stStoreMgt));
    
    // 校验合法性
    if (g_stStoreMgt.chunk_count > FD_STORE_MAX_CHUNKS) {
        Fd_Store_ClearAll();
    }
    
    // 清空RAM缓存
    memset(g_stBuffers, 0, sizeof(g_stBuffers));
}

// 判断是否处于保存中
BOOL Fd_Store_IsSessionActive(UINT8 fdid)
{
    return (Find_Buffer(fdid) >= 0);
}

// 查找该 ID 是否有还没存满的块
static INT8 Find_IncompleteChunk(UINT8 fdid) {
    for (int i = 0; i < FD_STORE_MAX_CHUNKS; i++) {
        // 检查位图：该块已被占用
        if (g_stStoreMgt.chunk_bitmap & (1 << i)) {
            // 检查归属：是同一个探测器
            if (g_stStoreMgt.chunks[i].fdid == fdid) {
                // 检查容量：还没存满 32 页
                if (g_stStoreMgt.chunks[i].curr_page_idx < FD_STORE_PAGES_PER_CHUNK) {
                    return i; // 返回这个不满的块索引
                }
            }
        }
    }
    return -1; // 没有找到不满的块
}

// 写入缓存函数
BOOL Fd_Store_Write(UINT8 fdid, const STR_CAN_STD_TYPE *pMsg)
{
    if (fdid == 0 || pMsg == NULL) return FALSE;

    // 1. 查找缓存
    INT8 bufIdx = Find_Buffer(fdid);

    // 2. 如果没有缓存，说明是新触发的异常，尝试申请新块
    if (bufIdx < 0) {
        // 先判定有无剩余缓存
        if(!HasFreeBuffer()){
            USER_LOG(4, "NO FREE BUFFER\r\n");
            return FALSE;
        };

        // --- .8修改部分开始 ---
        // 优先查找是否存在没存满的旧块
        INT8 chunkIdx = Find_IncompleteChunk(fdid);
        
        // 如果没有没存满的块，才去申请真正的新块
        if (chunkIdx < 0) {
            chunkIdx = Alloc_NewChunk(fdid);
            USER_LOG(4, "ALLOC NEW CHUNK: %d\r\n", chunkIdx);
        } else {
            USER_LOG(4, "RESUME INCOMPLETE CHUNK: %d\r\n", chunkIdx);
        }
        // --- .8修改部分结束 ---

        if (chunkIdx < 0){
            USER_LOG(4, "NO FREE CHUNK\r\n");
            return FALSE;
        }

        // 申请RAM缓存
        bufIdx = Alloc_Buffer(fdid, chunkIdx);
        if (bufIdx < 0){
            USER_LOG(4, "NO FREE BUFFER\r\n");
            return FALSE;
        }
    }

    // 3. 写入缓存
    stFd_RamBuffer_t *pBuf = &g_stBuffers[bufIdx];
    stFd_FlashPage_t *pPage = &pBuf->page_cache;

    // 填充数据到RAM页
    if (pPage->valid_count < FD_STORE_MSGS_PER_PAGE) {
        pPage->records[pPage->valid_count].timestamp = tTimeStamp;
        pPage->records[pPage->valid_count].can_msg = *pMsg;
        pPage->valid_count++;
        USER_LOG(4, "WRITE TO BUFFER, buf_idx = %d, pPage->valid_count = %d\r\n", bufIdx, pPage->valid_count);
    }

    // 4. 页满处理 (>=64条)或此为第一页
    if (pPage->valid_count >= FD_STORE_MSGS_PER_PAGE || g_stStoreMgt.chunks[pBuf->target_chunk_idx].curr_page_idx == 0) {
        // 刷入Flash (这里会把页头+64条数据一起写入)
        USER_LOG(4, "PAGE FULL, buf_idx = %d\r\n", bufIdx);
        Flush_Buffer(bufIdx);
        
        // 检查块是否已满 (>=32页)
        UINT8 chunkIdx = pBuf->target_chunk_idx;
        if (g_stStoreMgt.chunks[chunkIdx].curr_page_idx >= FD_STORE_PAGES_PER_CHUNK) {
            // 只有存满整个块，才释放缓存，停止录制
            Free_Buffer(bufIdx); 
            USER_LOG(4, "CHUNK FULL, buf_idx = %d\r\n", bufIdx);
        }
    }
    
    return TRUE;
}

void Fd_Store_GetStatus(UINT8 *pUsedCnt, UINT16 *pBitmap)
{
    if(pUsedCnt) *pUsedCnt = g_stStoreMgt.chunk_count;
    if(pBitmap) *pBitmap = g_stStoreMgt.chunk_bitmap;
}

void Fd_Store_ClearAll(void)
{
    memset(&g_stStoreMgt, 0, sizeof(g_stStoreMgt));
    Fd_Store_SaveMgt();
    memset(g_stBuffers, 0, sizeof(g_stBuffers));
}

void Fd_Store_PlaybackChunk(UINT8 chunk_id)
{
    if (chunk_id < 1 || chunk_id > FD_STORE_MAX_CHUNKS) return;
    UINT8 idx = chunk_id - 1;

    // 检查位图
    if (!(g_stStoreMgt.chunk_bitmap & (1 << idx))) return;

    // 强制刷新所有缓存，防止数据遗漏
    for(int i=0; i<FD_STORE_BUFFER_NUM; i++) {
        if(g_stBuffers[i].is_busy) Flush_Buffer(i);
    }

    stFd_ChunkInfo_t *pChunk = &g_stStoreMgt.chunks[idx];
    stFd_FlashPage_t readPage;
    STR_CAN_STD_TYPE stCan;
    stCan.exd = 1; 
    stCan.canid.exdid = 0x1A60F4F5;
    stCan.len = 8;

    // 发送头部信息
	stCan.canid.exdid = 0x1A60F4F0;
    memset(stCan.data, 0, 8);
    stCan.data[0] = chunk_id;
    stCan.data[1] = pChunk->fdid;
    stCan.data[2] = (pChunk->total_msgs >> 8) & 0xFF;
    stCan.data[3] = pChunk->total_msgs & 0xFF;
    stCan.data[4] = pChunk->curr_page_idx;
    CanIPSend(&stCan);
    delay_ms(10);

    // 逐页读取
    for (UINT8 p = 0; p < pChunk->curr_page_idx; p++) {
        USER_LOG(4, "PLAYBACK PAGE, chunk_id = %d, page_now = %d,page_max = %d\r\n", chunk_id, p, pChunk->curr_page_idx);
        // 计算Flash偏移: DataStart + ChunkOffset + PageOffset
        UINT32 base = DINDEX_FDABNORMAL_PARA_1 + idx; 
        // UINT32 offset = p * sizeof(stFd_FlashPage_t);
        UINT32 offset = p * FLASH_PAGE_SIZE;
        
        if (NVMReadByOffset(base, offset, &readPage, sizeof(stFd_FlashPage_t))) {
            // 发送页头信息
			stCan.canid.exdid = 0x1A60F4F1;
            memset(stCan.data, 0, 8);
            stCan.data[0] = 0xAA;
            stCan.data[1] = p;
            stCan.data[2] = readPage.valid_count;
            CanIPSend(&stCan);

            USER_LOG(4, "read ok page_now = %d\r\n", p);
            for (UINT8 m = 0; m < readPage.valid_count; m++) {
                stFd_StoreRecord_t *pRec = &readPage.records[m];
                
                // 发送时间戳帧
				stCan.canid.exdid = 0x1A60F4F2;
                memset(stCan.data, 0, 8);
                stCan.data[0] = (m >> 8) & 0xFF; // Index High
                stCan.data[1] = m & 0xFF;        // Index Low
                stCan.data[2] = (pRec->timestamp >> 24) & 0xFF;
                stCan.data[3] = (pRec->timestamp >> 16) & 0xFF;
                stCan.data[4] = (pRec->timestamp >> 8) & 0xFF;
                stCan.data[5] = pRec->timestamp & 0xFF;
                CanIPSend(&stCan);
                // 发送原始数据帧
                CanIPSend(&pRec->can_msg);
				delay_ms(10);
            }
        }
    }
	// 发送尾帧
	stCan.canid.exdid = 0x1A60F4F5;
    memset(stCan.data, 0, 8);
    stCan.data[0] = 0xED;
    stCan.data[1] = 0xED;
    stCan.data[2] = 0xED;
    stCan.data[3] = 0xED;
    CanIPSend(&stCan);
}

/* ================= 内部辅助函数 ================= */

static void Fd_Store_SaveMgt(void) {
    NVMWrite(DINDEX_FDABNORMAL_MGT, &g_stStoreMgt, sizeof(g_stStoreMgt));
}

// 分配新Chunk
static INT8 Alloc_NewChunk(UINT8 fdid) {
    if (g_stStoreMgt.chunk_count >= FD_STORE_MAX_CHUNKS) return -1;
    
    // 1. 检查该ID已使用的块数
    UINT8 used_count = 0;
    for(int i=0; i<FD_STORE_MAX_CHUNKS; i++) {
        if ((g_stStoreMgt.chunk_bitmap & (1 << i)) && 
            (g_stStoreMgt.chunks[i].fdid == fdid)) {
            used_count++;
        }
    }
    
    // 限制：同一个探测器最多存2个块
    if (used_count >= FD_STORE_CHUNK_MAX) {
        return -1; // 已达上限，不分配
    }
    
    // 2. 查找空闲位分配
    for (int i = 0; i < FD_STORE_MAX_CHUNKS; i++) {
        if (!(g_stStoreMgt.chunk_bitmap & (1 << i))) {
            SPI_FLASH_SectorErase(0x0480000 + 0x20000*i);
            delay_ms(25);
            
            g_stStoreMgt.chunk_bitmap |= (1 << i);
            g_stStoreMgt.chunk_count++;
            g_stStoreMgt.chunks[i].fdid = fdid;
            g_stStoreMgt.chunks[i].curr_page_idx = 0;
            g_stStoreMgt.chunks[i].total_msgs = 0;
            g_stStoreMgt.chunks[i].start_time = tTimeStamp;
            
            Fd_Store_SaveMgt();
            return i;
        }
    }
    return -1;
}

// 锁定缓存区
static INT8 Alloc_Buffer(UINT8 fdid, UINT8 chunk_idx) {
    for (int i = 0; i < FD_STORE_BUFFER_NUM; i++) {
        if (!g_stBuffers[i].is_busy) {
            memset(&g_stBuffers[i], 0, sizeof(stFd_RamBuffer_t));
            g_stBuffers[i].is_busy = TRUE;
            g_stBuffers[i].owner_fdid = fdid;
            g_stBuffers[i].target_chunk_idx = chunk_idx;
            return i;
        }
    }
    return -1;
}

INT8 Find_Buffer(UINT8 fdid) {
    for (int i = 0; i < FD_STORE_BUFFER_NUM; i++) {
        if (g_stBuffers[i].is_busy && g_stBuffers[i].owner_fdid == fdid) return i;
    }
    return -1;
}

BOOL HasFreeBuffer(void) {
    for (int i = 0; i < FD_STORE_BUFFER_NUM; i++) {
        if (!g_stBuffers[i].is_busy) return TRUE;
    }
    return FALSE;
}

// 释放缓存
static void Free_Buffer(UINT8 buf_idx) {
        USER_LOG(4, "FREE\r\n");
        // Flush_Buffer(buf_idx); // 释放前强制刷盘
        g_stBuffers[buf_idx].is_busy = FALSE;
        g_stBuffers[buf_idx].owner_fdid = 0;
        g_stBuffers[buf_idx].target_chunk_idx = 0;
        memset(&g_stBuffers[buf_idx].page_cache, 0, sizeof(stFd_FlashPage_t));
}

// 写入到flash中
static void Flush_Buffer(UINT8 buf_idx) {
    stFd_RamBuffer_t *pBuf = &g_stBuffers[buf_idx];
    if (pBuf->page_cache.valid_count == 0) return;

    UINT8 cIdx = pBuf->target_chunk_idx;
    UINT8 pIdx = g_stStoreMgt.chunks[cIdx].curr_page_idx;
    
    USER_LOG(4, "WRITE FLASH, buf_idx = %d\r\n", buf_idx);
    // 写入Flash
    // UINT32 offset = pIdx * sizeof(stFd_FlashPage_t);
    UINT32 offset = pIdx * FLASH_PAGE_SIZE;
    BOOL ret = NVMWriteByOffset(DINDEX_FDABNORMAL_PARA_1 + cIdx, offset, &pBuf->page_cache, sizeof(stFd_FlashPage_t));
    delay_ms(5);
    USER_LOG(4, "WRITE FLASH ret = %d\r\n", ret);

    // 更新Chunk信息
    g_stStoreMgt.chunks[cIdx].curr_page_idx++;
    g_stStoreMgt.chunks[cIdx].total_msgs += pBuf->page_cache.valid_count;
    Fd_Store_SaveMgt();

    // 重置缓存页
    memset(&pBuf->page_cache, 0, sizeof(stFd_FlashPage_t));
}



