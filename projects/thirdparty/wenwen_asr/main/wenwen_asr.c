#include <common/bk_include.h>
#include <modules/pm.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>

#include "audio_record.h"
#include "wenwen_asr.h"
#include "mobvoi_bk7258_pipeline.h"


#define TAG "ww_asr"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


#define RAW_READ_SIZE    (320)

static audio_record_t *audio_record = NULL;
static beken_thread_t wenwen_asr_task_hdl = NULL;
static beken_queue_t wenwen_asr_msg_que = NULL;
static void *wenwen_asr_inst = NULL;


static bk_err_t wenwen_asr_send_msg(wenwen_asr_op_t op, void *param)
{
	bk_err_t ret;
	wenwen_asr_msg_t msg;

	msg.op = op;
	msg.param = param;
	if (wenwen_asr_msg_que) {
		ret = rtos_push_to_queue(&wenwen_asr_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("wenwen_asr_send_msg fail \r\n");
			return kOverrunErr;
		}

		return ret;
	}
	return kNoResourcesErr;
}

static void wenwen_asr_task_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;
	int read_size = 0;
    void *mob_memory = NULL;

	uint8_t *aud_temp_data = psram_malloc(RAW_READ_SIZE);
	if (!aud_temp_data)
	{
		BK_LOGE(TAG, "malloc aud_temp_data\n");
		goto wenwen_asr_exit;
	}
	os_memset(aud_temp_data, 0, RAW_READ_SIZE);

	unsigned int wenwen_asr_mem_size = mobvoi_dsp_get_memory_needed();
	LOGI("wenwen_asr_mem_size: %d\n", wenwen_asr_mem_size);
    wenwen_asr_mem_size = (wenwen_asr_mem_size + 4) & (~4);
	mob_memory = psram_malloc(wenwen_asr_mem_size);
	if (mob_memory == NULL)
    {
		LOGE("malloc asr_memory: %d fail\n", wenwen_asr_mem_size);
        goto wenwen_asr_exit;
	}
    os_memset(mob_memory, 0, wenwen_asr_mem_size);

	mobvoi_dsp_set_memory_base(mob_memory, wenwen_asr_mem_size);

	wenwen_asr_inst = mobvoi_bk7258_pipeline_init(16000, 10);
    if (wenwen_asr_inst == NULL)
    {
        LOGE("wenwen asr init fail\n");
        goto wenwen_asr_exit;
    }
	BK_LOGI(TAG, "Wenwen asr init OK!\n");

	wenwen_asr_op_t task_state = WENWEN_ASR_IDLE;

	wenwen_asr_msg_t msg;
	uint32_t wait_time = BEKEN_WAIT_FOREVER;
	while (1) {
		ret = rtos_pop_from_queue(&wenwen_asr_msg_que, &msg, wait_time);
		if (kNoErr == ret) {
			switch (msg.op) {
				case WENWEN_ASR_IDLE:
					task_state = WENWEN_ASR_IDLE;
					wait_time = BEKEN_WAIT_FOREVER;
					break;

				case WENWEN_ASR_EXIT:
					LOGD("goto: WENWEN_ASR_EXIT \r\n");
					goto wenwen_asr_exit;
					break;

				case WENWEN_ASR_START:
					task_state = WENWEN_ASR_START;
					wait_time = 0;
					break;

				default:
					break;
			}
		}

		/* read mic data and wenwen asr process */
		if (task_state == WENWEN_ASR_START) {
 			read_size = audio_record_read_data(audio_record, (char *)aud_temp_data, RAW_READ_SIZE);
			if (read_size == RAW_READ_SIZE) {
                int16_t out_data[160] = {0};
                mobvoi_bk7258_pipeline_process(wenwen_asr_inst, (short*)aud_temp_data, (short*)aud_temp_data, out_data);
#if 0
                /* debug, test exit */
                int result = mobvoi_bk7258_pipeline_process(wenwen_asr_inst, (short*)aud_temp_data, (short*)aud_temp_data, out_data);
                if (result == 33)
                {
                    wenwen_asr_deinit();
                }
#endif
			} else {
				BK_LOGE(TAG, "wenwen_read_mic_data fail, read_size: %d \n", read_size);
			}
		}

	}

wenwen_asr_exit:
	if (aud_temp_data) {
		psram_free(aud_temp_data);
		aud_temp_data == NULL;
	}

	if (mob_memory) {
		psram_free(mob_memory);
		mob_memory == NULL;
	}

    if (wenwen_asr_inst)
    {
        mobvoi_bk7258_pipeline_cleanup(wenwen_asr_inst);
        wenwen_asr_inst = NULL;
    }

	/* delete msg queue */
	ret = rtos_deinit_queue(&wenwen_asr_msg_que);
	if (ret != kNoErr) {
		LOGE("delete message queue fail \r\n");
	}
	wenwen_asr_msg_que = NULL;
	LOGI("delete wenwen_asr_que \r\n");

	/* delete task */
	wenwen_asr_task_hdl = NULL;

	rtos_delete_thread(NULL);
}

static bk_err_t wenwen_asr_task_init(void)
{
	bk_err_t ret = BK_OK;

	ret = rtos_init_queue(&wenwen_asr_msg_que,
						  "wenwen_asr_que",
						  sizeof(wenwen_asr_msg_t),
						  2);
	if (ret != kNoErr)
	{
		LOGE("ceate wenwen asr message queue fail\n");
		return BK_FAIL;
	}
	LOGI("ceate wenwen asr message queue complete\n");

	ret = rtos_create_thread(&wenwen_asr_task_hdl,
							 6,
							 "wenwen_asr",
							 (beken_thread_function_t)wenwen_asr_task_main,
							 8192,
							 NULL);
	if (ret != kNoErr)
	{
		LOGE("Error: Failed to create send_mic_task task\n");
		ret = rtos_deinit_queue(&wenwen_asr_msg_que);
		if (ret != kNoErr) {
			LOGE("delete message queue fail\n");
		}
		wenwen_asr_msg_que = NULL;
		return kGeneralErr;
	}

	LOGI("init wenwen_asr_task task complete\n");

	return BK_OK;
}


bk_err_t wenwen_asr_init(void)
{
	bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_480M);

    audio_record_cfg_t config = DEFAULT_AUDIO_RECORD_CONFIG();
    config.sampRate = 16000;
    config.adc_gain = 0x3f;
    config.frame_size = RAW_READ_SIZE;
    config.pool_size = config.frame_size * 2;
	audio_record = audio_record_create(AUDIO_RECORD_ONBOARD_MIC, &config);
    if (!audio_record)
    {
        LOGE("create audio record fail\n");
        return BK_FAIL;
    }

	/* init wenwen asr task */
	wenwen_asr_task_init();

	return BK_OK;
}

bk_err_t wenwen_asr_deinit(void)
{
	wenwen_asr_send_msg(WENWEN_ASR_EXIT, NULL);

	audio_record_destroy(audio_record);
    audio_record = NULL;

	bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);

	return BK_OK;
}

bk_err_t wenwen_asr_start(void)
{
	audio_record_open(audio_record);

	wenwen_asr_send_msg(WENWEN_ASR_START, NULL);

	return BK_OK;
}

bk_err_t wenwen_asr_stop(void)
{
	wenwen_asr_send_msg(WENWEN_ASR_IDLE, NULL);

	audio_record_close(audio_record);

	return BK_OK;
}

