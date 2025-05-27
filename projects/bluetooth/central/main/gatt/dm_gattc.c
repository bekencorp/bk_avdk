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
#include "dm_gattc.h"
#include "dm_gatt_connection.h"

#define DYNAMIC_ADD_ATTR 0
//#define SPECIAL_HANDLE 4 //xunfei

#define SPECIAL_HANDLE 15 //7256 server


#define INVALID_ATTR_HANDLE 0
#define MIN_VALUE(x, y) (((x) < (y)) ? (x): (y))

#define AUTO_ENABLE_NOTIFY 1

enum
{
    GATTC_STATUS_IDLE,
    GATTC_STATUS_DISCOVERY_SELF,
    GATTC_STATUS_READ_BY_TYPE,
    GATTC_STATUS_READ_CHAR,
    GATTC_STATUS_READ_CHAR_DESC,
    GATTC_STATUS_READ_MULTI,
    GATTC_STATUS_WRITE_DESC_NEED_RSP,
    GATTC_STATUS_WRITE_DESC_NO_RSP,
    GATTC_STATUS_PREP_WRITE_STEP_1,
    GATTC_STATUS_PREP_WRITE_STEP_2,
    GATTC_STATUS_WRITE_EXEC,
    GATTC_STATUS_WRITE_READ_SAMETIME,
};


//typedef struct
//{
//    uint8_t job_status; //see GATTC_STATUS_IDLE
//    uint8_t noti_indica_switch;
//    uint8_t noti_indicate_recv_count;
//} dm_gattc_app_env_t;

#define dm_gattc_app_env_t dm_gatt_demo_app_env_t

//static dm_gattc_app_env_t s_dm_gattc_app_env_array[GATT_MAX_CONNECTION_COUNT];

static bk_gatt_if_t s_gattc_if;
static beken_semaphore_t s_ble_sema = NULL;

static uint8_t s_dm_gattc_local_addr_is_public = 0;

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

    default:
        return DM_BLE_GAP_APP_CB_RET_NO_INTERESTING;
        break;
    }

    return DM_BLE_GAP_APP_CB_RET_PROCESSED;
}

