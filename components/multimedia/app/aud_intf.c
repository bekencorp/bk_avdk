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
#include "sys_driver.h"
#include "aud_intf_private.h"

#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include <driver/pwr_clk.h>
#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif
#include "aud_tras.h"

#define AUD_INTF_TAG "aud_intf"

#define LOGI(...) BK_LOGI(AUD_INTF_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(AUD_INTF_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(AUD_INTF_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(AUD_INTF_TAG, ##__VA_ARGS__)

/* check aud_intf busy status */
#define CHECK_AUD_INTF_BUSY_STA() do {\
		if (aud_intf_info.api_info.busy_status) {\
			return BK_ERR_AUD_INTF_BUSY;\
		}\
		aud_intf_info.api_info.busy_status = true;\
	} while(0)


#define AUD_RX_COUNT

#ifdef AUD_RX_COUNT

#include "count_util.h"
static count_util_t aud_rx_count_util = {0};
#define AUD_RX_COUNT_INTERVAL           (1000 * 5)
#define AUD_RX_COUNT_TAG                "AUD Rx"

#define AUD_RX_COUNT_OPEN()               count_util_create(&aud_rx_count_util, AUD_RX_COUNT_INTERVAL, AUD_RX_COUNT_TAG)
#define AUD_RX_COUNT_CLOSE()              count_util_destroy(&aud_rx_count_util)
#define AUD_RX_COUNT_ADD_SIZE(size)       count_util_add_size(&aud_rx_count_util, size)

#else

#define AUD_RX_COUNT_OPEN()
#define AUD_RX_COUNT_CLOSE()
#define AUD_RX_COUNT_ADD_SIZE(size)

#endif  //AUD_RX_COUNT

//aud_intf_all_setup_t aud_all_setup;
aud_intf_info_t aud_intf_info = DEFAULT_AUD_INTF_CONFIG();

static beken_semaphore_t aud_intf_task_sem = NULL;

/* extern api */
static bk_err_t aud_intf_voc_write_spk_data(uint8_t *dac_buff, uint32_t size);

static void *audio_intf_malloc(uint32_t size)
{
#if CONFIG_PSRAM_AS_SYS_MEMORY
	return psram_malloc(size);
#else
	return os_malloc(size);
#endif
}

static void audio_intf_free(void *mem)
{
	os_free(mem);
}

bk_err_t mailbox_media_aud_send_msg(media_event_t event, void *param)
{
	bk_err_t ret = BK_OK;

	ret = msg_send_req_to_media_app_mailbox_sync(event, (uint32_t)param, NULL);
	if (ret != kNoErr)
	{
		LOGE("%s, %d, fail, ret: 0x%x\n", __func__, __LINE__, ret);
	}

	aud_intf_info.api_info.busy_status = false;
	return ret;
}


bk_err_t aud_intf_send_msg(aud_intf_event_t op, uint32_t data, uint32_t size)
{
	bk_err_t ret;
	aud_intf_msg_t msg;

	msg.op = op;
	msg.data = data;
	msg.size = size;
	if (aud_intf_info.aud_intf_msg_que) {
		ret = rtos_push_to_queue(&aud_intf_info.aud_intf_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("%s, %d, aud_tras_send_int_msg fail \n", __func__, __LINE__);
			return kOverrunErr;
		}

		return ret;
	}
	return kNoResourcesErr;
}

static bk_err_t aud_intf_deinit(void)
{
	bk_err_t ret;
	aud_intf_msg_t msg;

	msg.op = AUD_INTF_EVENT_EXIT;
	msg.data = 0;
	msg.size = 0;
	if (aud_intf_info.aud_intf_msg_que) {
		ret = rtos_push_to_queue_front(&aud_intf_info.aud_intf_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("%s, %d, audio send msg: AUD_INTF_EVENT_EXIT fail \n", __func__, __LINE__);
			return kOverrunErr;
		}

		ret = rtos_get_semaphore(&aud_intf_task_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
			return BK_FAIL;
		}

		if(aud_intf_task_sem)
		{
			rtos_deinit_semaphore(&aud_intf_task_sem);
			aud_intf_task_sem = NULL;
		}

		return ret;
	}
	return kNoResourcesErr;
}

static void aud_intf_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;

	rtos_set_semaphore(&aud_intf_task_sem);

	aud_intf_msg_t msg;
	while (1) {
		ret = rtos_pop_from_queue(&aud_intf_info.aud_intf_msg_que, &msg, BEKEN_WAIT_FOREVER);
		if (kNoErr == ret) {
			switch (msg.op) {
				case AUD_INTF_EVENT_IDLE:
					break;

				case AUD_INTF_EVENT_MIC_TX:
					if (aud_intf_info.drv_info.setup.aud_intf_tx_mic_data) {
						aud_intf_info.drv_info.setup.aud_intf_tx_mic_data((unsigned char *)msg.data, msg.size);
					}
					break;

				case AUD_INTF_EVENT_SPK_RX:
					if (aud_intf_info.drv_info.setup.aud_intf_rx_spk_data) {
						aud_intf_info.drv_info.setup.aud_intf_rx_spk_data(msg.size);
					}
					break;

				case AUD_INTF_EVENT_UAC_STATE:
					if (aud_intf_info.aud_intf_uac_connect_state_cb) {
						aud_intf_info.aud_intf_uac_connect_state_cb((uint8_t)msg.data);
					}
					break;

				case AUD_INTF_EVENT_EXIT:
					goto aud_intf_exit;
					break;

				default:
					break;
			}
		}
	}

