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
#include "components/bluetooth/bk_dm_gap_ble.h"
#include "components/bluetooth/bk_dm_gatt_common.h"
#include "dm_gatt.h"
#include "bluetooth_storage.h"
#include "dm_gap_utils.h"

typedef int32_t (* dm_ble_gap_app_cb)(bk_ble_gap_cb_event_t event, bk_ble_gap_cb_param_t *param);

uint8_t g_dm_gap_use_rpa = 1;

static bk_bd_addr_t s_dm_gap_rpa;
static beken_semaphore_t s_ble_sema = NULL;
static dm_ble_gap_app_cb s_gap_cb_list[4];
static bk_bd_addr_t s_peer_bdaddr;
static bk_ble_bond_dev_t s_dm_gatt_bond_dev_list[GATT_MAX_BOND_COUNT];
static uint8_t s_dm_gatt_is_inited;
static uint8_t s_dm_gatt_privacy_enable = 0;
static bk_ble_local_keys_t s_dm_gap_local_key;

static beken_semaphore_t s_ble_er_ir_sema = NULL;

#if 1
    static uint8_t s_dm_gatt_iocap = BK_IO_CAP_NONE;
    static uint8_t s_dm_gatt_auth_req = BK_LE_AUTH_BOND;
    static uint8_t s_dm_gatt_init_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK | BK_BLE_KEY_DISTR_CSR_KEY_MASK;
    static uint8_t s_dm_gatt_rsp_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK;
#elif 0
    static uint8_t s_dm_gatt_iocap = BK_IO_CAP_DISPLAY_YESNO;
    static uint8_t s_dm_gatt_auth_req = BK_LE_AUTH_REQ_BOND_MITM;
    static uint8_t s_dm_gatt_init_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK | BK_BLE_KEY_DISTR_CSR_KEY_MASK;
    static uint8_t s_dm_gatt_rsp_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK;
#elif 1
    static uint8_t s_dm_gatt_iocap = BK_IO_CAP_DISPLAY_YESNO;
    static uint8_t s_dm_gatt_auth_req = BK_LE_AUTH_REQ_SC_MITM_BOND;
    static uint8_t s_dm_gatt_init_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK | BK_BLE_KEY_DISTR_CSR_KEY_MASK;
    static uint8_t s_dm_gatt_rsp_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK;
#elif 1
    static uint8_t s_dm_gatt_iocap = BK_IO_CAP_DISPLAY_YESNO;
    static uint8_t s_dm_gatt_auth_req = BK_LE_AUTH_REQ_SC_MITM_BOND;
    static uint8_t s_dm_gatt_init_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK | BK_BLE_KEY_DISTR_CSR_KEY_MASK | BK_BLE_KEY_DISTR_LINK_KEY_MASK;
    static uint8_t s_dm_gatt_rsp_key_distr = BK_BLE_KEY_DISTR_ENC_KEY_MASK | BK_BLE_KEY_DISTR_ID_KEY_MASK | BK_BLE_KEY_DISTR_LINK_KEY_MASK;
#endif

static bk_ble_bond_dev_t *dm_ble_find_bond_info_by_nominal_addr(uint8_t *addr, bk_ble_addr_type_t addr_type)
{
    if (!dm_gap_is_addr_valid(addr))
    {
        return NULL;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (!os_memcmp(s_dm_gatt_bond_dev_list[i].bd_addr, addr, sizeof(addr))
                //&& s_dm_gatt_bond_dev_list[i].key.pid_key.addr_type == addr_type  //todo: addr_type not used now.
           )
        {
            return &s_dm_gatt_bond_dev_list[i];
        }
    }

    return NULL;
}

static uint8_t dm_ble_del_bond_info_by_nominal_addr(uint8_t *addr)
{
    if (!dm_gap_is_addr_valid(addr))
    {
        return 1;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (!os_memcmp(s_dm_gatt_bond_dev_list[i].bd_addr, addr, BK_BD_ADDR_LEN))
        {
            os_memset(&s_dm_gatt_bond_dev_list[i], 0, sizeof(s_dm_gatt_bond_dev_list[i]));
            return 0;
        }
    }

    return 1;
}

static uint8_t dm_ble_clean_bond_info(void)
{
    os_memset(s_dm_gatt_bond_dev_list, 0, sizeof(s_dm_gatt_bond_dev_list));

    return 0;
}

static bk_ble_bond_dev_t *dm_ble_alloc_bond_info_by_nominal_addr(uint8_t *addr)
{
    bk_ble_bond_dev_t *tmp = NULL;

    tmp = dm_ble_find_bond_info_by_nominal_addr(addr, 0);

    if (tmp)
    {
        return tmp;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (!dm_gap_is_addr_valid(s_dm_gatt_bond_dev_list[i].bd_addr))
        {
            return &s_dm_gatt_bond_dev_list[i];
        }
    }

    return NULL;
}

static void dm_ble_gap_private_cb(bk_ble_gap_cb_event_t event, bk_ble_gap_cb_param_t *param)
{
    int32_t ret;

    for (int i = 0; i < sizeof(s_gap_cb_list) / sizeof(s_gap_cb_list[0]); ++i)
    {
        if (s_gap_cb_list[i])
        {
            ret = s_gap_cb_list[i](event, param);

            if (ret == DM_BLE_GAP_APP_CB_RET_PROCESSED)
            {
                return;
            }
        }
    }
}

