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

#include <os/os.h>
#include <os/str.h>
#include <os/mem.h>
#include <components/log.h>
#include <components/system.h>
#include <common/bk_include.h>
#include <driver/int.h>

#include "media_core.h"
#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "media_mailbox_asr_tras.h"

#define TAG "mailbox_custom"
#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static media_mailbox_asr_tras_recv_callback_t s_recv_cb = NULL;

bk_err_t media_mailbox_asr_tras_event_handle(media_cpu_t src, void *param)
{
    LOGD("%s :src:%d, param:%u\n", __func__, src, *((uint32_t *)param));
    if(s_recv_cb){
        return s_recv_cb(src, param);
    }
    return BK_OK;
}
bk_err_t media_mailbox_asr_tras_init(media_mailbox_asr_tras_recv_callback_t callback)
{
    s_recv_cb = callback;
    return BK_OK;
}
bk_err_t media_mailbox_asr_tras_data(media_cpu_t src, media_cpu_t dest, void *param)
{
	bk_err_t ret = BK_FAIL;
    if(src == MONO_CPU){
    #if (CONFIG_SYS_CPU0)
        if(dest == MAJOR_CPU){
            ret = msg_send_req_to_media_app_mailbox_sync(EVENT_AUD_ASR_CPU0_TO_CPU1_DATA_REQ, (uint32_t)param, NULL);
        }else if(dest == MINOR_CPU){
            ret = msg_send_cp2_req_to_media_app_mailbox_sync(EVENT_AUD_ASR_CPU0_TO_CPU2_DATA_REQ, (uint32_t)param, NULL);
        }else{
            LOGE("src param error\r\n");
            ret = BK_FAIL;
        }
    #else
        LOGE("src param error\r\n");
        ret = BK_FAIL;
    #endif
    }else if(src == MAJOR_CPU){
    #if (CONFIG_SYS_CPU1)
        if(dest == MONO_CPU){
            ret = msg_send_req_to_media_major_mailbox_sync(EVENT_AUD_ASR_CPU1_TO_CPU0_DATA_REQ, APP_MODULE, (uint32_t)param, NULL);
        }else if(dest == MINOR_CPU){
            ret = msg_send_req_to_media_major_mailbox_sync(EVENT_AUD_ASR_CPU1_TO_CPU2_DATA_REQ, MINOR_MODULE, (uint32_t)param, NULL);
        }else{
            LOGE("src param error\r\n");
            ret = BK_FAIL;
        }
    #else
        LOGE("src param error\r\n");
        ret = BK_FAIL;
    #endif
    }else if(src == MINOR_CPU){
    #if (CONFIG_SYS_CPU2)
        if(dest == MONO_CPU){
            ret = msg_send_req_to_media_minor_mailbox_sync(EVENT_AUD_ASR_CPU2_TO_CPU0_DATA_REQ, APP_MODULE, (uint32_t)param, NULL);
        }else if(dest == MAJOR_CPU){
            ret = msg_send_req_to_media_minor_mailbox_sync(EVENT_AUD_ASR_CPU2_TO_CPU1_DATA_REQ, MAJOR_MODULE, (uint32_t)param, NULL);
        }else{
            LOGE("src param error\r\n");
            ret = BK_FAIL;
        }
    #else
        LOGE("src param error\r\n");
        ret = BK_FAIL;
    #endif
    }else{
        LOGE("src param error\r\n");
    }
	return ret;
}
bk_err_t media_mailbox_asr_tras_deinit()
{
    s_recv_cb = NULL;
    return BK_OK;
}
