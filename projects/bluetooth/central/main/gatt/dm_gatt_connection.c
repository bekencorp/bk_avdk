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

int32_t dm_ble_app_env_init()
{
    os_memset(s_dm_gatt_env_array, 0, sizeof(s_dm_gatt_env_array));

    return 0;
}

int32_t dm_ble_app_env_deinit()
{
    dm_ble_free_all_app_env();
    os_memset(s_dm_gatt_env_array, 0, sizeof(s_dm_gatt_env_array));

    return 0;
}

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

    gatt_logi("%02x:%02x:%02x:%02x:%02x:%02x",
              addr[5],
              addr[4],
              addr[3],
              addr[2],
              addr[1],
              addr[0]);

    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (!os_memcmp(s_dm_gatt_env_array[i].addr, addr, BK_BD_ADDR_LEN))
        {
            if (s_dm_gatt_env_array[i].data)
            {
                os_free(s_dm_gatt_env_array[i].data);
            }

            if (s_dm_gatt_env_array[i].server_sem)
            {
                rtos_deinit_semaphore(&s_dm_gatt_env_array[i].server_sem);
                s_dm_gatt_env_array[i].server_sem = NULL;
            }

            if (s_dm_gatt_env_array[i].client_sem)
            {
                rtos_deinit_semaphore(&s_dm_gatt_env_array[i].client_sem);
                s_dm_gatt_env_array[i].client_sem = NULL;
            }

            for (int j = 0; j < sizeof(s_dm_gatt_env_array[i].profile_array) / sizeof(s_dm_gatt_env_array[i].profile_array[0]); ++j)
            {
                if (s_dm_gatt_env_array[i].profile_array[j].data)
                {
                    os_free(s_dm_gatt_env_array[i].profile_array[j].data);
                    s_dm_gatt_env_array[i].profile_array[j].data = NULL;
                    s_dm_gatt_env_array[i].profile_array[j].id = 0;
                }
            }

            os_memset(&s_dm_gatt_env_array[i], 0, sizeof(s_dm_gatt_env_array[i]));
            return 0;
        }
    }

    return 1;
}

uint8_t dm_ble_free_all_app_env()
{
    for (int i = 0; i < sizeof(s_dm_gatt_env_array) / sizeof(s_dm_gatt_env_array[0]); ++i)
    {
        if (s_dm_gatt_env_array[i].data)
        {
            os_free(s_dm_gatt_env_array[i].data);
        }

        if (s_dm_gatt_env_array[i].server_sem)
        {
            rtos_deinit_semaphore(&s_dm_gatt_env_array[i].server_sem);
            s_dm_gatt_env_array[i].server_sem = NULL;
        }

        if (s_dm_gatt_env_array[i].client_sem)
        {
            rtos_deinit_semaphore(&s_dm_gatt_env_array[i].client_sem);
            s_dm_gatt_env_array[i].client_sem = NULL;
        }

        for (int j = 0; j < sizeof(s_dm_gatt_env_array[i].profile_array) / sizeof(s_dm_gatt_env_array[i].profile_array[0]); ++j)
        {
            if (s_dm_gatt_env_array[i].profile_array[j].data)
            {
                os_free(s_dm_gatt_env_array[i].profile_array[j].data);
                s_dm_gatt_env_array[i].profile_array[j].data = NULL;
                s_dm_gatt_env_array[i].profile_array[j].id = 0;
            }
        }

        os_memset(&s_dm_gatt_env_array[i], 0, sizeof(s_dm_gatt_env_array[i]));
    }

    return 0;
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

dm_gatt_app_env_t *dm_ble_alloc_profile_data_by_addr(uint8_t profile_id, uint8_t *addr, uint32_t data_len, uint8_t **output_param)
{
    dm_gatt_app_env_t *tmp = dm_ble_find_app_env_by_addr(addr);
    uint32_t i = 0;

    if (!profile_id)
    {
        gatt_loge("invalid profile id");
        return NULL;
    }

    if (!tmp)
    {
        gatt_logw("not exist %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        return NULL;
    }

    for (i = 0; i < sizeof(tmp->profile_array) / sizeof(tmp->profile_array[0]); ++i)
    {
        if (tmp->profile_array[i].id == profile_id)
        {
            gatt_loge("profile id %d exist %d !!!", profile_id, i);

            while (1);

            if (output_param)
            {
                *output_param = tmp->profile_array[i].data;
            }

            return tmp;
        }
    }

    for (i = 0; i < sizeof(tmp->profile_array) / sizeof(tmp->profile_array[0]); ++i)
    {
        if (!tmp->profile_array[i].id)
        {
            tmp->profile_array[i].data = os_malloc(data_len);

            if (!tmp->profile_array[i].data)
            {
                gatt_loge("alloc err %d", data_len);
                return NULL;
            }

            os_memset(tmp->profile_array[i].data, 0, data_len);
            tmp->profile_array[i].data_len = data_len;
            tmp->profile_array[i].id = profile_id;

            if (output_param)
            {
                *output_param = tmp->profile_array[i].data;
            }

            return tmp;
        }
    }

    if (i >= sizeof(tmp->profile_array) / sizeof(tmp->profile_array[0]))
    {
        gatt_loge("full !!!");

        while (1);
    }

    return NULL;
}

uint8_t *dm_ble_find_profile_data_by_profile_id(dm_gatt_app_env_t *env, uint8_t profile_id)
{
    if (!env || !profile_id)
    {
        gatt_loge("param err");
        return NULL;
    }

    for (int i = 0; i < sizeof(env->profile_array) / sizeof(env->profile_array[0]); ++i)
    {
        if (env->profile_array[i].id == profile_id)
        {
            return env->profile_array[i].data;
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