static int32_t dm_ble_gap_common_cb(bk_ble_gap_cb_event_t event, bk_ble_gap_cb_param_t *param)
{
    dm_gatt_app_env_t *dm_gatt_env_p = NULL;
    bk_ble_bond_dev_t *dm_bond_dev_p = NULL;

    gatt_logd("event %d", event);

    switch (event)
    {
    case BK_BLE_GAP_CONNECT_COMPLETE_EVT:
    {
        struct ble_connect_complete_param *evt = (typeof(evt))param;

        gatt_logi("BK_BLE_GAP_CONNECT_COMPLETE_EVT %02x:%02x:%02x:%02x:%02x:%02x status 0x%x role %d hci_handle 0x%x",
                  evt->remote_bda[5],
                  evt->remote_bda[4],
                  evt->remote_bda[3],
                  evt->remote_bda[2],
                  evt->remote_bda[1],
                  evt->remote_bda[0],
                  evt->status,
                  evt->link_role,
                  evt->hci_handle
                 );

        if (evt->status)
        {
            dm_ble_del_app_env_by_addr(evt->remote_bda);
        }
    }
    break;

    case BK_BLE_GAP_DISCONNECT_COMPLETE_EVT:
    {
        struct ble_disconnect_complete_param *evt = (typeof(evt))param;

        gatt_logi("BK_BLE_GAP_DISCONNECT_COMPLETE_EVT %02x:%02x:%02x:%02x:%02x:%02x %d status 0x%x reason 0x%x hci_handle 0x%x",
                  evt->remote_bda[5],
                  evt->remote_bda[4],
                  evt->remote_bda[3],
                  evt->remote_bda[2],
                  evt->remote_bda[1],
                  evt->remote_bda[0],
                  evt->remote_bda_type,
                  evt->status,
                  evt->reason,
                  evt->hci_handle
                 );

        dm_ble_del_app_env_by_addr(evt->remote_bda);

        if (evt->reason == BK_BT_STATUS_TERMINATED_MIC_FAILURE)
        {
            bk_ble_bond_dev_t *tmp_dev = NULL;

            if ((tmp_dev = dm_ble_find_bond_info_by_nominal_addr(evt->remote_bda, evt->remote_bda_type)) == NULL)
            {
                gatt_loge("addr %02x:%02x:%02x:%02x:%02x:%02x bond info not found",
                          evt->remote_bda[5],
                          evt->remote_bda[4],
                          evt->remote_bda[3],
                          evt->remote_bda[2],
                          evt->remote_bda[1],
                          evt->remote_bda[0]);
                break;
            }

            if (bk_ble_gap_bond_dev_list_operation(BK_GAP_BOND_DEV_LIST_OPERATION_REMOVE, tmp_dev))
            {
                gatt_loge("remove bond dev list op err");
            }

            if (dm_ble_del_bond_info_by_nominal_addr(evt->remote_bda))
            {
                gatt_loge("addr %02x:%02x:%02x:%02x:%02x:%02x bond info del err",
                          evt->remote_bda[5],
                          evt->remote_bda[4],
                          evt->remote_bda[3],
                          evt->remote_bda[2],
                          evt->remote_bda[1],
                          evt->remote_bda[0]);
            }

#if BLE_USE_STORAGE
            bluetooth_storage_save_ble_key_info(s_dm_gatt_bond_dev_list, sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]));
            bluetooth_storage_sync_to_flash();
#endif
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_CONNECT_CANCEL_EVT:
    {
        struct ble_connect_cancel_param *evt = (typeof(evt))param;

        gatt_logi("BK_BLE_GAP_CONNECT_CANCEL_EVT status 0x%x", evt->status);
    }
    break;

    case BK_BLE_GAP_UPDATE_CONN_PARAMS_EVT:
    {
        struct ble_update_conn_params_evt_param *pm = (typeof(pm))param;

        gatt_logi("BK_BLE_GAP_UPDATE_CONN_PARAMS_EVT %d 0x%x 0x%x current intv 0x%x", pm->status, pm->max_int, pm->min_int, pm->conn_int);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }

    break;

    case BK_BLE_GAP_UPDATE_CONN_PARAMS_REQ_EVT:
    {
        struct ble_conntection_update_param_req *pm = (typeof(pm))param;

        gatt_logi("BK_BLE_GAP_UPDATE_CONN_PARAMS_REQ_EVT can_modify %d intv 0x%x 0x%x tout 0x%x", pm->can_modify, pm->param.max_int, pm->param.min_int, pm->param.timeout);

        pm->accept = 1;
    }
    break;

    case BK_BLE_GAP_AUTH_CMPL_EVT: //todo: report when second connect
    {
        bk_ble_sec_t *pm = (typeof(pm))param;
        gatt_logi("BK_BLE_GAP_AUTH_CMPL_EVT %s %d %02X:%02X:%02X:%02X:%02X:%02X, 0x%x 0x%x 0x%x",
                  pm->auth_cmpl.success ? "sucess" : "fail", pm->auth_cmpl.fail_reason,
                  pm->auth_cmpl.bd_addr[5],
                  pm->auth_cmpl.bd_addr[4],
                  pm->auth_cmpl.bd_addr[3],
                  pm->auth_cmpl.bd_addr[2],
                  pm->auth_cmpl.bd_addr[1],
                  pm->auth_cmpl.bd_addr[0],
                  pm->auth_cmpl.addr_type,
                  pm->auth_cmpl.dev_type,
                  pm->auth_cmpl.auth_mode
                 );

        dm_gatt_env_p = dm_ble_find_app_env_by_addr(pm->auth_cmpl.bd_addr);

        if (!dm_gatt_env_p)
        {
            gatt_loge("app env not found !!!");
        }

        if (pm->auth_cmpl.success)
        {
            if (dm_gatt_env_p)
            {
                dm_gatt_env_p->is_authen = 1;
            }
        }
        else
        {
            if (dm_gatt_env_p)
            {
                dm_gatt_env_p->is_authen = 0;
            }

            bk_ble_bond_dev_t *tmp_dev = NULL;

            if ((tmp_dev = dm_ble_find_bond_info_by_nominal_addr(pm->auth_cmpl.bd_addr, pm->auth_cmpl.addr_type)) == NULL)
            {
                gatt_loge("addr %02x:%02x:%02x:%02x:%02x:%02x bond info not found",
                          pm->auth_cmpl.bd_addr[5],
                          pm->auth_cmpl.bd_addr[4],
                          pm->auth_cmpl.bd_addr[3],
                          pm->auth_cmpl.bd_addr[2],
                          pm->auth_cmpl.bd_addr[1],
                          pm->auth_cmpl.bd_addr[0]);
                break;
            }

            if (bk_ble_gap_bond_dev_list_operation(BK_GAP_BOND_DEV_LIST_OPERATION_REMOVE, tmp_dev))
            {
                gatt_loge("remove bond dev list op err");
            }

            dm_ble_del_bond_info_by_nominal_addr(pm->auth_cmpl.bd_addr);

#if BLE_USE_STORAGE
            bluetooth_storage_save_ble_key_info(s_dm_gatt_bond_dev_list, sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]));
            bluetooth_storage_sync_to_flash();
#endif
        }
    }
    break;

    case BK_BLE_GAP_BOND_DEV_LIST_OPERATEION_COMPLETE_EVT:
    {
        struct ble_bond_dev_list_evt_param *pm = (typeof(pm))param;

        gatt_logi("BK_BLE_GAP_BOND_DEV_LIST_OPERATEION_COMPLETE_EVT %d %d", pm->status, pm->op);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_SET_LOCAL_PRIVACY_COMPLETE_EVT:
    {
        struct ble_local_privacy_cmpl_evt_param *pm = (typeof(pm))param;

        gatt_logi("BK_BLE_GAP_SET_LOCAL_PRIVACY_COMPLETE_EVT %d", pm->status);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_SET_SECURITY_PARAMS_COMPLETE_EVT:
    {
        bk_ble_gap_cb_param_t *pm = (typeof(pm))param;
        gatt_logi("BK_BLE_GAP_SET_SECURITY_PARAMS_COMPLETE_EVT %d status %d", pm->set_security_params_cmpl.param, pm->set_security_params_cmpl.status);

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

    case BK_BLE_GAP_BOND_KEY_GENERATE_EVT:
    {
        bk_ble_gap_cb_param_t *tmp = (typeof(tmp))param;
        struct ble_bond_dev_key_evt_param *pm = &tmp->bond_dev_key_generate_evt;
        char tmp_log[128] = {0};
        int index = 0;

        gatt_logi("BK_BLE_GAP_BOND_KEY_GENERATE_EVT type 0x%x %02x:%02x:%02x:%02x:%02x:%02x %d",
                  pm->bond_dev.bond_key.key_mask,
                  pm->bond_dev.bd_addr[5],
                  pm->bond_dev.bd_addr[4],
                  pm->bond_dev.bd_addr[3],
                  pm->bond_dev.bd_addr[2],
                  pm->bond_dev.bd_addr[1],
                  pm->bond_dev.bd_addr[0],
                  pm->bond_dev.addr_type);

        dm_bond_dev_p = dm_ble_alloc_bond_info_by_nominal_addr(pm->bond_dev.bd_addr);

        if (!dm_bond_dev_p)
        {
            gatt_loge("app env buff full !!!");
            break;
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_PENC)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "peer enc\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "ltk:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.penc_key.ltk); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.penc_key.ltk[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");

            gatt_logi("%s", tmp_log);
            os_memset(tmp_log, 0, sizeof(tmp_log));
            index = 0;

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "rand:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.penc_key.rand); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.penc_key.rand[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "ediv 0x%x keysize %d seclevel 0x%x\n",
                              pm->bond_dev.bond_key.penc_key.ediv,
                              pm->bond_dev.bond_key.penc_key.key_size,
                              pm->bond_dev.bond_key.penc_key.sec_level);

            gatt_logi("%s", tmp_log);

            //os_memcpy(&dm_bond_dev_p->key.penc_key, &pm->bond_dev.bond_key.penc_key, sizeof(pm->bond_dev.bond_key.penc_key));
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_PID)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "peer idkey\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "irk:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.pid_key.irk); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.pid_key.irk[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "addr:%02X:%02X:%02X:%02X:%02X:%02X %d\n",
                              pm->bond_dev.bond_key.pid_key.static_addr[5],
                              pm->bond_dev.bond_key.pid_key.static_addr[4],
                              pm->bond_dev.bond_key.pid_key.static_addr[3],
                              pm->bond_dev.bond_key.pid_key.static_addr[2],
                              pm->bond_dev.bond_key.pid_key.static_addr[1],
                              pm->bond_dev.bond_key.pid_key.static_addr[0],
                              pm->bond_dev.bond_key.pid_key.addr_type);

            gatt_logi("%s", tmp_log);

            //os_memcpy(&dm_bond_dev_p->key.pid_key, &pm->bond_dev.bond_key.pid_key, sizeof(pm->bond_dev.bond_key.pid_key));
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_PCSRK)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "peer csrk\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "csrk:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.pcsrk_key.csrk); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.pcsrk_key.csrk[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "counter: %d seclevel %d\n",
                              pm->bond_dev.bond_key.pcsrk_key.counter,
                              pm->bond_dev.bond_key.pcsrk_key.sec_level);

            gatt_logi("%s", tmp_log);

            //os_memcpy(&dm_bond_dev_p->key.pcsrk_key, &pm->bond_dev.bond_key.pcsrk_key, sizeof(pm->bond_dev.bond_key.pcsrk_key));
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_PLK)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_LLK)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "local linkkey\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "linkkey:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.llink_key.key); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.llink_key.key[i]);
            }

            gatt_logi("%s", tmp_log);
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_LENC)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "local enc\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "ltk:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.lenc_key.ltk); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.lenc_key.ltk[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");


            gatt_logi("%s", tmp_log);
            os_memset(tmp_log, 0, sizeof(tmp_log));
            index = 0;

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "rand:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.lenc_key.rand); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.lenc_key.rand[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");


            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "div 0x%x keysize %d seclevel %d\n",
                              pm->bond_dev.bond_key.lenc_key.div,
                              pm->bond_dev.bond_key.lenc_key.key_size,
                              pm->bond_dev.bond_key.lenc_key.sec_level);

            gatt_logi("%s", tmp_log);

            //os_memcpy(&dm_bond_dev_p->key.lenc_key, &pm->bond_dev.bond_key.lenc_key, sizeof(pm->bond_dev.bond_key.lenc_key));
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_LID)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "local idkey\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "irk:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.lid_key.irk); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.lid_key.irk[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "addr:%02X:%02X:%02X:%02X:%02X:%02X %d\n",
                              pm->bond_dev.bond_key.lid_key.static_addr[5],
                              pm->bond_dev.bond_key.lid_key.static_addr[4],
                              pm->bond_dev.bond_key.lid_key.static_addr[3],
                              pm->bond_dev.bond_key.lid_key.static_addr[2],
                              pm->bond_dev.bond_key.lid_key.static_addr[1],
                              pm->bond_dev.bond_key.lid_key.static_addr[0],
                              pm->bond_dev.bond_key.lid_key.addr_type);

            gatt_logi("%s", tmp_log);

            //os_memcpy(&dm_bond_dev_p->key.lid_key, &pm->bond_dev.bond_key.lid_key, sizeof(pm->bond_dev.bond_key.lid_key));
        }

        if (pm->bond_dev.bond_key.key_mask & BK_LE_KEY_LCSRK)
        {
            index = 0;
            os_memset(tmp_log, 0, sizeof(tmp_log));

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "local csrk\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "csrk:");

            for (int i = 0; i < sizeof(pm->bond_dev.bond_key.lcsrk_key.csrk); ++i)
            {
                index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->bond_dev.bond_key.lcsrk_key.csrk[i]);
            }

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "\n");

            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "counter: %d seclevel %d\n",
                              pm->bond_dev.bond_key.lcsrk_key.counter,
                              pm->bond_dev.bond_key.lcsrk_key.sec_level);

            gatt_logi("%s", tmp_log);

            //os_memcpy(&dm_bond_dev_p->key.lcsrk_key, &pm->bond_dev.bond_key.lcsrk_key, sizeof(pm->bond_dev.bond_key.lcsrk_key));
        }

        os_memcpy(dm_bond_dev_p, &pm->bond_dev, sizeof(*dm_bond_dev_p));

