#pragma once

#include "components/bluetooth/bk_dm_gatts.h"
#include "components/bluetooth/bk_dm_gatt_common.h"
#include "dm_gatt.h"

#define BK_GATT_ATTR_TYPE(iuuid) {.len = BK_UUID_LEN_16, .uuid = {.uuid16 = iuuid}}
#define BK_GATT_ATTR_CONTENT(iuuid) {.len = BK_UUID_LEN_16, .uuid = {.uuid16 = iuuid}}
#define BK_GATT_ATTR_VALUE(ilen, ivalue) {.attr_max_len = ilen, .attr_len = ilen, .attr_value = ivalue}

#define BK_GATT_ATTR_TYPE_128(iuuid) {.len = BK_UUID_LEN_128, .uuid = {.uuid128 = {iuuid[0], iuuid[1], iuuid[2], iuuid[3], iuuid[4], \
                iuuid[5], iuuid[6], iuuid[7], iuuid[8], iuuid[9], iuuid[10], iuuid[11], iuuid[12], iuuid[13], iuuid[14], iuuid[15]}}}

#define BK_GATT_ATTR_CONTENT_128(iuuid) {.len = BK_UUID_LEN_128, .uuid = {.uuid128 = {iuuid[0], iuuid[1], iuuid[2], iuuid[3], iuuid[4], \
                iuuid[5], iuuid[6], iuuid[7], iuuid[8], iuuid[9], iuuid[10], iuuid[11], iuuid[12], iuuid[13], iuuid[14], iuuid[15]}}}

#define BK_GATT_PRIMARY_SERVICE_DECL(iuuid) \
    .att_desc =\
               {\
                .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_PRI_SERVICE),\
                .attr_content = BK_GATT_ATTR_CONTENT(iuuid),\
               }

#define BK_GATT_PRIMARY_SERVICE_DECL_128(iuuid) \
    .att_desc =\
               {\
                .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_PRI_SERVICE),\
                .attr_content = BK_GATT_ATTR_CONTENT_128(iuuid)\
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

#define BK_GATT_CHAR_DECL_128(iuuid, ilen, ivalue, iprop, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE(BK_GATT_UUID_CHAR_DECLARE),\
                 .attr_content = BK_GATT_ATTR_CONTENT_128(iuuid),\
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

#define BK_GATT_CHAR_DESC_DECL_128(iuuid, ilen, ivalue, iperm, irsp) \
    .att_desc = \
                {\
                 .attr_type = BK_GATT_ATTR_TYPE_128(iuuid),\
                 .value = BK_GATT_ATTR_VALUE(ilen, ivalue),\
                 .perm = iperm,\
                },\
                .attr_control = {.auto_rsp = irsp}


typedef int32_t (* dm_ble_gatts_db_cb)(bk_gatts_cb_event_t event, bk_gatt_if_t gatts_if, bk_ble_gatts_cb_param_t *param);

int32_t dm_gatts_is_init(void);
int dm_gatts_main(cli_gatt_param_t *param);
int32_t dm_gatts_disconnect(uint8_t *addr);
int32_t dm_gatts_enable_adv(uint8_t enable);
int32_t dm_gatts_enable_service(uint32_t index, uint8_t enable);
int32_t dm_gatts_reg_db(bk_gatts_attr_db_t *list, uint32_t count, uint16_t *attr_handle_list, dm_ble_gatts_db_cb cb);
int32_t dm_gatts_get_buff_from_attr_handle(bk_gatts_attr_db_t *attr_list, uint16_t *attr_handle_list, uint32_t size, uint16_t attr_handle, uint32_t *output_index, uint8_t **output_buff, uint32_t *output_size);
