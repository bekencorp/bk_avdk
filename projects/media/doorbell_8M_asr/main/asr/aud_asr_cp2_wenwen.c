#include <os/os.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bk_fifo.h"
#include "modules/pm.h"
#include <driver/pwr_clk.h>
#include "aud_asr.h"
#include "mobvoi_bk7258_pipeline.h"
#include "media_core.h"
#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "media_mailbox_asr_tras.h"

#define AUDIO_MAX_KFIFO_SIZE (1024 * 2)
#define RAW_READ_SIZE (320)

static kfifo_ptr_t s_audio_kfifo = NULL;
static beken_thread_t s_audio_rx_thread = NULL;
static beken_semaphore_t s_audio_rx_sem = NULL;
static uint8_t s_run_th = 0;

#define ENABLE_BK7258_KWS_PIPELINE 1

#ifdef ENABLE_BK7258_KWS_PIPELINE
void *inst;
#endif

static void aud_wenwen_asr_recv_mic_data(aud_asr_mb_data_t *msg_data)
{
    int data_len = msg_data->param2;
    int unused;
    unused = kfifo_unused(s_audio_kfifo);
    if (!s_run_th) {
        return;
    }

    if (unused < data_len) {
        bk_printf("kfifo full,unused:%d,data_len:%d!!!!\r\n", unused, data_len);
    }else{
        kfifo_put(s_audio_kfifo, (unsigned char *)msg_data->param1, msg_data->param2);
        rtos_set_semaphore(&s_audio_rx_sem);
    }
}
static bk_err_t media_mb_audio_tras_cb(media_cpu_t src, void *param)
{
    aud_asr_mb_data_t *msg_data = (aud_asr_mb_data_t *)param;
    switch (msg_data->cmd)
    {
    case AUD_ASR_CMD_SEND_MIC_DATA:
        aud_wenwen_asr_recv_mic_data(msg_data);
        break;
    default:

        break;
    }
    return BK_OK;
}

static bk_err_t aud_wenwen_asr_init(void)
{
    s_audio_kfifo = kfifo_alloc(AUDIO_MAX_KFIFO_SIZE);
    if (NULL == s_audio_kfifo)
    {
        goto error_0;
    }
    media_mailbox_asr_tras_init(media_mb_audio_tras_cb);
    int ret = rtos_init_semaphore_ex(&s_audio_rx_sem, 1, 0);
    if (ret != 0)
    {
        os_printf("[%s][%d]tx sem init failed\r\n", __func__, __LINE__);
        s_audio_rx_sem = NULL;
        goto error_1;
    }
    
    return BK_OK;
error_1:
    kfifo_free(s_audio_kfifo);
    s_audio_kfifo = NULL;
error_0:
    return BK_FAIL;
}

static bk_err_t aud_wenwen_asr_deinit(void)
{
    if (s_audio_rx_sem){
        rtos_deinit_semaphore(&s_audio_rx_sem);
        s_audio_rx_sem = NULL;
    }
    if (s_audio_kfifo){
        kfifo_free(s_audio_kfifo);
        s_audio_kfifo = NULL;
    }
    return BK_OK;
}