aud_intf_exit:

	/* delete msg queue */
	ret = rtos_deinit_queue(&aud_intf_info.aud_intf_msg_que);
	if (ret != kNoErr) {
		LOGE("%s, %d, delete message queue fail\n", __func__, __LINE__);
	}
	aud_intf_info.aud_intf_msg_que = NULL;
	LOGI("%s, %d, delete aud_intf_int_msg_que \n", __func__, __LINE__);

	/* delete task */
	aud_intf_info.aud_intf_thread_hdl = NULL;

	rtos_set_semaphore(&aud_intf_task_sem);

	rtos_delete_thread(NULL);
}


bk_err_t bk_aud_intf_set_mode(aud_intf_work_mode_t work_mode)
{
	bk_err_t ret = BK_OK;

	if (aud_intf_info.drv_info.setup.work_mode == work_mode)
		return BK_ERR_AUD_INTF_OK;

	CHECK_AUD_INTF_BUSY_STA();
	aud_intf_work_mode_t temp = aud_intf_info.drv_info.setup.work_mode;
	aud_intf_info.drv_info.setup.work_mode = work_mode;

	if (work_mode == AUD_INTF_WORK_MODE_VOICE) {
		if (aud_intf_task_sem == NULL) {
			ret = rtos_init_semaphore(&aud_intf_task_sem, 1);
			if (ret != BK_OK)
			{
				LOGE("%s, %d, create audio intf task semaphore failed \n", __func__, __LINE__);
				goto fail;
			}
		}

		if (!aud_intf_info.aud_intf_msg_que) {
			ret = rtos_init_queue(&aud_intf_info.aud_intf_msg_que,
								  "aud_intf_int_que",
								  sizeof(aud_intf_msg_t),
								  10);
			if (ret != kNoErr) {
				LOGE("%s, %d, ceate aud_intf_msg_que fail \n", __func__, __LINE__);
				aud_intf_info.aud_intf_msg_que = NULL;
				goto fail;
			}
			LOGD("%s, %d, ceate aud_intf_msg_que complete \n", __func__, __LINE__);
		}

		if (!aud_intf_info.aud_intf_thread_hdl) {
			ret = rtos_create_thread(&aud_intf_info.aud_intf_thread_hdl,
								 6,
								 "aud_intf",
								 (beken_thread_function_t)aud_intf_main,
								 2048,
								 NULL);
			if (ret != kNoErr) {
				LOGE("%s, %d, create audio transfer task fail \n", __func__, __LINE__);
				rtos_deinit_queue(&aud_intf_info.aud_intf_msg_que);
				aud_intf_info.aud_intf_msg_que = NULL;
				aud_intf_info.aud_intf_thread_hdl = NULL;
				goto fail;
			}

			ret = rtos_get_semaphore(&aud_intf_task_sem, BEKEN_WAIT_FOREVER);
			if (ret != BK_OK)
			{
				LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
				goto fail;
			}

			LOGD("%s, %d, create audio transfer driver task complete \n", __func__, __LINE__);
		}

		/* init audio transfer task */
		aud_tras_setup_t aud_tras_setup_cfg = {0};
		aud_tras_setup_cfg.aud_tras_send_data_cb = aud_intf_info.drv_info.setup.aud_intf_tx_mic_data;
		ret = aud_tras_init(&aud_tras_setup_cfg);
		if (ret != BK_OK) {
			LOGE("%s, %d, aud_tras_init fail \n", __func__, __LINE__);
			goto fail;
		}

		ret = mailbox_media_aud_send_msg(EVENT_AUD_SET_MODE_REQ, (void *)aud_intf_info.drv_info.setup.work_mode);
		if (ret != BK_OK) {
			LOGE("%s, %d, set mode fail, ret: %d \n", __func__, __LINE__, ret);
			goto fail;
		}
	} else if (work_mode == AUD_INTF_WORK_MODE_NULL) {
		ret = mailbox_media_aud_send_msg(EVENT_AUD_SET_MODE_REQ, (void *)aud_intf_info.drv_info.setup.work_mode);
		if (ret != BK_OK) {
			LOGE("%s, %d, set mode fail, ret: %d \n", __func__, __LINE__, ret);
			return ret;
		}

		aud_intf_deinit();
		aud_tras_deinit();
	} else {
		//TODO nothing
	}

	aud_intf_info.api_info.busy_status = false;

	return ret;

fail:
	aud_intf_info.drv_info.setup.work_mode = temp;

	aud_tras_deinit();

	if (aud_intf_info.aud_intf_thread_hdl && aud_intf_info.aud_intf_msg_que) {
		aud_intf_deinit();
	}

	if(aud_intf_task_sem)
	{
		rtos_deinit_semaphore(&aud_intf_task_sem);
		aud_intf_task_sem = NULL;
	}

	aud_intf_info.api_info.busy_status = false;

	return ret;
}

