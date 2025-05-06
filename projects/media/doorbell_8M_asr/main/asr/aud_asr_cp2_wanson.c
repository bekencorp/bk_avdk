#include <os/os.h>
#include "FreeRTOS.h"
#include "task.h"
#include "bk_fifo.h"
#include "modules/pm.h"
#include <driver/pwr_clk.h>
#include "aud_asr.h"
#include "asr.h"
#include "media_core.h"
#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "media_mailbox_asr_tras.h"

#define AUDIO_MAX_KFIFO_SIZE (1024 * 2)

#define RAW_READ_SIZE (480)

static kfifo_ptr_t s_audio_kfifo = NULL;
static beken_thread_t s_audio_rx_thread = NULL;
static beken_semaphore_t s_audio_rx_sem = NULL;
static uint8_t s_run_th = 0;

static char result0[13] = {0xE5,0xB0,0x8F,0xE8,0x9C,0x82,0xE7,0xAE,0xA1,0xE5,0xAE,0xB6,0x00};
static char result1[13] = {0xE9,0x98,0xBF,0xE5,0xB0,0x94,0xE7,0xB1,0xB3,0xE8,0xAF,0xBA,0x00};
static char result2[13] = {0xE4,0xBC,0x9A,0xE5,0xAE,0xA2,0xE6,0xA8,0xA1,0xE5,0xBC,0x8F,0x00};
static char result3[13] = {0xE7,0x94,0xA8,0xE9,0xA4,0x90,0xE6,0xA8,0xA1,0xE5,0xBC,0x8F,0x00};
static char resulta[13] = {0xE7,0xA6,0xBB,0xE5,0xBC,0x80,0xE6,0xA8,0xA1,0xE5,0xBC,0x8F,0x00};
static char resultc[13] = {0xE5,0x9B,0x9E,0xE5,0xAE,0xB6,0xE6,0xA8,0xA1,0xE5,0xBC,0x8F,0x00};
const static char *d_test = NULL;
const static char **text = NULL;
static float __maybe_unused score = 0.0;
extern int os_strcmp(const char *s1, const char *s2);
static bk_err_t wanson_asr_result_handle(void)
{
	BK_LOG_RAW("ASR Result: \n");
	for (uint8_t n = 0; n < 13; n++)
	{
		BK_LOG_RAW("0x%02x,", (uint8_t)d_test[n]);
		if (d_test[n] == 0x00) {
			break;
		}
	}
	BK_LOG_RAW("\n");
	if (os_strcmp(d_test, result0) == 0) {	//
	//	os_printf("%s \n", "xiao feng guan jia ");
	} else if (os_strcmp(d_test, result1) == 0) {    //
	//	os_printf("%s \n", "a er mi nuo ");
	} else if (os_strcmp(d_test, result2) == 0) {    //
	//	os_printf("%s \n", "hui ke mo shi ");
	} else if (os_strcmp(d_test, result3) == 0) {    //
	//	os_printf("%s \n", "yong can mo shi ");
	} else if (os_strcmp(d_test, resulta) == 0) {    //
	//	os_printf("%s \n", "li kai mo shi ");
	} else if (os_strcmp(d_test, resultc) == 0) {    //
	//	os_printf("%s \n", "hui jia mo shi ");
	} else {
		os_printf("other command \n");
	}

	return BK_OK;
}

static void aud_wanson_asr_recv_mic_data(aud_asr_mb_data_t *msg_data)
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
        aud_wanson_asr_recv_mic_data(msg_data);
        break;
    default:

        break;
    }
    return BK_OK;
}

static bk_err_t aud_wanson_asr_init(void)
{
	{
		if (!d_test)
		{
			d_test = (char *)os_malloc(sizeof(char) * 13);
			if (d_test == NULL)
				os_printf("text malloc fail\n");
		}

		text = &d_test;
	}

    s_audio_kfifo = kfifo_alloc(AUDIO_MAX_KFIFO_SIZE);
    if (NULL == s_audio_kfifo)
    {
        goto error_0;
    }
    media_mailbox_asr_tras_init(media_mb_audio_tras_cb);
    bk_err_t ret = rtos_init_semaphore_ex(&s_audio_rx_sem, 1, 0);
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

static bk_err_t aud_wanson_asr_deinit(void)
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
	if (!aud_temp_data)
	{
        os_printf("malloc aud_temp_data fail.\r\n");
        goto exit_0;
	}

    os_memset(aud_temp_data, 0, RAW_READ_SIZE * 2);
    if (Wanson_ASR_Init() < 0)
    {
        os_printf("Wanson_ASR_Init Failed!\n");
        os_free(aud_temp_data);
        goto exit_0;
    }
    Wanson_ASR_Reset();
    os_printf("Wanson_ASR_Init OK!\n");
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

        result = Wanson_ASR_Recog((short*)aud_temp_data, RAW_READ_SIZE, text, &score);

        if (result == 1) {
            wanson_asr_result_handle();
            msg_data.cmd = AUD_ASR_CMD_RSP_RESULT;
            msg_data.param1 = result;
            media_mailbox_asr_tras_data(MINOR_CPU, MONO_CPU, (void *)&msg_data);
        }
    }
    Wanson_ASR_Release();
exit_0:
    s_run_th = 0;
    if(aud_temp_data){
        os_free(aud_temp_data);
        aud_temp_data = NULL;
    }
    rtos_delete_thread(NULL);
    return;
}

bk_err_t aud_wanson_asr_start(void)
{
    if (!s_run_th)
    {
        bk_err_t ret = aud_wanson_asr_init();
        if(ret) {
            return BK_FAIL;
        }
        s_run_th = 1;
        ret = rtos_create_psram_thread(&s_audio_rx_thread, 7, "asr_rx", asr_cpu2_rx_main, 8 * 1024, NULL);
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

bk_err_t aud_wanson_asr_stop(void)
{
    if (s_run_th)
    {
        s_run_th = 0;
        rtos_set_semaphore(&s_audio_rx_sem);
    }
    aud_wanson_asr_deinit();
    return BK_OK;
}