static void asr_cpu2_rx_main(beken_thread_arg_t arg)
{
    int result = 0;
    aud_asr_mb_data_t msg_data = {0};
    uint8_t *aud_temp_data = os_malloc(RAW_READ_SIZE * 2);
    uint8_t *aud_temp_ptr = aud_temp_data + RAW_READ_SIZE;
    if (!aud_temp_data)
    {
        os_printf("malloc aud_temp_data fail.\r\n");
        s_run_th = 0;
        rtos_delete_thread(NULL);
        return;
    }
    os_memset(aud_temp_data, 0, RAW_READ_SIZE * 2);
//	int mem = mobvoi_dsp_get_memory_needed();
//	os_printf("====mem:%x===\r\n", mem);
//	void *mob_memory = psram_malloc((mem + 4) & (~4));
//	os_printf("============cpu2 asr task 33333==========\r\n");
//	if (mob_memory == NULL)
//	{
//	 os_printf("malloc err\n");
//	 goto exit_0;
//	}
//	mobvoi_dsp_set_memory_base(mob_memory, mem);
    inst = mobvoi_bk7258_pipeline_init(16000, 10);
    if(!inst){
        os_printf("mobvoi_bk7258_pipeline_init err\n");
        goto exit_0;
    }
    os_printf("Wewnen_ASR_Init OK!, s_run_t=%d\n", s_run_th);
    while (s_run_th)
    {
        rtos_get_semaphore(&s_audio_rx_sem, BEKEN_WAIT_FOREVER);
        if (!s_run_th)
        {
            break;
        }
        if((RAW_READ_SIZE * 2) > kfifo_data_size(s_audio_kfifo)){
            continue;
        }
        result = kfifo_get(s_audio_kfifo, (uint8_t *)aud_temp_data, RAW_READ_SIZE * 2);
        if (result < RAW_READ_SIZE * 2)
        {
            continue;
        }

        //os_printf("get data:[%02x][%02x][%02x][%02x]\n", aud_temp_data[0], aud_temp_data[1], aud_temp_data[RAW_READ_SIZE * 2 - 2], aud_temp_data[RAW_READ_SIZE * 2 - 1]);
#ifdef ENABLE_BK7258_KWS_PIPELINE
        int16_t out_data[160] = {0};
        result = mobvoi_bk7258_pipeline_process(inst, (short *)aud_temp_data, NULL, out_data); // for ovkws
        if (result != -1 && result != 100)
        {
            os_printf("mobvoi_bk7258_pipeline_process result : %d\n", result);
            msg_data.cmd = AUD_ASR_CMD_RSP_RESULT;
            msg_data.param1 = (uint32_t)result;
            media_mailbox_asr_tras_data(MINOR_CPU, MONO_CPU, (void *)&msg_data);
        }
        result = mobvoi_bk7258_pipeline_process(inst, (short *)aud_temp_ptr, NULL, out_data); // for ovkws
        if (result != -1 && result != 100)
        {
            os_printf("mobvoi_bk7258_pipeline_process result : %d\n", result);
            msg_data.cmd = AUD_ASR_CMD_RSP_RESULT;
            msg_data.param1 = (uint32_t)result;
            media_mailbox_asr_tras_data(MINOR_CPU, MONO_CPU, (void *)&msg_data);
        }
#endif
    }
    mobvoi_bk7258_pipeline_cleanup(inst);
exit_0:
    s_run_th = 0;
//	if(mob_memory){
//	 os_free(mob_memory);
//	 mob_memory = NULL;
//	}
    if(aud_temp_data){
        os_free(aud_temp_data);
        aud_temp_data = NULL;
    }
    rtos_delete_thread(NULL);
    return;
}

bk_err_t aud_wenwen_asr_start(void)
{
    if (!s_run_th)
    {
        bk_err_t ret = aud_wenwen_asr_init();
        if(ret){
            return BK_FAIL;
        }
        s_run_th = 1;
        ret = rtos_create_thread(&s_audio_rx_thread, 7, "asr_rx", asr_cpu2_rx_main, 8 * 1024, NULL);
        if (ret != 0)
        {
            os_printf("[%s][%d]asr rx thread create failed\r\n", __func__, __LINE__);
            s_audio_rx_thread = NULL;
            s_run_th = 0;
            return BK_FAIL;
        }
    }
    else
    {
        os_printf("[%s][%d]asr rx thread already start\r\n", __func__, __LINE__);
    }
    return BK_OK;
}

bk_err_t aud_wenwen_asr_stop(void)
{
    if (s_run_th)
    {
        s_run_th = 0;
        rtos_set_semaphore(&s_audio_rx_sem);
    }
    aud_wenwen_asr_deinit();
    return BK_OK;
}