bk_err_t bk_aud_intf_set_mic_gain(uint8_t value)
{
	bk_err_t ret = BK_ERR_AUD_INTF_STA;

	/* check value range */
	if (value > 0x3F)
		return BK_ERR_AUD_INTF_PARAM;

	CHECK_AUD_INTF_BUSY_STA();

	uint8_t temp = 0;

	/*check mic status */
	if (aud_intf_info.voc_status != AUD_INTF_VOC_STA_NULL) {
		temp = aud_intf_info.voc_info.aud_setup.adc_gain;
		aud_intf_info.voc_info.aud_setup.adc_gain = value;
		ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_SET_MIC_GAIN_REQ, &aud_intf_info.voc_info.aud_setup.adc_gain);
		if (ret != BK_OK)
			aud_intf_info.voc_info.aud_setup.adc_gain = temp;
	}

	aud_intf_info.api_info.busy_status = false;
	return ret;
}

bk_err_t bk_aud_intf_set_spk_gain(uint32_t value)
{
	bk_err_t ret = BK_ERR_AUD_INTF_STA;

	CHECK_AUD_INTF_BUSY_STA();
	uint16_t temp = 0;

	/*check mic status */
	if (aud_intf_info.voc_status != AUD_INTF_VOC_STA_NULL) {
		/* check value range */
		if (aud_intf_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			if (value > 0x3F) {
				LOGE("%s, %d, the spk_gain out of range:0x00-0x3F \n", __func__, __LINE__);
				aud_intf_info.api_info.busy_status = false;
				return BK_ERR_AUD_INTF_PARAM;
			}
		} else {
			if (value > 100) {
				LOGE("%s, %d, the spk_gain out of range:0-100 \n", __func__, __LINE__);
				aud_intf_info.api_info.busy_status = false;
				return BK_ERR_AUD_INTF_PARAM;
			}
		}
		temp = aud_intf_info.voc_info.aud_setup.dac_gain;
		aud_intf_info.voc_info.aud_setup.dac_gain = value;
		LOGI("%s, %d, set spk_gain: %d \n", __func__, __LINE__, aud_intf_info.voc_info.aud_setup.dac_gain);
		ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_SET_SPK_GAIN_REQ, &aud_intf_info.voc_info.aud_setup.dac_gain);
		if (ret != BK_OK)
			aud_intf_info.voc_info.aud_setup.dac_gain = temp;
	}

	aud_intf_info.api_info.busy_status = false;
	return ret;
}

bk_err_t bk_aud_intf_register_uac_connect_state_cb(void *cb)
{
	bk_err_t ret = BK_ERR_AUD_INTF_STA;

	CHECK_AUD_INTF_BUSY_STA();

	ret = mailbox_media_aud_send_msg(EVENT_AUD_UAC_REGIS_CONT_STATE_CB_REQ, cb);
	if (ret == BK_OK) {
		aud_intf_info.aud_intf_uac_connect_state_cb = cb;
	}

	return ret;
}

bk_err_t bk_aud_intf_uac_auto_connect_ctrl(bool enable)
{
	bk_err_t ret = BK_ERR_AUD_INTF_STA;

	if (aud_intf_info.uac_auto_connect == enable) {
		return BK_ERR_AUD_INTF_OK;
	}

	/* check if aud_intf driver init */
	if (aud_intf_info.drv_status != AUD_INTF_DRV_STA_IDLE && aud_intf_info.drv_status != AUD_INTF_DRV_STA_WORK) {
		return BK_ERR_AUD_INTF_STA;
	}

	CHECK_AUD_INTF_BUSY_STA();
	bool temp = aud_intf_info.uac_auto_connect;
	aud_intf_info.uac_auto_connect = enable;
	ret = mailbox_media_aud_send_msg(EVENT_AUD_UAC_AUTO_CONT_CTRL_REQ, (void *)aud_intf_info.uac_auto_connect);
	if (ret != BK_OK) {
		aud_intf_info.uac_auto_connect = temp;
	}

	return ret;
}