#if 0//CONFIG_BT

        if ((pm->bond_dev.bond_key.key_mask & (BK_LE_KEY_LENC | BK_LE_KEY_LLK)) == (BK_LE_KEY_LENC | BK_LE_KEY_LLK) &&
                pm->bond_dev.bond_key.lenc_key.sec_level == BK_BLE_SECURITY_LEVEL_4)
        {
#if CONFIG_A2DP_SOURCE_DEMO
            extern int32_t bt_a2dp_source_demo_set_linkkey(uint8_t *addr, uint8_t *linkkey);
            bt_a2dp_source_demo_set_linkkey(pm->bond_dev.bond_key.pid_key.static_addr, pm->bond_dev.bond_key.llink_key.key);
#elif CONFIG_A2DP_SINK_DEMO
            extern void bt_manager_set_tmp_linkkey(uint8_t *addr, uint8_t *linkkey);
            bt_manager_set_tmp_linkkey(pm->bond_dev.bond_key.pid_key.static_addr, pm->bond_dev.bond_key.llink_key.key);
#endif
        }

#endif

#if BLE_USE_STORAGE
        bluetooth_storage_save_ble_key_info(s_dm_gatt_bond_dev_list, sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]));
        bluetooth_storage_sync_to_flash();
#endif
    }
    break;

    case BK_BLE_GAP_SEC_REQ_EVT:
    {
        bk_ble_gap_cb_param_t *pm = (typeof(pm))param;
        gatt_logi("BK_BLE_GAP_SEC_REQ_EVT %02x:%02x:%02x:%02x:%02x:%02x %d",
                  param->ble_security.ble_req.bd_addr[5],
                  param->ble_security.ble_req.bd_addr[4],
                  param->ble_security.ble_req.bd_addr[3],
                  param->ble_security.ble_req.bd_addr[2],
                  param->ble_security.ble_req.bd_addr[1],
                  param->ble_security.ble_req.bd_addr[0],
                  param->ble_security.ble_req.addr_type);

        bk_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, 1);
    }
    break;

    case BK_BLE_GAP_PASSKEY_NOTIF_EVT:
    {
        bk_ble_gap_cb_param_t *pm = (typeof(pm))param;
        gatt_logi("BK_BLE_GAP_PASSKEY_NOTIF_EVT %06d, pls input on peer device !!!!", pm->ble_security.key_notif.passkey);
    }
    break;

    case BK_BLE_GAP_PASSKEY_REQ_EVT:
    {
        bk_ble_gap_cb_param_t *pm = (typeof(pm))param;

        gatt_logi("BK_BLE_GAP_PASSKEY_REQ_EVT, pls input num that peer device display !!!!");

        os_memcpy(s_peer_bdaddr, pm->ble_security.key_notif.bd_addr, sizeof(pm->ble_security.key_notif.bd_addr));
        //bk_ble_passkey_reply(pm->ble_req.bd_addr, true, 0);
    }
    break;

    case BK_BLE_GAP_NC_REQ_EVT:
    {
        bk_ble_gap_cb_param_t *pm = (typeof(pm))param;

        gatt_logi("BK_BLE_GAP_NC_REQ_EVT number compare %06d", pm->ble_security.key_notif.passkey);

        bk_ble_confirm_reply(pm->ble_security.key_notif.bd_addr, true);
    }
    break;

    case BK_BLE_GAP_OOB_REQ_EVT:
    {
        bk_ble_gap_cb_param_t *pm = (typeof(pm))param;
        gatt_logi("BK_BLE_GAP_OOB_REQ_EVT");
    }
    break;

    case BK_BLE_GAP_LOCAL_ER_EVT:
    {
        bk_ble_sec_t *pm = (typeof(pm))param;
        char tmp_log[128] = {0};
        int32_t index = 0;

        for (int i = 0; i < sizeof(pm->ble_local_keys.er); ++i)
        {
            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->ble_local_keys.er[i]);
        }

        gatt_logi("BK_BLE_GAP_LOCAL_ER_EVT %s", tmp_log);

        os_memcpy(s_dm_gap_local_key.er, pm->ble_local_keys.er, sizeof(pm->ble_local_keys.er));

