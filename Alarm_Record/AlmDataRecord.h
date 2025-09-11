#ifndef __ALMDATARECORD_H__
#define __ALMDATARECORD_H__

#include "cjcrc.h"
#include "para.h" 
#include "app_conf.h"
#include "nvm.h"
#include "c_mem.h"
#include "w25n01.h"

#define ALM_DATA_ITEM_MAX   64
#define ALM_DATA_COUNT_MAX  256

#define C_ALMDATA_RECORD    2048

typedef struct {
    uint8_t             ucTimers[4];
    STR_CAN_STD_TYPE    stCANData;
} __attribute__((aligned (32))) STR_ALMDATA_REC;


extern void AlmDataRec_Init(void);
extern void AlmDataRec_Store(const STR_CAN_STD_TYPE *ptrCan);
extern uint16_t AlmDataRec_AlmDataNum(void);
extern STR_ALMDATA_REC *AlmDataRec_ReadStore(uint16_t usReadIndex);
extern void AlmDataRec_ClearRecord(void);
extern void AlmDataRec_Task();
#endif

