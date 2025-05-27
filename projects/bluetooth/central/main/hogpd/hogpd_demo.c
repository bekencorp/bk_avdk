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
#include "components/bluetooth/bk_dm_gatt_types.h"
#include "components/bluetooth/bk_dm_gatts.h"

#include "dm_gatts.h"
#include "hogpd_demo.h"
#include <stdint.h>

#if HOGPD_DEMO_ENABLE


#define MIN_VALUE(x, y) (((x) < (y)) ? (x): (y))

typedef struct
{
    uint8_t status; //0 idle 1 connected
} hogpd_app_env_t;

static uint8_t s_hogpd_is_init;
static uint8_t s_protpcol_mode = 1;

static const uint8_t s_hid_rprtmap[] =
{
    0x05U, 0x01U, 0x09U, 0x06U, 0xA1U, 0x01U, 0x05U, 0x07U,
    0x19U, 0xE0U, 0x29U, 0xE7U, 0x15U, 0x00U, 0x25U, 0x01U,
    0x75U, 0x01U, 0x95U, 0x08U, 0x81U, 0x02U, 0x95U, 0x01U,
    0x75U, 0x08U, 0x81U, 0x03U, 0x95U, 0x05U, 0x75U, 0x01U,
    0x05U, 0x08U, 0x19U, 0x01U, 0x29U, 0x05U, 0x91U, 0x02U,
    0x95U, 0x01U, 0x75U, 0x03U, 0x91U, 0x03U, 0x95U, 0x06U,
    0x75U, 0x08U, 0x15U, 0x00U, 0x25U, 0x65U, 0x05U, 0x07U,
    0x19U, 0x00U, 0x29U, 0x65U, 0x81U, 0x00U, 0xC0U
};
static uint16_t s_report_map_desc;

static uint8_t s_input_hid_rprt[] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
static uint16_t s_input_hid_rprt_client_conf;
static const uint8_t s_input_hid_rprt_desc[] = {0, 1};

static uint8_t s_output_hid_rprt[] = {0x00U, 0x00U, 0x00U};
static const uint8_t s_output_hid_rprt_desc[] = { 0x00U, 0x02U };

static uint8_t s_feature_hid_rprt[] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
static const uint8_t s_feature_hid_rprt_desc[] = { 0x00U, 0x03U };

static const uint8_t s_hid_info[] = {0x13U, 0x02U, 0x40U, 0x01U};

static uint8_t s_boot_kbd_input_rprt[] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
static uint16_t s_boot_kbd_input_rprt_client_conf;

static uint8_t s_boot_kbd_output_rprt[] = {0x00U, 0x00U, 0x00U};

//static uint8_t s_boot_mouse_input_rprt[] = {0x00U, 0x00U, 0x00U};


