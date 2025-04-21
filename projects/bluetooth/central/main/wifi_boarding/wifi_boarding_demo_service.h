#pragma once

#include <os/os.h>

#include <stdint.h>

typedef enum
{
    BOARDING_OP_UNKNOWN = 0,
    BOARDING_OP_STATION_START = 1,
    BOARDING_OP_SOFT_AP_START = 2,
    BOARDING_OP_SERVICE_UDP_START = 3,
    BOARDING_OP_SERVICE_TCP_START = 4,
    BOARDING_OP_SET_CS2_DID = 5,
    BOARDING_OP_SET_CS2_APILICENSE = 6,
    BOARDING_OP_SET_CS2_KEY = 7,
    BOARDING_OP_SET_CS2_INIT_STRING = 8,
    BOARDING_OP_SRRVICE_CS2_START = 9,
    BOARDING_OP_BLE_DISABLE = 10,
    BOARDING_OP_SET_WIFI_CHANNEL = 11,
    BOARDING_OP_AGORA_AGENT_RSP = 12,
    BOARDING_OP_SET_AGORA_AGENT_INFO = 13,
    BOARDING_OP_NET_PAN_START = 14,
    BOARDING_OP_NETWORK_PROVISIONING_FIRST_TIME = 15,
    BOARDING_OP_START_AGENT_FROM_DEV = 16,
    BOARDING_OP_OTA_START_DOWNLOAD = 20,
    BOARDING_OP_OTA_DO_DOWNLOADING = 21,
    BOARDING_OP_OTA_COMPLETE_DOWNLOAD = 22,
} boarding_opcode_t;

typedef void (*ble_boarding_op_cb_t)(uint16_t opcode, uint16_t length, uint8_t *data);
typedef struct
{
    char *ssid_value;
    char *password_value;
    ble_boarding_op_cb_t cb;
    uint8_t boarding_notify[2];
    uint16_t ssid_length;
    uint16_t password_length;
} ble_boarding_info_t;

typedef struct
{
    ble_boarding_info_t boarding_info;
    uint16_t channel;
} bk_boarding_info_t;


#define EVT_STATUS_OK               (0)
#define EVT_STATUS_ERROR            (1)

typedef enum
{
    DBEVT_WIFI_STATION_CONNECT,
    DBEVT_WIFI_STATION_CONNECTED,
    DBEVT_WIFI_STATION_DISCONNECTED,

    DBEVT_WIFI_SOFT_AP_TURNING_ON,

    DBEVT_BLE_DISABLE,
    DBEVT_OTA_START_DOWNLOAD,
    DBEVT_OTA_DO_DOWNLOADING,
    DBEVT_OTA_COMPLETE_DOWNLOAD,
    DBEVT_EXIT,
} dbevt_t;

typedef struct
{
    uint32_t event;
    uint32_t param;
    uint16_t length;
} boarding_msg_t;

bk_err_t boarding_send_msg(boarding_msg_t *msg);
int32_t wifi_boarding_demo_service_main(void);
void ble_ota_start_timer(void);
void ble_ota_stop_timer(void);