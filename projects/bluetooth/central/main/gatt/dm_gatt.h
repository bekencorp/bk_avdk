#pragma once

#include "dm_gatt_connection.h"
#include <stdint.h>

#define BLE_USE_STORAGE 1
#define GATTS_TEST_ATTR_ENABLE 1

#define GAP_IS_OLD_API 0

#define GATT_MAX_BOND_COUNT 7
enum
{
    GATT_DEBUG_LEVEL_ERROR,
    GATT_DEBUG_LEVEL_WARNING,
    GATT_DEBUG_LEVEL_INFO,
    GATT_DEBUG_LEVEL_DEBUG,
    GATT_DEBUG_LEVEL_VERBOSE,
};

#define SYNC_CMD_TIMEOUT_MS 4000
#define GATT_DEBUG_LEVEL GATT_DEBUG_LEVEL_INFO

#define gatt_loge(format, ...) do{if(GATT_DEBUG_LEVEL >= GATT_DEBUG_LEVEL_ERROR)   BK_LOGE("app_gatt", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define gatt_logw(format, ...) do{if(GATT_DEBUG_LEVEL >= GATT_DEBUG_LEVEL_WARNING) BK_LOGW("app_gatt", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define gatt_logi(format, ...) do{if(GATT_DEBUG_LEVEL >= GATT_DEBUG_LEVEL_INFO)    BK_LOGI("app_gatt", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define gatt_logd(format, ...) do{if(GATT_DEBUG_LEVEL >= GATT_DEBUG_LEVEL_DEBUG)   BK_LOGI("app_gatt", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define gatt_logv(format, ...) do{if(GATT_DEBUG_LEVEL >= GATT_DEBUG_LEVEL_VERBOSE) BK_LOGI("app_gatt", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)


enum
{
    DM_BLE_GAP_APP_CB_RET_PROCESSED,
    DM_BLE_GAP_APP_CB_RET_NO_INTERESTING,
};


#define INTERESTING_SERIVCE_UUID 0x1234
#define INTERESTING_CHAR_UUID 0x5678

#define GATT_PARAM_MEMBER(type) \
                type rpa;           \
                type *p_rpa;        \
                type privacy;       \
                type *p_privacy;    \
                type iocap;         \
                type *p_iocap;      \
                type auth;          \
                type *p_auth;       \
                type ikd;           \
                type *p_ikd;        \
                type rkd;           \
                type *p_rkd;        \
                type pa;            \
                type *p_pa;

typedef struct
{
    GATT_PARAM_MEMBER(uint8_t)
}__attribute__((packed)) cli_gatt_param_t;


int dm_gatt_main(cli_gatt_param_t *param);
int dm_gatt_add_gap_callback(void * cb);
int32_t dm_gatt_get_authen_status(uint8_t *nominal_addr, uint8_t *nominal_addr_type, uint8_t *identity_addr, uint8_t *identity_addr_type);
int32_t dm_gatt_find_id_info_by_nominal_info(uint8_t *nominal_addr, uint8_t nominal_addr_type, uint8_t *identity_addr, uint8_t *identity_addr_type);
int dm_gatt_passkey_reply(uint8_t accept, uint32_t passkey);
int dm_gatt_set_security_method(uint8_t iocap, uint8_t auth_req, uint8_t key_distr);
bool dm_gatt_is_linkkey_distr_from_ltk(void);
int dm_ble_gap_create_bond(uint8_t *addr);
int dm_ble_gap_remove_bond(uint8_t *addr);
int32_t dm_ble_gap_clean_bond(void);
int32_t dm_ble_gap_show_bond_list(void);
int32_t dm_ble_gap_clean_local_key(void);
int dm_ble_gap_update_param(uint8_t *addr, uint16_t interval, uint16_t tout);
int32_t dm_ble_gap_get_rpa(uint8_t *rpa);
void dm_ble_gap_get_identity_addr(uint8_t *addr);

extern uint8_t g_dm_gap_use_rpa;