#if BLE_USE_STORAGE
        bluetooth_storage_save_local_key(&s_dm_gap_local_key);
        bluetooth_storage_sync_to_flash();
#endif

        if (s_ble_er_ir_sema)
        {
            rtos_set_semaphore(&s_ble_er_ir_sema);
        }
    }
    break;

    case BK_BLE_GAP_LOCAL_IR_EVT:
    {
        bk_ble_sec_t *pm = (typeof(pm))param;
        char tmp_log[128] = {0};
        int32_t index = 0;

        for (int i = 0; i < sizeof(pm->ble_local_keys.ir); ++i)
        {
            index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", pm->ble_local_keys.ir[i]);
        }

        gatt_logi("BK_BLE_GAP_LOCAL_IR_EVT %s", tmp_log);

        os_memcpy(s_dm_gap_local_key.ir, pm->ble_local_keys.ir, sizeof(pm->ble_local_keys.ir));

#if BLE_USE_STORAGE
        bluetooth_storage_save_local_key(&s_dm_gap_local_key);
        bluetooth_storage_sync_to_flash();
#endif

        if (s_ble_er_ir_sema)
        {
            rtos_set_semaphore(&s_ble_er_ir_sema);
        }
    }
    break;

    case BK_BLE_GAP_GENERATE_RPA_COMPLETE_EVT:
    {
        struct ble_generate_rpa_cmpl *pm = (typeof(pm))param;

        if (pm->status == 0)
        {
            gatt_logi("BK_BLE_GAP_GENERATE_RPA_COMPLETE_EVT %02x:%02x:%02x:%02x:%02x:%02x",
                      pm->addr[5],
                      pm->addr[4],
                      pm->addr[3],
                      pm->addr[2],
                      pm->addr[1],
                      pm->addr[0]);

            os_memcpy(s_dm_gap_rpa, pm->addr, sizeof(s_dm_gap_rpa));
        }
        else
        {
            gatt_loge("BK_BLE_GAP_GENERATE_RPA_COMPLETE_EVT err status %d", pm->status);
        }

        if (s_ble_sema != NULL)
        {
            rtos_set_semaphore( &s_ble_sema );
        }
    }
    break;

#if 0

    case BK_BLE_GAP_BOND_KEY_REQ_EVT:
    {
        bk_ble_gap_cb_param_t *tmp = (typeof(tmp))param;
        struct ble_bond_dev_key_req_evt_param *pm = &tmp->bond_key_req_evt;
        gatt_logi("BK_BLE_GAP_BOND_KEY_REQ_EVT 0x%x %02x:%02x:%02x:%02x:%02x:%02x",
                  pm->key_req.key_type,
                  pm->key_req.peer_addr[5],
                  pm->key_req.peer_addr[4],
                  pm->key_req.peer_addr[3],
                  pm->key_req.peer_addr[2],
                  pm->key_req.peer_addr[1],
                  pm->key_req.peer_addr[0]);

        if (pm->key_req.key_type == BK_LE_KEY_LENC)
        {
            if ((dm_bond_dev_p = dm_ble_find_bond_info_by_nominal_addr(pm->key_req.peer_addr, 0)) != NULL )
            {
                char tmp_log[128] = {0};
                int32_t index = 0;

                for (int i = 0; i < sizeof(dm_bond_dev_p->bond_key.lenc_key.ltk); ++i)
                {
                    index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", dm_bond_dev_p->bond_key.lenc_key.ltk[i]);
                }

                gatt_logi("pair key reply accept, lenc_key %s", tmp_log);

                os_memset(tmp_log, 0, sizeof(tmp_log));
                index = 0;

                for (int i = 0; i < sizeof(dm_bond_dev_p->bond_key.lenc_key.rand); ++i)
                {
                    index += snprintf(tmp_log + index, sizeof(tmp_log) - 1 - index, "%02x", dm_bond_dev_p->bond_key.lenc_key.rand[i]);
                }

                gatt_logi("div 0x%04x rand %s", dm_bond_dev_p->bond_key.lenc_key.div, tmp_log);

                bk_ble_pair_key_reply(pm->key_req.peer_addr, true, pm->key_req.key_type, &dm_bond_dev_p->bond_key.lenc_key, sizeof(dm_bond_dev_p->bond_key.lenc_key));
            }
            else
            {
                gatt_logi("pair key reply negative");
                bk_ble_pair_key_reply(pm->key_req.peer_addr, false, pm->key_req.key_type, NULL, 0);
            }
        }
    }
    break;