bk_err_t bk_aud_intf_set_aec_para(aud_intf_voc_aec_para_t aec_para, uint32_t value)
{
	bk_err_t ret = BK_OK;
	aud_intf_voc_aec_ctl_t *aec_ctl = NULL;

	/*check aec status */
	if (aud_intf_info.voc_status == AUD_INTF_VOC_STA_NULL)
		return BK_ERR_AUD_INTF_STA;

	CHECK_AUD_INTF_BUSY_STA();

	aec_ctl = audio_intf_malloc(sizeof(aud_intf_voc_aec_ctl_t));
	if (aec_ctl == NULL) {
		aud_intf_info.api_info.busy_status = false;
		return BK_ERR_AUD_INTF_MEMY;
	}
	aec_ctl->op = aec_para;
	aec_ctl->value = value;

	//TODO
	//cpy value before set

	switch (aec_para) {
		case AUD_INTF_VOC_AEC_MIC_DELAY:
			aud_intf_info.voc_info.aec_setup->mic_delay = value;
			break;

		case AUD_INTF_VOC_AEC_EC_DEPTH:
			aud_intf_info.voc_info.aec_setup->ec_depth = value;
			break;

		case AUD_INTF_VOC_AEC_REF_SCALE:
			aud_intf_info.voc_info.aec_setup->ref_scale = value;
			break;

		case AUD_INTF_VOC_AEC_VOICE_VOL:
			aud_intf_info.voc_info.aec_setup->voice_vol = value;
			break;

		case AUD_INTF_VOC_AEC_TXRX_THR:
			aud_intf_info.voc_info.aec_setup->TxRxThr = value;
			break;

		case AUD_INTF_VOC_AEC_TXRX_FLR:
			aud_intf_info.voc_info.aec_setup->TxRxFlr = value;
			break;

		case AUD_INTF_VOC_AEC_NS_LEVEL:
			aud_intf_info.voc_info.aec_setup->ns_level = value;
			break;

		case AUD_INTF_VOC_AEC_NS_PARA:
			aud_intf_info.voc_info.aec_setup->ns_para = value;
			break;

		case AUD_INTF_VOC_AEC_DRC:
			aud_intf_info.voc_info.aec_setup->drc = value;
			break;

		case AUD_INTF_VOC_AEC_INIT_FLAG:
			aud_intf_info.voc_info.aec_setup->init_flags = value;
			break;

		default:
			break;
	}

	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_SET_AEC_PARA_REQ, aec_ctl);

	audio_intf_free(aec_ctl);
	return ret;
}

bk_err_t bk_aud_intf_get_aec_para(void)
{
	/*check aec status */
	if (aud_intf_info.voc_status == AUD_INTF_VOC_STA_NULL)
		return BK_ERR_AUD_INTF_STA;

	CHECK_AUD_INTF_BUSY_STA();
	return mailbox_media_aud_send_msg(EVENT_AUD_VOC_GET_AEC_PARA_REQ, NULL);
}

static void aud_intf_voc_deconfig(void)
{
	/* audio deconfig */
	aud_intf_info.voc_info.samp_rate = 8000;	//default value
	aud_intf_info.voc_info.aud_setup.adc_gain = 0;
	aud_intf_info.voc_info.aud_setup.dac_gain = 0;
	aud_intf_info.voc_info.aud_setup.mic_frame_number = 0;
	aud_intf_info.voc_info.aud_setup.mic_samp_rate_points = 0;
	aud_intf_info.voc_info.aud_setup.speaker_frame_number = 0;
	aud_intf_info.voc_info.aud_setup.speaker_samp_rate_points = 0;

	/* aec deconfig */
	if (aud_intf_info.voc_info.aec_enable && aud_intf_info.voc_info.aec_setup) {
		audio_intf_free(aud_intf_info.voc_info.aec_setup);
	}
	aud_intf_info.voc_info.aec_setup = NULL;
	aud_intf_info.voc_info.aec_enable = false;

	/* tx deconfig */
	aud_intf_info.voc_info.tx_info.buff_length = 0;
	aud_intf_info.voc_info.tx_info.tx_buff_status = false;
	aud_intf_info.voc_info.tx_info.ping.busy_status = false;
	if (aud_intf_info.voc_info.tx_info.ping.buff_addr) {
		audio_intf_free(aud_intf_info.voc_info.tx_info.ping.buff_addr);
		aud_intf_info.voc_info.tx_info.ping.buff_addr = NULL;
	}

	aud_intf_info.voc_info.tx_info.pang.busy_status = false;
	if (aud_intf_info.voc_info.tx_info.pang.buff_addr) {
		audio_intf_free(aud_intf_info.voc_info.tx_info.pang.buff_addr);
		aud_intf_info.voc_info.tx_info.pang.buff_addr = NULL;
	}

	/* rx deconfig */
	aud_intf_info.voc_info.rx_info.rx_buff_status = false;
	if (aud_intf_info.voc_info.rx_info.decoder_ring_buff) {
		audio_intf_free(aud_intf_info.voc_info.rx_info.decoder_ring_buff);
		aud_intf_info.voc_info.rx_info.decoder_ring_buff = NULL;
	}

	if (aud_intf_info.voc_info.rx_info.decoder_rb) {
		audio_intf_free(aud_intf_info.voc_info.rx_info.decoder_rb);
		aud_intf_info.voc_info.rx_info.decoder_rb = NULL;
	}

	aud_intf_info.voc_info.rx_info.frame_num = 0;
	aud_intf_info.voc_info.rx_info.frame_size = 0;
	aud_intf_info.voc_info.rx_info.fifo_frame_num = 0;
	aud_intf_info.voc_info.rx_info.rx_buff_seq_tail = 0;
	aud_intf_info.voc_info.rx_info.aud_trs_read_seq = 0;
}

