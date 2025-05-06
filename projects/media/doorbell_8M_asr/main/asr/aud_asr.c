#include <os/os.h>
#include "modules/pm.h"
#include <driver/pwr_clk.h>
#include "aud_asr.h"
#include "media_core.h"
#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "media_mailbox_asr_tras.h"

static aud_asr_recv_result_callback_t s_aud_recv_result_cb = NULL;
static bk_err_t media_mb_audio_tras_cb(media_cpu_t src, void *param)
{
    aud_asr_mb_data_t *msg_data = (aud_asr_mb_data_t *)param;
    switch (msg_data->cmd)
    {
    case AUD_ASR_CMD_RSP_RESULT:
        if(s_aud_recv_result_cb){
            s_aud_recv_result_cb(msg_data->param1);
        }
        break;
    default:

        break;
    }
    return BK_OK;
}
#if CONFIG_SYS_CPU1
static beken_semaphore_t s_aud_asr_init_sem = NULL;
static uint8_t s_aud_asr_start = 0;
void aud_asr_cp2_init_notify(void)
{
    os_printf("%s, cpu2 start complete.\r\n", __func__);
	if (s_aud_asr_init_sem)
	{
		rtos_set_semaphore(&s_aud_asr_init_sem);
	}
}

bk_err_t aud_asr_start(aud_asr_recv_result_callback_t recv_result_cb)
{
    bk_err_t ret = BK_OK;
    if(s_aud_asr_start){
        return ret;
    }
    bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);
    os_printf("%s, aud_asr_start\r\n", __func__);
    if(!s_aud_asr_init_sem){
        ret = rtos_init_semaphore(&s_aud_asr_init_sem, 1);
        if (ret != BK_OK)
        {
            os_printf("%s, init s_aud_asr_init_sem failed\r\n", __func__);
            return ret;
        }
    }
    media_mailbox_asr_tras_init(media_mb_audio_tras_cb);
    if(CPU2_USER_AUD_ASR == vote_start_cpu2_core(CPU2_USER_AUD_ASR))	//first owner start CPU2, so needs to wait sem
    {
        rtos_get_semaphore(&s_aud_asr_init_sem, BEKEN_WAIT_FOREVER);
    }
    os_printf("%s, aud_asr_start complete0\r\n", __func__);
    s_aud_recv_result_cb = recv_result_cb;
    ret = msg_send_req_to_media_major_mailbox_sync(EVENT_AUD_ASR_INIT_NOTIFY, MINOR_MODULE, 0, NULL);
	if (ret != BK_OK) {
		os_printf("[-]%s, fail %d\n", __func__, __LINE__);
	}
    s_aud_asr_start = 1;
    os_printf("%s, aud_asr_start complete\r\n", __func__);
    return ret;
}

bk_err_t aud_asr_stop()
{
    bk_err_t ret = BK_OK;
    if(!s_aud_asr_start){
        return ret;
    }
    s_aud_asr_start = 0;
	msg_send_req_to_media_major_mailbox_sync(EVENT_AUD_ASR_DEINIT_NOTIFY, MINOR_MODULE, 0, NULL);
    vote_stop_cpu2_core(CPU2_USER_AUD_ASR);
    return ret;
}
bk_err_t aud_asr_process(char *aud_data, uint32_t aud_len)
{
    aud_asr_mb_data_t msg_data;
    msg_data.cmd = AUD_ASR_CMD_SEND_MIC_DATA;
    msg_data.param1 = (uint32_t)aud_data;
    msg_data.param2 = aud_len;
    media_mailbox_asr_tras_data(MAJOR_CPU, MINOR_CPU, (void *)&msg_data);
    return BK_OK;
}
int aud_asr_is_start(void)
{
    return s_aud_asr_start;
}
#endif

#if CONFIG_SYS_CPU0
bk_err_t aud_asr_start(aud_asr_recv_result_callback_t recv_result_cb)
{
    s_aud_recv_result_cb = recv_result_cb;
    media_mailbox_asr_tras_init(media_mb_audio_tras_cb);
    return BK_OK;
}
bk_err_t aud_asr_stop()
{
    s_aud_recv_result_cb = NULL;
    return BK_OK;
}
#endif

#if CONFIG_SYS_CPU2
bk_err_t aud_asr_start(aud_asr_recv_result_callback_t recv_result_cb)
{
#if CONFIG_WENWEN_ASR
    extern bk_err_t aud_wenwen_asr_start(void);
    return aud_wenwen_asr_start();
#endif
#if CONFIG_WANSON_ASR
    extern bk_err_t aud_wanson_asr_start(void);
    return aud_wanson_asr_start();
#endif
    return BK_OK;
}
bk_err_t aud_asr_stop()
{
#if CONFIG_WENWEN_ASR
    extern bk_err_t aud_wenwen_asr_stop(void);
    return aud_wenwen_asr_stop();
#endif
#if CONFIG_WANSON_ASR
    extern bk_err_t aud_wanson_asr_stop(void);
    return aud_wanson_asr_stop();
#endif
    return BK_OK;
}
#endif