#endif

    case BK_BLE_GAP_EXT_ADV_STOP_COMPLETE_EVT:
    {
        struct ble_adv_stop_cmpl_evt_param *pm = (typeof(pm))param;

        if (pm->status)
        {
            gatt_loge("set adv disable err %d", pm->status);
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


int dm_gatt_add_gap_callback(void *param)
{
    dm_ble_gap_app_cb cb = (typeof(cb))param;

    if (!cb)
    {
        return -1;
    }

    for (int i = 0; i < sizeof(s_gap_cb_list) / sizeof(s_gap_cb_list[0]); ++i)
    {
        if (!s_gap_cb_list[i])
        {
            s_gap_cb_list[i] = cb;
            return 0;
        }
    }

    return -1;
}

int dm_gatt_passkey_reply(uint8_t accept, uint32_t passkey)
{
    gatt_logi("accept %d %06d", accept, passkey);
    return bk_ble_passkey_reply(s_peer_bdaddr, accept, passkey);
}

static int32_t dm_gatt_set_security_method_private(void)
{
    int32_t ret = 0;
    gatt_logi("iocap 0x%x authen mode 0x%x key_dist 0x%x 0x%x", s_dm_gatt_iocap, s_dm_gatt_auth_req, s_dm_gatt_init_key_distr, s_dm_gatt_rsp_key_distr);

    if (!s_ble_sema)
    {
        ret = rtos_init_semaphore(&s_ble_sema, 1);

        if (ret != 0)
        {
            gatt_loge("rtos_init_semaphore err %d", ret);
            return -1;
        }
    }

    ret = bk_ble_gap_set_security_param(BK_BLE_SM_IOCAP_MODE, (void *)&s_dm_gatt_iocap, sizeof(s_dm_gatt_iocap));

    if (ret)
    {
        gatt_loge("set security param BK_BLE_SM_IOCAP_MODE err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set iocap err %d", ret);
        goto error;
    }

    ret = bk_ble_gap_set_security_param(BK_BLE_SM_AUTHEN_REQ_MODE, (void *)&s_dm_gatt_auth_req, sizeof(s_dm_gatt_auth_req));

    if (ret)
    {
        gatt_loge("set security param BK_BLE_SM_AUTHEN_REQ_MODE err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set authen err %d", ret);
        goto error;
    }

    ret = bk_ble_gap_set_security_param(BK_BLE_SM_SET_INIT_KEY, (void *)&s_dm_gatt_init_key_distr, sizeof(s_dm_gatt_init_key_distr));

    if (ret)
    {
        gatt_loge("set security param BK_BLE_SM_SET_INIT_KEY err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set init key err %d", ret);
        goto error;
    }

    ret = bk_ble_gap_set_security_param(BK_BLE_SM_SET_RSP_KEY, (void *)&s_dm_gatt_rsp_key_distr, sizeof(s_dm_gatt_rsp_key_distr));

    if (ret)
    {
        gatt_loge("set security param BK_BLE_SM_SET_RSP_KEY err %d", ret);
        goto error;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set rsp key err %d", ret);
        goto error;
    }

    rtos_delay_milliseconds(100);

error:;

    if (s_ble_sema)
    {
        ret = rtos_deinit_semaphore(&s_ble_sema);

        if (ret != 0)
        {
            gatt_loge("rtos_deinit_semaphore err %d", ret);
            return -1;
        }
    }

    s_ble_sema = NULL;

    return ret;
}

int dm_gatt_set_security_method(uint8_t iocap, uint8_t auth_req, uint8_t key_distr)
{
    s_dm_gatt_iocap = iocap;
    s_dm_gatt_auth_req = auth_req;
    s_dm_gatt_rsp_key_distr = s_dm_gatt_init_key_distr = key_distr;
    return dm_gatt_set_security_method_private();
}

bool dm_gatt_is_linkkey_distr_from_ltk(void)
{
#if CONFIG_BT

    if ((s_dm_gatt_init_key_distr & BK_BLE_KEY_DISTR_LINK_KEY_MASK) &&
            (s_dm_gatt_rsp_key_distr & BK_BLE_KEY_DISTR_LINK_KEY_MASK))
    {
        return true;
    }
    else
#endif
        return 0;
}

int32_t dm_gatt_get_authen_status(uint8_t *nominal_addr, uint8_t *nominal_addr_type, uint8_t *identity_addr, uint8_t *identity_addr_type)
{
    uint32_t i = 0;

    for (i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (dm_gap_is_addr_valid(s_dm_gatt_bond_dev_list[i].bd_addr))
        {
            break;
        }
    }

    if (i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]))
    {
        gatt_logi("find storage device 0x%x, nominal addr 0x%x %02x:%02x:%02x:%02x:%02x:%02x, identity addr 0x%x %02x:%02x:%02x:%02x:%02x:%02x",
                  s_dm_gatt_bond_dev_list[i].bond_key.key_mask,
                  s_dm_gatt_bond_dev_list[i].addr_type,
                  s_dm_gatt_bond_dev_list[i].bd_addr[5],
                  s_dm_gatt_bond_dev_list[i].bd_addr[4],
                  s_dm_gatt_bond_dev_list[i].bd_addr[3],
                  s_dm_gatt_bond_dev_list[i].bd_addr[2],
                  s_dm_gatt_bond_dev_list[i].bd_addr[1],
                  s_dm_gatt_bond_dev_list[i].bd_addr[0],
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.addr_type,
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[5],
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[4],
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[3],
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[2],
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[1],
                  s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[0]);

        if (nominal_addr)
        {
            os_memcpy(nominal_addr, s_dm_gatt_bond_dev_list[i].bd_addr, sizeof(s_dm_gatt_bond_dev_list[i].bd_addr));
        }

        if (nominal_addr_type)
        {
            *nominal_addr_type = s_dm_gatt_bond_dev_list[i].addr_type;
        }

        if (s_dm_gatt_bond_dev_list[i].bond_key.key_mask & BK_LE_KEY_PID)
        {
            if (identity_addr)
            {
                os_memcpy(identity_addr, s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr, sizeof(s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr));
            }

            if (identity_addr_type)
            {
                *identity_addr_type = s_dm_gatt_bond_dev_list[i].bond_key.pid_key.addr_type;
            }
        }

        return 0;
    }

    return -1;
}