static int32_t bk_gattc_cb (bk_gattc_cb_event_t event, bk_gatt_if_t gattc_if, bk_ble_gattc_cb_param_t *comm_param)
{
    ble_err_t ret = 0;
    bk_gatt_auth_req_t auth_req = BK_GATT_AUTH_REQ_NONE;
    dm_gattc_app_env_t *app_env_tmp = NULL;
    dm_gatt_app_env_t *common_env_tmp = NULL;

    const uint16_t client_config_noti_enable = 1, client_config_indic_enable = 2, client_config_all_disable = 0;

    switch (event)
    {
    case BK_GATTC_REG_EVT:
    {
        struct gattc_reg_evt_param *param = (typeof(param))comm_param;

        s_gattc_if = param->gatt_if;
        gatt_logi("reg ret gatt_if %d", s_gattc_if);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }

    break;

    case BK_GATTC_DIS_SRVC_CMPL_EVT:
    {
        struct gattc_dis_srvc_cmpl_evt_param *param = (typeof(param))comm_param;

        bk_bt_uuid_t uuid = {BK_UUID_LEN_16, {BK_GATT_UUID_CHAR_DECLARE}};

        gatt_logi("BK_GATTC_DIS_SRVC_CMPL_EVT %d %d", param->status, param->conn_id);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        gatt_logi("job_status %d", app_env_tmp->job_status);

        if (app_env_tmp->job_status == GATTC_STATUS_IDLE)
        {
            if (0 != bk_ble_gattc_discover(s_gattc_if, param->conn_id, auth_req))
            {
                gatt_loge("bk_ble_gattc_discover err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_DISCOVERY_SELF;
            }
        }
        else if (app_env_tmp->job_status == GATTC_STATUS_DISCOVERY_SELF)
        {
            if (0 != bk_ble_gattc_read_by_type(s_gattc_if, param->conn_id, 1, 98, &uuid, auth_req))
            {
                gatt_loge("bk_ble_gattc_read_by_type err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_READ_BY_TYPE;
            }
        }
    }
    break;

    case BK_GATTC_DIS_RES_SERVICE_EVT:
    {
        struct gattc_dis_res_service_evt_param *param = (typeof(param))comm_param;
        uint16_t short_uuid = 0;

        gatt_logi("BK_GATTC_DIS_RES_SERVICE_EVT count %d", param->count);

        for (int i = 0; i < param->count; ++i)
        {
            switch (param->array[i].srvc_id.uuid.len)
            {
            case BK_UUID_LEN_16:
            {
                short_uuid = param->array[i].srvc_id.uuid.uuid.uuid16;
            }
            break;

            case BK_UUID_LEN_128:
            {
                memcpy(&short_uuid, &param->array[i].srvc_id.uuid.uuid.uuid128[BK_UUID_LEN_128 - 4], sizeof(short_uuid));
            }
            break;
            }

            gatt_logi("0x%04x %d~%d", short_uuid, param->array[i].start_handle, param->array[i].end_handle);

            common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

            if (!common_env_tmp || !common_env_tmp->data)
            {
                gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
                break;
            }

            app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

            if (INTERESTING_SERIVCE_UUID == short_uuid)
            {
                app_env_tmp->peer_interest_service_start_handle = param->array[i].start_handle;
                app_env_tmp->peer_interest_service_end_handle = param->array[i].end_handle;

                gatt_logi("interesting service 0x%04x %d~%d", short_uuid, param->array[i].start_handle, param->array[i].end_handle);
            }

            if (BK_GATT_UUID_GAP_SVC == short_uuid)
            {
                app_env_tmp->peer_gap_service_start_handle = param->array[i].start_handle;
                app_env_tmp->peer_gap_service_end_handle = param->array[i].end_handle;

                gatt_logi("gap service %d~%d", param->array[i].start_handle, param->array[i].end_handle);
            }

            if (BK_GATT_UUID_GATT_SVC == short_uuid)
            {
                gatt_logi("gatt service %d~%d", param->array[i].start_handle, param->array[i].end_handle);
            }
        }
    }
    break;

    case BK_GATTC_DIS_RES_CHAR_EVT:
    {
        struct gattc_dis_res_char_evt_param *param = (typeof(param))comm_param;
        uint16_t short_uuid = 0;

        gatt_logi("BK_GATTC_DIS_RES_CHAR_EVT count %d", param->count);

        for (int i = 0; i < param->count; ++i)
        {
            switch (param->array[i].uuid.uuid.len)
            {
            case BK_UUID_LEN_16:
            {
                short_uuid = param->array[i].uuid.uuid.uuid.uuid16;
            }
            break;

            case BK_UUID_LEN_128:
            {
                memcpy(&short_uuid, &param->array[i].uuid.uuid.uuid.uuid128[BK_UUID_LEN_128 - 4], sizeof(short_uuid));
            }
            break;
            }

            gatt_logi("0x%04x %d~%d char_value_handle %d", short_uuid, param->array[i].start_handle, param->array[i].end_handle, param->array[i].char_value_handle);

            common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

            if (!common_env_tmp || !common_env_tmp->data)
            {
                gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
                break;
            }

            app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

            if ( INTERESTING_CHAR_UUID == short_uuid &&
                    app_env_tmp->peer_interest_service_start_handle <= param->array[i].char_value_handle &&
                    app_env_tmp->peer_interest_service_end_handle >= param->array[i].char_value_handle
               )
            {
                app_env_tmp->peer_interest_char_handle = param->array[i].char_value_handle;

                gatt_logi("interesting char 0x%04x %d", short_uuid, param->array[i].char_value_handle);
            }

            if ( app_env_tmp->peer_gap_service_start_handle <= param->array[i].char_value_handle &&
                    app_env_tmp->peer_gap_service_end_handle >= param->array[i].char_value_handle
               )
            {
                switch (short_uuid)
                {
                case BK_GATT_UUID_GAP_DEVICE_NAME:
                    gatt_logi("device name value attr handle 0x%04x", param->array[i].char_value_handle);
                    break;

                case BK_GATT_UUID_GAP_CENTRAL_ADDR_RESOL:
                    gatt_logi("central addr resolvable attr handle 0x%04x", param->array[i].char_value_handle);
                    break;

                case BK_GATT_UUID_GAP_RESOLV_RPVIATE_ADDR_ONLY:
                    gatt_logi("peer resolve RPA only !!!");
                    break;

                default:
                    break;
                }
            }
        }
    }
    break;

    case BK_GATTC_DIS_RES_CHAR_DESC_EVT:
    {
        struct gattc_dis_res_char_desc_evt_param *param = (typeof(param))comm_param;
        uint16_t short_uuid = 0;

        gatt_logi("BK_GATTC_DIS_RES_CHAR_DESC_EVT count %d", param->count);

        for (int i = 0; i < param->count; ++i)
        {
            switch (param->array[i].uuid.uuid.len)
            {
            case BK_UUID_LEN_16:
            {
                short_uuid = param->array[i].uuid.uuid.uuid.uuid16;
            }
            break;

            case BK_UUID_LEN_128:
            {
                memcpy(&short_uuid, &param->array[i].uuid.uuid.uuid.uuid128[BK_UUID_LEN_128 - 4], sizeof(short_uuid));
            }
            break;

            default:
                gatt_loge("unknow uuid len %d", param->array[i].uuid.uuid.len);
                break;
            }

            gatt_logi("0x%04x char_handle %d desc_handle %d", short_uuid, param->array[i].char_handle, param->array[i].desc_handle);

            common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

            if (!common_env_tmp || !common_env_tmp->data)
            {
                gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
                break;
            }

            app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

            if (BK_GATT_UUID_CHAR_CLIENT_CONFIG == short_uuid &&
                    app_env_tmp->peer_interest_char_handle == param->array[i].char_handle)
            {
                gatt_logi("interesting char desc 0x%04x %d", short_uuid, param->array[i].desc_handle);

#if AUTO_ENABLE_NOTIFY
                app_env_tmp->peer_interest_char_desc_handle = param->array[i].desc_handle;
#endif
            }
        }
    }
    break;

    case BK_GATTC_READ_CHAR_EVT:
    {
        struct gattc_read_char_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_READ_CHAR_EVT 0x%x %d %d", param->status, param->handle, param->value_len);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (!param->status && app_env_tmp->job_status == GATTC_STATUS_READ_CHAR)
        {
            if (0 != bk_ble_gattc_read_char_descr(s_gattc_if, param->conn_id, SPECIAL_HANDLE, auth_req))
            {
                gatt_loge("bk_ble_gattc_read_char_descr err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_READ_CHAR_DESC;
            }
        }
    }
    break;

    case BK_GATTC_READ_DESCR_EVT:
    {
        struct gattc_read_char_evt_param *param = (typeof(param))comm_param;
        bk_gattc_multi_t multi;

        gatt_logi("BK_GATTC_READ_DESCR_EVT %x %d %d", param->status, param->handle, param->value_len);

        memset(&multi, 0, sizeof(multi));

        multi.num_attr = 5;
        multi.handles[0] = 3;
        multi.handles[1] = 5;
        multi.handles[2] = 9;
        multi.handles[3] = 13;
        multi.handles[4] = 16;

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_READ_CHAR_DESC)
        {
            if (0 != bk_ble_gattc_read_multiple(s_gattc_if, param->conn_id, &multi, auth_req))
            {
                gatt_loge("bk_ble_gattc_read_multiple err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_READ_MULTI;
            }
        }
    }
    break;

    case BK_GATTC_READ_BY_TYPE_EVT:
    {
        struct gattc_read_by_type_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_READ_BY_TYPE_EVT %d %d %d", param->status, param->conn_id, param->elem_count);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_READ_BY_TYPE)
        {
            if (0 != bk_ble_gattc_read_char(s_gattc_if, param->conn_id, 3, auth_req))
            {
                gatt_loge("bk_ble_gattc_read_char err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_READ_CHAR;
            }
        }
    }
    break;

    case BK_GATTC_READ_MULTIPLE_EVT:
    {
        struct gattc_read_char_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_READ_MULTIPLE_EVT %x %d %d", param->status, param->handle, param->value_len);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_READ_MULTI)
        {
            if (0 != bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, app_env_tmp->peer_interest_char_desc_handle, sizeof(client_config_noti_enable), (uint8_t *)&client_config_noti_enable, BK_GATT_WRITE_TYPE_RSP, auth_req))
            {
                gatt_loge("bk_ble_gattc_write_char_descr err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_WRITE_DESC_NEED_RSP;
            }
        }
    }
    break;

    case BK_GATTC_WRITE_CHAR_EVT:
    {
        struct gattc_write_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_WRITE_CHAR_EVT %d %d %d %d", param->status, param->conn_id, param->handle, param->offset);

        if (param->status == BK_GATT_INSUF_AUTHENTICATION)
        {
            //we need create bond
            gatt_logw("status insufficient authentication, need bond !!!");
        }
    }
    break;

    case BK_GATTC_WRITE_DESCR_EVT:
    {
        struct gattc_write_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_WRITE_DESCR_EVT %d %d %d %d", param->status, param->conn_id, param->handle, param->offset);

        if (param->status == BK_GATT_INSUF_AUTHENTICATION)
        {
            //we need create bond
            gatt_logw("status insufficient authentication, need bond !!!");
        }

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_WRITE_DESC_NEED_RSP)
        {
            if (0 != bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, app_env_tmp->peer_interest_char_desc_handle, sizeof(client_config_all_disable), (uint8_t *)&client_config_all_disable, BK_GATT_WRITE_TYPE_NO_RSP, auth_req))
            {
                gatt_loge("bk_ble_gattc_write_char_descr err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_WRITE_DESC_NO_RSP;
            }
        }
        else if (app_env_tmp->job_status == GATTC_STATUS_WRITE_DESC_NO_RSP)
        {
            uint8_t buff = 1;

            if (0 != bk_ble_gattc_prepare_write(s_gattc_if, param->conn_id, SPECIAL_HANDLE, 0, sizeof(buff), (uint8_t *)&buff, auth_req))
            {
                gatt_loge("bk_ble_gattc_prepare_write err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_PREP_WRITE_STEP_1;
            }
        }
    }
    break;

    case BK_GATTC_PREP_WRITE_EVT:
    {
        struct gattc_write_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_PREP_WRITE_EVT %d %d %d %d", param->status, param->conn_id, param->handle, param->offset);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_PREP_WRITE_STEP_1)
        {
            uint8_t buff = 0;

            if (0 != bk_ble_gattc_prepare_write(s_gattc_if, param->conn_id, SPECIAL_HANDLE, 1, /*400,*/sizeof(buff), (uint8_t *)&buff, auth_req))
            {
                gatt_loge("bk_ble_gattc_prepare_write err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_PREP_WRITE_STEP_2;
            }
        }
        else if (app_env_tmp->job_status == GATTC_STATUS_PREP_WRITE_STEP_2)
        {
            if (0 != bk_ble_gattc_execute_write(s_gattc_if, param->conn_id, 1))
            {
                gatt_loge("bk_ble_gattc_execute_write err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_WRITE_EXEC;
            }
        }
    }
    break;

    case BK_GATTC_EXEC_EVT:
    {
        struct gattc_exec_cmpl_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_EXEC_EVT %d %d", param->status, param->conn_id);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_WRITE_EXEC)
        {
            //consecutive write read test
            if (0 != bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, SPECIAL_HANDLE, sizeof(client_config_noti_enable), (uint8_t *)&client_config_noti_enable, BK_GATT_WRITE_TYPE_RSP, auth_req))
            {
                gatt_loge("bk_ble_gattc_write_char_descr 1 err");
            }
            else if (0 != bk_ble_gattc_read_char_descr(s_gattc_if, param->conn_id, SPECIAL_HANDLE, auth_req))
            {
                gatt_loge("bk_ble_gattc_read_char_descr 2 err");
            }
            else if (0 != bk_ble_gattc_read_char_descr(s_gattc_if, param->conn_id, SPECIAL_HANDLE, auth_req))
            {
                gatt_loge("bk_ble_gattc_read_char_descr 3 err");
            }
            else if (0 != bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, SPECIAL_HANDLE, sizeof(client_config_all_disable), (uint8_t *)&client_config_all_disable, BK_GATT_WRITE_TYPE_RSP, auth_req))
            {
                gatt_loge("bk_ble_gattc_write_char_descr 4 err");
            }
            else if (0 != bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, SPECIAL_HANDLE, sizeof(client_config_noti_enable), (uint8_t *)&client_config_noti_enable, BK_GATT_WRITE_TYPE_NO_RSP, auth_req))
            {
                gatt_loge("bk_ble_gattc_write_char_descr 5 err");
            }
            else
            {
                app_env_tmp->job_status = GATTC_STATUS_WRITE_READ_SAMETIME;
            }
        }
    }
    break;

    case BK_GATTC_NOTIFY_EVT:
    {
        struct gattc_notify_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_NOTIFY_EVT %d %d handle %d vallen %d %02X:%02X:%02X:%02X:%02X:%02X", param->conn_id,
                  param->is_notify,
                  param->handle,
                  param->value_len,
                  param->remote_bda[5],
                  param->remote_bda[4],
                  param->remote_bda[3],
                  param->remote_bda[2],
                  param->remote_bda[1],
                  param->remote_bda[0]);

        common_env_tmp = dm_ble_find_app_env_by_conn_id(param->conn_id);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("conn_id %d not found %d %p", param->conn_id, common_env_tmp->data);
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->data;

        if (app_env_tmp->job_status == GATTC_STATUS_WRITE_READ_SAMETIME && app_env_tmp->noti_indicate_recv_count++ >= 3)
        {
            app_env_tmp->noti_indicate_recv_count = 0;

            if (app_env_tmp->noti_indica_switch)
            {
                ret = bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, app_env_tmp->peer_interest_char_desc_handle, sizeof(client_config_noti_enable), (uint8_t *)&client_config_noti_enable, BK_GATT_WRITE_TYPE_RSP, auth_req);
            }
            else
            {
                ret = bk_ble_gattc_write_char_descr(s_gattc_if, param->conn_id, app_env_tmp->peer_interest_char_desc_handle, sizeof(client_config_indic_enable), (uint8_t *)&client_config_indic_enable, BK_GATT_WRITE_TYPE_RSP, auth_req);
            }

            app_env_tmp->noti_indica_switch = (app_env_tmp->noti_indica_switch + 1) % 2;

            if (ret)
            {
                gatt_loge("bk_ble_gattc_write_char_descr switch err");
            }
        }
    }
    break;

    case BK_GATTC_CFG_MTU_EVT:
    {
        struct gattc_cfg_mtu_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_CFG_MTU_EVT status %d %d %d", param->status, param->conn_id, param->mtu);

        if (0 != bk_ble_gattc_discover(s_gattc_if, param->conn_id, auth_req))
        {
            gatt_loge("bk_ble_gattc_discover err");
        }
    }
    break;

    case BK_GATTC_CONNECT_EVT:
    {
        struct gattc_connect_evt_param *param = (typeof(param))comm_param;
        uint16_t hci_handle = 0;
        ble_err_t ret = bk_ble_get_hci_handle_from_gatt_conn_id(param->conn_id, &hci_handle);

        gatt_logi("BK_GATTC_CONNECT_EVT role %d %02X:%02X:%02X:%02X:%02X:%02X conn_id %d hci_handle 0x%x",
                  param->conn_id,
                  param->link_role,
                  param->remote_bda[5],
                  param->remote_bda[4],
                  param->remote_bda[3],
                  param->remote_bda[2],
                  param->remote_bda[1],
                  param->remote_bda[0],
                  param->conn_id,
                  (!ret ? hci_handle : 0xffff)
                 );

        common_env_tmp = dm_ble_find_app_env_by_addr(param->remote_bda);

        if (!common_env_tmp)
        {
            gatt_logw("not found addr, alloc it !!!!");
            common_env_tmp = dm_ble_alloc_app_env_by_addr(param->remote_bda, sizeof(dm_gattc_app_env_t));

            if (!common_env_tmp || !common_env_tmp->data)
            {
                gatt_loge("conn max %p %p !!!!", common_env_tmp, common_env_tmp ? common_env_tmp->data : NULL);
                break;
            }

            if (common_env_tmp->status != GAP_CONNECT_STATUS_IDLE)
            {
                gatt_loge("connect status is not idle %d", common_env_tmp->status);
                break;
            }
        }

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("not found addr or data %p %p !!!!", common_env_tmp, common_env_tmp ? common_env_tmp->data : NULL);
            break;
        }

        common_env_tmp->status = GAP_CONNECT_STATUS_CONNECTED;
        common_env_tmp->conn_id = param->conn_id;
        common_env_tmp->local_is_master = (param->link_role == 0 ? 1 : 0);

        gatt_logi("local is master %d", common_env_tmp->local_is_master);

        if (common_env_tmp->local_is_master)
        {
            // only do mtu req when local is master
            if (0 != bk_ble_gattc_send_mtu_req(s_gattc_if, common_env_tmp->conn_id))
            {
                gatt_loge("bk_ble_gattc_send_mtu_req err");
            }
        }
        else
        {
            if (0 != bk_ble_gattc_discover(s_gattc_if, param->conn_id, auth_req))
            {
                gatt_loge("bk_ble_gattc_discover err");
            }
        }
    }
    break;

    case BK_GATTC_DISCONNECT_EVT:
    {
        struct gattc_disconnect_evt_param *param = (typeof(param))comm_param;

        gatt_logi("BK_GATTC_DISCONNECT_EVT %02X:%02X:%02X:%02X:%02X:%02X conn_id %d",
                  param->remote_bda[5],
                  param->remote_bda[4],
                  param->remote_bda[3],
                  param->remote_bda[2],
                  param->remote_bda[1],
                  param->remote_bda[0],
                  param->conn_id
                 );

        common_env_tmp = dm_ble_find_app_env_by_addr(param->remote_bda);

        if (!common_env_tmp || !common_env_tmp->data)
        {
            gatt_loge("not found addr or data %p %p !!!!", common_env_tmp, common_env_tmp ? common_env_tmp->data : NULL);
            break;
        }

        dm_ble_del_app_env_by_addr(param->remote_bda);
    }
    break;

    default:
        break;
    }

    return ret;
}

