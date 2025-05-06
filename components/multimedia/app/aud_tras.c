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
#include <os/mem.h>
#include <os/str.h>
#include "stdio.h"
#include <driver/audio_ring_buff.h>
#include "aud_tras.h"
#include <driver/uart.h>
#include "gpio_driver.h"
#include <driver/timer.h>
#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif


#define AUD_TRAS "aud_tras"

#define LOGI(...) BK_LOGI(AUD_TRAS, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(AUD_TRAS, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(AUD_TRAS, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(AUD_TRAS, ##__VA_ARGS__)

#define TU_QITEM_COUNT      (40)
#define AUD_TRAS_BUFF_SIZE    (320 * 5)

#define AUD_TRAS_MAX_FRAME_SIZE     (768)   //refer to encoder output

#define AUD_TX_DEBUG

typedef struct
{
	beken_thread_t aud_tras_task_hdl;
	beken_queue_t aud_tras_int_msg_que;
	uint8_t *aud_tras_buff_addr;
	RingBufferContext aud_tras_rb;			//save mic data needed to send by aud_tras task
	bool is_running;
} aud_tras_info_t;

#define AUD_TX_COUNT

#ifdef AUD_TX_COUNT
#include "count_util.h"
static count_util_t aud_tx_count_util = {0};
#define AUD_TX_COUNT_INTERVAL           (1000 * 2)
#define AUD_TX_COUNT_TAG                "AUD Tx"

#define AUD_TX_COUNT_OPEN()               count_util_create(&aud_tx_count_util, AUD_TX_COUNT_INTERVAL, AUD_TX_COUNT_TAG)
#define AUD_TX_COUNT_CLOSE()              count_util_destroy(&aud_tx_count_util)
#define AUD_TX_COUNT_ADD_SIZE(size)       count_util_add_size(&aud_tx_count_util, size)
#else
#define AUD_TX_COUNT_OPEN()
#define AUD_TX_COUNT_CLOSE()
#define AUD_TX_COUNT_ADD_SIZE(size)
#endif  //AUD_TX_COUNT


//#define AUD_TX_DATA_DUMP

#ifdef AUD_TX_DATA_DUMP

/* dump audio tx data by uart or tfcard, only choose one */
#define AUD_TX_DATA_DUMP_BY_UART
//#define AUD_TX_DATA_DUMP_BY_TFCARD       /* you must sure CONFIG_FATFS=y */

#ifdef AUD_TX_DATA_DUMP_BY_UART
#include "uart_util.h"
static uart_util_t g_aud_tx_uart_util = {0};
#define AUD_TX_DATA_DUMP_UART_ID            (1)
#define AUD_TX_DATA_DUMP_UART_BAUD_RATE     (2000000)
#endif

#ifdef AUD_TX_DATA_DUMP_BY_TFCARD
#include "tfcard_util.h"
static tfcard_util_t g_aud_tx_tfcard_util = {0};
#define AUD_TX_DATA_DUMP_TFCARD_OUT_NAME     "aud_tx.pcm"
#endif

static void aud_tx_data_dump_open(void)
{
#ifdef AUD_TX_DATA_DUMP_BY_UART
    uart_util_create(&g_aud_tx_uart_util, AUD_TX_DATA_DUMP_UART_ID, AUD_TX_DATA_DUMP_UART_BAUD_RATE);
#endif

#ifdef AUD_TX_DATA_DUMP_BY_TFCARD
    tfcard_util_create(&g_aud_tx_tfcard_util, AUD_TX_DATA_DUMP_TFCARD_OUT_NAME);
#endif
}

static void aud_tx_data_dump_close(void)
{
#ifdef AUD_TX_DATA_DUMP_BY_UART
    uart_util_destroy(&g_aud_tx_uart_util);
#endif

#ifdef AUD_TX_DATA_DUMP_BY_TFCARD
    tfcard_util_destroy(&g_aud_tx_tfcard_util);
#endif
}

static void aud_tx_data_dump_data(void *data_buf, uint32_t len)
{
#ifdef AUD_TX_DATA_DUMP_BY_UART
    uart_util_tx_data(&g_aud_tx_uart_util, data_buf, len);
#endif

#ifdef AUD_TX_DATA_DUMP_BY_TFCARD
    tfcard_util_tx_data(&g_aud_tx_tfcard_util, data_buf, len);
#endif
}

#define AUD_TX_DATA_DUMP_OPEN()                        aud_tx_data_dump_open()
#define AUD_TX_DATA_DUMP_CLOSE()                       aud_tx_data_dump_close()
#define AUD_TX_DATA_DUMP_DATA(data_buf, len)           aud_tx_data_dump_data(data_buf, len)
#else
#define AUD_TX_DATA_DUMP_OPEN()
#define AUD_TX_DATA_DUMP_CLOSE()
#define AUD_TX_DATA_DUMP_DATA(data_buf, len)
#endif  //AUD_TX_DATA_DUMP


static aud_tras_setup_t aud_trs_setup_bk = {0};
static aud_tras_info_t *aud_tras_info = NULL;

