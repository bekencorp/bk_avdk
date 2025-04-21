#pragma once
#include "sdkconfig.h"
#include <stdint.h>
#include <string.h>
#include "ota_base_drv.h"

typedef enum
{
    F_OTA_COMM_OK            = 0,
    F_OTA_START_MAGIC_ERROR  = 1,
    F_OTA_FINISH_MAGIC_ERROR = 2,
    F_OTA_COMM_DATA_ERROR    = 3,
    F_OTA_COMM_LENGTH_ERROR  = 4,
    F_OTA_COMM_CRC_ERROR     = 5,
}f_ota_status_t;

#define OTA_SEQUENCE_NUMBER         (2)
#define OTA_STORE_ENTIRE_IMAGE_SIZE (4)
#define OTA_START_MAGIC_LENGTH      (9)
#define OTA_COMPLETE_MAGIC_LENGTH   (14)
#define OTA_CHECK_CRC_LENGTH        (4)
#define OTA_START_MAGIC             "\x4F\x54\x41\x5F\x53\x54\x41\x52\x54"
#define OTA_COMPLETE_MAGIC          "\x4F\x54\x41\x5F\x44\x4F\x57\x4E\x4C\x4F\x41\x44\x45\x44"