bk_err_t bk_aud_intf_voc_init(aud_intf_voc_setup_t setup)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;
	CHECK_AUD_INTF_BUSY_STA();

    AUD_RX_COUNT_OPEN();

	//aud_tras_drv_setup.aud_trs_mode = demo_setup.mode;
	aud_intf_info.voc_info.samp_rate = setup.samp_rate;
	aud_intf_info.voc_info.aec_enable = setup.aec_enable;
	aud_intf_info.voc_info.data_type = setup.data_type;
	/* audio config */
	aud_intf_info.voc_info.aud_setup.adc_gain = setup.mic_gain;	//default: 0x2d
	aud_intf_info.voc_info.aud_setup.dac_gain = setup.spk_gain;	//default: 0x2d
	if (aud_intf_info.voc_info.samp_rate == 16000) {
		aud_intf_info.voc_info.aud_setup.mic_samp_rate_points = 320;	//if AEC enable , the value is equal to aec_samp_rate_points, and the value not need to set
		aud_intf_info.voc_info.aud_setup.speaker_samp_rate_points = 320;	//if AEC enable , the value is equal to aec_samp_rate_points, and the value not need to set
	} else {
		aud_intf_info.voc_info.aud_setup.mic_samp_rate_points = 160;	//if AEC enable , the value is equal to aec_samp_rate_points, and the value not need to set
		aud_intf_info.voc_info.aud_setup.speaker_samp_rate_points = 160;	//if AEC enable , the value is equal to aec_samp_rate_points, and the value not need to set
	}
	aud_intf_info.voc_info.aud_setup.mic_frame_number = 2;
	aud_intf_info.voc_info.aud_setup.speaker_frame_number = 2;
	aud_intf_info.voc_info.aud_setup.spk_mode = setup.spk_mode;
	aud_intf_info.voc_info.mic_en = setup.mic_en;
	aud_intf_info.voc_info.spk_en = setup.spk_en;
	aud_intf_info.voc_info.mic_type = setup.mic_type;
	aud_intf_info.voc_info.spk_type = setup.spk_type;

	/* aec config */
	if (aud_intf_info.voc_info.aec_enable) {
		aud_intf_info.voc_info.aec_setup = audio_intf_malloc(sizeof(aec_config_t));
		if (aud_intf_info.voc_info.aec_setup == NULL) {
			LOGE("%s, %d, malloc aec_setup fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_intf_voc_init_exit;
		}
		aud_intf_info.voc_info.aec_setup->init_flags = 0x1f;
		aud_intf_info.voc_info.aec_setup->mic_delay = 0;
		aud_intf_info.voc_info.aec_setup->ec_depth = setup.aec_cfg.ec_depth;
		aud_intf_info.voc_info.aec_setup->ref_scale = setup.aec_cfg.ref_scale;
		aud_intf_info.voc_info.aec_setup->TxRxThr = setup.aec_cfg.TxRxThr;
		aud_intf_info.voc_info.aec_setup->TxRxFlr = setup.aec_cfg.TxRxFlr;
		aud_intf_info.voc_info.aec_setup->voice_vol = 14;
		aud_intf_info.voc_info.aec_setup->ns_level = setup.aec_cfg.ns_level;
		aud_intf_info.voc_info.aec_setup->ns_para = setup.aec_cfg.ns_para;
		aud_intf_info.voc_info.aec_setup->drc = 15;
	} else {
		aud_intf_info.voc_info.aec_setup = NULL;
	}

	/* tx config */
	switch (aud_intf_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			aud_intf_info.voc_info.tx_info.buff_length = aud_intf_info.voc_info.aud_setup.mic_samp_rate_points;
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			aud_intf_info.voc_info.tx_info.buff_length = aud_intf_info.voc_info.aud_setup.mic_samp_rate_points * 2;
			break;

		default:
			break;
	}
	aud_intf_info.voc_info.tx_info.ping.busy_status = false;
	aud_intf_info.voc_info.tx_info.ping.buff_addr = audio_intf_malloc(aud_intf_info.voc_info.tx_info.buff_length);
	if (aud_intf_info.voc_info.tx_info.ping.buff_addr == NULL) {
		LOGE("%s, %d, malloc pingpang buffer of tx fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_intf_voc_init_exit;
	}
	aud_intf_info.voc_info.tx_info.pang.busy_status = false;
	aud_intf_info.voc_info.tx_info.pang.buff_addr = audio_intf_malloc(aud_intf_info.voc_info.tx_info.buff_length);
	if (aud_intf_info.voc_info.tx_info.pang.buff_addr == NULL) {
		LOGE("%s, %d, malloc pang buffer of tx fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_intf_voc_init_exit;
	}
	aud_intf_info.voc_info.tx_info.tx_buff_status = true;

	/* rx config */
	aud_intf_info.voc_info.rx_info.aud_trs_read_seq = 0;
	switch (aud_intf_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			aud_intf_info.voc_info.rx_info.frame_size = 320;		//apk receive one frame 40ms
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			aud_intf_info.voc_info.rx_info.frame_size = 320 * 2;		//apk receive one frame 40ms
			break;

		default:
			break;
	}
	aud_intf_info.voc_info.rx_info.frame_num = setup.frame_num;
	aud_intf_info.voc_info.rx_info.rx_buff_seq_tail = 0;
	aud_intf_info.voc_info.rx_info.fifo_frame_num = setup.fifo_frame_num;
	aud_intf_info.voc_info.rx_info.decoder_ring_buff = audio_intf_malloc(aud_intf_info.voc_info.rx_info.frame_size * aud_intf_info.voc_info.rx_info.frame_num + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_intf_info.voc_info.rx_info.decoder_ring_buff == NULL) {
		LOGE("%s, %d, malloc decoder ring buffer of rx fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_intf_voc_init_exit;
	}
	LOGI("%s, %d, malloc decoder_ring_buff:%p, size:%d \r\n", __func__, __LINE__, aud_intf_info.voc_info.rx_info.decoder_ring_buff, aud_intf_info.voc_info.rx_info.frame_size * aud_intf_info.voc_info.rx_info.frame_num);
	aud_intf_info.voc_info.rx_info.decoder_rb = audio_intf_malloc(sizeof(RingBufferContext));
	if (aud_intf_info.voc_info.rx_info.decoder_rb == NULL) {
		LOGE("%s, %d, malloc decoder_rb fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_intf_voc_init_exit;
	}
	ring_buffer_init(aud_intf_info.voc_info.rx_info.decoder_rb, (uint8_t *)aud_intf_info.voc_info.rx_info.decoder_ring_buff, aud_intf_info.voc_info.rx_info.frame_size * aud_intf_info.voc_info.rx_info.frame_num + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);
	aud_intf_info.voc_info.rx_info.rx_buff_status = true;

	LOGI("%s, %d, decoder_rb:%p \n", __func__, __LINE__, aud_intf_info.voc_info.rx_info.decoder_rb);

	aud_intf_info.voc_info.aud_tx_rb = aud_tras_get_tx_rb();

	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_INIT_REQ, &aud_intf_info.voc_info);
	if (ret != BK_OK) {
		err = ret;
		LOGE("%s, %d, fail, err:%d \r\n", __func__, __LINE__, err);
		goto aud_intf_voc_init_exit;
	}

	aud_intf_info.voc_status = AUD_INTF_VOC_STA_IDLE;

	return BK_ERR_AUD_INTF_OK;

aud_intf_voc_init_exit:

    AUD_RX_COUNT_CLOSE();

	if (aud_intf_info.voc_info.aec_setup != NULL) {
		audio_intf_free(aud_intf_info.voc_info.aec_setup);
		aud_intf_info.voc_info.aec_setup = NULL;
	}

	if (aud_intf_info.voc_info.tx_info.ping.buff_addr != NULL) {
		audio_intf_free(aud_intf_info.voc_info.tx_info.ping.buff_addr);
		aud_intf_info.voc_info.tx_info.ping.buff_addr = NULL;
	}

	if (aud_intf_info.voc_info.tx_info.pang.buff_addr != NULL) {
		audio_intf_free(aud_intf_info.voc_info.tx_info.pang.buff_addr);
		aud_intf_info.voc_info.tx_info.pang.buff_addr = NULL;
	}

	if (aud_intf_info.voc_info.rx_info.decoder_ring_buff != NULL) {
		audio_intf_free(aud_intf_info.voc_info.rx_info.decoder_ring_buff);
		aud_intf_info.voc_info.rx_info.decoder_ring_buff = NULL;
	}

	if (aud_intf_info.voc_info.rx_info.decoder_rb != NULL) {
		audio_intf_free(aud_intf_info.voc_info.rx_info.decoder_rb);
		aud_intf_info.voc_info.rx_info.decoder_rb = NULL;
	}

	aud_intf_info.api_info.busy_status = false;
	return err;
}

bk_err_t bk_aud_intf_voc_deinit(void)
{
	bk_err_t ret = BK_OK;

	if (aud_intf_info.voc_status == AUD_INTF_VOC_STA_NULL) {
		LOGI("%s, %d, voice is alreay deinit \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_OK;
	}

	CHECK_AUD_INTF_BUSY_STA();
	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_DEINIT_REQ, NULL);
	if (ret != BK_OK) {
		LOGE("%s, %d, fail, result: %d \n", __func__, __LINE__, ret);
	} else {
		aud_intf_voc_deconfig();
		aud_intf_info.voc_status = AUD_INTF_VOC_STA_NULL;
	}

    AUD_RX_COUNT_CLOSE();

	return ret;
}

bk_err_t bk_aud_intf_voc_start(void)
{
	bk_err_t ret = BK_OK;

	switch (aud_intf_info.voc_status) {
		case AUD_INTF_VOC_STA_NULL:
		case AUD_INTF_VOC_STA_START:
			return BK_ERR_AUD_INTF_OK;

		case AUD_INTF_VOC_STA_IDLE:
		case AUD_INTF_VOC_STA_STOP:
			if (ring_buffer_get_fill_size(aud_intf_info.voc_info.rx_info.decoder_rb)/aud_intf_info.voc_info.rx_info.frame_size < aud_intf_info.voc_info.rx_info.fifo_frame_num) {
				uint8_t *temp_buff = NULL;
				uint32_t temp_size = aud_intf_info.voc_info.rx_info.frame_size * aud_intf_info.voc_info.rx_info.fifo_frame_num - ring_buffer_get_fill_size(aud_intf_info.voc_info.rx_info.decoder_rb);
				temp_buff = audio_intf_malloc(temp_size);
				if (temp_buff == NULL) {
					return BK_ERR_AUD_INTF_MEMY;
				} else {
					switch (aud_intf_info.voc_info.data_type) {
						case AUD_INTF_VOC_DATA_TYPE_G711A:
							os_memset(temp_buff, 0xD5, temp_size);
							break;

						case AUD_INTF_VOC_DATA_TYPE_PCM:
							os_memset(temp_buff, 0x00, temp_size);
							break;

						case AUD_INTF_VOC_DATA_TYPE_G711U:
							os_memset(temp_buff, 0xFF, temp_size);
							break;

						default:
							break;
					}

					aud_intf_voc_write_spk_data(temp_buff, temp_size);
					audio_intf_free(temp_buff);
				}
			}
			break;

		default:
			return BK_ERR_AUD_INTF_FAIL;
	}

	CHECK_AUD_INTF_BUSY_STA();
	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_START_REQ, NULL);
	if (ret != BK_OK) {
		LOGE("%s, %d, fail, result: %d \n", __func__, __LINE__, ret);
	} else {
		aud_intf_info.voc_status = AUD_INTF_VOC_STA_START;
	}

	return ret;
}

bk_err_t bk_aud_intf_voc_stop(void)
{
	bk_err_t ret = BK_OK;

	switch (aud_intf_info.voc_status) {
		case AUD_INTF_VOC_STA_NULL:
		case AUD_INTF_VOC_STA_IDLE:
		case AUD_INTF_VOC_STA_STOP:
			return BK_ERR_AUD_INTF_STA;
			break;

		case AUD_INTF_VOC_STA_START:
			break;

		default:
			return BK_ERR_AUD_INTF_FAIL;
	}

	CHECK_AUD_INTF_BUSY_STA();
	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_STOP_REQ, NULL);
	if (ret != BK_OK) {
		LOGE("%s, %d, fail, result: %d \n", __func__, __LINE__, ret);
	} else {
		aud_intf_info.voc_status = AUD_INTF_VOC_STA_STOP;
	}

	return ret;
}

bk_err_t bk_aud_intf_voc_mic_ctrl(aud_intf_voc_mic_ctrl_t mic_en)
{
	bk_err_t ret = BK_OK;
	if (aud_intf_info.voc_info.mic_en == mic_en) {
		return BK_OK;
	}

	aud_intf_voc_mic_ctrl_t temp = aud_intf_info.voc_info.mic_en;
	aud_intf_info.voc_info.mic_en = mic_en;

	CHECK_AUD_INTF_BUSY_STA();
	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_CTRL_MIC_REQ, (void *)aud_intf_info.voc_info.mic_en);
	if (ret != BK_OK)
		aud_intf_info.voc_info.mic_en = temp;

	return ret;
}