int32_t dm_gattc_connect(uint8_t *addr, uint32_t addr_type)
{
    dm_gatt_app_env_t *common_env_tmp = NULL;
    int32_t err = 0;

    gatt_logi("0x%02x:%02x:%02x:%02x:%02x:%02x %d",
              addr[5],
              addr[4],
              addr[3],
              addr[2],
              addr[1],
              addr[0],
              addr_type);

    if (!s_gattc_if)
    {
        gatt_loge("gattc not init");

        return -1;
    }

    common_env_tmp = dm_ble_alloc_app_env_by_addr(addr, sizeof(dm_gattc_app_env_t));

    if (!common_env_tmp || !common_env_tmp->data)
    {
        gatt_loge("conn max %p %p !!!!", common_env_tmp, common_env_tmp ? common_env_tmp->data : NULL);
        return -1;
    }

    if (common_env_tmp->status != GAP_CONNECT_STATUS_IDLE)
    {
        gatt_loge("connect status is not idle %d", common_env_tmp->status);
        return -1;
    }

    bk_gap_create_conn_params_t param = {0};
    bk_bd_addr_t peer_id_addr = {0};
    bk_ble_addr_type_t peer_id_addr_type = BLE_ADDR_TYPE_PUBLIC;

    param.scan_interval = 800;
    param.scan_window = param.scan_interval / 2;
    param.initiator_filter_policy = 0;

    /* attention: some device could only send rpa adv after pair, some device is opposite.
     * so we need to decide if rpa should be used in connection.
     */

    if (g_dm_gap_use_rpa && 0 == dm_gatt_find_id_info_by_nominal_info(addr, addr_type, peer_id_addr, &peer_id_addr_type))
    {
        gatt_logi("local use rpa");
        param.local_addr_type = (s_dm_gattc_local_addr_is_public ? BLE_ADDR_TYPE_RPA_PUBLIC : BLE_ADDR_TYPE_RPA_RANDOM);
        os_memcpy(param.peer_addr, addr, sizeof(param.peer_addr));
        param.peer_addr_type = addr_type;
    }
    else
    {
        param.local_addr_type = (s_dm_gattc_local_addr_is_public ? BLE_ADDR_TYPE_PUBLIC : BLE_ADDR_TYPE_RANDOM);
        os_memcpy(param.peer_addr, addr, sizeof(param.peer_addr));
        param.peer_addr_type = addr_type;
    }

    param.conn_interval_min = 0x20;
    param.conn_interval_max = 0x20;
    param.conn_latency = 0;
    param.supervision_timeout = 500;
    param.min_ce = 0;
    param.max_ce = 0;

    err = bk_ble_gap_connect(&param);

    if (err)
    {
        gatt_loge("connect fail %d", err);
    }
    else
    {
        os_memcpy(common_env_tmp->addr, addr, sizeof(common_env_tmp->addr));
        common_env_tmp->status = GAP_CONNECT_STATUS_CONNECTING;
    }

    return err;
}

