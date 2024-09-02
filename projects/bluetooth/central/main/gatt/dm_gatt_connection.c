#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <components/log.h>
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>

#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "components/bluetooth/bk_dm_gap_ble.h"
#include "components/bluetooth/bk_dm_gatt_common.h"
#include "dm_gatt.h"
#include "dm_gatt_connection.h"
#include "dm_gap_utils.h"

static dm_gatt_app_env_t s_dm_gatt_env_array[GATT_MAX_CONNECTION_COUNT];

dm_gatt_app_env_t *dm_ble_find_app_env_by_addr(uint8_t *addr)
{
    if (!dm_gap_is_addr_valid(addr))
    {
        return NULL;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (!os_memcmp(s_dm_gatt_env_array[i].addr, addr, BK_BD_ADDR_LEN))
        {
            return s_dm_gatt_env_array + i;
        }
    }

    return NULL;
}

dm_gatt_app_env_t *dm_ble_find_app_env_by_conn_id(uint16_t conn_id)
{
    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (dm_gap_is_addr_valid(s_dm_gatt_env_array[i].addr) && s_dm_gatt_env_array[i].conn_id == conn_id)
        {
            return s_dm_gatt_env_array + i;
        }
    }

    return NULL;
}

uint8_t dm_ble_del_app_env_by_addr(uint8_t *addr)
{
    if (!dm_gap_is_addr_valid(addr))
    {
        return 1;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (!os_memcmp(s_dm_gatt_env_array[i].addr, addr, BK_BD_ADDR_LEN))
        {
            if (s_dm_gatt_env_array[i].data)
            {
                os_free(s_dm_gatt_env_array[i].data);
            }

            os_memset(&s_dm_gatt_env_array[i], 0, sizeof(s_dm_gatt_env_array[i]));
            return 0;
        }
    }

    return 1;
}


dm_gatt_app_env_t *dm_ble_alloc_app_env_by_addr(uint8_t *addr, uint32_t data_len)
{
    dm_gatt_app_env_t *tmp = NULL;

    tmp = dm_ble_find_app_env_by_addr(addr);

    if (tmp)
    {
        gatt_logw("already exist %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        return tmp;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (!dm_gap_is_addr_valid(s_dm_gatt_env_array[i].addr))
        {
            os_memcpy(s_dm_gatt_env_array[i].addr, addr, BK_BD_ADDR_LEN);

            if (data_len)
            {
                s_dm_gatt_env_array[i].data_len = data_len;
                s_dm_gatt_env_array[i].data = os_malloc(data_len);

                if (!s_dm_gatt_env_array[i].data)
                {
                    gatt_loge("malloc err");
                    os_memset(&s_dm_gatt_env_array[i], 0, sizeof(s_dm_gatt_env_array[0]));
                    return NULL;
                }

                os_memset(s_dm_gatt_env_array[i].data, 0, data_len);
            }

            return &s_dm_gatt_env_array[i];
        }
    }

    return NULL;
}

uint8_t dm_ble_app_env_foreach( int32_t (*func) (dm_gatt_app_env_t *env, void *arg), void *arg )
{
    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (dm_gap_is_addr_valid(s_dm_gatt_env_array[i].addr))
        {
            func(s_dm_gatt_env_array + i, arg);
        }
    }

    return 0;
}