bk_err_t bk_aud_intf_voc_spk_ctrl(aud_intf_voc_spk_ctrl_t spk_en)
{
	bk_err_t ret = BK_OK;
	if (aud_intf_info.voc_info.spk_en == spk_en) {
		return BK_OK;
	}

	aud_intf_voc_spk_ctrl_t temp = aud_intf_info.voc_info.spk_en;
	aud_intf_info.voc_info.spk_en = spk_en;

	CHECK_AUD_INTF_BUSY_STA();
	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_CTRL_SPK_REQ, (void *)aud_intf_info.voc_info.spk_en);
	if (ret != BK_OK)
		aud_intf_info.voc_info.spk_en = temp;

	return ret;
}

bk_err_t bk_aud_intf_voc_aec_ctrl(bool aec_en)
{
	bk_err_t ret = BK_OK;
	if (aud_intf_info.voc_info.aec_enable == aec_en) {
		return BK_OK;
	}

	bool temp = aud_intf_info.voc_info.aec_enable;
	aud_intf_info.voc_info.aec_enable = aec_en;

	CHECK_AUD_INTF_BUSY_STA();
	ret = mailbox_media_aud_send_msg(EVENT_AUD_VOC_CTRL_AEC_REQ, (void *)aud_intf_info.voc_info.aec_enable);
	if (ret != BK_OK)
		aud_intf_info.voc_info.aec_enable = temp;

	return ret;
}