int32_t dm_gatt_find_id_info_by_nominal_info(uint8_t *nominal_addr, uint8_t nominal_addr_type, uint8_t *identity_addr, uint8_t *identity_addr_type)
{
    uint32_t i = 0;

    if (!nominal_addr)
    {
        return -1;
    }

    for (i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (!dm_gap_is_addr_valid(s_dm_gatt_bond_dev_list[i].bd_addr))
        {
            continue;
        }

        if (!os_memcmp(nominal_addr, s_dm_gatt_bond_dev_list[i].bd_addr, sizeof(s_dm_gatt_bond_dev_list[i].bd_addr)) &&
                s_dm_gatt_bond_dev_list[i].addr_type == nominal_addr_type)
        {
            if (s_dm_gatt_bond_dev_list[i].bond_key.key_mask & BK_LE_KEY_PID)
            {
                gatt_logi("find storage device 0x%x, nominal addr 0x%x %02x:%02x:%02x:%02x:%02x:%02x, identity addr 0x%x %02x:%02x:%02x:%02x:%02x:%02x",
                          s_dm_gatt_bond_dev_list[i].bond_key.key_mask,
                          s_dm_gatt_bond_dev_list[i].addr_type,
                          s_dm_gatt_bond_dev_list[i].bd_addr[5],
                          s_dm_gatt_bond_dev_list[i].bd_addr[4],
                          s_dm_gatt_bond_dev_list[i].bd_addr[3],
                          s_dm_gatt_bond_dev_list[i].bd_addr[2],
                          s_dm_gatt_bond_dev_list[i].bd_addr[1],
                          s_dm_gatt_bond_dev_list[i].bd_addr[0],
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.addr_type,
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[5],
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[4],
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[3],
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[2],
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[1],
                          s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[0]);

                os_memcpy(identity_addr, s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr, sizeof(s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr));
                *identity_addr_type = s_dm_gatt_bond_dev_list[i].bond_key.pid_key.addr_type;

                return 0;
            }
            else
            {
                return -1;
            }
        }
    }

    return -1;
}

int dm_ble_gap_create_bond(uint8_t *addr)
{
    if (!s_dm_gatt_is_inited)
    {
        gatt_loge("not init");
        return -1;
    }

    return bk_ble_gap_create_bond(addr);
}

int dm_ble_gap_remove_bond(uint8_t *addr)
{
    int32_t ret = 0;
    bk_ble_bond_dev_t *tmp_dev = NULL;

    if (!s_dm_gatt_is_inited)
    {
        gatt_loge("not init");
        return 0;
    }

    if (!dm_gap_is_addr_valid(addr))
    {
        gatt_loge("addr invalid");

        return -1;
    }

    if ((tmp_dev = dm_ble_find_bond_info_by_nominal_addr(addr, 0)) == NULL)
    {
        gatt_loge("addr %02x:%02x:%02x:%02x:%02x:%02x bond info not found",
                  addr[5],
                  addr[4],
                  addr[3],
                  addr[2],
                  addr[1],
                  addr[0]);
        return -1;
    }

    if (!s_ble_sema)
    {
        ret = rtos_init_semaphore(&s_ble_sema, 1);

        if (ret != 0)
        {
            gatt_loge("rtos_init_semaphore err %d", ret);
            return -1;
        }
    }

    const uint8_t ext_adv_inst[] = {0};

    ret = bk_ble_gap_adv_stop(1, ext_adv_inst);

    if (ret)
    {
        gatt_loge("bk_ble_gap_adv_stop err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv disable err %d", ret);
        return -1;
    }

    bk_ble_bond_dev_t bond_dev;

    os_memset(&bond_dev, 0, sizeof(bond_dev));

    os_memcpy(&bond_dev, tmp_dev, sizeof(*tmp_dev));

    ret = bk_ble_gap_bond_dev_list_operation(BK_GAP_BOND_DEV_LIST_OPERATION_REMOVE, &bond_dev);

    if (ret)
    {
        gatt_loge("remove bond dev list op err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait bond dev list op err %d", ret);
        return -1;
    }

    if (dm_ble_del_bond_info_by_nominal_addr(addr))
    {
        gatt_loge("addr %02x:%02x:%02x:%02x:%02x:%02x bond info del err",
                  addr[5],
                  addr[4],
                  addr[3],
                  addr[2],
                  addr[1],
                  addr[0]);
    }

    dm_gatt_app_env_t *app_env = dm_ble_find_app_env_by_addr(addr);

    while (app_env)
    {
        if (app_env->status != GAP_CONNECT_STATUS_CONNECTED)
        {
            gatt_loge("connect status is not connected %d", app_env->status);
            break;
        }

        bk_bd_addr_t peer_addr;
        os_memcpy(peer_addr, addr, sizeof(peer_addr));

        gatt_logi("disconnect because remove pair %02x:%02x:%02x:%02x:%02x:%02x", addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
        ret = bk_ble_gap_disconnect(peer_addr);

        if (ret)
        {
            gatt_loge("disconnect fail %d", ret);
            break;
        }
        else
        {
            app_env->status = GAP_CONNECT_STATUS_DISCONNECTING;

            ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

            if (ret != kNoErr)
            {
                gatt_loge("wait disconnect err %d", ret);
                break;
            }
        }

        break;
    }

    ret = rtos_deinit_semaphore(&s_ble_sema);

    if (ret != 0)
    {
        gatt_loge("rtos_deinit_semaphore err %d", ret);
        return -1;
    }

    s_ble_sema = NULL;

#if BLE_USE_STORAGE
    bluetooth_storage_save_ble_key_info(s_dm_gatt_bond_dev_list, sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]));
    bluetooth_storage_sync_to_flash();
#endif

    return ret;
}