static beken_semaphore_t aud_tras_task_sem = NULL;
static uint32_t aud_tras_max_frame_size = AUD_TRAS_MAX_FRAME_SIZE;


static void *audio_tras_malloc(uint32_t size)
{
#if CONFIG_PSRAM_AS_SYS_MEMORY
	return psram_malloc(size);
#else
	return os_malloc(size);
#endif
}

static void audio_tras_free(void *mem)
{
	os_free(mem);
}

bk_err_t aud_tras_send_msg(aud_tras_op_t op, void *param)
{
	bk_err_t ret;
	aud_tras_msg_t msg;

	msg.op = op;
	msg.param = param;
	if (aud_tras_info->is_running && aud_tras_info && aud_tras_info->aud_tras_int_msg_que) {
		ret = rtos_push_to_queue(&aud_tras_info->aud_tras_int_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("aud_tras_send_int_msg fail \r\n");
			return kOverrunErr;
		}

		return ret;
	}
	return kNoResourcesErr;
}

bk_err_t aud_tras_deinit(void)
{
	bk_err_t ret = BK_OK;
	aud_tras_msg_t msg;

	msg.op = AUD_TRAS_EXIT;
	msg.param = NULL;
	if (aud_tras_info && aud_tras_info->aud_tras_int_msg_que) {
		ret = rtos_push_to_queue_front(&aud_tras_info->aud_tras_int_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("audio send msg: AUD_TRAS_EXIT fail \r\n");
			return kOverrunErr;
		}
	} else {
		return BK_OK;
	}

	ret = rtos_get_semaphore(&aud_tras_task_sem, BEKEN_WAIT_FOREVER);
	if (ret != BK_OK)
	{
		LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
		return BK_FAIL;
	}

	if(aud_tras_task_sem)
	{
		rtos_deinit_semaphore(&aud_tras_task_sem);
		aud_tras_task_sem = NULL;
	}

	return kNoResourcesErr;
}

static void aud_tras_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;
	uint32_t fill_size = 0;
	aud_tras_setup_t *aud_trs_setup = NULL;
	aud_trs_setup = (aud_tras_setup_t *)param_data;
	uint8_t *aud_temp_data = NULL;
	int tx_size = 0;
    uint32_t aud_tras_frame_size = 0;

    AUD_TX_DATA_DUMP_OPEN();

    AUD_TX_COUNT_OPEN();

	GLOBAL_INT_DECLARATION();
	aud_temp_data = audio_tras_malloc(aud_tras_max_frame_size);
	if (!aud_temp_data)
	{
		LOGE("malloc aud_temp_data\n");
		goto aud_tras_exit;
	}
	os_memset(aud_temp_data, 0, aud_tras_max_frame_size);


	rtos_set_semaphore(&aud_tras_task_sem);

	aud_tras_info->is_running = true;

	//aud_tras_send_msg(AUD_TRAS_TX, NULL);

	aud_tras_msg_t msg;
	while (1) {
		ret = rtos_pop_from_queue(&aud_tras_info->aud_tras_int_msg_que, &msg, BEKEN_WAIT_FOREVER);
		if (kNoErr == ret) {
			switch (msg.op) {
				case AUD_TRAS_IDLE:
					break;

				case AUD_TRAS_EXIT:
					LOGD("goto: AUD_TRAS_EXIT \r\n");
					goto aud_tras_exit;
					break;

				case AUD_TRAS_TX:
                    /* check whether trasfer size out of aud_tras_max_frame_size */
                    aud_tras_frame_size = (uint32_t)msg.param;
                    if (aud_tras_frame_size > aud_tras_max_frame_size) {
                        audio_tras_free(aud_temp_data);
                        aud_temp_data = NULL;
                        aud_tras_max_frame_size = aud_tras_frame_size;
                        aud_temp_data = audio_tras_malloc(aud_tras_max_frame_size);
                        if (!aud_temp_data)
                        {
                            LOGE("malloc aud_temp_data\n");
                            aud_tras_max_frame_size = 0;
                            //goto aud_tras_exit;
                            /* discard frame data */
                            //TODO
                            break;
                        }
                        os_memset(aud_temp_data, 0, aud_tras_max_frame_size);
                    }
#if (CONFIG_CACHE_ENABLE)
					flush_all_dcache();
#endif
					GLOBAL_INT_DISABLE();
					fill_size = ring_buffer_read(&aud_tras_info->aud_tras_rb, aud_temp_data, aud_tras_frame_size);
					GLOBAL_INT_RESTORE();

                    if (fill_size <= 0 || fill_size != aud_tras_frame_size)
                    {
                        LOGW("fill_size: %d is not aud_tras_frame_size: %d\n", fill_size, aud_tras_frame_size);
                    }

                    AUD_TX_DATA_DUMP_DATA(aud_temp_data, fill_size);

					tx_size = aud_trs_setup->aud_tras_send_data_cb(aud_temp_data, fill_size);
					if (tx_size > 0) {
    					AUD_TX_COUNT_ADD_SIZE(tx_size);
					}
					break;

				default:
					break;
			}
		}
	}