bk_err_t bk_aud_intf_drv_init(aud_intf_drv_setup_t *setup)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;

	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO, PM_POWER_MODULE_STATE_ON);

	if (aud_intf_info.drv_status != AUD_INTF_DRV_STA_NULL) {
		LOGI("%s, %d, aud_intf driver already init \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_OK;
	}

	/* save drv_info */
	aud_intf_info.drv_info.setup = *setup;

	CHECK_AUD_INTF_BUSY_STA();

	/* init audio interface driver */
	LOGD("%s, %d, init aud_intf driver in CPU1 mode \n", __func__, __LINE__);
	ret = mailbox_media_aud_send_msg(EVENT_AUD_INIT_REQ, &aud_intf_info.drv_info);
	if (ret != BK_OK) {
		LOGE("%s, %d, init aud_intf driver fail \n", __func__, __LINE__);
		goto aud_intf_drv_init_exit;
	} else {
		aud_intf_info.drv_status = AUD_INTF_DRV_STA_IDLE;
	}

	return BK_ERR_AUD_INTF_OK;

aud_intf_drv_init_exit:
	LOGE("%s, %d, init aud_intf driver fail \n", __func__, __LINE__);

	return err;
}

bk_err_t bk_aud_intf_drv_deinit(void)
{
	bk_err_t ret = BK_OK;

	if (aud_intf_info.drv_status == AUD_INTF_DRV_STA_NULL) {
		LOGI("%s, %d, aud_intf already deinit \n", __func__, __LINE__);
		return BK_OK;
	}

	/* reset uac_auto_connect */
	aud_intf_info.uac_auto_connect = true;

	CHECK_AUD_INTF_BUSY_STA();

	ret = mailbox_media_aud_send_msg(EVENT_AUD_DEINIT_REQ, NULL);
	if (ret != BK_OK) {
		LOGE("%s, %d, deinit audio transfer fail \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_FAIL;
	}

	aud_intf_info.drv_status = AUD_INTF_DRV_STA_NULL;
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO, PM_POWER_MODULE_STATE_OFF);

	return BK_ERR_AUD_INTF_OK;
}