static const bk_gatts_attr_db_t s_gatts_attr_db_service_hidd[] =
{
    {
        BK_GATT_PRIMARY_SERVICE_DECL(BK_GATT_UUID_HID_SVC),
    },

    //proto mode
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_PROTO_MODE,
                          sizeof(s_protpcol_mode), &s_protpcol_mode,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE_NR,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          //BK_GATT_PERM_READ_ENCRYPTED | BK_GATT_PERM_WRITE_ENCRYPTED,
                          BK_GATT_RSP_BY_APP),
    },

    //report map
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_REPORT_MAP,
                          sizeof(s_hid_rprtmap), (uint8_t *)s_hid_rprtmap,
                          BK_GATT_CHAR_PROP_BIT_READ,
                          BK_GATT_PERM_READ_ENCRYPTED,
                          //BK_GATT_PERM_READ_ENCRYPTED | BK_GATT_PERM_WRITE_ENCRYPTED,
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_EXT_RPT_REF_DESCR,
                               sizeof(s_report_map_desc), (uint8_t *)&s_report_map_desc,
                               BK_GATT_PERM_READ,
                               BK_GATT_AUTO_RSP),
    },

    //input report
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_REPORT,
                          sizeof(s_input_hid_rprt), s_input_hid_rprt,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_NOTIFY,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_CLIENT_CONFIG,
                               sizeof(s_input_hid_rprt_client_conf), (uint8_t *)&s_input_hid_rprt_client_conf,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_RPT_REF_DESCR,
                               sizeof(s_input_hid_rprt_desc), (uint8_t *)s_input_hid_rprt_desc,
                               BK_GATT_PERM_READ,
                               BK_GATT_AUTO_RSP),
    },

    //output report
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_REPORT,
                          sizeof(s_output_hid_rprt), s_output_hid_rprt,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_WRITE_NR,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_RPT_REF_DESCR,
                               sizeof(s_output_hid_rprt_desc), (uint8_t *)s_output_hid_rprt_desc,
                               BK_GATT_PERM_READ,
                               BK_GATT_AUTO_RSP),
    },

    //feature report
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_REPORT,
                          sizeof(s_feature_hid_rprt), s_feature_hid_rprt,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_RPT_REF_DESCR,
                               sizeof(s_feature_hid_rprt_desc), (uint8_t *)s_feature_hid_rprt_desc,
                               BK_GATT_PERM_READ,
                               BK_GATT_AUTO_RSP),
    },

    //control point
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_CONTROL_POINT,
                          0, NULL,
                          BK_GATT_CHAR_PROP_BIT_WRITE_NR,
                          BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },

    //info
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_INFORMATION,
                          sizeof(s_hid_info), (uint8_t *)s_hid_info,
                          BK_GATT_CHAR_PROP_BIT_READ,
                          BK_GATT_PERM_READ,
                          BK_GATT_AUTO_RSP),
    },

    //bootkeyboardinput report
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_BT_KB_INPUT,
                          sizeof(s_boot_kbd_input_rprt), s_boot_kbd_input_rprt,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_NOTIFY,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
    {
        BK_GATT_CHAR_DESC_DECL(BK_GATT_UUID_CHAR_CLIENT_CONFIG,
                               sizeof(s_boot_kbd_input_rprt_client_conf), (uint8_t *)&s_boot_kbd_input_rprt_client_conf,
                               BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                               BK_GATT_AUTO_RSP),
    },

    //bootkeyboardoutput report
    {
        BK_GATT_CHAR_DECL(BK_GATT_UUID_HID_BT_KB_OUTPUT,
                          sizeof(s_boot_kbd_output_rprt), s_boot_kbd_output_rprt,
                          BK_GATT_CHAR_PROP_BIT_READ | BK_GATT_CHAR_PROP_BIT_WRITE | BK_GATT_CHAR_PROP_BIT_WRITE_NR,
                          BK_GATT_PERM_READ | BK_GATT_PERM_WRITE,
                          BK_GATT_AUTO_RSP),
    },
};

static uint16_t s_hogpd_attr_handle_list[sizeof(s_gatts_attr_db_service_hidd) / sizeof(s_gatts_attr_db_service_hidd[0])];


