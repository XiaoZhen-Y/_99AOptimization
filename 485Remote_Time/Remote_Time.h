#ifndef __REMOTE_TIME_H__
#define __REMOTE_TIME_H__
#include <stdint.h>

#define YEAR_ADDRESS 0x140

extern void Remote_TimeChange(uint16_t *uspMbHoldData);

#endif