aud_tras_exit:
	aud_tras_info->is_running = false;

	if (aud_temp_data) {
		audio_tras_free(aud_temp_data);
		aud_temp_data = NULL;
	}

    AUD_TX_COUNT_CLOSE();

    AUD_TX_DATA_DUMP_CLOSE();

	if (aud_tras_info->aud_tras_buff_addr) {
		ring_buffer_clear(&aud_tras_info->aud_tras_rb);
		audio_tras_free(aud_tras_info->aud_tras_buff_addr);
		aud_tras_info->aud_tras_buff_addr = NULL;
	}

	/* delete msg queue */
	ret = rtos_deinit_queue(&aud_tras_info->aud_tras_int_msg_que);
	if (ret != kNoErr) {
		LOGE("delete message queue fail \r\n");
	}
	aud_tras_info->aud_tras_int_msg_que = NULL;
	LOGI("delete aud_tras_int_msg_que \r\n");

	os_memset(&aud_trs_setup_bk, 0, sizeof(aud_tras_setup_t));

	/* delete task */
	aud_tras_info->aud_tras_task_hdl = NULL;

	if (aud_tras_info)
	{
		audio_tras_free(aud_tras_info);
		aud_tras_info = NULL;
	}

	rtos_set_semaphore(&aud_tras_task_sem);

	rtos_delete_thread(NULL);
}

bk_err_t aud_tras_init(aud_tras_setup_t *setup_cfg)
{
	bk_err_t ret = BK_OK;

	aud_tras_info = audio_tras_malloc(sizeof(aud_tras_info_t));

	if (aud_tras_info == NULL)
	{
		LOGE("malloc aud_tras_info\n");
		return BK_FAIL;
	}
	os_memset(aud_tras_info, 0, sizeof(aud_tras_info_t));

	aud_tras_info->aud_tras_buff_addr = audio_tras_malloc(AUD_TRAS_BUFF_SIZE + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (!aud_tras_info->aud_tras_buff_addr) {
		LOGE("malloc aud_tras_buff_addr\n");
		goto out;
	}
	ring_buffer_init(&aud_tras_info->aud_tras_rb, aud_tras_info->aud_tras_buff_addr, AUD_TRAS_BUFF_SIZE + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);
	LOGD("aud_tras_info->aud_tras_rb: %p \n", &aud_tras_info->aud_tras_rb);

	os_memcpy(&aud_trs_setup_bk, setup_cfg, sizeof(aud_tras_setup_t));

	if (aud_tras_task_sem == NULL) {
		ret = rtos_init_semaphore(&aud_tras_task_sem, 1);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, create audio tras task semaphore failed \n", __func__, __LINE__);
			goto out;
		}
	}

	if ((!aud_tras_info->aud_tras_task_hdl) && (!aud_tras_info->aud_tras_int_msg_que))
	{
		ret = rtos_init_queue(&aud_tras_info->aud_tras_int_msg_que,
							  "aud_tras_int_que",
							  sizeof(aud_tras_msg_t),
							  TU_QITEM_COUNT);
		if (ret != kNoErr)
		{
			LOGE("create audio transfer internal message queue fail\n");
			goto out;
		}
		LOGD("create audio transfer internal message queue complete\n");

		ret = rtos_create_thread(&aud_tras_info->aud_tras_task_hdl,
								 4,
								 "aud_tras",
								 (beken_thread_function_t)aud_tras_main,
								 1024 * 2,
								 (beken_thread_arg_t)&aud_trs_setup_bk);
		if (ret != kNoErr)
		{
			LOGE("Error: Failed to create aud_tras task \n");
			return kGeneralErr;
		}

		ret = rtos_get_semaphore(&aud_tras_task_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
			goto out;
		}

		LOGD("init aud_tras task complete \n");
	}
	else
	{
		goto out;
	}

	return BK_OK;

out:
	if(aud_tras_task_sem)
	{
		rtos_deinit_semaphore(&aud_tras_task_sem);
		aud_tras_task_sem = NULL;
	}

	if (aud_tras_info->aud_tras_int_msg_que)
	{
		ret = rtos_deinit_queue(&aud_tras_info->aud_tras_int_msg_que);
		if (ret != kNoErr) {
			LOGE("delete message queue fail \r\n");
		}
		aud_tras_info->aud_tras_int_msg_que = NULL;
	}

	if (aud_tras_info->aud_tras_buff_addr) {
		ring_buffer_clear(&aud_tras_info->aud_tras_rb);
		audio_tras_free(aud_tras_info->aud_tras_buff_addr);
		aud_tras_info->aud_tras_buff_addr == NULL;
	}

	if (aud_tras_info)
	{
		audio_tras_free(aud_tras_info);
		aud_tras_info = NULL;
	}

	return BK_FAIL;
}

RingBufferContext *aud_tras_get_tx_rb(void)
{
	return &aud_tras_info->aud_tras_rb;
}