int32_t dm_ble_gap_clean_bond(void)
{
    int32_t ret = 0;

    if (!s_dm_gatt_is_inited)
    {
        gatt_loge("not init");
        return -1;
    }

    dm_ble_clean_bond_info();

    if (!s_ble_sema)
    {
        ret = rtos_init_semaphore(&s_ble_sema, 1);

        if (ret != 0)
        {
            gatt_loge("rtos_init_semaphore err %d", ret);
            return -1;
        }
    }

    const uint8_t ext_adv_inst[] = {0};

    ret = bk_ble_gap_adv_stop(1, ext_adv_inst);

    if (ret)
    {
        gatt_loge("bk_ble_gap_adv_stop err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set adv disable err %d", ret);
        return -1;
    }

    ret = bk_ble_gap_bond_dev_list_operation(BK_GAP_BOND_DEV_LIST_OPERATION_CLEAN, NULL);

    if (ret)
    {
        gatt_loge("remove bond dev list op err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait bond dev list op err %d", ret);
        return -1;
    }

    int32_t nest_func_disconnect_all(dm_gatt_app_env_t *env, void *arg)
    {
        if (env && env->status == GAP_CONNECT_STATUS_CONNECTED)
        {
            bk_bd_addr_t peer_addr;
            os_memcpy(peer_addr, env->addr, sizeof(peer_addr));

            gatt_logi("disconnect because remove pair %02x:%02x:%02x:%02x:%02x:%02x", env->addr[5], env->addr[4], env->addr[3], env->addr[2], env->addr[1], env->addr[0]);
            ret = bk_ble_gap_disconnect(peer_addr);

            if (ret)
            {
                gatt_loge("disconnect fail %d %02x:%02x:%02x:%02x:%02x:%02x", ret, env->addr[5], env->addr[4], env->addr[3], env->addr[2], env->addr[1], env->addr[0]);
                return -1;
            }
            else
            {
                env->status = GAP_CONNECT_STATUS_DISCONNECTING;

                ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

                if (ret != kNoErr)
                {
                    gatt_loge("wait disconnect err %d", ret);
                    return -1;
                }
            }
        }

        return 0;
    }

    dm_ble_app_env_foreach(nest_func_disconnect_all, NULL);

    ret = rtos_deinit_semaphore(&s_ble_sema);

    if (ret != 0)
    {
        gatt_loge("rtos_deinit_semaphore err %d", ret);
        return -1;
    }

    s_ble_sema = NULL;

#if BLE_USE_STORAGE
    bluetooth_storage_save_ble_key_info(s_dm_gatt_bond_dev_list, sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]));
    bluetooth_storage_sync_to_flash();
#endif

    return ret;
}

int32_t dm_ble_gap_show_bond_list(void)
{
    if (!s_dm_gatt_is_inited)
    {
        gatt_loge("not init");
        return -1;
    }

    for (int i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (!dm_gap_is_addr_valid(s_dm_gatt_bond_dev_list[i].bd_addr))
        {
            continue;
        }

        if (s_dm_gatt_bond_dev_list[i].bond_key.key_mask & BK_LE_KEY_PID)
        {
            gatt_logi("nominal addr %02x:%02x:%02x:%02x:%02x:%02x %d, key 0x%x, identity addr %02x:%02x:%02x:%02x:%02x:%02x %d",
                      s_dm_gatt_bond_dev_list[i].bd_addr[5],
                      s_dm_gatt_bond_dev_list[i].bd_addr[4],
                      s_dm_gatt_bond_dev_list[i].bd_addr[3],
                      s_dm_gatt_bond_dev_list[i].bd_addr[2],
                      s_dm_gatt_bond_dev_list[i].bd_addr[1],
                      s_dm_gatt_bond_dev_list[i].bd_addr[0],
                      s_dm_gatt_bond_dev_list[i].addr_type,
                      s_dm_gatt_bond_dev_list[i].bond_key.key_mask,
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[5],
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[4],
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[3],
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[2],
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[1],
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.static_addr[0],
                      s_dm_gatt_bond_dev_list[i].bond_key.pid_key.addr_type
                     );
        }
        else
        {
            gatt_logi("nominal addr %02x:%02x:%02x:%02x:%02x:%02x %d, key 0x%x",
                      s_dm_gatt_bond_dev_list[i].bd_addr[5],
                      s_dm_gatt_bond_dev_list[i].bd_addr[4],
                      s_dm_gatt_bond_dev_list[i].bd_addr[3],
                      s_dm_gatt_bond_dev_list[i].bd_addr[2],
                      s_dm_gatt_bond_dev_list[i].bd_addr[1],
                      s_dm_gatt_bond_dev_list[i].bd_addr[0],
                      s_dm_gatt_bond_dev_list[i].addr_type,
                      s_dm_gatt_bond_dev_list[i].bond_key.key_mask
                     );
        }
    }

    return 0;
}

int32_t dm_ble_gap_clean_local_key(void)
{
    if (!s_dm_gatt_is_inited)
    {
        gatt_loge("not init");
        return -1;
    }

    os_memset(&s_dm_gap_local_key, 0, sizeof(s_dm_gap_local_key));

#if BLE_USE_STORAGE
    bluetooth_storage_save_local_key(&s_dm_gap_local_key);
    bluetooth_storage_sync_to_flash();
#endif

    return 0;
}

int32_t dm_ble_gap_get_rpa(uint8_t *rpa)
{
    if (dm_gap_is_data_valid(s_dm_gap_rpa, sizeof(s_dm_gap_rpa)))
    {
        if (rpa)
        {
            os_memcpy(rpa, s_dm_gap_rpa, sizeof(s_dm_gap_rpa));
        }

        return 0;
    }
    else
    {
        return -1;
    }
}

void dm_ble_gap_get_identity_addr(uint8_t *addr)
{
    uint8_t *identity_addr = addr;
    bk_get_mac((uint8_t *)identity_addr, MAC_TYPE_BLUETOOTH);

    for (int i = 0; i < BK_BD_ADDR_LEN / 2; i++)
    {
        uint8_t tmp = identity_addr[i];
        identity_addr[i] = identity_addr[BK_BD_ADDR_LEN - 1 - i];
        identity_addr[BK_BD_ADDR_LEN - 1 - i] = tmp;
    }
}

int dm_ble_gap_update_param(uint8_t *addr, uint16_t interval, uint16_t tout)
{
    int32_t ret = 0;
    bk_ble_conn_update_params_t param;

    if (!s_ble_sema)
    {
        ret = rtos_init_semaphore(&s_ble_sema, 1);

        if (ret != 0)
        {
            gatt_loge("rtos_init_semaphore err %d", ret);
            return -1;
        }
    }

    os_memset(&param, 0, sizeof(param));

    os_memcpy(param.bda, addr, sizeof(param.bda));
    param.latency = 0;
    param.timeout = tout;
    param.min_int = param.max_int = interval;

    bk_ble_gap_update_conn_params(&param);

    if (ret)
    {
        gatt_loge("remove bond dev list op err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait bond dev list op err %d", ret);
        return -1;
    }

    ret = rtos_deinit_semaphore(&s_ble_sema);

    if (ret != 0)
    {
        gatt_loge("rtos_deinit_semaphore err %d", ret);
        return -1;
    }

    return 0;
}

