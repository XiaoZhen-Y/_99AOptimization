#include "sae_crc8.h"

UINT8 SAE_CRC8(const void *vptr, int count)
{
    int            si;
    int            i;
    UINT8        ucCrc    = 0xFFU;
    UINT8        ucPoly   = 0x1DU;
    const UINT8 *ucPtr    = (uint8_t *)vptr;

    for (si = 0; si < count; si++) {
        ucCrc ^= ucPtr[si];
        for (i = 0; i < 8; i++) {
            if (ucCrc & 0x80) {
                ucCrc = (ucCrc << 1) ^ ucPoly;
            } else {
                ucCrc <<= 1;
            }
        }
    }

    ucCrc ^= (uint8_t)0xFFU;
    return ucCrc;
}
