// Copyright 2020-2021 Beken
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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

#include "components/bluetooth/bk_dm_bluetooth_types.h"
#include "components/bluetooth/bk_dm_gap_ble_types.h"
#include "components/bluetooth/bk_dm_gap_ble.h"

#include "dm_gatts.h"
#include "dm_gatt_connection.h"
#include "dm_gap_utils.h"

#define DYNAMIC_ADD_ATTR 0


#define INVALID_ATTR_HANDLE 0
#define ADV_HANDLE 0

#define MIN_VALUE(x, y) (((x) < (y)) ? (x): (y))


//typedef struct
//{
//    uint8_t notify_status; //0 disable; 1 notify; 2 indicate
//} dm_gatts_app_env_t;

#define dm_gatts_app_env_t dm_gatt_addition_app_env_t


static int32_t dm_gatts_set_adv_param(uint8_t local_addr_is_public);

static beken_semaphore_t s_ble_sema = NULL;
static uint16_t s_service_attr_handle = INVALID_ATTR_HANDLE;

static uint16_t s_char_attr_handle = INVALID_ATTR_HANDLE;
static uint8_t s_char_buff[4] = {0};

static uint16_t s_char_desc_attr_handle = INVALID_ATTR_HANDLE;
static uint16_t s_char_desc_buff = 0;

static uint16_t s_char2_attr_handle = INVALID_ATTR_HANDLE;
static uint8_t s_char2_buff[312] = {0};

static uint16_t s_char2_desc_attr_handle = INVALID_ATTR_HANDLE;
static uint32_t s_char2_desc_buff = 0;

static uint16_t s_char_auto_rsp_attr_handle = INVALID_ATTR_HANDLE;
static uint8_t s_char_auto_rsp_buff[300] = {0};

static uint16_t s_char4_attr_handle = INVALID_ATTR_HANDLE;

static beken_timer_t s_char_notify_timer;
static uint16_t s_conn_id = 0xff;

static bk_gatt_if_t s_gatts_if;
//static uint8_t s_timer_send_is_indicate = 0;

static uint8_t s_dm_gatts_local_addr_is_public = 0;

#if DYNAMIC_ADD_ATTR

#else

#define BK_GATT_ATTR_TYPE(iuuid) {.len = BK_UUID_LEN_16, .uuid = {.uuid16 = iuuid}}
#define BK_GATT_ATTR_CONTENT(iuuid) {.len = BK_UUID_LEN_16, .uuid = {.uuid16 = iuuid}}
#define BK_GATT_ATTR_VALUE(ilen, ivalue) {.attr_max_len = ilen, .attr_len = ilen, .attr_value = ivalue}


#define BK_GATT_PRIMARY_SERVICE_DECL(iuuid) \
    .att_desc =\
               {\
                .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_PRI_SERVICE),\
                .attr_content = BK_GATT_ATTR_CONTENT(iuuid),\
               }

#define BK_GATT_CHAR_DECL(iuuid, ilen, ivalue, iprop, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_CHAR_DECLARE),\
                 .attr_content = BK_GATT_ATTR_CONTENT(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .prop = iprop,\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}

#define BK_GATT_CHAR_DESC_DECL(iuuid, ilen, ivalue, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}

