#pragma once

#define HOGPD_DEMO_ENABLE 1

enum
{
    HOGPD_DEBUG_LEVEL_ERROR,
    HOGPD_DEBUG_LEVEL_WARNING,
    HOGPD_DEBUG_LEVEL_INFO,
    HOGPD_DEBUG_LEVEL_DEBUG,
    HOGPD_DEBUG_LEVEL_VERBOSE,
};

#define HOGPD_DEBUG_LEVEL HOGPD_DEBUG_LEVEL_INFO

#define hogpd_loge(format, ...) do{if(HOGPD_DEBUG_LEVEL_INFO >= HOGPD_DEBUG_LEVEL_ERROR)   BK_LOGE("app_hogpd", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define hogpd_logw(format, ...) do{if(HOGPD_DEBUG_LEVEL_INFO >= HOGPD_DEBUG_LEVEL_WARNING) BK_LOGW("app_hogpd", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define hogpd_logi(format, ...) do{if(HOGPD_DEBUG_LEVEL_INFO >= HOGPD_DEBUG_LEVEL_INFO)    BK_LOGI("app_hogpd", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define hogpd_logd(format, ...) do{if(HOGPD_DEBUG_LEVEL_INFO >= HOGPD_DEBUG_LEVEL_DEBUG)   BK_LOGI("app_hogpd", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)
#define hogpd_logv(format, ...) do{if(HOGPD_DEBUG_LEVEL_INFO >= HOGPD_DEBUG_LEVEL_VERBOSE) BK_LOGI("app_hogpd", "%s:" format "\n", __func__, ##__VA_ARGS__);} while(0)

int32_t hogpd_demo_init(void);