static int32_t hogpd_demo_gatts_cb(bk_gatts_cb_event_t event, bk_gatt_if_t gatts_if, bk_ble_gatts_cb_param_t *comm_param)
{
    ble_err_t ret = 0;
    dm_gatt_app_env_t *common_env_tmp = NULL;
    hogpd_app_env_t *app_env_tmp = NULL;

    switch (event)
    {
    case BK_GATTS_CONNECT_EVT:
    {
        struct gatts_connect_evt_param *param = (typeof(param))comm_param;

        hogpd_logi("BK_GATTS_CONNECT_EVT %d role %d %02X:%02X:%02X:%02X:%02X:%02X", param->conn_id, param->link_role,
                   param->remote_bda[5],
                   param->remote_bda[4],
                   param->remote_bda[3],
                   param->remote_bda[2],
                   param->remote_bda[1],
                   param->remote_bda[0]);

        common_env_tmp = dm_ble_alloc_addition_data_by_addr(param->remote_bda, sizeof(hogpd_app_env_t));

        if (!common_env_tmp)
        {
            hogpd_loge("alloc addition data err !!!!");
            break;
        }

        app_env_tmp = (typeof(app_env_tmp))common_env_tmp->addition_data;
        app_env_tmp->status = 1;
    }
    break;

    case BK_GATTS_DISCONNECT_EVT:
    {
        struct gatts_disconnect_evt_param *param = (typeof(param))comm_param;


        hogpd_logi("BK_GATTS_DISCONNECT_EVT %02X:%02X:%02X:%02X:%02X:%02X",
                   param->remote_bda[5],
                   param->remote_bda[4],
                   param->remote_bda[3],
                   param->remote_bda[2],
                   param->remote_bda[1],
                   param->remote_bda[0]);

        common_env_tmp = dm_ble_find_app_env_by_addr(param->remote_bda);

        if (!common_env_tmp)
        {
            hogpd_loge("cant find app env");
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
        hogpd_logi("BK_GATTS_CONF_EVT");
    }
    break;

    case BK_GATTS_RESPONSE_EVT:
    {
        hogpd_logi("BK_GATTS_RESPONSE_EVT");
    }
    break;

    case BK_GATTS_READ_EVT:
    {
        struct gatts_read_evt_param *param = (typeof(param))comm_param;
        bk_gatt_rsp_t rsp;
        uint16_t final_len = 0;

        memset(&rsp, 0, sizeof(rsp));
        hogpd_logi("read attr handle %d need rsp %d", param->handle, param->need_rsp);

        uint8_t *tmp_buff = NULL;
        uint32_t buff_size = 0;
        uint32_t index = 0;

        if (dm_gatts_get_buff_from_attr_handle((bk_gatts_attr_db_t *)s_gatts_attr_db_service_hidd, s_hogpd_attr_handle_list,
                                               sizeof(s_hogpd_attr_handle_list) / sizeof(s_hogpd_attr_handle_list[0]), param->handle, &index, &tmp_buff, &buff_size))
        {
            hogpd_logi("handle invalid");
            break;
        }

        hogpd_logi("index %d size %d buff %p", index, buff_size, tmp_buff);

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

        hogpd_logi("write attr handle %d need rsp %d", param->handle, param->need_rsp);

        uint8_t *tmp_buff = NULL;
        uint32_t buff_size = 0;
        uint32_t index = 0;

        if (dm_gatts_get_buff_from_attr_handle((bk_gatts_attr_db_t *)s_gatts_attr_db_service_hidd, s_hogpd_attr_handle_list,
                                               sizeof(s_hogpd_attr_handle_list) / sizeof(s_hogpd_attr_handle_list[0]), param->handle, &index, &tmp_buff, &buff_size))
        {
            hogpd_logi("handle invalid");
            break;
        }

        hogpd_logi("index %d size %d buff %p", index, buff_size, tmp_buff);

        if (param->handle == s_hogpd_attr_handle_list[1])
        {
            hogpd_logi("write proto mode");
        }
        else if (param->handle == s_hogpd_attr_handle_list[4])
        {
            hogpd_logi("write input report");
        }
        else if (param->handle == s_hogpd_attr_handle_list[5])
        {
            uint16_t config = (((uint16_t)(param->value[1])) << 8) | param->value[0];

            hogpd_logi("write input report ccc");

            if (config & 1)
            {
                hogpd_logi("client notify open");
            }
            else
            {
                hogpd_logi("client write invalid data 0x%x", config);
            }
        }
        else if (param->handle == s_hogpd_attr_handle_list[7])
        {
            hogpd_logi("write output report");
        }
        else if (param->handle == s_hogpd_attr_handle_list[9])
        {
            hogpd_logi("write feature report");
        }
        else if (param->handle == s_hogpd_attr_handle_list[11])
        {
            hogpd_logi("write control point");
        }
        else if (param->handle == s_hogpd_attr_handle_list[13])
        {
            hogpd_logi("write bootkeyboardinput report");
        }

        else if (param->handle == s_hogpd_attr_handle_list[14])
        {
            uint16_t config = (((uint16_t)(param->value[1])) << 8) | param->value[0];

            hogpd_logi("write bootkeyboardinput report ccc");

            if (config & 1)
            {
                hogpd_logi("client notify open");
            }
            else
            {
                hogpd_logi("client write invalid data 0x%x", config);
            }
        }
        else if (param->handle == s_hogpd_attr_handle_list[15])
        {
            hogpd_logi("write bootkeyboardoutput report");
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
    }
    break;

    case BK_GATTS_EXEC_WRITE_EVT:
    {
        struct gatts_exec_write_evt_param *param = (typeof(param))comm_param;
        hogpd_logi("exec write");
    }
    break;

    default:
        break;
    }

    return 0;
}

static int32_t hogpd_demo_reg_db(void)
{
    int32_t ret = dm_gatts_reg_db((bk_gatts_attr_db_t *)s_gatts_attr_db_service_hidd,
                                  sizeof(s_gatts_attr_db_service_hidd) / sizeof(s_gatts_attr_db_service_hidd[0]),
                                  s_hogpd_attr_handle_list,
                                  hogpd_demo_gatts_cb);

    if (ret)
    {
        hogpd_loge("reg db err");
        return ret;
    }

    return ret;
}

#endif

int32_t hogpd_demo_init(void)
{
#if HOGPD_DEMO_ENABLE

    if (!dm_gatts_is_init())
    {
        hogpd_loge("gatts is not init");
        return -1;
    }

    if (s_hogpd_is_init)
    {
        hogpd_loge("already init");
        return -1;
    }

    s_hogpd_is_init = 1;

    hogpd_demo_reg_db();

    hogpd_logi("done");
#else
    hogpd_loge("hogpd not enable");
#endif
    return 0;
}