int32_t dm_gattc_disconnect(uint8_t *addr)
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

    if (!s_gattc_if)
    {
        gatt_loge("gattc not init");

        return -1;
    }

    common_env_tmp = dm_ble_find_app_env_by_addr(addr);

    if (!common_env_tmp || !common_env_tmp->data)
    {
        gatt_loge("conn not found !!!!");
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

int32_t dm_gattc_connect_cancel(void)
{
    int32_t err = 0;

    if (!s_gattc_if)
    {
        gatt_loge("gattc not init");

        return -1;
    }

    err = bk_ble_gap_cancel_connect();

    if (err)
    {
        gatt_loge("cancel fail %d", err);
    }

    return err;
}

int32_t dm_gattc_discover(uint16_t conn_id)
{
    if (!s_gattc_if)
    {
        gatt_loge("gattc not init");

        return -1;
    }

    if (bk_ble_gattc_discover(s_gattc_if, conn_id, BK_GATT_AUTH_REQ_NONE))
    {
        gatt_loge("err");
    }

    return 0;
}

//ble_gatt_demo gattc write 5 18 111111111111111111111
int32_t dm_gattc_write(uint16_t conn_id, uint16_t attr_handle, uint8_t *data, uint32_t len)
{
    if (!s_gattc_if)
    {
        gatt_loge("gattc not init");

        return -1;
    }

    if (0 != bk_ble_gattc_write_char_descr(s_gattc_if, conn_id, attr_handle, len, data, BK_GATT_WRITE_TYPE_RSP, BK_GATT_AUTH_REQ_NONE))
    {
        gatt_loge("err");
    }

    return 0;
}

int dm_gattc_main(cli_gatt_param_t *param)
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
            s_dm_gattc_local_addr_is_public = *param->p_pa;
        }
    }

    dm_gatt_add_gap_callback(dm_ble_gap_cb);
    bk_ble_gattc_register_callback(bk_gattc_cb);

    bk_bd_addr_t current_addr = {0}, identity_addr = {0};
    char dev_name[64] = {0};

    dm_ble_gap_get_identity_addr(identity_addr);

    os_memcpy(current_addr, identity_addr, sizeof(identity_addr));

    snprintf((char *)(dev_name), sizeof(dev_name) - 1, "CENTRAL-%02X%02X%02X", identity_addr[2], identity_addr[1], identity_addr[0]);

    ret = bk_ble_gap_set_device_name(dev_name);

    if (ret)
    {
        gatt_loge("bk_ble_gap_set_device_name err %d", ret);
        return -1;
    }

    ret = bk_ble_gattc_app_register(0);

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

    uint8_t need_set_random_addr = 0;

    if (g_dm_gap_use_rpa && dm_ble_gap_get_rpa(current_addr) == 0)
    {
        gatt_logw("set connect/scan random addr with generate rpa");
        need_set_random_addr = 1;
    }
    else if (!s_dm_gattc_local_addr_is_public)
    {
        gatt_logw("set connect/scan random addr with user define");

        current_addr[0]++;
        current_addr[5] |= 0xc0; // static random addr[47:46] must be 0b11 in msb !!!
        need_set_random_addr = 1;
    }

    if (need_set_random_addr)
    {
        ret = bk_ble_gap_set_rand_addr(current_addr);

        if (ret)
        {
            gatt_loge("bk_ble_gap_set_rand_addr err %d", ret);
            goto error;
        }

        ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

        if (ret != kNoErr)
        {
            gatt_loge("wait set rand addr err %d", ret);
            goto error;
        }
    }

    return 0;

error:;
    return -1;
}
