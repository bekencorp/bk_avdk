#include <common/sys_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <components/log.h>
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>

#include "wifi_boarding_demo.h"

#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "components/bluetooth/bk_dm_gap_ble_types.h"
#include "components/bluetooth/bk_dm_gap_ble.h"
#include "components/bluetooth/bk_dm_gatt_types.h"
#include "components/bluetooth/bk_dm_gatts.h"
#include "dm_gatts.h"

#if WIFI_BOARDING_DEMO_ENABLE

typedef struct
{
    uint8_t status; //0 idle 1 connected
} wifi_boarding_app_env_t;

#define MIN_VALUE(x, y) (((x) < (y)) ? (x): (y))


static uint16_t s_prop_cli_config;
static uint8_t s_ssid[64];
static uint8_t s_password[64];
static uint8_t s_wifi_boarding_is_init;

static const bk_gatts_attr_db_t s_gatts_attr_db_service_boarding[] =
{
    {
        BK_GATT_PRIMARY_SERVICE_DECL(0xfa00),
    },

    //    {
    //        BK_GATT_CHAR_DECL(0xea01,
    //                          0, NULL,
    //                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_NOTIFY,
    //                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
    //                          BK_GATT_RSP_BY_APP),
    //    },
    //    {
    //        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_CLIENT_CONFIG,
    //                               sizeof(s_prop_cli_config), (uint8_t *)&s_prop_cli_config,
    //                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
    //                               BK_GATT_RSP_BY_APP),
    //    },
    //
    //    //operation
    //    {
    //        BK_GATT_CHAR_DECL(0xea02,
    //                          0, NULL,
    //                          BK_GATT_CHAR_PROP_BIT_WRITE,
    //                          BK_GATT_PERM_WRITE,
    //                          BK_GATT_RSP_BY_APP),
    //    },

    //ssid
    {
        BK_GATT_CHAR_DECL(0xea05,
                          sizeof(s_password), (uint8_t *)s_password,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },

    //password
    {
        BK_GATT_CHAR_DECL(0xea06,
                          sizeof(s_ssid), (uint8_t *)s_ssid,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
};

static uint16_t s_boarding_attr_handle_list[sizeof(s_gatts_attr_db_service_boarding) / sizeof(s_gatts_attr_db_service_boarding[0])];

static int32_t wifi_boarding_gatts_cb(bk_gatts_cb_event_t event, bk_gatt_if_t gatts_if, bk_ble_gatts_cb_param_t *comm_param)
{
    ble_err_t ret = 0;
    dm_gatt_app_env_t *common_env_tmp = NULL;
    wifi_boarding_app_env_t *app_env_tmp = NULL;

    switch (event)
    {
    case BK_GATTS_CONNECT_EVT:
    {
        struct gatts_connect_evt_param *param = (typeof(param))comm_param;

        wboard_logi("BK_GATTS_CONNECT_EVT %d role %d %02X:%02X:%02X:%02X:%02X:%02X", param->conn_id, param->link_role,
                    param->remote_bda[5],
                    param->remote_bda[4],
                    param->remote_bda[3],
                    param->remote_bda[2],
                    param->remote_bda[1],
                    param->remote_bda[0]);

        common_env_tmp = dm_ble_alloc_addition_data_by_addr(param->remote_bda, sizeof(*app_env_tmp));

        if (!common_env_tmp)
        {
            wboard_loge("alloc addition data err !!!!");
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->addition_data;
        app_env_tmp->status = 1;
    }
    break;

    case BK_GATTS_DISCONNECT_EVT:
    {
        struct gatts_disconnect_evt_param *param = (typeof(param))comm_param;


        wboard_logi("BK_GATTS_DISCONNECT_EVT %02X:%02X:%02X:%02X:%02X:%02X",
                    param->remote_bda[5],
                    param->remote_bda[4],
                    param->remote_bda[3],
                    param->remote_bda[2],
                    param->remote_bda[1],
                    param->remote_bda[0]);

        common_env_tmp = dm_ble_find_app_env_by_addr(param->remote_bda);

        if (!common_env_tmp)
        {
            wboard_loge("cant find app env");
            break;
        }

        if (common_env_tmp->addition_data)
        {
            os_free(common_env_tmp->addition_data);
            common_env_tmp->addition_data = NULL;
        }

        common_env_tmp->addition_data_len = 0;
    }
    break;

    case BK_GATTS_CONF_EVT:
    {
        wboard_logi("BK_GATTS_CONF_EVT");
    }
    break;

    case BK_GATTS_RESPONSE_EVT:
    {
        wboard_logi("BK_GATTS_RESPONSE_EVT");
    }
    break;

    case BK_GATTS_READ_EVT:
    {
        struct gatts_read_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        memset(&rsp, 0, sizeof(rsp));
        wboard_logi("read attr handle %d need rsp %d", param->handle, param->need_rsp);

        uint8_t *tmp_buff = NULL;
        uint32_t buff_size = 0;
        uint32_t index = 0;

        if (dm_gatts_get_buff_from_attr_handle((bk_gatts_attr_db_t *)s_gatts_attr_db_service_boarding, s_boarding_attr_handle_list,
                                               sizeof(s_boarding_attr_handle_list) / sizeof(s_boarding_attr_handle_list[0]), param->handle, &index, &tmp_buff, &buff_size))
        {
            wboard_logi("handle invalid");
            break;
        }

        wboard_logi("index %d size %d buff %p", index, buff_size, tmp_buff);

        if (param->need_rsp)
        {
            final_len = buff_size - param->offset;

            rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
            rsp.attr_value.handle = param->handle;
            rsp.attr_value.offset = param->offset;
            rsp.attr_value.len = final_len;
            rsp.attr_value.value = tmp_buff + param->offset;

            ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
        }
    }
    break;

    case BK_GATTS_WRITE_EVT:
    {
        struct gatts_write_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        memset(&rsp, 0, sizeof(rsp));

        wboard_logi("write attr handle %d need rsp %d", param->handle, param->need_rsp);

        uint8_t *tmp_buff = NULL;
        uint32_t buff_size = 0;
        uint32_t index = 0;

        if (dm_gatts_get_buff_from_attr_handle((bk_gatts_attr_db_t *)s_gatts_attr_db_service_boarding, s_boarding_attr_handle_list,
                                               sizeof(s_boarding_attr_handle_list) / sizeof(s_boarding_attr_handle_list[0]), param->handle, &index, &tmp_buff, &buff_size))
        {
            wboard_logi("handle invalid");
            break;
        }

        wboard_logi("index %d size %d buff %p", index, buff_size, tmp_buff);

        if (param->handle == s_boarding_attr_handle_list[1])
        {
            os_memset(s_ssid, 0, sizeof(s_ssid));
            os_memcpy(s_ssid, param->value, param->len);
            wboard_logi("write ssid %s", s_ssid);
        }
        else if (param->handle == s_boarding_attr_handle_list[2])
        {
            os_memset(s_password, 0, sizeof(s_password));
            os_memcpy(s_password, param->value, param->len);
            wboard_logi("write password %s", s_password);
        }

        if (param->need_rsp)
        {
            final_len = MIN_VALUE(param->len, buff_size - param->offset);
            memcpy(tmp_buff + param->offset, param->value, final_len);

            rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
            rsp.attr_value.handle = param->handle;
            rsp.attr_value.offset = param->offset;
            rsp.attr_value.len = final_len;
            rsp.attr_value.value = tmp_buff + param->offset;

            ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
        }

        if (strnlen((char *)s_ssid, sizeof(s_ssid)) && strnlen((char *)s_password, sizeof(s_password)))
        {
#if CONFIG_WIFI_ENABLE
            extern int demo_sta_app_init(char *oob_ssid, char *connect_key);
            wboard_logi("ssid %s password %s, start connect ap", s_ssid, s_password);
            demo_sta_app_init((char *)s_ssid, (char *)s_password);
#else
            wboard_logw("wifi not enable");
#endif
        }
    }
    break;

    case BK_GATTS_EXEC_WRITE_EVT:
    {
        struct gatts_exec_write_evt_param *param = (typeof(param))comm_param;
        wboard_logi("exec write");
    }
    break;

    default:
        break;
    }

    return 0;
}

static int32_t wifi_boarding_demo_reg_db(void)
{
    int32_t ret = dm_gatts_reg_db((bk_gatts_attr_db_t *)s_gatts_attr_db_service_boarding,
                                  sizeof(s_gatts_attr_db_service_boarding) / sizeof(s_gatts_attr_db_service_boarding[0]),
                                  s_boarding_attr_handle_list,
                                  wifi_boarding_gatts_cb);

    if (ret)
    {
        wboard_loge("reg db err");
        return ret;
    }

    for (int i = 0; i < sizeof(s_gatts_attr_db_service_boarding) / sizeof(s_gatts_attr_db_service_boarding[0]); ++i)
    {
        wboard_logi("attr handle %d", s_boarding_attr_handle_list[i]);
    }

    return ret;
}

#endif

int32_t wifi_boarding_demo_main(void)
{
#if WIFI_BOARDING_DEMO_ENABLE

    if (!dm_gatts_is_init())
    {
        wboard_loge("gatts is not init");
        return -1;
    }

    if (s_wifi_boarding_is_init)
    {
        wboard_loge("already init");
        return -1;
    }

    s_wifi_boarding_is_init = 1;

    wifi_boarding_demo_reg_db();

    wboard_logi("done");
#else
    wboard_loge("wifi boarding demo not enable");
#endif
    return 0;
}