static const bk_gatts_attr_db_t s_gatts_attr_db_service_1[] =
{
    //service
    {
        BK_GATT_PRIMARY_SERVICE_DECL(INTERESTING_SERIVCE_UUID),
    },

    //char 1
    {
        BK_GATT_CHAR_DECL(INTERESTING_CHAR_UUID,
                          sizeof(s_char_buff), s_char_buff,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_NOTIFY,
                          //BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_PERM_READ_ENCRYPTED | BK_GATT_PERM_WRITE_ENCRYPTED,
                          //BK_GATT_PERM_READ_ENC_MITM | BK_GATT_PERM_WRITE_ENC_MITM, //gap iocap must not be BK_IO_CAP_NONE !!!
                          BK_GATT_RSP_BY_APP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_CLIENT_CONFIG,
                               sizeof(s_char_desc_buff), (uint8_t *)&s_char_desc_buff,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_RSP_BY_APP),
    },

    //char 2
    {
        BK_GATT_CHAR_DECL(0x9abc,
                          sizeof(s_char2_buff), s_char2_buff,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          //BK_GATT_PERM_READ_ENCRYPTED | BK_GATT_PERM_WRITE_ENCRYPTED,
                          //BK_GATT_PERM_READ_ENC_MITM | BK_GATT_PERM_WRITE_ENC_MITM, //gap iocap must not be BK_IO_CAP_NONE !!!
                          BK_GATT_RSP_BY_APP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_SRVR_CONFIG,
                               sizeof(s_char2_desc_buff), (uint8_t *)&s_char2_desc_buff,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_RSP_BY_APP),
    },

    //char 3
    {
        BK_GATT_CHAR_DECL(0x15ab,
                          sizeof(s_char_auto_rsp_buff), s_char_auto_rsp_buff,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },

    //char 4 no buffer
    {
        BK_GATT_CHAR_DECL(0x15ac,
                          0, NULL,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP), //Without buffer, BK_GATT_AUTO_RSP will cause "NOT PERMITTED" err to peer when recv write or read req. So set BK_GATT_RSP_BY_APP instead.
    },
};

static const bk_gatts_attr_db_t s_gatts_attr_db_service_2[] =
{
    //service
    {
        BK_GATT_PRIMARY_SERVICE_DECL(0x2234),
    },

    //char 1
    {
        BK_GATT_CHAR_DECL(0x5678,
                          sizeof(s_char_buff), s_char_buff,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_NOTIFY,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          //BK_GATT_PERM_READ_ENCRYPTED | BK_GATT_PERM_WRITE_ENCRYPTED,
                          //BK_GATT_PERM_READ_ENC_MITM | BK_GATT_PERM_WRITE_ENC_MITM, //gap iocap must not be BK_IO_CAP_NONE !!!
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_CLIENT_CONFIG,
                               sizeof(s_char_desc_buff), (uint8_t *)&s_char_desc_buff,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_AUTO_RSP),
    },

    //char 2
    {
        BK_GATT_CHAR_DECL(0x9abc,
                          sizeof(s_char2_buff), s_char2_buff,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ_ENCRYPTED | BK_GATT_PERM_WRITE_ENCRYPTED,
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_SRVR_CONFIG,
                               sizeof(s_char2_desc_buff), (uint8_t *)&s_char2_desc_buff,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_AUTO_RSP),
    },

    //char 3
    {
        BK_GATT_CHAR_DECL(0x15ab,
                          sizeof(s_char_auto_rsp_buff), s_char_auto_rsp_buff,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ_ENC_MITM | BK_GATT_PERM_WRITE_ENC_MITM, //gap iocap must not be BK_IO_CAP_NONE !!!
                          BK_GATT_AUTO_RSP),
    },
};

static uint16_t *const s_attr_handle_list[sizeof(s_gatts_attr_db_service_1) / sizeof(s_gatts_attr_db_service_1[0])] =
{
    &s_service_attr_handle,
    &s_char_attr_handle,
    &s_char_desc_attr_handle,
    &s_char2_attr_handle,
    &s_char2_desc_attr_handle,
    &s_char_auto_rsp_attr_handle,
    &s_char4_attr_handle,
};


static uint16_t s_attr_handle_list2[sizeof(s_gatts_attr_db_service_2) / sizeof(s_gatts_attr_db_service_2[0])];


#endif

static int32_t dm_ble_gap_cb(bk_ble_gap_cb_event_t event, bk_ble_gap_cb_param_t *param)
{
    gatt_logd("event %d", event);

    switch (event)
    {
    case BK_BLE_GAP_SET_STATIC_RAND_ADDR_EVT:
    {
        struct ble_set_rand_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set rand addr err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT:
    {
        struct ble_adv_set_rand_addr_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set adv rand addr err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_PARAMS_SET_COMPLETE_EVT:
    {
        struct ble_adv_params_set_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set adv param err 0x%x", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_DATA_SET_COMPLETE_EVT:
    {
        struct ble_adv_data_set_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set adv data err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT:
    {
        struct ble_adv_scan_rsp_set_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set adv data scan rsp err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_EXT_ADV_START_COMPLETE_EVT:
    {
        struct ble_adv_start_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set adv enable err %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    default:
        return DM_BLE_GAP_APP_CB_RET_NO_INTERESTING;
        break;

    }

    return DM_BLE_GAP_APP_CB_RET_PROCESSED;
}

static int32_t nest_func_send_indicate(dm_gatt_app_env_t *env, void *arg)
{
    uint8_t *tmp_buff = (typeof(tmp_buff))arg;

    if (env && env->status == GAP_CONNECT_STATUS_CONNECTED && env->conn_id != 0xffff && env->data) //env->local_is_master == 0
    {
        dm_gatts_app_env_t *tmp = (typeof(tmp))env->data;

        if (tmp->notify_status)
        {
            if (bk_ble_gatts_send_indicate(0, env->conn_id, s_char_attr_handle, sizeof(s_char_buff), tmp_buff, tmp->notify_status == 2 ? 1 : 0))
            {
                gatt_loge("notify err");
            }
        }
    }

    return 0;
}

static int32_t nest_func_check_timer_ref_count(dm_gatt_app_env_t *env, void *arg)
{
    uint8_t *tmp_count = arg;

    if (env && env->status == GAP_CONNECT_STATUS_CONNECTED && env->conn_id != 0xffff && env->data) //&& env->local_is_master == 0
    {
        dm_gatts_app_env_t *tmp_env = (typeof(tmp_env))env->data;

        if (tmp_env->notify_status)
        {
            (*tmp_count)++;
        }
    }

    return 0;
}

static void ble_char_timer_callback(void *param)
{
    static uint8_t value = 1;

    uint8_t *tmp_buff = NULL;
    uint32_t type = (typeof(type))param;

    if (s_conn_id == 0xff)
    {
        return;
    }

    tmp_buff = os_malloc(sizeof(s_char_buff));

    if (!tmp_buff)
    {
        gatt_loge("alloc send failed");
        return;
    }

    os_memset(tmp_buff, 0, sizeof(s_char_buff));
    tmp_buff[0] = value++;

    //nest func only for gcc

    dm_ble_app_env_foreach(nest_func_send_indicate, tmp_buff);
    //retval = bk_ble_gatts_send_indicate(0, s_conn_id, s_char_attr_handle, sizeof(s_char_buff), tmp_buff, s_timer_send_is_indicate);

    os_free(tmp_buff);

    //    if (retval != 0)
    //    {
    //        gatt_loge("notify err %d", retval);
    //    }
}

static int32_t bk_gatts_cb (bk_gatts_cb_event_t event, bk_gatt_if_t gatts_if, bk_ble_gatts_cb_param_t *comm_param)
{
    ble_err_t ret = 0;
    dm_gatt_app_env_t *common_env_tmp = NULL;
    dm_gatts_app_env_t *app_env_tmp = NULL;

    switch (event)
    {
    case BK_GATTS_REG_EVT:
    {
        struct gatts_reg_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_REG_EVT %d %d", param->status, param->gatt_if);
        s_gatts_if = param->gatt_if;

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_START_EVT:
    {
        struct gatts_start_evt_param *param = (typeof(param))comm_param;
        gatt_logi("BK_GATTS_START_EVT compl %d %d", param->status, param->service_handle);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_STOP_EVT:
    {
        struct gatts_stop_evt_param *param = (typeof(param))comm_param;
        gatt_logi("BK_GATTS_STOP_EVT compl %d %d", param->status, param->service_handle);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_CREAT_ATTR_TAB_EVT:
    {
#if DYNAMIC_ADD_ATTR
#else
        struct gatts_add_attr_tab_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_CREAT_ATTR_TAB_EVT %d %d", param->status, param->num_handle);

        if (s_service_attr_handle == INVALID_ATTR_HANDLE)
        {
            for (int i = 0; i < param->num_handle; ++i)
            {
                *s_attr_handle_list[i] = param->handles[i];

                if (i) //service handle cant get buff
                {
                    uint16_t tmp_len = 0;
                    uint8_t *tmp_buff = NULL;
                    ble_err_t g_status = 0;

                    g_status = bk_ble_gatts_get_attr_value(param->handles[i], &tmp_len, &tmp_buff);

                    if (g_status)
                    {
                        gatt_loge("get attr value err %d", g_status);
                    }


                    if (tmp_len != s_gatts_attr_db_service_1[i].att_desc.value.attr_len ||
                            tmp_buff != s_gatts_attr_db_service_1[i].att_desc.value.attr_value)
                    {
                        gatt_loge("get attr value not match create attr handle %d i %d %d %d %p %p!!!!",
                                  param->handles[i], i,
                                  tmp_len, s_gatts_attr_db_service_1[i].att_desc.value.attr_len,
                                  tmp_buff, s_gatts_attr_db_service_1[i].att_desc.value.attr_value);
                    }
                }
            }
        }
        else
        {
            for (int i = 0; i < param->num_handle; ++i)
            {
                s_attr_handle_list2[i] = param->handles[i];

                if (i) //service handle cant get buff
                {
                    uint16_t tmp_len = 0;
                    uint8_t *tmp_buff = NULL;
                    ble_err_t g_status = 0;

                    g_status = bk_ble_gatts_get_attr_value(param->handles[i], &tmp_len, &tmp_buff);

                    if (g_status)
                    {
                        gatt_loge("get attr value err %d", g_status);
                    }


                    if (tmp_len != s_gatts_attr_db_service_2[i].att_desc.value.attr_len ||
                            tmp_buff != s_gatts_attr_db_service_2[i].att_desc.value.attr_value)
                    {
                        gatt_loge("get attr value not match create attr handle %d i %d %d %d %p %p!!!!",
                                  param->handles[i], i,
                                  tmp_len, s_gatts_attr_db_service_2[i].att_desc.value.attr_len,
                                  tmp_buff, s_gatts_attr_db_service_2[i].att_desc.value.attr_value);
                    }
                }
            }
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }

#endif
    }
    break;

    case BK_GATTS_CREATE_EVT:
    {
        struct gatts_create_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_CREATE_EVT %d %d", param->status, param->service_handle);
        s_service_attr_handle = param->service_handle;

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_ADD_CHAR_EVT:
    {
        struct gatts_add_char_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_ADD_CHAR_EVT %d %d %d", param->status, param->attr_handle, param->service_handle);

        if (s_char_attr_handle == INVALID_ATTR_HANDLE)
        {
            s_char_attr_handle = param->attr_handle;
        }
        else if (s_char2_attr_handle == INVALID_ATTR_HANDLE)
        {
            s_char2_attr_handle = param->attr_handle;
        }
        else
        {
            s_char_auto_rsp_attr_handle = param->attr_handle;
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_ADD_CHAR_DESCR_EVT:
    {
        struct gatts_add_char_descr_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_ADD_CHAR_DESCR_EVT %d %d %d", param->status, param->attr_handle, param->service_handle);

        if (s_char_desc_attr_handle == INVALID_ATTR_HANDLE)
        {
            s_char_desc_attr_handle = param->attr_handle;
        }
        else if (s_char2_desc_attr_handle == INVALID_ATTR_HANDLE)
        {
            s_char2_desc_attr_handle = param->attr_handle;
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_GATTS_READ_EVT:
    {
        struct gatts_read_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        gatt_logi("BK_GATTS_READ_EVT %d %d %d %d %d %d %02X:%02X:%02X:%02X:%02X:%02X", param->conn_id, param->trans_id, param->handle,
                  param->offset, param->is_long, param->need_rsp,
                  param->bda[5],
                  param->bda[4],
                  param->bda[3],
                  param->bda[2],
                  param->bda[1],
                  param->bda[0]);

        memset(&rsp, 0, sizeof(rsp));

        if (param->handle == s_char_attr_handle)
        {
            if (param->need_rsp)
            {
                final_len = sizeof(s_char_buff) - param->offset;

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.value = s_char_buff + param->offset;
                rsp.attr_value.len = final_len;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }
        }
        else if (param->handle == s_char_desc_attr_handle)
        {
            if (param->need_rsp)
            {
                final_len = sizeof(s_char_desc_buff) - param->offset;

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.value = (uint8_t *)&s_char_desc_buff + param->offset;
                rsp.attr_value.len = final_len;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }

        }
        else if (param->handle == s_char2_attr_handle)
        {
            if (param->need_rsp)
            {
                final_len = sizeof(s_char2_buff) - param->offset;

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.value = s_char2_buff + param->offset;
                rsp.attr_value.len = final_len;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }
        }
        else if (param->handle == s_char2_desc_attr_handle)
        {
            if (param->need_rsp)
            {
                final_len = sizeof(s_char2_desc_buff) - param->offset;

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.value = (uint8_t *)&s_char2_desc_buff + param->offset;
                rsp.attr_value.len = final_len;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
                //ret = bk_ble_gatts_send_response(s_gatt_if, param->conn_id, param->trans_id, BK_GATT_INSUF_RESOURCE, &rsp);
            }
        }
        else if (param->handle == s_char_auto_rsp_attr_handle)
        {
            if (param->need_rsp)
            {
                final_len = sizeof(s_char_auto_rsp_buff) - param->offset;

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.value = s_char_auto_rsp_buff + param->offset;
                rsp.attr_value.len = final_len;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }
        }
        else
        {
            if (param->need_rsp)
            {
                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_REQ_NOT_SUPPORTED, &rsp);
            }
        }
    }
    break;

    case BK_GATTS_WRITE_EVT:
    {
        struct gatts_write_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        memset(&rsp, 0, sizeof(rsp));

        gatt_logi("BK_GATTS_WRITE_EVT %d %d %d %d %d %d %d 0x%02X%02X %02X:%02X:%02X:%02X:%02X:%02X",
                  param->conn_id, param->trans_id, param->handle,
                  param->offset, param->need_rsp, param->is_prep, param->len,
                  param->value[0], param->value[1],
                  param->bda[5],
                  param->bda[4],
                  param->bda[3],
                  param->bda[2],
                  param->bda[1],
                  param->bda[0]);

        if (param->handle == s_char_attr_handle)
        {
            gatt_logi("len %d 0x%02X%02X%02X%02X", param->len,
                      param->value[0],
                      param->value[1],
                      param->value[2],
                      param->value[3]);

            if (param->need_rsp)
            {
                final_len = MIN_VALUE(param->len, sizeof(s_char_buff) - param->offset);
                memcpy(s_char_buff + param->offset, param->value, final_len);

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.len = final_len;
                rsp.attr_value.value = s_char_buff + param->offset;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }
        }
        else if (param->handle == s_char_desc_attr_handle)
        {
            uint16_t config = (((uint16_t)(param->value[1])) << 8) | param->value[0];

            common_env_tmp = dm_ble_find_app_env_by_addr(param->bda);

            if (!common_env_tmp || !common_env_tmp->data)
            {
                gatt_loge("conn max %p %p !!!!", common_env_tmp, common_env_tmp ? common_env_tmp : 0);
                break;
            }

            app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

            if (config & 1)
            {
                gatt_logi("client notify open");

                app_env_tmp->notify_status = 1;

                if (!rtos_is_timer_init(&s_char_notify_timer))
                {
                    rtos_init_timer(&s_char_notify_timer, 1000, ble_char_timer_callback, (void *)0);
                    rtos_start_timer(&s_char_notify_timer);
                }
            }
            else if (config & 2)
            {
                gatt_logi("client indicate open");

                app_env_tmp->notify_status = 2;

                if (!rtos_is_timer_init(&s_char_notify_timer))
                {
                    rtos_init_timer(&s_char_notify_timer, 1000, ble_char_timer_callback, (void *)0);
                    rtos_start_timer(&s_char_notify_timer);
                }
            }
            else if (!config)
            {
                uint8_t timer_ref_count = 0;
                gatt_logi("client config close");

                app_env_tmp->notify_status = 0;

                //nest func only for gcc

                dm_ble_app_env_foreach(nest_func_check_timer_ref_count, &timer_ref_count);

                if (!timer_ref_count && rtos_is_timer_init(&s_char_notify_timer))
                {
                    if (rtos_is_timer_running(&s_char_notify_timer))
                    {
                        rtos_stop_timer(&s_char_notify_timer);
                    }

                    rtos_deinit_timer(&s_char_notify_timer);
                }
            }
            else
            {
                gatt_logi("client config close");
            }

            if (param->need_rsp)
            {
                final_len = MIN_VALUE(param->len, sizeof(s_char_desc_attr_handle) - param->offset);
                memcpy((uint8_t *)&s_char_desc_buff + param->offset, param->value, final_len);

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.len = final_len;
                rsp.attr_value.value = (uint8_t *)&s_char_desc_buff + param->offset;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }
        }
        else if (param->handle == s_char2_attr_handle)
        {
            if (param->need_rsp)
            {
                final_len = MIN_VALUE(param->len, sizeof(s_char2_buff) - param->offset);
                memcpy(s_char2_buff + param->offset, param->value, final_len);

                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.len = final_len;
                rsp.attr_value.value = s_char2_buff + param->offset;

                ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
            }
        }
        else if (param->handle == s_char2_desc_attr_handle)
        {
            if (param->need_rsp)
            {
                rsp.attr_value.auth_req = BK_GATT_AUTH_REQ_NONE;
                rsp.attr_value.handle = param->handle;
                rsp.attr_value.offset = param->offset;
                rsp.attr_value.value = (uint8_t *)&s_char2_desc_buff + param->offset;

                if ((param->value[0] % 2) == 0)
                {
                    final_len = MIN_VALUE(param->len, sizeof(s_char2_desc_buff) - param->offset);

                    memcpy((uint8_t *)&s_char2_desc_buff + param->offset, param->value, final_len);

                    rsp.attr_value.len = final_len;

                    ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
                }
                else
                {
                    gatt_loge("this attr %d must write with even num [0], not %d", param->handle, param->value[0]);
                    ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_PARAM_VAL_NOT_ALLOW, &rsp);
                }
            }
        }
        else if (param->handle == s_char_auto_rsp_attr_handle)
        {

        }
    }
    break;

    case BK_GATTS_EXEC_WRITE_EVT:
    {
        struct gatts_exec_write_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_EXEC_WRITE_EVT %d %d %d %d", param->conn_id, param->trans_id, param->exec_write_flag, param->need_rsp);

        if (param->need_rsp)
        {
            bk_gatt_rsp_t rsp;

            memset(&rsp, 0, sizeof(rsp));

            ret = bk_ble_gatts_send_response(gatts_if, param->conn_id, param->trans_id, BK_GATT_OK, &rsp);
        }
    }
    break;

    case BK_GATTS_CONF_EVT:
    {
        struct gatts_conf_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_CONF_EVT %d %d %d", param->status, param->conn_id, param->handle);
    }
    break;

    case BK_GATTS_RESPONSE_EVT: //todo done: internal resp, don't report to app.
    {
        struct gatts_rsp_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_RESPONSE_EVT %d %d", param->status, param->handle);
    }
    break;

    case BK_GATTS_SEND_SERVICE_CHANGE_EVT:
    {
        struct gatts_send_service_change_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_SEND_SERVICE_CHANGE_EVT %02x:%02x:%02x:%02x:%02x:%02x %d %d",
                  param->remote_bda[5],
                  param->remote_bda[4],
                  param->remote_bda[3],
                  param->remote_bda[2],
                  param->remote_bda[1],
                  param->remote_bda[0],
                  param->status, param->conn_id);
    }
    break;

    case BK_GATTS_CONNECT_EVT:
    {
        struct gatts_connect_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_CONNECT_EVT %d role %d %02X:%02X:%02X:%02X:%02X:%02X", param->conn_id, param->link_role,
                  param->remote_bda[5],
                  param->remote_bda[4],
                  param->remote_bda[3],
                  param->remote_bda[2],
                  param->remote_bda[1],
                  param->remote_bda[0]);

        common_env_tmp = dm_ble_alloc_app_env_by_addr(param->remote_bda, sizeof(dm_gatts_app_env_t));

        if (!common_env_tmp)
        {
            gatt_loge("alloc app env err !!!!");
            break;
        }

        common_env_tmp->status = GAP_CONNECT_STATUS_CONNECTED;
        common_env_tmp->conn_id = param->conn_id;
        common_env_tmp->local_is_master = (param->link_role == 0 ? 1 : 0);

        s_conn_id = param->conn_id;
    }
    break;

    case BK_GATTS_DISCONNECT_EVT:
    {
        struct gatts_disconnect_evt_param *param = (typeof(param))comm_param;
        uint8_t timer_ref_count = 0;

        gatt_logi("BK_GATTS_DISCONNECT_EVT %02X:%02X:%02X:%02X:%02X:%02X",
                  param->remote_bda[5],
                  param->remote_bda[4],
                  param->remote_bda[3],
                  param->remote_bda[2],
                  param->remote_bda[1],
                  param->remote_bda[0]);

        common_env_tmp = dm_ble_find_app_env_by_addr(param->remote_bda);

        if (!common_env_tmp)
        {
            gatt_loge("not found addr !!!!");
            //break;
        }
        else
        {
            dm_ble_del_app_env_by_addr(param->remote_bda);
        }

        s_conn_id = 0xff;

        dm_ble_app_env_foreach(nest_func_check_timer_ref_count, &timer_ref_count);

        if (!timer_ref_count && rtos_is_timer_init(&s_char_notify_timer))
        {
            if (rtos_is_timer_running(&s_char_notify_timer))
            {
                rtos_stop_timer(&s_char_notify_timer);
            }

            rtos_deinit_timer(&s_char_notify_timer);
        }

#if 0
        bk_bd_addr_t nominal_addr = {0};
        uint8_t nominal_addr_type = 0;
        bk_bd_addr_t identity_addr = {0};
        uint8_t identity_addr_type = 0;

        if (1 && 0 == dm_gatt_get_authen_status(nominal_addr, &nominal_addr_type, identity_addr, &identity_addr_type))
        {
            // for bonded adv
            bk_ble_gap_ext_adv_params_t adv_param =
            {
                .type = BK_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
                .interval_min = 120,
                .interval_max = 160,
                .channel_map = BK_ADV_CHNL_ALL,
                .own_addr_type = BLE_ADDR_TYPE_RPA_RANDOM,
                .peer_addr_type = identity_addr_type,
                .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
                .primary_phy = BK_BLE_GAP_PRI_PHY_1M,
                .secondary_phy = BK_BLE_GAP_PHY_1M,
                .sid = 0,
                .scan_req_notif = 0,
            };

            adv_param.peer_addr_type = identity_addr_type;
            os_memcpy(adv_param.peer_addr, identity_addr, sizeof(identity_addr));

            ret = bk_ble_gap_set_adv_params(0, &adv_param);

            if (ret)
            {
                gatt_loge("bk_ble_gap_set_adv_params err %d", ret);
                break;
            }

            rtos_delay_milliseconds(100);
        }

#else
        dm_gatts_set_adv_param(s_dm_gatts_local_addr_is_public);
        rtos_delay_milliseconds(100);
#endif
        const bk_ble_gap_ext_adv_t ext_adv =
        {
            .instance = 0,
            .duration = 0,
            .max_events = 0,
        };

        ret = bk_ble_gap_adv_start(1, &ext_adv);

        if (ret)
        {
            gatt_loge("bk_ble_gap_adv_start err %d", ret);
        }

        rtos_delay_milliseconds(100);
    }
    break;

    case BK_GATTS_MTU_EVT:
    {
        struct gatts_mtu_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTS_MTU_EVT %d %d", param->conn_id, param->mtu);
    }
    break;

    default:
        break;
    }

    return ret;
}

int32_t dm_gatts_disconnect(uint8_t *addr)
{
    dm_gatt_app_env_t *common_env_tmp = NULL;
    int32_t err = 0;

    gatt_logi("0x%02x:%02x:%02x:%02x:%02x:%02x",
              addr[5],
              addr[4],
              addr[3],
              addr[2],
              addr[1],
              addr[0]);

    if (!s_gatts_if)
    {
        gatt_loge("gatts not init");

        return -1;
    }

    common_env_tmp = dm_ble_find_app_env_by_addr(addr);

    if (!common_env_tmp || !common_env_tmp->data)
    {
        gatt_loge("conn not found !!!!", common_env_tmp, common_env_tmp ? common_env_tmp->data : NULL);
        return -1;
    }

    if (common_env_tmp->status != GAP_CONNECT_STATUS_CONNECTED)
    {
        gatt_loge("connect status is not connected %d", common_env_tmp->status);
        return -1;
    }

    bk_bd_addr_t peer_addr;
    os_memcpy(peer_addr, addr, sizeof(peer_addr));

    err = bk_ble_gap_disconnect(peer_addr);

    if (err)
    {
        gatt_loge("disconnect fail %d", err);
    }
    else
    {
        common_env_tmp->status = GAP_CONNECT_STATUS_DISCONNECTING;
    }

    return err;
}

static int32_t dm_gatts_set_adv_param(uint8_t local_addr_is_public)
{
    int32_t ret = 0;
    bk_bd_addr_t nominal_addr = {0};
    uint8_t nominal_addr_type = 0;
    bk_bd_addr_t identity_addr = {0};
    uint8_t identity_addr_type = 0;
    bk_ble_gap_ext_adv_params_t adv_param = {0};

    adv_param = (bk_ble_gap_ext_adv_params_t)
    {
        .type = BK_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND,
        .interval_min = 120,
        .interval_max = 160,
        .channel_map = BK_ADV_CHNL_ALL,
        .filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
        .primary_phy = BK_BLE_GAP_PRI_PHY_1M,
        .secondary_phy = BK_BLE_GAP_PHY_1M,
        .sid = 0,
        .scan_req_notif = 0,
    };

    // attention: some device could only connect rpa adv after pair, some device is opposite, some device behavior depend on which adv is used when pairing.
    // so we need to decide if rpa should be used in adv.

    if (g_dm_gap_use_rpa && 0 == dm_gatt_get_authen_status(nominal_addr, &nominal_addr_type, identity_addr, &identity_addr_type))
    {
        adv_param.own_addr_type = (local_addr_is_public ? BLE_ADDR_TYPE_RPA_PUBLIC : BLE_ADDR_TYPE_RPA_RANDOM);

        adv_param.peer_addr_type = nominal_addr_type;
        os_memcpy(adv_param.peer_addr, nominal_addr, sizeof(nominal_addr));
    }
    else
    {
        adv_param.own_addr_type = (local_addr_is_public ? BLE_ADDR_TYPE_PUBLIC : BLE_ADDR_TYPE_RANDOM);
    }

    ret = bk_ble_gap_set_adv_params(ADV_HANDLE, &adv_param);

    if (ret)
    {
        gatt_loge("bk_ble_gap_set_adv_params err %d", ret);
        goto error;
    }

    return 0;

error:;

    gatt_loge("err");
    return -1;
}

int32_t dm_gatts_start_adv(void)
{
    int32_t ret = 0;

    if (!s_gatts_if)
    {
        gatt_loge("gatts not init");

        return -1;
    }

    const bk_ble_gap_ext_adv_t ext_adv =
    {
        .instance = 0,
        .duration = 0,
        .max_events = 0,
    };

    ret = bk_ble_gap_adv_start(1, &ext_adv);

    if (ret)
    {
        gatt_loge("bk_ble_gap_adv_start err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv enable err %d", ret);
        goto error;
    }

    return 0;
error:;
    gatt_loge("err");
    return -1;
}

int32_t dm_gatts_enable_service(uint32_t index, uint8_t enable)
{
    int32_t ret = 0;
    uint16_t handle = (index == 0 ? s_service_attr_handle : s_attr_handle_list2[0]);

    if (!s_gatts_if)
    {
        gatt_loge("gatts not init");

        return -1;
    }

    if (enable)
    {
        bk_ble_gatts_start_service(handle);
    }
    else
    {
        bk_ble_gatts_stop_service(handle);
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait start/stop service err %d", ret);
        return -1;
    }

    bk_ble_gatts_send_service_change_indicate(s_gatts_if, 0, 1);

    return 0;
}

int dm_gatts_main(cli_gatt_param_t *param)
{
    ble_err_t ret = 0;

    ret = rtos_init_semaphore(&s_ble_sema, 1);

    if (ret != 0)
    {
        gatt_loge("rtos_init_semaphore err %d", ret);
        return -1;
    }

    if (param)
    {
        if (param->p_pa)
        {
            s_dm_gatts_local_addr_is_public = *param->p_pa;
        }
    }

    bk_ble_gatts_register_callback(bk_gatts_cb);

    ret = bk_ble_gatts_app_register(0);

    if (ret)
    {
        gatt_loge("reg err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore reg err %d", ret);
        return -1;
    }

#if DYNAMIC_ADD_ATTR

    bk_bt_uuid_t uuid;
    const bk_gatt_perm_t perm = BK_GATT_PERM_READ | BK_GATT_PERM_WRITE;
    bk_attr_value_t value;
    bk_attr_control_t ctrl;
    const bk_gatt_char_prop_t prop = BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR | BK_GATT_CHAR_PROP_BIT_WRITE
                                     | BK_GATT_CHAR_PROP_BIT_NOTIFY;

    bk_gatt_srvc_id_t srvc_id;

    memset(&srvc_id, 0, sizeof(srvc_id));
    srvc_id.is_primary = 1;
    srvc_id.id.uuid.len = BK_UUID_LEN_16;
    srvc_id.id.uuid.uuid.uuid16 = 0x1234;

    ret = bk_ble_gatts_create_service(0, &srvc_id, 30);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_create_service err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }


    memset(&uuid, 0, sizeof(uuid));
    memset(&value, 0, sizeof(value));
    memset(&ctrl, 0, sizeof(ctrl));

    uuid.len = BK_UUID_LEN_16;
    uuid.uuid.uuid16 = 0x5678;
    value.attr_value = s_char_buff;
    value.attr_len = value.attr_max_len = sizeof(s_char_buff);
    ctrl.auto_rsp = 0;

    ret = bk_ble_gatts_add_char(s_service_attr_handle, &uuid, perm, prop, &value, &ctrl);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_add_char err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    {
        uint16_t tmp_len = 0;
        uint8_t *tmp_buff = NULL;
        bk_gatt_status_t g_status = 0;

        g_status = bk_ble_gatts_get_attr_value(s_char_attr_handle, &tmp_len, &tmp_buff);

        if (g_status)
        {
            gatt_loge("get attr value err %d", g_status);
        }

        if (tmp_len != value.attr_len || tmp_buff != value.attr_value)
        {
            gatt_loge("%d %d get attr value not match create %d %d %p %p!!!!", __LINE__, g_status,
                      tmp_len, value.attr_len, tmp_buff, value.attr_value);
        }
    }

    memset(&uuid, 0, sizeof(uuid));
    memset(&value, 0, sizeof(value));
    memset(&ctrl, 0, sizeof(ctrl));

    uuid.len = BK_UUID_LEN_16;
    uuid.uuid.uuid16 = BK_GATT_UUID_CHAR_CLIENT_CONFIG;
    value.attr_value = (uint8_t *)&s_char_desc_buff;
    value.attr_len = value.attr_max_len = sizeof(s_char_desc_buff);
    ctrl.auto_rsp = 0;

    ret = bk_ble_gatts_add_char_descr(s_service_attr_handle, s_char_attr_handle, &uuid, perm, &value, &ctrl);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_add_char_descr err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    {
        uint16_t tmp_len = 0;
        uint8_t *tmp_buff = NULL;
        bk_gatt_status_t g_status = 0;

        g_status = bk_ble_gatts_get_attr_value(s_char_desc_attr_handle, &tmp_len, &tmp_buff);

        if (g_status)
        {
            gatt_loge("get attr value err %d", g_status);
        }

        if (tmp_len != value.attr_len || tmp_buff != value.attr_value)
        {
            gatt_loge("%d %d get attr value not match create %d %d %p %p!!!!", __LINE__, g_status,
                      tmp_len, value.attr_len, tmp_buff, value.attr_value);
        }
    }

    memset(&uuid, 0, sizeof(uuid));
    memset(&value, 0, sizeof(value));
    memset(&ctrl, 0, sizeof(ctrl));

    uuid.len = BK_UUID_LEN_16;
    uuid.uuid.uuid16 = 0x9abc;
    value.attr_value = s_char2_buff;
    value.attr_len = value.attr_max_len = sizeof(s_char2_buff);
    ctrl.auto_rsp = 0;

    ret = bk_ble_gatts_add_char(s_service_attr_handle, &uuid, perm, prop, &value, &ctrl);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_add_char 2 err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    {
        uint16_t tmp_len = 0;
        uint8_t *tmp_buff = NULL;
        bk_gatt_status_t g_status = 0;

        g_status = bk_ble_gatts_get_attr_value(s_char2_attr_handle, &tmp_len, &tmp_buff);

        if (g_status)
        {
            gatt_loge("get attr value err %d", g_status);
        }

        if (tmp_len != value.attr_len || tmp_buff != value.attr_value)
        {
            gatt_loge("%d %d get attr value not match create %d %d %p %p!!!!", __LINE__, g_status,
                      tmp_len, value.attr_len, tmp_buff, value.attr_value);
        }
    }

    memset(&uuid, 0, sizeof(uuid));
    memset(&value, 0, sizeof(value));
    memset(&ctrl, 0, sizeof(ctrl));

    uuid.len = BK_UUID_LEN_16;
    uuid.uuid.uuid16 = BK_GATT_UUID_CHAR_SRVR_CONFIG;
    value.attr_value = (uint8_t *)&s_char2_desc_buff;
    value.attr_len = value.attr_max_len = sizeof(s_char2_desc_buff);
    ctrl.auto_rsp = 0;

    ret = bk_ble_gatts_add_char_descr(s_service_attr_handle, s_char2_attr_handle, &uuid, perm, &value, &ctrl);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_add_char_descr 2 err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }


    {
        uint16_t tmp_len = 0;
        uint8_t *tmp_buff = NULL;
        bk_gatt_status_t g_status = 0;

        g_status = bk_ble_gatts_get_attr_value(s_char2_desc_attr_handle, &tmp_len, &tmp_buff);

        if (g_status)
        {
            gatt_loge("get attr value err %d", g_status);
        }

        if (tmp_len != value.attr_len || tmp_buff != value.attr_value)
        {
            gatt_loge("%d %d get attr value not match create %d %d %p %p!!!!", __LINE__, g_status,
                      tmp_len, value.attr_len, tmp_buff, value.attr_value);
        }
    }


    memset(&uuid, 0, sizeof(uuid));
    memset(&value, 0, sizeof(value));
    memset(&ctrl, 0, sizeof(ctrl));

    uuid.len = BK_UUID_LEN_16;
    uuid.uuid.uuid16 = 0x15ab;
    value.attr_value = s_char_auto_rsp_buff;
    value.attr_len = value.attr_max_len = sizeof(s_char_auto_rsp_buff);
    ctrl.auto_rsp = 1;

    ret = bk_ble_gatts_add_char(s_service_attr_handle, &uuid, perm, prop, &value, &ctrl);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_add_char auto err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    {
        uint16_t tmp_len = 0;
        uint8_t *tmp_buff = NULL;
        bk_gatt_status_t g_status = 0;

        g_status = bk_ble_gatts_get_attr_value(s_char_auto_rsp_attr_handle, &tmp_len, &tmp_buff);

        if (g_status)
        {
            gatt_loge("get attr value err %d", g_status);
        }

        if (tmp_len != value.attr_len || tmp_buff != value.attr_value)
        {
            gatt_loge("%d %d get attr value not match create %d %d %p %p!!!!", __LINE__, g_status,
                      tmp_len, value.attr_len, tmp_buff, value.attr_value);
        }
    }


#else

    ret = bk_ble_gatts_create_attr_tab(s_gatts_attr_db_service_1, s_gatts_if, sizeof(s_gatts_attr_db_service_1) / sizeof(s_gatts_attr_db_service_1[0]), 30);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_create_attr_tab err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    ret = bk_ble_gatts_create_attr_tab(s_gatts_attr_db_service_2, s_gatts_if, sizeof(s_gatts_attr_db_service_2) / sizeof(s_gatts_attr_db_service_2[0]), 30);

    if (ret != 0)
    {
        gatt_loge("bk_ble_gatts_create_attr_tab 2 err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

#endif

    bk_ble_gatts_start_service(s_service_attr_handle);

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    bk_ble_gatts_start_service(s_attr_handle_list2[0]);

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("rtos_get_semaphore err %d", ret);
        return -1;
    }

    bk_bd_addr_t current_addr = {0}, identity_addr = {0};
    char adv_name[64] = {0};

    dm_ble_gap_get_identity_addr(identity_addr);

    os_memcpy(current_addr, identity_addr, sizeof(identity_addr));

    dm_gatt_add_gap_callback(dm_ble_gap_cb);

    snprintf((char *)(adv_name), sizeof(adv_name) - 1, "CENTRAL-%02X%02X%02X", identity_addr[2], identity_addr[1], identity_addr[0]);

    gatt_logi("adv name %s", adv_name);

    ret = bk_ble_gap_set_device_name(adv_name);

    if (ret)
    {
        gatt_loge("bk_ble_gap_set_device_name err %d", ret);
        goto error;
    }

    ret = dm_gatts_set_adv_param(s_dm_gatts_local_addr_is_public);

    if (ret != kNoErr)
    {
        gatt_loge("set adv param err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv param err %d", ret);
        goto error;
    }

    uint8_t need_set_random_addr = 0;

    if (g_dm_gap_use_rpa && dm_ble_gap_get_rpa(current_addr) == 0)
    {
        gatt_logw("set adv random addr with generate rpa");
        need_set_random_addr = 1;
    }
    else if (!s_dm_gatts_local_addr_is_public)
    {
        gatt_logw("set adv random addr with user define");

        current_addr[0]++;
        current_addr[5] |= 0xc0; // static random addr[47:46] must be 0b11 in msb !!!

        need_set_random_addr = 1;
    }

    if (need_set_random_addr)
    {
        ret = bk_ble_gap_set_adv_rand_addr(ADV_HANDLE, current_addr);

        if (ret)
        {
            gatt_loge("bk_ble_gap_set_adv_rand_addr err %d", ret);
            goto error;
        }

        ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

        if (ret != kNoErr)
        {
            gatt_loge("wait set adv rand addr err %d", ret);
            goto error;
        }
    }

    bk_ble_adv_data_t adv_data =
    {
        .set_scan_rsp = 0,
        .include_name = 1,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = 0x06,
    };

    ret = bk_ble_gap_set_adv_data((bk_ble_adv_data_t *)&adv_data);

    if (ret)
    {
        gatt_loge("bk_ble_gap_set_adv_data err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv data err %d", ret);
        goto error;
    }

    adv_data.set_scan_rsp = 1;

    ret = bk_ble_gap_set_adv_data((bk_ble_adv_data_t *)&adv_data);

    if (ret)
    {
        gatt_loge("bk_ble_gap_set_adv_data scan rsp err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv data scan rsp err %d", ret);
        goto error;
    }

    const bk_ble_gap_ext_adv_t ext_adv =
    {
        .instance = 0,
        .duration = 0,
        .max_events = 0,
    };

    ret = bk_ble_gap_adv_start(1, &ext_adv);

    if (ret)
    {
        gatt_loge("bk_ble_gap_adv_start err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv enable err %d", ret);
        goto error;
    }

error:
    return 0;
}