int dm_gatt_main(cli_gatt_param_t *param)
{
    ble_err_t ret = 0;

    if (s_dm_gatt_is_inited)
    {
        gatt_loge("already init");
        return -1;
    }

    if (param)
    {
        if (param->p_rpa)
        {
            g_dm_gap_use_rpa = *param->p_rpa;

            gatt_logi("set rpa %d", g_dm_gap_use_rpa);
        }

        if (param->p_privacy)
        {
            s_dm_gatt_privacy_enable = *param->p_privacy;
        }

        if (param->p_iocap)
        {
            s_dm_gatt_iocap = *param->p_iocap;
        }

        if (param->p_auth)
        {
            s_dm_gatt_auth_req = *param->p_auth;
        }

        if (param->p_ikd)
        {
            s_dm_gatt_init_key_distr = *param->p_ikd;
        }

        if (param->p_rkd)
        {
            s_dm_gatt_rsp_key_distr = *param->p_rkd;
        }

        if (param->p_iocap)
        {
            s_dm_gatt_iocap = *param->p_iocap;
        }
    }

#if BLE_USE_STORAGE
    uint32_t list_count = GATT_MAX_BOND_COUNT;

    bluetooth_storage_init();
    bluetooth_storage_read_ble_key_info(s_dm_gatt_bond_dev_list, &list_count);
#endif

    ret = rtos_init_semaphore(&s_ble_sema, 1);

    if (ret != 0)
    {
        gatt_loge("rtos_init_semaphore err %d", ret);
        return -1;
    }

    ret = rtos_init_semaphore(&s_ble_er_ir_sema, 1);

    if (ret != 0)
    {
        gatt_loge("rtos_init_semaphore s_ble_er_ir_sema err %d", ret);
        return -1;
    }

    bk_ble_gap_register_callback(dm_ble_gap_private_cb);
    dm_gatt_add_gap_callback(dm_ble_gap_common_cb);

    // set ir er
#if BLE_USE_STORAGE
    bluetooth_storage_read_local_key(&s_dm_gap_local_key);
#else
    s_dm_gap_local_key = (typeof(s_dm_gap_local_key))
    {
        .er = {0x00U, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0AU, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU},
        .ir = {0x0FU, 0x0EU, 0x0DU, 0x0CU, 0x0BU, 0x0AU, 0x09U, 0x08U, 0x07U, 0x06U, 0x05U, 0x04U, 0x03U, 0x02U, 0x01U, 0x00U},
    };

#endif

    if (!dm_gap_is_data_valid(s_dm_gap_local_key.er, sizeof(s_dm_gap_local_key.er)))
    {
        for (int i = 0; i < sizeof(s_dm_gap_local_key.er); ++i)
        {
            s_dm_gap_local_key.er[i] = rand();
        }
    }

    gatt_logw("set BK_BLE_SM_SET_ER start");

    ret = bk_ble_gap_set_security_param(BK_BLE_SM_SET_ER, (void *)s_dm_gap_local_key.er, sizeof(s_dm_gap_local_key.er));

    if (ret)
    {
        gatt_loge("set security param BK_BLE_SM_SET_ER err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set er err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_er_ir_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait er report err %d", ret);
        return -1;
    }

    if (!dm_gap_is_data_valid(s_dm_gap_local_key.ir, sizeof(s_dm_gap_local_key.ir)))
    {
        for (int i = 0; i < sizeof(s_dm_gap_local_key.ir); ++i)
        {
            s_dm_gap_local_key.ir[i] = rand();
        }
    }

    gatt_logw("set BK_BLE_SM_SET_IR start");

    ret = bk_ble_gap_set_security_param(BK_BLE_SM_SET_IR, (void *)s_dm_gap_local_key.ir, sizeof(s_dm_gap_local_key.ir));

    if (ret)
    {
        gatt_loge("set security param BK_BLE_SM_SET_IR err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait set ir err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_er_ir_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait ir report err %d", ret);
        return -1;
    }


    if (g_dm_gap_use_rpa)
    {
        rtos_delay_milliseconds(100);
        //generate rpa
        ret = bk_ble_gap_generate_rpa(NULL);

        if (ret)
        {
            gatt_loge("generate rpa err %d", ret);
            return -1;
        }

        ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

        if (ret != kNoErr)
        {
            gatt_loge("wait generate rpa err %d", ret);
            return -1;
        }
    }

    //set privacy
    gatt_logi("set privacy %d", s_dm_gatt_privacy_enable);

    ret = bk_ble_gap_config_local_privacy(s_dm_gatt_privacy_enable);

    if (ret)
    {
        gatt_loge("set privacy err %d", ret);
        return -1;
    }

    ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

    if (ret != kNoErr)
    {
        gatt_loge("wait privacy err %d", ret);
        return -1;
    }

    //add bond list
    for (int i = 0; i < sizeof(s_dm_gatt_bond_dev_list) / sizeof(s_dm_gatt_bond_dev_list[0]); ++i)
    {
        if (dm_gap_is_addr_valid(s_dm_gatt_bond_dev_list[i].bd_addr))
        {
            bk_ble_bond_dev_t bond_dev;

            os_memset(&bond_dev, 0, sizeof(bond_dev));

            os_memcpy(&bond_dev, s_dm_gatt_bond_dev_list + i, sizeof(bond_dev));

            gatt_logi("add bond dev nominal %02x:%02x:%02x:%02x:%02x:%02x 0x%x",
                      bond_dev.bd_addr[5],
                      bond_dev.bd_addr[4],
                      bond_dev.bd_addr[3],
                      bond_dev.bd_addr[2],
                      bond_dev.bd_addr[1],
                      bond_dev.bd_addr[0],
                      bond_dev.addr_type);

            if (bond_dev.bond_key.key_mask & BK_LE_KEY_PID)
            {
                gatt_logi("peer identity info %02x:%02x:%02x:%02x:%02x:%02x 0x%x",
                          bond_dev.bond_key.pid_key.static_addr[5],
                          bond_dev.bond_key.pid_key.static_addr[4],
                          bond_dev.bond_key.pid_key.static_addr[3],
                          bond_dev.bond_key.pid_key.static_addr[2],
                          bond_dev.bond_key.pid_key.static_addr[1],
                          bond_dev.bond_key.pid_key.static_addr[0],
                          bond_dev.bond_key.pid_key.addr_type);
            }

            ret = bk_ble_gap_bond_dev_list_operation(BK_GAP_BOND_DEV_LIST_OPERATION_ADD, &bond_dev);

            if (ret)
            {
                gatt_loge("add bond dev list op err %d", ret);
                return -1;
            }

            ret = rtos_get_semaphore(&s_ble_sema, SYNC_CMD_TIMEOUT_MS);

            if (ret != kNoErr)
            {
                gatt_loge("wait bond dev list op err %d", ret);
                return -1;
            }
        }
    }

    //set other security param
    dm_gatt_set_security_method_private();

    //set mtu
    bk_ble_gatt_set_local_mtu(517);

    if (s_ble_er_ir_sema)
    {
        ret = rtos_deinit_semaphore(&s_ble_er_ir_sema);

        if (ret != 0)
        {
            gatt_loge("rtos_deinit_semaphore s_ble_er_ir_sema err %d", ret);
            return -1;
        }

        s_ble_er_ir_sema = NULL;
    }

    if (s_ble_sema)
    {
        ret = rtos_deinit_semaphore(&s_ble_sema);

        if (ret != 0)
        {
            gatt_loge("rtos_deinit_semaphore err %d", ret);
            return -1;
        }
    }

    s_ble_sema = NULL;
    s_dm_gatt_is_inited = 1;

    return 0;
}

