#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <components/log.h>
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "dm_gatt_connection.h"

uint8_t dm_gap_is_data_valid(uint8_t *data, uint32_t len)
{
    uint8_t sum_ff = 0xff, sum_zero = 0;

    for (int i = 0; i < len; ++i)
    {
        sum_ff &= data[i];
        sum_zero |= data[i];
    }

    return sum_ff != 0xff && sum_zero != 0;
}


uint8_t dm_gap_is_addr_valid(uint8_t *addr)
{
    if(!addr)
    {
        return 0;
    }

    return dm_gap_is_data_valid(addr, BK_BD_ADDR_LEN);
}