/* write speaker data in voice work mode */
static bk_err_t aud_intf_voc_write_spk_data(uint8_t *dac_buff, uint32_t size)
{
	uint32_t write_size = 0;

	/* check aud_intf status */
	if (aud_intf_info.voc_status == AUD_INTF_VOC_STA_NULL)
		return BK_ERR_AUD_INTF_STA;

#if (CONFIG_CACHE_ENABLE)
	flush_all_dcache();
#endif

	if (ring_buffer_get_free_size(aud_intf_info.voc_info.rx_info.decoder_rb) >= size) {
		write_size = ring_buffer_write(aud_intf_info.voc_info.rx_info.decoder_rb, dac_buff, size);
		if (write_size != size) {
			LOGE("%s, %d, write decoder_ring_buff fail, size:%d \n", __func__, __LINE__, size);
			return BK_FAIL;
		}
		aud_intf_info.voc_info.rx_info.rx_buff_seq_tail += size/(aud_intf_info.voc_info.rx_info.frame_size);
	}

	return BK_OK;
}

bk_err_t bk_aud_intf_write_spk_data(uint8_t *dac_buff, uint32_t size)
{
	bk_err_t ret = BK_OK;
	//LOGI("%s \n", __func__);

	switch (aud_intf_info.drv_info.setup.work_mode) {
		case AUD_INTF_WORK_MODE_VOICE:

            AUD_RX_COUNT_ADD_SIZE(size);

			ret = aud_intf_voc_write_spk_data(dac_buff, size);
			break;

		default:
			ret = BK_FAIL;
			break;
	}

	return ret;
}

#if (CONFIG_AUD_ASR)
bk_err_t bk_aud_intf_voc_asr_ctrl(bool asr_en)
{
	if (aud_intf_info.voc_status == AUD_INTF_VOC_STA_NULL)
		return BK_ERR_AUD_INTF_STA;
	CHECK_AUD_INTF_BUSY_STA();
	if(asr_en){
		return mailbox_media_aud_send_msg(EVENT_AUD_VOC_ASR_START_REQ, NULL);
	}else{
		return mailbox_media_aud_send_msg(EVENT_AUD_VOC_ASR_STOP_REQ, NULL);
	}
}

bk_err_t bk_aud_intf_voc_register_asr_detect_result(aud_asr_recv_result_callback_t asr_ret_callback)
{
	return aud_asr_start(asr_ret_callback);
}
#endif