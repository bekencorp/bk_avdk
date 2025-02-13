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

//#include <common/bk_include.h>
#include <os/os.h>
#include <os/mem.h>
#include <os/str.h>
#include <driver/aud_adc_types.h>
#include <driver/aud_adc.h>
#include <driver/aud_dac_types.h>
#include <driver/aud_dac.h>

#include <driver/dma.h>
#include <bk_general_dma.h>
#include "sys_driver.h"
#include "aud_intf_private.h"
#include "aud_tras_drv.h"
#include <driver/psram.h>
#include <driver/audio_ring_buff.h>
#include <modules/g711.h>
#include "gpio_driver.h"
#include <driver/gpio.h>

#include "bk_misc.h"
#include <soc/mapping.h>

#include "media_evt.h"
#include "media_mailbox_list_util.h"
#include "aud_aec.h"
#include "psram_mem_slab.h"
#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif


#define AUD_TRAS_DRV_TAG "tras_drv"
#define LOGI(...) BK_LOGI(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(AUD_TRAS_DRV_TAG, ##__VA_ARGS__)

#define CONFIG_AUD_TRAS_AEC_MIC_DELAY_POINTS   53
//#define CONFIG_AUD_RING_BUFF_SAFE_INTERVAL    20

#define AUD_MIC_COUNT
//#define AUD_SPK_COUNT

#ifdef AUD_MIC_COUNT
#include "count_util.h"
static count_util_t aud_mic_count_util = {0};
#define AUD_MIC_COUNT_INTERVAL           (1000 * 5)
#define AUD_MIC_COUNT_TAG                "UAC MIC"

#define AUD_MIC_COUNT_OPEN()               count_util_create(&aud_mic_count_util, AUD_MIC_COUNT_INTERVAL, AUD_MIC_COUNT_TAG)
#define AUD_MIC_COUNT_CLOSE()              count_util_destroy(&aud_mic_count_util)
#define AUD_MIC_COUNT_ADD_SIZE(size)       count_util_add_size(&aud_mic_count_util, size)
#else
#define AUD_MIC_COUNT_OPEN()
#define AUD_MIC_COUNT_CLOSE()
#define AUD_MIC_COUNT_ADD_SIZE(size)
#endif  //AUD_MIC_COUNT

#ifdef AUD_SPK_COUNT
#include "count_util.h"
static count_util_t aud_spk_count_util = {0};
#define AUD_SPK_COUNT_INTERVAL           (1000 * 5)
#define AUD_SPK_COUNT_TAG                "UAC SPK"

#define AUD_SPK_COUNT_OPEN()               count_util_create(&aud_spk_count_util, AUD_SPK_COUNT_INTERVAL, AUD_SPK_COUNT_TAG)
#define AUD_SPK_COUNT_CLOSE()              count_util_destroy(&aud_spk_count_util)
#define AUD_SPK_COUNT_ADD_SIZE(size)       count_util_add_size(&aud_spk_count_util, size)
#else
#define AUD_SPK_COUNT_OPEN()
#define AUD_SPK_COUNT_CLOSE()
#define AUD_SPK_COUNT_ADD_SIZE(size)
#endif  //AUD_SPK_COUNT


//#define UAC_MIC_DATA_DUMP_BY_UART
//#define UAC_SPK_DATA_DUMP_BY_UART

#ifdef UAC_MIC_DATA_DUMP_BY_UART
#include "uart_util.h"
static uart_util_t g_uac_mic_uart_util = {0};
#define UAC_MIC_DATA_DUMP_UART_ID            (1)
#define UAC_MIC_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define UAC_MIC_DATA_DUMP_OPEN()                    uart_util_create(&g_uac_mic_uart_util, UAC_MIC_DATA_DUMP_UART_ID, UAC_MIC_DATA_DUMP_UART_BAUD_RATE)
#define UAC_MIC_DATA_DUMP_CLOSE()                   uart_util_destroy(&g_uac_mic_uart_util)
#define UAC_MIC_DATA_DUMP_DATA(data_buf, len)       uart_util_tx_data(&g_uac_mic_uart_util, data_buf, len)
#else
#define UAC_MIC_DATA_DUMP_OPEN()
#define UAC_MIC_DATA_DUMP_CLOSE()
#define UAC_MIC_DATA_DUMP_DATA(data_buf, len)
#endif

#ifdef UAC_SPK_DATA_DUMP_BY_UART
#include "uart_util.h"
static uart_util_t g_uac_spk_uart_util = {0};
#define UAC_SPK_DATA_DUMP_UART_ID            (1)
#define UAC_SPK_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define UAC_SPK_DATA_DUMP_OPEN()                    uart_util_create(&g_uac_spk_uart_util, UAC_SPK_DATA_DUMP_UART_ID, UAC_SPK_DATA_DUMP_UART_BAUD_RATE)
#define UAC_SPK_DATA_DUMP_CLOSE()                   uart_util_destroy(&g_uac_spk_uart_util)
#define UAC_SPK_DATA_DUMP_DATA(data_buf, len)       uart_util_tx_data(&g_uac_spk_uart_util, data_buf, len)
#else
#define UAC_SPK_DATA_DUMP_OPEN()
#define UAC_SPK_DATA_DUMP_CLOSE()
#define UAC_SPK_DATA_DUMP_DATA(data_buf, len)
#endif


#define TU_QITEM_COUNT      (120)
static beken_thread_t  aud_trs_drv_thread_hdl = NULL;
static beken_queue_t aud_trs_drv_int_msg_que = NULL;

aud_tras_drv_info_t aud_tras_drv_info = DEFAULT_AUD_TRAS_DRV_INFO();
static bool uac_mic_read_flag = false;
static bool uac_spk_write_flag = false;

media_mailbox_msg_t uac_connect_state_msg = {0};

media_mailbox_msg_t mic_to_media_app_msg = {0};

static beken_semaphore_t aud_tras_drv_task_sem = NULL;
static struct usbh_urb *s_usbh_uac_mic_urb = NULL;
static struct usbh_urb *s_usbh_uac_spk_urb = NULL;

#if CONFIG_AUD_TRAS_AEC_MIC_DELAY_DEBUG
static uint8_t mic_delay_num = 0;
#endif


/* extern api */
bk_err_t aud_tras_drv_deinit(void);
static bk_err_t aud_tras_drv_voc_start(void);
static bk_err_t aud_tras_drv_set_spk_gain(uint16_t value);

void *audio_tras_drv_malloc(uint32_t size)
{
#if CONFIG_AUD_TRAS_USE_SRAM
    return os_malloc(size);
#else
    return bk_psram_frame_buffer_malloc(PSRAM_HEAP_AUDIO, size);
#endif
}

void audio_tras_drv_free(void *mem)
{
#if CONFIG_AUD_TRAS_USE_SRAM
    os_free(mem);
#else
    return bk_psram_frame_buffer_free(mem);
#endif
}

static void aud_tras_dac_pa_ctrl(bool en)
{
	if (en) {
#if CONFIG_AUD_TRAS_DAC_PA_CTRL
	/* delay 2ms to avoid po audio data, and then open pa */
	delay_ms(2);
	/* open pa according to congfig */
	gpio_dev_unmap(AUD_DAC_PA_CTRL_GPIO);
	bk_gpio_enable_output(AUD_DAC_PA_CTRL_GPIO);
#if AUD_DAC_PA_ENABLE_LEVEL
	bk_gpio_set_output_high(AUD_DAC_PA_CTRL_GPIO);
#else
	bk_gpio_set_output_low(AUD_DAC_PA_CTRL_GPIO);
#endif
#endif
	} else {
#if CONFIG_AUD_TRAS_DAC_PA_CTRL
		/* open pa according to congfig */
		//gpio_dev_unmap(AUD_DAC_PA_CTRL_GPIO);
		//bk_gpio_enable_output(AUD_DAC_PA_CTRL_GPIO);
#if AUD_DAC_PA_ENABLE_LEVEL
		bk_gpio_set_output_low(AUD_DAC_PA_CTRL_GPIO);
#else
		bk_gpio_set_output_high(AUD_DAC_PA_CTRL_GPIO);
#endif
#endif
	}
}

bk_err_t aud_tras_drv_send_msg(aud_tras_drv_op_t op, void *param)
{
	bk_err_t ret;
	aud_tras_drv_msg_t msg;

	msg.op = op;
	msg.param = param;
	if (aud_trs_drv_int_msg_que) {
		ret = rtos_push_to_queue(&aud_trs_drv_int_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("%s, %d, send:%d fail ret:%d\n", __func__, __LINE__, op, ret);
			return kOverrunErr;
		}

		return ret;
	}
	return kNoResourcesErr;
}

static bk_err_t aud_tras_adc_config(aud_adc_config_t *adc_config)
{
	bk_err_t ret = BK_OK;

	/* init audio driver and config adc */
	ret = bk_aud_driver_init();
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio driver fail \n", __func__, __LINE__);
		goto aud_adc_exit;
	}

	ret = bk_aud_adc_init(adc_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio adc fail \n", __func__, __LINE__);
		goto aud_adc_exit;
	}

	return BK_OK;

aud_adc_exit:
	LOGE("%s, %d, audio adc config fail \n", __func__, __LINE__);
	bk_aud_driver_deinit();
	return BK_FAIL;
}

static bk_err_t aud_tras_dac_config(aud_dac_config_t *dac_config)
{
	bk_err_t ret = BK_OK;

	/* init audio driver and config dac */
	ret = bk_aud_driver_init();
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio driver fail \n", __func__, __LINE__);
		goto aud_dac_exit;
	}

	ret = bk_aud_dac_init(dac_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, init audio dac fail \n", __func__, __LINE__);
		goto aud_dac_exit;
	}

	return BK_OK;

aud_dac_exit:
	LOGE("%s, %d, audio dac config fail \n", __func__, __LINE__);
	bk_aud_driver_deinit();
	return BK_FAIL;
}

/* 搬运audio adc 采集到的一帧mic和ref信号后，触发中断通知AEC处理数据 */
static void aud_tras_adc_dma_finish_isr(void)
{
	bk_err_t ret = BK_OK;

	/* send msg to AEC or ENCODER to process mic data */
	if (aud_tras_drv_info.voc_info.aec_enable)
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_AEC, NULL);
	else
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);

	if (ret != kNoErr) {
		LOGD("%s, %d, send msg: AUD_TRAS_DRV_AEC fail \n", __func__, __LINE__);
	}
}

static bk_err_t aud_tras_adc_dma_config(dma_id_t dma_id, int32_t *ring_buff_addr, uint32_t ring_buff_size, uint32_t transfer_len, aud_intf_mic_chl_t mic_chl)
{
	bk_err_t ret = BK_OK;
	dma_config_t dma_config = {0};
	uint32_t adc_port_addr;

	os_memset(&dma_config, 0, sizeof(dma_config));

	dma_config.mode = DMA_WORK_MODE_REPEAT;
	dma_config.chan_prio = 1;
	dma_config.src.dev = DMA_DEV_AUDIO_RX;
	dma_config.dst.dev = DMA_DEV_DTCM;
	switch (mic_chl) {
		case AUD_INTF_MIC_CHL_MIC1:
			dma_config.src.width = DMA_DATA_WIDTH_16BITS;
			break;

		case AUD_INTF_MIC_CHL_DUAL:
			dma_config.src.width = DMA_DATA_WIDTH_32BITS;
			break;

		default:
			break;
	}
	dma_config.dst.width = DMA_DATA_WIDTH_32BITS;

	/* get adc fifo address */
	if (bk_aud_adc_get_fifo_addr(&adc_port_addr) != BK_OK) {
		LOGE("%s, %d, get adc fifo address fail \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_ADC;
	} else {
		dma_config.src.addr_inc_en = DMA_ADDR_INC_ENABLE;
		dma_config.src.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
		dma_config.src.start_addr = adc_port_addr;
		dma_config.src.end_addr = adc_port_addr + 4;
	}

	dma_config.dst.addr_inc_en = DMA_ADDR_INC_ENABLE;
	dma_config.dst.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
	dma_config.dst.start_addr = (uint32_t)ring_buff_addr;
	dma_config.dst.end_addr = (uint32_t)ring_buff_addr + ring_buff_size;

	/* init dma channel */
	ret = bk_dma_init(dma_id, &dma_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, audio adc dma channel init fail \n", __func__, __LINE__);
		return BK_ERR_AUD_INTF_DMA;
	}

	/* set dma transfer length */
	bk_dma_set_transfer_len(dma_id, transfer_len);

	//register isr
	bk_dma_register_isr(dma_id, NULL, (void *)aud_tras_adc_dma_finish_isr);
	bk_dma_enable_finish_interrupt(dma_id);
#if (CONFIG_SPE)
	BK_LOG_ON_ERR(bk_dma_set_dest_sec_attr(dma_id, DMA_ATTR_SEC));
	BK_LOG_ON_ERR(bk_dma_set_src_sec_attr(dma_id, DMA_ATTR_SEC));
#endif

	return BK_ERR_AUD_INTF_OK;
}

/* 搬运audio dac 一帧dac信号后，触发中断通知decoder处理数据 */
static void aud_tras_dac_dma_finish_isr(void)
{
	bk_err_t ret = BK_OK;

	/* send msg to decoder to decoding recevied data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_DECODER, NULL);
	if (ret != kNoErr) {
		LOGE("%s, %d, dac send msg: AUD_TRAS_DRV_DECODER fail \n", __func__, __LINE__);
	}
}

static bk_err_t aud_tras_dac_dma_config(dma_id_t dma_id, int32_t *ring_buff_addr, uint32_t ring_buff_size, uint32_t transfer_len, aud_intf_spk_chl_t spk_chl)
{
	bk_err_t ret = BK_OK;
	dma_config_t dma_config = {0};
	uint32_t dac_port_addr;

    os_memset(&dma_config, 0, sizeof(dma_config));

	dma_config.mode = DMA_WORK_MODE_REPEAT;
	dma_config.chan_prio = 1;
	dma_config.src.dev = DMA_DEV_DTCM;
	dma_config.dst.dev = DMA_DEV_AUDIO;
	dma_config.src.width = DMA_DATA_WIDTH_32BITS;
	switch (spk_chl) {
		case AUD_INTF_SPK_CHL_LEFT:
			dma_config.dst.width = DMA_DATA_WIDTH_16BITS;
			break;

		case AUD_INTF_SPK_CHL_DUAL:
			dma_config.dst.width = DMA_DATA_WIDTH_32BITS;
			break;

		default:
			break;
	}

	/* get dac fifo address */
	if (bk_aud_dac_get_fifo_addr(&dac_port_addr) != BK_OK) {
		LOGE("%s, %d, get dac fifo address fail \n", __func__, __LINE__);
		return BK_FAIL;
	} else {
		dma_config.dst.addr_inc_en = DMA_ADDR_INC_ENABLE;
		dma_config.dst.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
		dma_config.dst.start_addr = dac_port_addr;
		dma_config.dst.end_addr = dac_port_addr + 4;
	}
	dma_config.src.addr_inc_en = DMA_ADDR_INC_ENABLE;
	dma_config.src.addr_loop_en = DMA_ADDR_LOOP_ENABLE;
	dma_config.src.start_addr = (uint32_t)ring_buff_addr;
	dma_config.src.end_addr = (uint32_t)(ring_buff_addr) + ring_buff_size;

	/* init dma channel */
	ret = bk_dma_init(dma_id, &dma_config);
	if (ret != BK_OK) {
		LOGE("%s, %d, audio dac dma channel init fail \n", __func__, __LINE__);
		return BK_FAIL;
	}

	/* set dma transfer length */
	bk_dma_set_transfer_len(dma_id, transfer_len);

	//register isr
	bk_dma_register_isr(dma_id, NULL, (void *)aud_tras_dac_dma_finish_isr);
	bk_dma_enable_finish_interrupt(dma_id);
#if (CONFIG_SPE)
	BK_LOG_ON_ERR(bk_dma_set_dest_sec_attr(dma_id, DMA_ATTR_SEC));
	BK_LOG_ON_ERR(bk_dma_set_src_sec_attr(dma_id, DMA_ATTR_SEC));
#endif

	return BK_OK;
}

static bk_err_t aud_tras_aec(void)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
		return BK_OK;

	aec_info_t *aec_info_pr = aud_tras_drv_info.voc_info.aec_info;

#if (CONFIG_CACHE_ENABLE)
	flush_all_dcache();
#endif

	/* get a fram mic data from mic_ring_buff */
	if (ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.mic_rb)) >= aec_info_pr->samp_rate_points*2) {
		size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
		if (size != aec_info_pr->samp_rate_points*2) {
			LOGE("%s, %d, read mic_ring_buff fail, size:%d \n", __func__, __LINE__, size);
			//return BK_FAIL;
		}
	} else {
		LOGD("%s, %d, do not have mic data need to aec \n", __func__, __LINE__);
		return BK_OK;
	}

	/* dump uac data by uart */
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		UAC_MIC_DATA_DUMP_DATA(aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
	}

	if (aud_tras_drv_info.voc_info.spk_en == AUD_INTF_VOC_SPK_CLOSE || aud_tras_drv_info.voc_info.aec_enable == false) {
		/* save mic data after aec processed to aec_ring_buffer */
		if (ring_buffer_get_free_size(&(aec_info_pr->aec_rb)) >= aec_info_pr->samp_rate_points*2) {
			size = ring_buffer_write(&(aec_info_pr->aec_rb), (uint8_t*)aec_info_pr->mic_addr, aec_info_pr->samp_rate_points*2);
			if (size != aec_info_pr->samp_rate_points*2) {
				LOGE("%s, %d, the data write to aec_ring_buff is not a frame \n", __func__, __LINE__);
				//return BK_FAIL;
			}
		}

		/* send msg to encoder to encoding data */
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
		if (ret != kNoErr) {
			LOGE("%s, %d, send msg: AUD_TRAS_DRV_ENCODER fail \n", __func__, __LINE__);
			return BK_FAIL;
		}

		return BK_OK;
	}

	/* read ref data from ref_ring_buff */
	if (ring_buffer_get_fill_size(&(aec_info_pr->ref_rb)) >= aec_info_pr->samp_rate_points*2) {
		size = ring_buffer_read(&(aec_info_pr->ref_rb), (uint8_t*)aec_info_pr->ref_addr, aec_info_pr->samp_rate_points*2);
		if (size != aec_info_pr->samp_rate_points*2) {
			LOGE("%s, %d, the ref data readed from ref_ring_buff is not a frame \n", __func__, __LINE__);
			//return BK_FAIL;
			//os_memset(ref_addr, 0, frame_sample*2);
		}
	} else {
		//LOGE("no ref data \n");
		os_memset((void *)aec_info_pr->ref_addr, 0, aec_info_pr->samp_rate_points*2);
	}

	/* aec process data */
	//os_printf("ref_addr:%p, mic_addr:%p, out_addr:%p \r\n", aec_context_pr->ref_addr, aec_context_pr->mic_addr, aec_context_pr->out_addr);
	aud_aec_proc(aec_info_pr);

	/* save mic data after aec processed to aec_ring_buffer */
	if (ring_buffer_get_free_size(&(aec_info_pr->aec_rb)) >= aec_info_pr->samp_rate_points*2) {
		size = ring_buffer_write(&(aec_info_pr->aec_rb), (uint8_t*)aec_info_pr->out_addr, aec_info_pr->samp_rate_points*2);
		if (size != aec_info_pr->samp_rate_points*2) {
			LOGE("%s, %d, the data writeten to aec_ring_buff is not a frame \n", __func__, __LINE__);
			//return BK_FAIL;
		}
	}

	/* send msg to encoder to encoding data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_ENCODER fail \n", __func__, __LINE__);
		return BK_FAIL;
	}

	return ret;
}

static bk_err_t aud_tras_uac_auto_connect_ctrl(bool en)
{
	aud_tras_drv_info.uac_auto_connect = en;

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_uac_mic_disconnect_handle(void)
{
//	LOGI("enter %s \n", __func__);

	/* notify app that uac disconnecting */
	if (aud_tras_drv_info.uac_connect_state_cb_exist && aud_tras_drv_info.uac_mic_status == AUD_INTF_UAC_MIC_ABNORMAL_DISCONNECTED) {
		uac_connect_state_msg.event = EVENT_UAC_CONNECT_STATE_NOTIFY;
		uac_connect_state_msg.param = AUD_INTF_UAC_MIC_ABNORMAL_DISCONNECTED;
		uac_connect_state_msg.sem = NULL;
		msg_send_notify_to_media_major_mailbox(&uac_connect_state_msg, APP_MODULE);
	}

	/* reset mic and spk current status */
	aud_tras_drv_info.uac_mic_open_current = false;

	return BK_OK;
}

static bk_err_t aud_tras_uac_spk_disconnect_handle(void)
{
//	LOGI("enter %s \n", __func__);

	/* notify app that uac disconnecting */
	if (aud_tras_drv_info.uac_connect_state_cb_exist && aud_tras_drv_info.uac_spk_status == AUD_INTF_UAC_SPK_ABNORMAL_DISCONNECTED) {
		uac_connect_state_msg.event = EVENT_UAC_CONNECT_STATE_NOTIFY;
		uac_connect_state_msg.param = AUD_INTF_UAC_SPK_ABNORMAL_DISCONNECTED;
		uac_connect_state_msg.sem = NULL;
		msg_send_notify_to_media_major_mailbox(&uac_connect_state_msg, APP_MODULE);
	}

	/* reset mic and spk current status */
	aud_tras_drv_info.uac_spk_open_current = false;

	return BK_OK;
}

/* used in AUD_INTF_WORK_MODE_VOICE mode */
static void usb_hub_voc_uac_mic_port_dev_complete_callback(void *pCompleteParam, int nbytes)
{
	struct usbh_urb *urb = (struct usbh_urb *)pCompleteParam;
	bk_err_t ret = BK_OK;

	//LOGI("[+]%s urb:0x%x nbytes:0x%x\r\n", __func__, urb, nbytes);

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
		urb->transfer_buffer = aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr;
		urb->transfer_buffer_length = aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_size;
		urb->actual_length = 0;
	} else {
		LOGE("%s, %d, aud_tras_drv_info.voc_info.status:%d not need read uac mic data \n", __func__, __LINE__, aud_tras_drv_info.voc_info.status);
		return;
	}

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START && nbytes > 0) {
		if (ring_buffer_get_free_size(&aud_tras_drv_info.voc_info.mic_rb) >= nbytes) {
			ring_buffer_write(&aud_tras_drv_info.voc_info.mic_rb, (uint8_t *)urb->transfer_buffer, nbytes);
		}
	}

	/* debug */
	AUD_MIC_COUNT_ADD_SIZE(nbytes);

	/* send msg to TX_DATA to process mic data */
	if (aud_tras_drv_info.voc_info.aec_enable) {
		ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_AEC, NULL);
	} else {
		if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START && nbytes > 0) {
			ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_ENCODER, NULL);
		}
	}

	if (ret != kNoErr) {
		LOGD("%s, %d, send msg: AUD_TRAS_DRV_AEC or AUD_TRAS_DRV_ENCODER fail \n", __func__, __LINE__);
	}

	return;
}

static bk_err_t usb_hub_voc_uac_mic_port_device_urb_fill(bk_usb_hub_port_info *usbh_hub_port_info, struct usbh_urb *urb)
{
	struct usbh_audio *mic_device;

	if(urb && usbh_hub_port_info && usbh_hub_port_info->device_index == USB_UAC_MIC_DEVICE) {
		mic_device = (struct usbh_audio *)(usbh_hub_port_info->usb_device);
		urb->pipe = (usbh_pipe_t)(mic_device->isoin);
		urb->complete = (usbh_complete_callback_t)usb_hub_voc_uac_mic_port_dev_complete_callback;
		urb->arg = urb;
		urb->timeout = 0;

		urb->transfer_buffer = aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr;
		urb->transfer_buffer_length = aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_size;
		urb->num_of_iso_packets = 1;
	} else {
		LOGE("%s, %d, parameters is invalid, usbh_hub_port_info: %p, urb: %p\n", __func__, __LINE__, usbh_hub_port_info, urb);
		return BK_FAIL;
	}

	return BK_OK;
}

static void usb_hub_voc_uac_spk_port_dev_complete_callback(void *pCompleteParam, int nbytes)
{
	struct usbh_urb *urb = (struct usbh_urb *)pCompleteParam;
	bk_err_t ret = BK_OK;

	//LOGI("[+]%s urb:0x%x nbytes: %d, buff_size: %d\n", __func__, urb, nbytes, aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size);

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
		urb->transfer_buffer = aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr;
		urb->transfer_buffer_length = aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size;
		urb->actual_length = 0;
	} else {
		LOGE("%s, %d, aud_tras_drv_info.voc_info.status:%d not need write uac spk data \n", __func__, __LINE__, aud_tras_drv_info.voc_info.status);
		return;
	}

	/* check status and size */
	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
		os_memcpy(aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr, aud_tras_drv_info.voc_info.uac_spk_buff, aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size);

		//os_memset(aud_tras_drv_info.voc_info.uac_spk_buff, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	}

    /* debug */
    AUD_SPK_COUNT_ADD_SIZE(nbytes);

	/* send msg to notify app to write speaker data */
	ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_DECODER, NULL);
	if (ret != kNoErr) {
		LOGE("%s, %d, send msg: AUD_TRAS_DRV_DECODER fail \n", __func__, __LINE__);
	}

	return;
}

static bk_err_t usb_hub_voc_uac_spk_port_device_urb_fill(bk_usb_hub_port_info *usbh_hub_port_info, struct usbh_urb *urb)
{
	struct usbh_audio *spk_device;

	if(urb && usbh_hub_port_info && usbh_hub_port_info->device_index == USB_UAC_SPEAKER_DEVICE) {
		spk_device = (struct usbh_audio *)(usbh_hub_port_info->usb_device);
		urb->pipe = (usbh_pipe_t)(spk_device->isoout);
		urb->complete = (usbh_complete_callback_t)usb_hub_voc_uac_spk_port_dev_complete_callback;
		urb->arg = urb;
		urb->timeout = 0;

		urb->transfer_buffer = aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr;
		urb->transfer_buffer_length = aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size;
		urb->num_of_iso_packets = 1;
	} else {
		LOGE("%s, %d, parameters is invalid, usbh_hub_port_info: %p, urb: %p\n", __func__, __LINE__, usbh_hub_port_info, urb);
		return BK_FAIL;
	}

	return BK_OK;
}

static void usb_hub_print_port_info(bk_usb_hub_port_info *port_info)
{
	LOGD("%s port_info:0x%x\r\n", __func__, port_info);
	LOGD("%s hport_index:%d\r\n", __func__, port_info->port_index);
	LOGD("%s device_index:%d\r\n", __func__, port_info->device_index);
	LOGD("%s hport:0x%x\r\n", __func__, port_info->hport);
	LOGD("%s hport_address:%d\r\n", __func__, port_info->hport->dev_addr);
	LOGD("%s usb_device:0x%x\r\n", __func__, port_info->usb_device);
	LOGD("%s interface_num:%d\r\n", __func__, port_info->interface_num);
	LOGD("%s usb_device_param:0x%x\r\n", __func__, port_info->usb_device_param);
	LOGD("%s usb_device_param_config:0x%x\r\n", __func__, port_info->usb_device_param_config);
}

static void usb_hub_port_uac_disconnect_callback(bk_usb_hub_port_info *port_info, uint32_t n)
{
	uint8_t count = 6;
	bk_err_t ret = BK_OK;

	LOGI("%s \n",__func__);

	usb_hub_print_port_info(port_info);

	if (port_info->port_index > 4) {
		LOGE("%s, %d, port_index: %d is not support \n", __func__, __LINE__, port_info->port_index);
		return;
	}

	/* reset port_info */
	if (port_info->device_index == USB_UAC_MIC_DEVICE) {
        if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
			aud_tras_drv_info.voc_info.mic_port_info[port_info->port_index - 1] = NULL;
		} else {
			LOGE("%s, %d, aud_tras_drv_info.work_mode: %d is error \n", __func__, __LINE__, aud_tras_drv_info.work_mode);
		}
	} else if (port_info->device_index == USB_UAC_SPEAKER_DEVICE) {
		if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
			aud_tras_drv_info.voc_info.spk_port_info[port_info->port_index - 1] = NULL;
		} else {
			LOGE("%s, %d, aud_tras_drv_info.work_mode: %d is error \n", __func__, __LINE__, aud_tras_drv_info.work_mode);
		}
	} else {
		LOGE("%s, %d, device_index: %d is not support \n", __func__, __LINE__, port_info->device_index);
		return;
	}

	aud_tras_drv_op_t op = AUD_TRAS_DRV_MAX;
	if (aud_tras_drv_info.uac_mic_status != AUD_INTF_UAC_MIC_NORMAL_DISCONNECTED && port_info->device_index == USB_UAC_MIC_DEVICE) {
		aud_tras_drv_info.uac_mic_status = AUD_INTF_UAC_MIC_ABNORMAL_DISCONNECTED;
		op = AUD_TRAS_DRV_UAC_MIC_DISCONT;
	} else if (aud_tras_drv_info.uac_spk_status != AUD_INTF_UAC_SPK_NORMAL_DISCONNECTED && port_info->device_index == USB_UAC_SPEAKER_DEVICE) {
		aud_tras_drv_info.uac_spk_status = AUD_INTF_UAC_SPK_ABNORMAL_DISCONNECTED;
		op = AUD_TRAS_DRV_UAC_SPK_DISCONT;
	} else {
		return;
	}

	do {
		if (count == 0)
			break;
		count--;
		ret = aud_tras_drv_send_msg(op, NULL);
		if (ret != BK_OK) {
			LOGE("%s, %d, send msg: AUD_TRAS_DRV_UAC_DISCONT fail: %d \n", __func__, __LINE__, count);
			rtos_delay_milliseconds(20);
		}
	} while (ret != BK_OK);
}


static bk_err_t aud_tras_uac_mic_connect_handle(void)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_OK;
	bk_uac_mic_config_t *uac_mic_param_config = NULL;
	bk_uac_device_brief_info_t *uac_device_param = NULL;
	uint8_t i = 0;

	/* notify app that uac disconnecting */
	if (aud_tras_drv_info.uac_connect_state_cb_exist) {
		uac_connect_state_msg.event = EVENT_UAC_CONNECT_STATE_NOTIFY;
		uac_connect_state_msg.param = AUD_INTF_UAC_MIC_CONNECTED;
		uac_connect_state_msg.sem = NULL;
		msg_send_notify_to_media_major_mailbox(&uac_connect_state_msg, APP_MODULE);
	}

	/* check Automatic recover uac connect */
	if (aud_tras_drv_info.uac_mic_status == AUD_INTF_UAC_MIC_ABNORMAL_DISCONNECTED) {
		/* uac automatically connect */
		if (!aud_tras_drv_info.uac_auto_connect) {
			LOGI("%s, %d, uac not automatically connect, need user todo \n", __func__, __LINE__);
			return BK_OK;
		}
	}

	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE && aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL) {
		return BK_OK;
	}

	/* config uac */
	LOGI("%s, %d, config uac mic\n", __func__, __LINE__);
	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
		if (aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1] == NULL) {
			LOGE("%s, %d, mic_port_info is NULL, mic_port_index: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.mic_port_index);
			err = BK_ERR_AUD_INTF_UAC_MIC;
			goto fail;
		}

		uac_mic_param_config = (bk_uac_mic_config_t *)aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1]->usb_device_param_config;
		uac_device_param = (bk_uac_device_brief_info_t *)aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1]->usb_device_param;
		/* check whether format and sample_rate is support */
		if (aud_tras_drv_info.voc_info.uac_config->mic_config.mic_format_tag != uac_device_param->mic_format_tag) {
			LOGE("%s, %d, mic_format_tag: %d is not support, invalid value: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.uac_config->mic_config.mic_format_tag, uac_device_param->mic_format_tag);
			err = BK_ERR_AUD_INTF_UAC_MIC;
			goto fail;
		}
		if (uac_device_param->mic_samples_frequence_num) {
			for (i = 0; i < uac_device_param->mic_samples_frequence_num; i++) {
				if (uac_device_param->mic_samples_frequence[i] == aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate) {
					break;
				}
			}
		} else {
			LOGE("%s, %d, mic not support, mic_samples_frequence_num: %d \n", __func__, __LINE__, uac_device_param->mic_samples_frequence_num);
			err = BK_ERR_AUD_INTF_UAC_MIC;
			goto fail;
		}
		if (i >= uac_device_param->mic_samples_frequence_num) {
			LOGE("%s, %d, mic sample_rate: %d not support \n", __func__, __LINE__, aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate);
			err = BK_ERR_AUD_INTF_UAC_MIC;
			goto fail;
		}
		uac_mic_param_config->mic_format_tag = aud_tras_drv_info.voc_info.uac_config->mic_config.mic_format_tag;
		uac_mic_param_config->mic_samples_frequence = aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate;
		uac_mic_param_config->mic_ep_desc = uac_device_param->mic_ep_desc;
		ret = bk_aud_uac_hub_port_dev_open(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1]);
		if (ret != BK_OK) {
			LOGE("%s, %d, start uac mic fail, ret: %d \n", __func__, __LINE__, ret);
			err = BK_ERR_AUD_INTF_UAC_MIC;
			goto fail;
		}

		if (aud_tras_drv_info.uac_mic_open_status == true && aud_tras_drv_info.uac_mic_open_current == false) {
			usb_hub_voc_uac_mic_port_device_urb_fill(aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1], s_usbh_uac_mic_urb);
			ret = bk_aud_uac_hub_dev_request_data(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, s_usbh_uac_mic_urb);
			if (ret != BK_OK) {
				LOGE("%s, %d, start uac mic fail, ret: %d \n", __func__, __LINE__, ret);
				err = BK_ERR_AUD_INTF_UAC_MIC;
				goto fail;
			} else {
				//aud_tras_drv_info.uac_mic_open_status = true;
				aud_tras_drv_info.uac_mic_open_current = true;
			}
		}
	} else {
		LOGE("%s, %d, work_mode is fail, work_mode: %d \r\n", __func__, __LINE__, aud_tras_drv_info.work_mode);
		err = BK_ERR_AUD_INTF_PARAM;
		goto fail;
	}

	return ret;

fail:

	return err;
}

static bk_err_t aud_tras_uac_spk_connect_handle(void)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_OK;
	bk_uac_spk_config_t *uac_spk_param_config = NULL;
	bk_uac_device_brief_info_t *uac_device_param = NULL;
	uint8_t i = 0;

	/* notify app that uac disconnecting */
	if (aud_tras_drv_info.uac_connect_state_cb_exist) {
		uac_connect_state_msg.event = EVENT_UAC_CONNECT_STATE_NOTIFY;
		uac_connect_state_msg.param = AUD_INTF_UAC_SPK_CONNECTED;
		uac_connect_state_msg.sem = NULL;
		msg_send_notify_to_media_major_mailbox(&uac_connect_state_msg, APP_MODULE);
	}

	/* check Automatic recover uac connect */
	if (aud_tras_drv_info.uac_spk_status == AUD_INTF_UAC_SPK_ABNORMAL_DISCONNECTED) {
		/* uac automatically connect */
		if (!aud_tras_drv_info.uac_auto_connect) {
			LOGI("%s, %d, uac not automatically connect, need user todo \n", __func__, __LINE__);
			return BK_OK;
		}
	}

	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE && aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL) {
		return BK_OK;
	}

	/* config uac */
	LOGI("%s, %d, config uac spk \n", __func__, __LINE__);

	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
		if (aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1] == NULL) {
			LOGI("%s, %d, spk_port_info is NULL, spk_port_index: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.spk_port_index);
			return BK_OK;
		}

		uac_spk_param_config = (bk_uac_spk_config_t *)aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1]->usb_device_param_config;
		uac_device_param = (bk_uac_device_brief_info_t *)aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1]->usb_device_param;
		/* check whether format and sample_rate is support */
		if (aud_tras_drv_info.voc_info.uac_config->spk_config.spk_format_tag != uac_device_param->spk_format_tag) {
			LOGE("%s, %d, spk_format_tag: %d is not support, invalid value: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.uac_config->spk_config.spk_format_tag, uac_device_param->spk_format_tag);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto fail;
		}
		if (uac_device_param->spk_samples_frequence_num) {
			for (i = 0; i < uac_device_param->spk_samples_frequence_num; i++) {
				if (uac_device_param->spk_samples_frequence[i] == aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate) {
					break;
				}
			}
		} else {
			LOGE("%s, %d, spk not support, spk_samples_frequence_num: %d \n", __func__, __LINE__, uac_device_param->spk_samples_frequence_num);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto fail;
		}
		if (i >= uac_device_param->spk_samples_frequence_num) {
			LOGE("%s, %d, spk sample_rate: %d not support \n", __func__, __LINE__, aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto fail;
		}
		uac_spk_param_config->spk_format_tag = aud_tras_drv_info.voc_info.uac_config->spk_config.spk_format_tag;
		uac_spk_param_config->spk_samples_frequence = aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate;
		uac_spk_param_config->spk_ep_desc = uac_device_param->spk_ep_desc;
		ret = bk_aud_uac_hub_port_dev_open(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1]);
		if (ret != BK_OK) {
			LOGE("%s, %d, start uac spk fail, ret: %d \n", __func__, __LINE__, ret);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto fail;
		}

		if (aud_tras_drv_info.uac_spk_open_status == true && aud_tras_drv_info.uac_spk_open_current == false) {
			usb_hub_voc_uac_spk_port_device_urb_fill(aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1], s_usbh_uac_spk_urb);
			ret = bk_aud_uac_hub_dev_request_data(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, s_usbh_uac_spk_urb);
			if (ret != BK_OK) {
				LOGE("%s, %d, start uac spk fail, ret: %d \r\n", __func__, __LINE__, ret);
				err = BK_ERR_AUD_INTF_UAC_SPK;
				goto fail;
			} else {
				aud_tras_drv_info.uac_spk_open_status = true;
				aud_tras_drv_info.uac_spk_open_current = true;
			}
		}
	} else {
		LOGE("%s, %d, work_mode is fail, work_mode: %d \r\n", __func__, __LINE__, aud_tras_drv_info.work_mode);
		err = BK_ERR_AUD_INTF_PARAM;
		goto fail;
	}

	return ret;

fail:

	return err;
}

static void usb_hub_port_uac_connect_callback(bk_usb_hub_port_info *port_info, uint32_t n)
{
	uint8_t count = 6;
	bk_err_t ret = BK_OK;

	LOGI("%s \n", __func__);

	if (port_info->port_index > 4) {
		LOGE("%s, %d, port_index: %d is not support \n", __func__, __LINE__, port_info->port_index);
		return;
	}

	/* save port_info */
	if (port_info->device_index == USB_UAC_MIC_DEVICE) {
		if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
			aud_tras_drv_info.voc_info.mic_port_info[port_info->port_index - 1] = port_info;
		} else {
			LOGE("%s, %d, aud_tras_drv_info.work_mode: %d is error \n", __func__, __LINE__, aud_tras_drv_info.work_mode);
		}

		bk_uac_mic_config_t *uac_device_param_canfig = (bk_uac_mic_config_t *)port_info->usb_device_param_config;
		struct usb_endpoint_descriptor *mic_ep_desc = (struct usb_endpoint_descriptor *)uac_device_param_canfig->mic_ep_desc;
		LOGI("     ------------ Audio Data Mic Endpoint Descriptor -----------  \r\n");
		LOGI("bLength                        : 0x%x (%d bytes)\r\n", mic_ep_desc->bLength, mic_ep_desc->bLength);
		LOGI("bDescriptorType                : 0x%x (Audio Endpoint Descriptor)\r\n", mic_ep_desc->bDescriptorType);
		LOGI("bEndpointAddress               : 0x%x (General)\r\n", mic_ep_desc->bEndpointAddress);
		LOGI("bmAttributes                   : 0x%x\r\n", mic_ep_desc->bmAttributes);
		LOGI("wMaxPacketSize                 : 0x%x\r\n", mic_ep_desc->wMaxPacketSize);
		LOGI("bInterval                      : 0x%x\r\n", mic_ep_desc->bInterval);
	} else if (port_info->device_index == USB_UAC_SPEAKER_DEVICE) {
		if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) {
			aud_tras_drv_info.voc_info.spk_port_info[port_info->port_index - 1] = port_info;
		} else {
			LOGE("%s, %d, aud_tras_drv_info.work_mode: %d is error \n", __func__, __LINE__, aud_tras_drv_info.work_mode);
		}

		bk_uac_spk_config_t *uac_device_param_canfig = (bk_uac_spk_config_t *)port_info->usb_device_param_config;
		struct usb_endpoint_descriptor *spk_ep_desc = (struct usb_endpoint_descriptor *)uac_device_param_canfig->spk_ep_desc;
		LOGI("	   ------------ Audio Data Spk Endpoint Descriptor -----------	\r\n");
		LOGI("bLength						 : 0x%x (%d bytes)\r\n", spk_ep_desc->bLength, spk_ep_desc->bLength);
		LOGI("bDescriptorType				 : 0x%x (Audio Endpoint Descriptor)\r\n", spk_ep_desc->bDescriptorType);
		LOGI("bEndpointAddress				 : 0x%x (General)\r\n", spk_ep_desc->bEndpointAddress);
		LOGI("bmAttributes					 : 0x%x\r\n", spk_ep_desc->bmAttributes);
		LOGI("wMaxPacketSize				 : 0x%x\r\n", spk_ep_desc->wMaxPacketSize);
		LOGI("bInterval 					 : 0x%x\r\n", spk_ep_desc->bInterval);
	} else {
		LOGE("%s, %d, device_index: %d is not support \n", __func__, __LINE__, port_info->device_index);
		return;
	}

	usb_hub_print_port_info(port_info);

	aud_tras_drv_op_t op = AUD_TRAS_DRV_MAX;
	if (aud_tras_drv_info.uac_mic_status != AUD_INTF_UAC_MIC_CONNECTED && port_info->device_index == USB_UAC_MIC_DEVICE) {
		aud_tras_drv_info.uac_mic_status = AUD_INTF_UAC_MIC_CONNECTED;
		op = AUD_TRAS_DRV_UAC_MIC_CONT;
	} else if (aud_tras_drv_info.uac_spk_status != AUD_INTF_UAC_SPK_CONNECTED && port_info->device_index == USB_UAC_SPEAKER_DEVICE) {
		aud_tras_drv_info.uac_spk_status = AUD_INTF_UAC_SPK_CONNECTED;
		op = AUD_TRAS_DRV_UAC_SPK_CONT;
	} else {
		return;
	}

	do {
		if (count == 0)
			break;
		count--;
		ret = aud_tras_drv_send_msg(op, NULL);
		if (ret != BK_OK) {
			LOGE("%s, %d, send msg: AUD_TRAS_DRV_UAC_CONT fail: %d \n", __func__, __LINE__, count);
			rtos_delay_milliseconds(20);
		}
	} while (ret != BK_OK);
}


static bk_err_t aud_tras_enc(void)
{
	bk_err_t ret = BK_OK;
	uint32_t size = 0;
	uint32_t i = 0;
	uint16_t temp_mic_samp_rate_points = aud_tras_drv_info.voc_info.mic_samp_rate_points;
	tx_info_t temp_tx_info;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
		return BK_OK;

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		if (aud_tras_drv_info.voc_info.aec_enable) {
			/* get data from aec_ring_buff */
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.aec_info->aec_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, read aec_rb :%d \n", __func__, __LINE__, size);
				os_memset(aud_tras_drv_info.voc_info.encoder_temp.pcm_data, 0, temp_mic_samp_rate_points*2);
				//goto encoder_exit;
			}
		} else {
#if (CONFIG_CACHE_ENABLE)
			flush_all_dcache();
#endif
			/* get data from mic_ring_buff */
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, read mic_rb :%d \n", __func__, __LINE__, size);
				os_memset(aud_tras_drv_info.voc_info.encoder_temp.pcm_data, 0, temp_mic_samp_rate_points*2);
				//goto encoder_exit;
			}
		}
	} else {
		if (aud_tras_drv_info.voc_info.aec_enable) {
			/* get data from aec_ring_buff */
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.aec_info->aec_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, read aec_rb :%d \n", __func__, __LINE__, size);
				goto encoder_exit;
			}
		} else {
			/* get data from mic_ring_buff */
			size = ring_buffer_read(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t *)aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
			if (size != temp_mic_samp_rate_points*2) {
				LOGE("%s, %d, the data readed from mic_ring_buff is not a frame, size:%d \n", __func__, __LINE__, size);
				goto encoder_exit;
			}

			/* dump uac data by uart */
			UAC_MIC_DATA_DUMP_DATA(aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points*2);
		}
	}

	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
			/* G711A encoding pcm data to a-law data*/
			for (i=0; i<temp_mic_samp_rate_points; i++) {
				aud_tras_drv_info.voc_info.encoder_temp.law_data[i] = linear2alaw(aud_tras_drv_info.voc_info.encoder_temp.pcm_data[i]);
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_G711U:
			/* G711U encoding pcm data to u-law data*/
			for (i=0; i<temp_mic_samp_rate_points; i++) {
				aud_tras_drv_info.voc_info.encoder_temp.law_data[i] = linear2ulaw(aud_tras_drv_info.voc_info.encoder_temp.pcm_data[i]);
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			break;

		default:
			break;
	}

	temp_tx_info = aud_tras_drv_info.voc_info.tx_info;
	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			os_memcpy(temp_tx_info.ping.buff_addr, aud_tras_drv_info.voc_info.encoder_temp.law_data, temp_mic_samp_rate_points);
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			os_memcpy(temp_tx_info.ping.buff_addr, aud_tras_drv_info.voc_info.encoder_temp.pcm_data, temp_mic_samp_rate_points * 2);
			break;

		default:
			break;
	}

#if (CONFIG_CACHE_ENABLE)
		flush_all_dcache();
#endif

	if (aud_tras_drv_info.voc_info.aud_tx_rb) {
		int free_size = ring_buffer_get_free_size(aud_tras_drv_info.voc_info.aud_tx_rb);
		if (free_size > temp_tx_info.buff_length) {
			//GPIO_UP(4);
			ring_buffer_write(aud_tras_drv_info.voc_info.aud_tx_rb, (uint8_t *)temp_tx_info.ping.buff_addr, temp_tx_info.buff_length);
			//GPIO_DOWN(4);
			//send msg to aud_tras
			mic_to_media_app_msg.event = EVENT_AUD_MIC_DATA_NOTIFY;
			mic_to_media_app_msg.param = temp_tx_info.buff_length;
			msg_send_notify_to_media_major_mailbox(&mic_to_media_app_msg, APP_MODULE);
		} else {
			//LOGE("aud_tx_rb free_size: %d \n", free_size);
		}
	}

	return ret;

encoder_exit:

	return BK_FAIL;
}


static bk_err_t aud_tras_dec(void)
{
	uint32_t size = 0;
	uint32_t i = 0;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
		return BK_OK;

#if (CONFIG_CACHE_ENABLE)
	flush_all_dcache();
#endif

	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			/* check the frame number in decoder_ring_buffer */
			if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
				if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
					//os_printf("decoder process \r\n", size);
					/* get G711A data from decoder_ring_buff */
					size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
						LOGE("%s, %d, read decoder_ring_buff G711A data fail \n", __func__, __LINE__);
						if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						else
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					}
				} else {
					if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					else
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
				}

				if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U) {
					/* G711U decoding u-law data to pcm data*/
					for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
						aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = ulaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
					}
				} else {
					/* G711A decoding a-law data to pcm data*/
					for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
						aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = alaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
					}
				}
			} else {
				if (ring_buffer_get_free_size(&aud_tras_drv_info.voc_info.speaker_rb) > aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
					/* check the frame number in decoder_ring_buffer */
					if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
						//os_printf("decoder process \r\n", size);
						/* get G711A data from decoder_ring_buff */
						size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.law_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points) {
							LOGE("%s, %d, read decoder_ring_buff G711A data fail \n", __func__, __LINE__);
							if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
								os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
							else
								os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						}
					} else {
						if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U)
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xFF, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
						else
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.law_data, 0xD5, aud_tras_drv_info.voc_info.speaker_samp_rate_points);
					}

					if (aud_tras_drv_info.voc_info.data_type == AUD_INTF_VOC_DATA_TYPE_G711U) {
						/* G711U decoding u-law data to pcm data*/
						for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
							aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = ulaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
						}
					} else {
						/* G711A decoding a-law data to pcm data*/
						for (i=0; i<aud_tras_drv_info.voc_info.speaker_samp_rate_points; i++) {
							aud_tras_drv_info.voc_info.decoder_temp.pcm_data[i] = alaw2linear(aud_tras_drv_info.voc_info.decoder_temp.law_data[i]);
						}
					}
				}
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
				if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
					//os_printf("decoder process \r\n", size);
					/* get pcm data from decoder_ring_buff */
					size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
						LOGE("%s, %d, read decoder_ring_buff pcm data fail \n", __func__, __LINE__);
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					}
				} else {
					os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
				}
			} else {
				if (ring_buffer_get_free_size(&aud_tras_drv_info.voc_info.speaker_rb) > aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
					/* check the frame number in decoder_ring_buffer */
					if (ring_buffer_get_fill_size(aud_tras_drv_info.voc_info.rx_info.decoder_rb) >= aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
						//os_printf("decoder process \r\n", size);
						/* get pcm data from decoder_ring_buff */
						size = ring_buffer_read(aud_tras_drv_info.voc_info.rx_info.decoder_rb, (uint8_t*)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
						if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2) {
							LOGE("%s, %d, read decoder_ring_buff pcm data fail \n", __func__, __LINE__);
							os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
						}
					} else {
						os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2);
					}
				}
			}
			break;

		default:
			break;
	}

#if CONFIG_AUD_TRAS_AEC_MIC_DELAY_DEBUG
	mic_delay_num++;
	os_memset(aud_tras_drv_info.voc_info.decoder_temp.pcm_data, 0, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	if (mic_delay_num == 50) {
		aud_tras_drv_info.voc_info.decoder_temp.pcm_data[0] = 0x2FFF;
		mic_delay_num = 0;
		LOGI("%s, %d, mic_delay_num \n", __func__, __LINE__);
	}
#endif

	if (aud_tras_drv_info.voc_info.aec_enable) {
		/* read mic fill data size */
		uint32_t mic_fill_size = ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.mic_rb));
		//os_printf("mic_rb: fill_size=%d \r\n", mic_fill_size);
		uint32_t speaker_fill_size = ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.speaker_rb));
		//os_printf("speaker_rb: fill_size=%d \r\n", speaker_fill_size);
		uint32_t ref_fill_size = ring_buffer_get_fill_size(&(aud_tras_drv_info.voc_info.aec_info->ref_rb));
		//os_printf("ref_rb: fill_size=%d \r\n", ref_fill_size);
		/* 设置参考信号延迟(采样点数，需要dump数据观察) */
#if CONFIG_AUD_TRAS_AEC_MIC_DELAY_DEBUG
		os_printf("%s, %d, MIC_DELAY: %d \n", __func__, __LINE__, (mic_fill_size + speaker_fill_size - ref_fill_size)/2);
#endif
		if ((mic_fill_size + speaker_fill_size - ref_fill_size)/2 < 0) {
			LOGE("%s, %d, MIC_DELAY is error, ref_fill_size: %d \n", __func__, __LINE__, ref_fill_size);
			//aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, 0);
			aud_aec_set_mic_delay(aud_tras_drv_info.voc_info.aec_info->aec, 0);
		} else {
			//aec_ctrl(aud_tras_drv_info.voc_info.aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, (mic_fill_size + speaker_fill_size - ref_fill_size)/2 + CONFIG_AUD_TRAS_AEC_MIC_DELAY_POINTS);
            //aud_aec_set_mic_delay(aud_tras_drv_info.voc_info.aec_info->aec, (mic_fill_size + speaker_fill_size - ref_fill_size)/2 + CONFIG_AUD_TRAS_AEC_MIC_DELAY_POINTS);
		}

		if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.aec_info->ref_rb)) > aud_tras_drv_info.voc_info.aec_info->samp_rate_points*2) {
			size = ring_buffer_write(&(aud_tras_drv_info.voc_info.aec_info->ref_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.aec_info->samp_rate_points*2);
			if (size != aud_tras_drv_info.voc_info.aec_info->samp_rate_points*2) {
				LOGE("%s, %d, write data to ref_ring_buff fail, size=%d \n", __func__, __LINE__, size);
				goto decoder_exit;
			}
		}


	}

#if CONFIG_AUD_TRAS_DAC_DEBUG
	if (aud_voc_dac_debug_flag) {
		//dump the data write to speaker
		FRESULT fr;
		uint32 uiTemp = 0;
		uint32_t i = 0, j = 0;
		/* write data to file */
		fr = f_write(&dac_debug_file, (uint32_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2, &uiTemp);
		if (fr != FR_OK) {
			LOGE("%s, %d, write %s fail \n", __func__, __LINE__, dac_debug_file_name);
			return BK_FAIL;
		}

		//write 8K sin data
		for (i = 0; i < aud_tras_drv_info.voc_info.speaker_samp_rate_points*2; i++) {
			for (j = 0; j < 8; j++) {
				*(uint32_t *)0x47800048 = PCM_8000[j];
			}
			i += 8;
		}
	} else 
#endif
	{
	/* save the data after G711A processed to encoder_ring_buffer */
		if (ring_buffer_get_free_size(&(aud_tras_drv_info.voc_info.speaker_rb)) > aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
			size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)aud_tras_drv_info.voc_info.decoder_temp.pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
			if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
				LOGE("%s, %d, the data writeten to speaker_ring_buff is not a frame, size=%d \n", __func__, __LINE__, size);
				goto decoder_exit;
			}
			aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq++;
		}

		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
			size = ring_buffer_read(&aud_tras_drv_info.voc_info.speaker_rb, (uint8_t *)aud_tras_drv_info.voc_info.uac_spk_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
			if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
				LOGE("%s, %d, read one frame pcm speaker data fail \n", __func__, __LINE__);
				os_memset(aud_tras_drv_info.voc_info.uac_spk_buff, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
			}

            /* dump uac data by uart */
            UAC_SPK_DATA_DUMP_DATA(aud_tras_drv_info.voc_info.uac_spk_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		}
	}
	/* call callback to notify app */
	if (aud_tras_drv_info.aud_tras_rx_spk_data)
		aud_tras_drv_info.aud_tras_rx_spk_data((unsigned int)aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);

	return BK_OK;

decoder_exit:

	return BK_FAIL;
}


static bk_err_t aud_tras_drv_voc_deinit(void)
{
//	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_NULL)
//		return BK_ERR_AUD_INTF_OK;

    /* debug */
    if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
        AUD_MIC_COUNT_CLOSE();
        UAC_MIC_DATA_DUMP_CLOSE();
    }

    if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
        AUD_SPK_COUNT_CLOSE();
        UAC_SPK_DATA_DUMP_CLOSE();
    }

	/* disable mic */
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		bk_aud_adc_stop();
		bk_aud_adc_deinit();
		bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
		bk_dma_deinit(aud_tras_drv_info.voc_info.adc_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.voc_info.adc_dma_id);
		if (aud_tras_drv_info.voc_info.adc_config) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.adc_config);
			aud_tras_drv_info.voc_info.adc_config = NULL;
		}
	} else {
		//bk_aud_uac_stop_mic();
//		bk_aud_uac_unregister_mic_callback();
		aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_IDLE;
		aud_tras_drv_info.uac_mic_open_status = false;
		aud_tras_drv_info.uac_mic_open_current = false;
	}

	/* disable spk */
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		bk_aud_dac_stop();
		aud_tras_dac_pa_ctrl(false);
		bk_aud_dac_deinit();
		if (aud_tras_drv_info.voc_info.dac_config) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.dac_config);
			aud_tras_drv_info.voc_info.dac_config = NULL;
		}
		/* stop dma */
		bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		bk_dma_deinit(aud_tras_drv_info.voc_info.dac_dma_id);
		bk_dma_free(DMA_DEV_AUDIO, aud_tras_drv_info.voc_info.dac_dma_id);
	} else {
		//bk_aud_uac_stop_spk();
//		bk_aud_uac_unregister_spk_callback();
//		bk_aud_uac_register_spk_buff_ptr(NULL, 0);
		aud_tras_drv_info.uac_spk_open_status = false;
		aud_tras_drv_info.uac_spk_open_current = false;
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD)
		bk_aud_driver_deinit();

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		aud_tras_drv_info.uac_mic_status = AUD_INTF_UAC_MIC_NORMAL_DISCONNECTED;
		/* close usb port device */
		if (aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1]) {
			bk_aud_uac_hub_port_dev_close(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1]);
		}
		/* power down */
		bk_aud_uac_power_down(USB_HOST_MODE, aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE);
		/* free uac_urb_mic_buff */
		if (aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr);
			aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr = NULL;
		}
		aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_size = 0;
		/* free s_usbh_uac_mic_urb */
		if (s_usbh_uac_mic_urb) {
			audio_tras_drv_free(s_usbh_uac_mic_urb);
		}
		s_usbh_uac_mic_urb = NULL;
		/* deregister connect and disconnect callback */
		bk_aud_uac_register_disconnect_cb(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, NULL);
		bk_aud_uac_register_connect_cb(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, NULL);
		/* reset mic_port_info */
		for (uint8_t n = 0; n < 4; n++) {
			aud_tras_drv_info.voc_info.mic_port_info[n] = NULL;
		}
		aud_tras_drv_info.voc_info.mic_port_index = USB_HUB_PORT_1;
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		aud_tras_drv_info.uac_spk_status = AUD_INTF_UAC_SPK_NORMAL_DISCONNECTED;
		/* close usb port device */
		if (aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1]) {
			bk_aud_uac_hub_port_dev_close(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1]);
		}
		/* power down */
		bk_aud_uac_power_down(USB_HOST_MODE, aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE);
		/* free uac_urb_spk_buff */
		if (aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr);
			aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr = NULL;
		}
		aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size = 0;
		/* free s_usbh_uac_spk_urb */
		if (s_usbh_uac_spk_urb) {
			audio_tras_drv_free(s_usbh_uac_spk_urb);
		}
		s_usbh_uac_spk_urb = NULL;
		/* deregister connect and disconnect callback */
		bk_aud_uac_register_disconnect_cb(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, NULL);
		bk_aud_uac_register_connect_cb(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, NULL);
		/* reset spk_port_info */
		for (uint8_t n = 0; n < 4; n++) {
			aud_tras_drv_info.voc_info.spk_port_info[n] = NULL;
		}
		aud_tras_drv_info.voc_info.spk_port_index = USB_HUB_PORT_1;

		if (aud_tras_drv_info.voc_info.uac_spk_buff) {
			audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_spk_buff);
		}
		aud_tras_drv_info.voc_info.uac_spk_buff = NULL;
		aud_tras_drv_info.voc_info.uac_spk_buff_size = 0;
	}

	/* disable AEC */
	aud_aec_deinit(aud_tras_drv_info.voc_info.aec_info);

	if (aud_tras_drv_info.voc_info.uac_config) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_config);
		aud_tras_drv_info.voc_info.uac_config = NULL;
	}

	/* free audio ring buffer */
	//mic deconfig
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.mic_rb));
	if (aud_tras_drv_info.voc_info.mic_ring_buff) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.mic_ring_buff);
		aud_tras_drv_info.voc_info.mic_ring_buff = NULL;
	}
	aud_tras_drv_info.voc_info.mic_samp_rate_points = 0;
	aud_tras_drv_info.voc_info.mic_frame_number = 0;
	aud_tras_drv_info.voc_info.adc_dma_id = DMA_ID_MAX;

	//speaker deconfig
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
	if (aud_tras_drv_info.voc_info.speaker_ring_buff) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.speaker_ring_buff);
		aud_tras_drv_info.voc_info.speaker_ring_buff = NULL;
	}
	aud_tras_drv_info.voc_info.speaker_samp_rate_points = 0;
	aud_tras_drv_info.voc_info.speaker_frame_number = 0;
	aud_tras_drv_info.voc_info.dac_dma_id = DMA_ID_MAX;

	/* tx and rx deconfig */
	//tx deconfig
	aud_tras_drv_info.voc_info.tx_info.tx_buff_status = false;
	aud_tras_drv_info.voc_info.tx_info.buff_length = 0;
	aud_tras_drv_info.voc_info.tx_info.ping.busy_status = false;
	aud_tras_drv_info.voc_info.tx_info.ping.buff_addr = NULL;
	//rx deconfig
	aud_tras_drv_info.voc_info.rx_info.rx_buff_status = false;
	aud_tras_drv_info.voc_info.rx_info.decoder_ring_buff = NULL;
	aud_tras_drv_info.voc_info.rx_info.decoder_rb = NULL;
	aud_tras_drv_info.voc_info.rx_info.frame_size = 0;
	aud_tras_drv_info.voc_info.rx_info.frame_num = 0;
	aud_tras_drv_info.voc_info.rx_info.rx_buff_seq_tail = 0;
	aud_tras_drv_info.voc_info.rx_info.aud_trs_read_seq = 0;
	aud_tras_drv_info.voc_info.rx_info.fifo_frame_num = 0;

	/* uac spk buffer */
	if (aud_tras_drv_info.voc_info.uac_spk_buff) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.uac_spk_buff);
		aud_tras_drv_info.voc_info.uac_spk_buff = NULL;
		aud_tras_drv_info.voc_info.uac_spk_buff_size = 0;
	}

	/* encoder_temp and decoder_temp deconfig*/
	if (aud_tras_drv_info.voc_info.encoder_temp.law_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.encoder_temp.law_data);
		aud_tras_drv_info.voc_info.encoder_temp.law_data = NULL;
	}
	if (aud_tras_drv_info.voc_info.decoder_temp.law_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.decoder_temp.law_data);
		aud_tras_drv_info.voc_info.decoder_temp.law_data = NULL;
	}
	if (aud_tras_drv_info.voc_info.encoder_temp.pcm_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.encoder_temp.pcm_data);
		aud_tras_drv_info.voc_info.encoder_temp.pcm_data = NULL;
	}
	if (aud_tras_drv_info.voc_info.decoder_temp.pcm_data) {
		audio_tras_drv_free(aud_tras_drv_info.voc_info.decoder_temp.pcm_data);
		aud_tras_drv_info.voc_info.decoder_temp.pcm_data = NULL;
	}
	aud_tras_drv_info.voc_info.aud_tx_rb = NULL;
	aud_tras_drv_info.voc_info.data_type = AUD_INTF_VOC_DATA_TYPE_PCM;

	/* change status:
				AUD_TRAS_DRV_VOC_IDLE --> AUD_TRAS_DRV_VOC_NULL
				AUD_TRAS_DRV_VOC_START --> AUD_TRAS_DRV_VOC_NULL
				AUD_TRAS_DRV_VOC_STOP --> AUD_TRAS_DRV_VOC_NULL
	*/
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_NULL;

	LOGI("%s, %d, voc deinit complete \n", __func__, __LINE__);
	return BK_ERR_AUD_INTF_OK;
}

/* audio transfer driver voice mode init */
static bk_err_t aud_tras_drv_voc_init(aud_intf_voc_config_t* voc_cfg)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;
	bk_usb_hub_port_info *port_dev_info = NULL;
	uint8_t count = 6;

	/* callback config */
//	aud_tras_drv_info.voc_info.aud_tras_drv_voc_event_cb = voc_cfg->aud_tras_drv_voc_event_cb;

	/* get aec config */
	aud_tras_drv_info.voc_info.aec_enable = voc_cfg->aec_enable;
	if (aud_tras_drv_info.voc_info.aec_enable) {
		aud_tras_drv_info.voc_info.aec_info = audio_tras_drv_malloc(sizeof(aec_info_t));
		if (aud_tras_drv_info.voc_info.aec_info == NULL) {
			LOGE("%s, %d, aec_info audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			aud_tras_drv_info.voc_info.aec_info->aec = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_config = NULL;
			aud_tras_drv_info.voc_info.aec_info->samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.aec_info->samp_rate_points = 0;
			aud_tras_drv_info.voc_info.aec_info->ref_addr = NULL;
			aud_tras_drv_info.voc_info.aec_info->mic_addr = NULL;
			aud_tras_drv_info.voc_info.aec_info->out_addr = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_ref_ring_buff = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_out_ring_buff = NULL;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.address = NULL;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.capacity = 0;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.wp= 0;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.rp = 0;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.dma_id = DMA_ID_MAX;
			aud_tras_drv_info.voc_info.aec_info->ref_rb.dma_type = RB_DMA_TYPE_NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.address = NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.capacity = 0;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.wp= 0;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.rp = 0;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.dma_id = DMA_ID_MAX;
			aud_tras_drv_info.voc_info.aec_info->aec_rb.dma_type = RB_DMA_TYPE_NULL;
			aud_tras_drv_info.voc_info.aec_info->aec_config = audio_tras_drv_malloc(sizeof(aec_config_t));
			if (aud_tras_drv_info.voc_info.aec_info->aec_config == NULL) {
				LOGE("%s, %d, aec_config_t audio_tras_drv_malloc fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			} else {
				aud_tras_drv_info.voc_info.aec_info->aec_config->init_flags = voc_cfg->aec_setup->init_flags;
				aud_tras_drv_info.voc_info.aec_info->aec_config->mic_delay = voc_cfg->aec_setup->mic_delay;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ec_depth = voc_cfg->aec_setup->ec_depth;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ref_scale = voc_cfg->aec_setup->ref_scale;
				aud_tras_drv_info.voc_info.aec_info->aec_config->voice_vol = voc_cfg->aec_setup->voice_vol;
				aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxThr = voc_cfg->aec_setup->TxRxThr;
				aud_tras_drv_info.voc_info.aec_info->aec_config->TxRxFlr = voc_cfg->aec_setup->TxRxFlr;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ns_level = voc_cfg->aec_setup->ns_level;
				aud_tras_drv_info.voc_info.aec_info->aec_config->ns_para = voc_cfg->aec_setup->ns_para;
				aud_tras_drv_info.voc_info.aec_info->aec_config->drc = voc_cfg->aec_setup->drc;
			}
		}
	} else {
		aud_tras_drv_info.voc_info.aec_info = NULL;
	}

	aud_tras_drv_info.voc_info.aud_tx_rb = voc_cfg->aud_tx_rb;
	aud_tras_drv_info.voc_info.data_type = voc_cfg->data_type;
	LOGI("%s, %d, aud_tras_drv_info.voc_info.data_type:%d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.data_type);
	aud_tras_drv_info.voc_info.mic_en = voc_cfg->mic_en;
	aud_tras_drv_info.voc_info.spk_en = voc_cfg->spk_en;
	aud_tras_drv_info.voc_info.mic_type = voc_cfg->mic_type;
	aud_tras_drv_info.voc_info.spk_type = voc_cfg->spk_type;

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* get audio adc config */
		aud_tras_drv_info.voc_info.adc_config = audio_tras_drv_malloc(sizeof(aud_adc_config_t));
		if (aud_tras_drv_info.voc_info.adc_config == NULL) {
			LOGE("%s, %d, adc_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			aud_tras_drv_info.voc_info.adc_config->adc_chl = AUD_ADC_CHL_L;
			aud_tras_drv_info.voc_info.adc_config->samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.adc_config->adc_gain = voc_cfg->aud_setup.adc_gain;	//default: 0x2d
			aud_tras_drv_info.voc_info.adc_config->adc_samp_edge = AUD_ADC_SAMP_EDGE_RISING;
			aud_tras_drv_info.voc_info.adc_config->adc_mode = AUD_ADC_MODE_DIFFEN;
			aud_tras_drv_info.voc_info.adc_config->clk_src = AUD_CLK_XTAL;
		}
	} else {
		/* set audio uac config */
		if (aud_tras_drv_info.voc_info.uac_config == NULL) {
			aud_tras_drv_info.voc_info.uac_config = audio_tras_drv_malloc(sizeof(aud_uac_config_t));
			if (aud_tras_drv_info.voc_info.uac_config == NULL) {
				LOGE("%s, %d, uac_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			} else {
				aud_tras_drv_info.voc_info.uac_config->mic_config.mic_format_tag = AUD_UAC_DATA_FORMAT_TYPE_PCM;
				aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate = 8000;
				aud_tras_drv_info.voc_info.uac_config->spk_config.spk_format_tag = AUD_UAC_DATA_FORMAT_TYPE_PCM;
				aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate = 8000;
				aud_tras_drv_info.voc_info.uac_config->spk_config.spk_volume = 0;
			}
			aud_tras_drv_info.voc_info.mic_port_index = voc_cfg->mic_port_index;
			aud_tras_drv_info.voc_info.uac_config->mic_config.mic_samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.uac_config->spk_config.spk_samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.uac_config->spk_config.spk_volume = voc_cfg->aud_setup.dac_gain;
		}
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* get audio dac config */
		aud_tras_drv_info.voc_info.dac_config = audio_tras_drv_malloc(sizeof(aud_dac_config_t));
		if (aud_tras_drv_info.voc_info.adc_config == NULL) {
			LOGE("%s, %d, dac_config audio_tras_drv_malloc fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			aud_tras_drv_info.voc_info.dac_config->samp_rate = voc_cfg->samp_rate;
			aud_tras_drv_info.voc_info.dac_config->dac_chl = AUD_DAC_CHL_L;
			aud_tras_drv_info.voc_info.dac_config->work_mode = voc_cfg->aud_setup.spk_mode;
			aud_tras_drv_info.voc_info.dac_config->dac_gain = voc_cfg->aud_setup.dac_gain;	//default 2D  3F  15
			aud_tras_drv_info.voc_info.dac_config->dac_clk_invert = AUD_DAC_CLK_INVERT_RISING;
			aud_tras_drv_info.voc_info.dac_config->clk_src = AUD_CLK_XTAL;
		}
	} else {
		aud_tras_drv_info.voc_info.spk_port_index = voc_cfg->spk_port_index;
	}

	/* get ring buffer config */
	//aud_tras_drv_info.voc_info.mode = setup->aud_trs_mode;
	switch (voc_cfg->samp_rate) {
		case 8000:
			aud_tras_drv_info.voc_info.mic_samp_rate_points = 160;
			aud_tras_drv_info.voc_info.speaker_samp_rate_points = 160;
			break;

		case 16000:
			aud_tras_drv_info.voc_info.mic_samp_rate_points = 320;
			aud_tras_drv_info.voc_info.speaker_samp_rate_points = 320;
			break;

		default:
			break;
	}

	aud_tras_drv_info.voc_info.mic_frame_number = voc_cfg->aud_setup.mic_frame_number;
	aud_tras_drv_info.voc_info.speaker_frame_number = voc_cfg->aud_setup.speaker_frame_number;

	/* get tx and rx context config */
	aud_tras_drv_info.voc_info.tx_info = voc_cfg->tx_info;
	aud_tras_drv_info.voc_info.rx_info = voc_cfg->rx_info;


	/*  -------------------------step0: init audio and config ADC and DAC -------------------------------- */
	/* config mailbox according audio transfer work mode */

	/*  -------------------------step2: init AEC and malloc two ring buffers -------------------------------- */
	/* init aec and config aec according AEC_enable*/
	if (aud_tras_drv_info.voc_info.aec_enable) {
		LOGI("%s, %d, aec samp_rate_points: %d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.aec_info->samp_rate_points);
		//ret = aud_tras_drv_aec_cfg();
		ret = aud_aec_init(aud_tras_drv_info.voc_info.aec_info);
		if (ret != BK_OK) {
			err = BK_ERR_AUD_INTF_AEC;
			goto aud_tras_drv_voc_init_exit;
		}
#if 0
		ret = aud_tras_drv_aec_buff_cfg(aud_tras_drv_info.voc_info.aec_info);
		if (ret != BK_OK) {
			err = ret;
			goto aud_tras_drv_voc_init_exit;
		}
#endif
		LOGI("step2: init AEC and malloc two ring buffers complete \n");
	}

	/* -------------------step3: init and config DMA to carry mic and ref data ----------------------------- */
	aud_tras_drv_info.voc_info.mic_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_tras_drv_info.voc_info.mic_ring_buff == NULL) {
		LOGE("%s, %d, malloc mic ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* init dma driver */
		ret = bk_dma_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, dma driver init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* allocate free DMA channel */
		aud_tras_drv_info.voc_info.adc_dma_id = bk_dma_alloc(DMA_DEV_AUDIO);
		if ((aud_tras_drv_info.voc_info.adc_dma_id < DMA_ID_0) || (aud_tras_drv_info.voc_info.adc_dma_id >= DMA_ID_MAX)) {
			LOGE("%s, %d, malloc adc dma fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* config audio adc dma to carry mic data to "mic_ring_buff" */
		ret = aud_tras_adc_dma_config(aud_tras_drv_info.voc_info.adc_dma_id, aud_tras_drv_info.voc_info.mic_ring_buff, (aud_tras_drv_info.voc_info.mic_samp_rate_points*2)*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.mic_samp_rate_points*2, AUD_INTF_MIC_CHL_MIC1);
		if (ret != BK_OK) {
			LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
			err = ret;
			goto aud_tras_drv_voc_init_exit;
		}

		ring_buffer_init(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.adc_dma_id, RB_DMA_TYPE_WRITE);

		LOGI("step3: init and config mic DMA complete, adc_dma_id:%d, mic_ring_buff:%p, size:%d, carry_length:%d \n", aud_tras_drv_info.voc_info.adc_dma_id, aud_tras_drv_info.voc_info.mic_ring_buff, (aud_tras_drv_info.voc_info.mic_samp_rate_points*2)*aud_tras_drv_info.voc_info.mic_frame_number, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
	} else if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
		/* init mic_ring_buff */
		ring_buffer_init(&(aud_tras_drv_info.voc_info.mic_rb), (uint8_t*)aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);
		LOGI("%s, %d, uac mic_ring_buff:%p, size:%d \n", __func__, __LINE__, aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);

		/* debug */
		UAC_MIC_DATA_DUMP_OPEN();

		/* register uac connect and disconnect callback */
		ret = bk_aud_uac_register_disconnect_cb(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, usb_hub_port_uac_disconnect_callback);
		if (ret != BK_OK) {
			LOGE("%s, %d, register uac mic disconnect cb fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_DRV;
			goto aud_tras_drv_voc_init_exit;
		}

		ret = bk_aud_uac_register_connect_cb(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, usb_hub_port_uac_connect_callback);
		if (ret != BK_OK) {
			LOGE("%s, %d, register uac mic connect cb fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_DRV;
			goto aud_tras_drv_voc_init_exit;
		}

		s_usbh_uac_mic_urb = audio_tras_drv_malloc(sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet));
		if(s_usbh_uac_mic_urb == NULL) {
			LOGI("No memory to alloc urb\r\n");
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			memset(s_usbh_uac_mic_urb, 0, sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet));
		}

		LOGI("%s, %d, power on uac mic port \n", __func__, __LINE__);
        ret = bk_aud_uac_power_on(USB_HOST_MODE, aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE);
        if (ret != BK_OK) {
            LOGE("%s, %d, power on uac mic port fail \n", __func__, __LINE__);
            err = BK_ERR_AUD_INTF_UAC_DRV;
            goto aud_tras_drv_voc_init_exit;
        }
		/* check whether device power on */
		port_dev_info = NULL;
		ret = bk_aud_uac_hub_port_check_device(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, &port_dev_info);
		if (ret == BK_OK) {
			/* already power on */
			aud_tras_drv_info.voc_info.mic_port_info[port_dev_info->port_index - 1] = port_dev_info;

			/* set uac status and send  */
			count = 6;
			if (aud_tras_drv_info.uac_mic_status != AUD_INTF_UAC_MIC_CONNECTED) {
				aud_tras_drv_info.uac_mic_status = AUD_INTF_UAC_MIC_CONNECTED;
				do {
					if (count == 0)
						break;
					count--;
					ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_MIC_CONT, NULL);
					if (ret != BK_OK) {
						LOGE("%s, %d, send msg: AUD_TRAS_DRV_UAC_MIC_CONT fail: %d \n", __func__, __LINE__, count);
						rtos_delay_milliseconds(20);
					}
				} while (ret != BK_OK);
			}
		}

		aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_size = aud_tras_drv_info.voc_info.mic_samp_rate_points * 2;
		aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_size);
		if (aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr == NULL) {
			LOGE("%s, %d, malloc uac_urb_mic_buff fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			os_memset(aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_addr, 0, aud_tras_drv_info.voc_info.uac_urb_mic_buff.buff_size);
		}

		LOGI("step3: init voc mic ring buff:%p, size:%d complete \n", aud_tras_drv_info.voc_info.mic_ring_buff, aud_tras_drv_info.voc_info.mic_samp_rate_points*2*aud_tras_drv_info.voc_info.mic_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	} else {
		err = BK_ERR_AUD_INTF_UAC_MIC;
		goto aud_tras_drv_voc_init_exit;
	}

	/*  -------------------step4: init and config DMA to carry dac data ----------------------------- */
	aud_tras_drv_info.voc_info.speaker_ring_buff = (int32_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aud_tras_drv_info.voc_info.speaker_ring_buff == NULL) {
		LOGE("%s, %d, malloc speaker ring buffer fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* init dma driver */
		ret = bk_dma_driver_init();
		if (ret != BK_OK) {
			LOGE("%s, %d, dma driver init fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* allocate free DMA channel */
		aud_tras_drv_info.voc_info.dac_dma_id = bk_dma_alloc(DMA_DEV_AUDIO);
		if ((aud_tras_drv_info.voc_info.dac_dma_id < DMA_ID_0) || (aud_tras_drv_info.voc_info.dac_dma_id >= DMA_ID_MAX)) {
			LOGE("%s, %d, malloc dac dma fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_DMA;
			goto aud_tras_drv_voc_init_exit;
		}

		/* config audio dac dma to carry dac data to "speaker_ring_buff" */
		ret = aud_tras_dac_dma_config(aud_tras_drv_info.voc_info.dac_dma_id, aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2, AUD_INTF_SPK_CHL_LEFT);
		if (ret != BK_OK) {
			LOGE("%s, %d, config audio adc dma fail \n", __func__, __LINE__);
			err = ret;
			goto aud_tras_drv_voc_init_exit;
		}

		ring_buffer_init(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t*)aud_tras_drv_info.voc_info.speaker_ring_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.dac_dma_id, RB_DMA_TYPE_READ);

		LOGI("step4: init and config speaker DMA complete, dac_dma_id:%d, speaker_ring_buff:%p, size:%d, carry_length:%d \r\n", aud_tras_drv_info.voc_info.dac_dma_id, aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	} else if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {

		/* debug */
		UAC_SPK_DATA_DUMP_OPEN();

		/* save one frame pcm speaker data for usb used */
		aud_tras_drv_info.voc_info.uac_spk_buff_size = aud_tras_drv_info.voc_info.speaker_samp_rate_points*2;
		aud_tras_drv_info.voc_info.uac_spk_buff = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.uac_spk_buff_size);
		if (!aud_tras_drv_info.voc_info.uac_spk_buff) {
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			os_memset(aud_tras_drv_info.voc_info.uac_spk_buff, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		}

		ring_buffer_init(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t*)aud_tras_drv_info.voc_info.speaker_ring_buff, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

		/* register uac connect and disconnect callback */
		ret = bk_aud_uac_register_disconnect_cb(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, usb_hub_port_uac_disconnect_callback);
		if (ret != BK_OK) {
			LOGE("%s, %d, register uac spk disconnect cb fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto aud_tras_drv_voc_init_exit;
		}
		
		ret = bk_aud_uac_register_connect_cb(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, usb_hub_port_uac_connect_callback);
		if (ret != BK_OK) {
			LOGE("%s, %d, register uac spk connect cb fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_UAC_SPK;
			goto aud_tras_drv_voc_init_exit;
		}

		s_usbh_uac_spk_urb = audio_tras_drv_malloc(sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet));
		if(s_usbh_uac_spk_urb == NULL) {
			LOGI("No memory to alloc urb\r\n");
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			memset(s_usbh_uac_spk_urb, 0, sizeof(struct usbh_urb) + sizeof(struct usbh_iso_frame_packet));
		}

		LOGI("%s, %d, power on uac spk port \n", __func__, __LINE__);
        ret = bk_aud_uac_power_on(USB_HOST_MODE, aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE);
        if (ret != BK_OK) {
            LOGE("%s, %d, power on uac mic port fail \n", __func__, __LINE__);
            err = BK_ERR_AUD_INTF_UAC_DRV;
            goto aud_tras_drv_voc_init_exit;
        }
		/* check whether device power on */
		port_dev_info = NULL;
		ret = bk_aud_uac_hub_port_check_device(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, &port_dev_info);
		if (ret == BK_OK) {
			/* already power on */
			aud_tras_drv_info.voc_info.spk_port_info[port_dev_info->port_index - 1] = port_dev_info;

			/* set uac status and send  */
			count = 6;
			if (aud_tras_drv_info.uac_spk_status != AUD_INTF_UAC_SPK_CONNECTED) {
				aud_tras_drv_info.uac_spk_status = AUD_INTF_UAC_SPK_CONNECTED;
				do {
					if (count == 0)
						break;
					count--;
					ret = aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_SPK_CONT, NULL);
					if (ret != BK_OK) {
						LOGE("%s, %d, send msg: AUD_TRAS_DRV_UAC_SPK_CONT fail: %d \n", __func__, __LINE__, count);
						rtos_delay_milliseconds(20);
					}
				} while (ret != BK_OK);
			}
		}

		aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size = aud_tras_drv_info.voc_info.speaker_samp_rate_points * 2;
		aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size);
		if (aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr == NULL) {
			LOGE("%s, %d, malloc uac_urb_spk_buff fail \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto aud_tras_drv_voc_init_exit;
		} else {
			os_memset(aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_addr, 0, aud_tras_drv_info.voc_info.uac_urb_spk_buff.buff_size);
		}

		LOGI("step4: init uac speaker_ring_buff:%p, spk_ring_buff_size:%d, uac_spk_buff:%p, uac_spk_buff_size:%d\n", aud_tras_drv_info.voc_info.speaker_ring_buff, (aud_tras_drv_info.voc_info.speaker_samp_rate_points*2)*aud_tras_drv_info.voc_info.speaker_frame_number + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, aud_tras_drv_info.voc_info.uac_spk_buff, aud_tras_drv_info.voc_info.uac_spk_buff_size);
	} else {
		//TODO
	}

	/*  -------------------------step6: init all audio ring buffers -------------------------------- */
	/* init encoder and decoder temp buffer */
	aud_tras_drv_info.voc_info.encoder_temp.pcm_data = (int16_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points * 2);
	if (aud_tras_drv_info.voc_info.encoder_temp.pcm_data == NULL) {
		LOGE("%s, %d, malloc pcm_data of encoder used fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	aud_tras_drv_info.voc_info.decoder_temp.pcm_data = (int16_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
	if (aud_tras_drv_info.voc_info.decoder_temp.pcm_data == NULL) {
		LOGE("%s, %d, malloc pcm_data of decoder used fail \n", __func__, __LINE__);
		err = BK_ERR_AUD_INTF_MEMY;
		goto aud_tras_drv_voc_init_exit;
	}

	switch (aud_tras_drv_info.voc_info.data_type) {
		case AUD_INTF_VOC_DATA_TYPE_G711A:
		case AUD_INTF_VOC_DATA_TYPE_G711U:
			LOGI("%s, %d, malloc law_data temp buffer \n", __func__, __LINE__);
			aud_tras_drv_info.voc_info.encoder_temp.law_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points);
			if (aud_tras_drv_info.voc_info.encoder_temp.law_data == NULL) {
				LOGE("%s, %d, malloc law_data of encoder used fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			}

			aud_tras_drv_info.voc_info.decoder_temp.law_data = (unsigned char *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points);
			if (aud_tras_drv_info.voc_info.decoder_temp.law_data == NULL) {
				LOGE("%s, %d, malloc law_data of decoder used fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_MEMY;
				goto aud_tras_drv_voc_init_exit;
			}
			break;

		case AUD_INTF_VOC_DATA_TYPE_PCM:
			//os_printf("not need to malloc law_data temp buffer \r\n");
			break;

		default:
			break;
	}

    /* debug */
    if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) {
        AUD_MIC_COUNT_OPEN();
    }

    if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
        AUD_SPK_COUNT_OPEN();
    }

	/* change status: AUD_TRAS_DRV_VOC_NULL --> AUD_TRAS_DRV_VOC_IDLE */
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_IDLE;
	LOGD("step6: init aud ring buff complete \n");

	LOGI("%s, %d, init voc complete \n", __func__, __LINE__);

	return BK_ERR_AUD_INTF_OK;

aud_tras_drv_voc_init_exit:
	/* audio transfer driver deconfig */
	aud_tras_drv_voc_deinit();
	return err;
}

static bk_err_t aud_tras_drv_voc_start(void)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;
	uint32_t size = 0;
	uint8_t *pcm_data = NULL;

	LOGI("%s \n", __func__);

	if (aud_tras_drv_info.voc_info.mic_en == AUD_INTF_VOC_MIC_OPEN) {
		if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
			/* init audio and config ADC and DAC */
			ret = aud_tras_adc_config(aud_tras_drv_info.voc_info.adc_config);
			if (ret != BK_OK) {
				LOGE("%s, %d, audio adc init fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_ADC;
				goto audio_start_transfer_exit;
			}

			/* start DMA */
			ret = bk_dma_start(aud_tras_drv_info.voc_info.adc_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start adc dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto audio_start_transfer_exit;
			}

			/* enable adc */
			/* wait receive data and then open adc */
			bk_aud_adc_start();
		} else {
			LOGI("%s, %d, start uac mic \n", __func__, __LINE__);

			/* check uac connect status */
			if (aud_tras_drv_info.uac_mic_status == AUD_INTF_UAC_MIC_CONNECTED) {
				usb_hub_voc_uac_mic_port_device_urb_fill(aud_tras_drv_info.voc_info.mic_port_info[aud_tras_drv_info.voc_info.mic_port_index - 1], s_usbh_uac_mic_urb);
				ret = bk_aud_uac_hub_dev_request_data(aud_tras_drv_info.voc_info.mic_port_index, USB_UAC_MIC_DEVICE, s_usbh_uac_mic_urb);
				if (ret != BK_OK) {
					LOGE("%s, %d, start uac mic fail, ret: %d \n", __func__, __LINE__, ret);
					err = BK_ERR_AUD_INTF_UAC_MIC;
					goto audio_start_transfer_exit;
				} else {
					aud_tras_drv_info.uac_mic_open_status = true;
					aud_tras_drv_info.uac_mic_open_current = true;
				}
			} else {
				aud_tras_drv_info.uac_mic_open_status = true;
			}
		}
	}

	if (aud_tras_drv_info.voc_info.spk_en == AUD_INTF_VOC_SPK_OPEN) {
		pcm_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		if (pcm_data == NULL) {
			LOGE("%s, %d, malloc temp pcm_data fial \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto audio_start_transfer_exit;
		} else {
			os_memset(pcm_data, 0x00, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		}

		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			ret = aud_tras_dac_config(aud_tras_drv_info.voc_info.dac_config);
			if (ret != BK_OK) {
				LOGE("%s, %d, audio dac init fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DAC;
				goto audio_start_transfer_exit;
			}

			/* enable dac */
			bk_aud_dac_start();
			aud_tras_dac_pa_ctrl(true);

			ret = bk_dma_start(aud_tras_drv_info.voc_info.dac_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start dac dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto audio_start_transfer_exit;
			}
		} else {
#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS
			/* reopen uac mic */
			if ((aud_tras_drv_info.voc_info.mic_en == AUD_INTF_VOC_MIC_OPEN) && (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC)) {
//				bk_aud_uac_start_mic();
			}
#endif

			LOGI("%s, %d, start uac spk \n", __func__, __LINE__);
			/* check uac connect status */
			if (aud_tras_drv_info.uac_spk_status == AUD_INTF_UAC_SPK_CONNECTED) {
				usb_hub_voc_uac_spk_port_device_urb_fill(aud_tras_drv_info.voc_info.spk_port_info[aud_tras_drv_info.voc_info.spk_port_index - 1], s_usbh_uac_spk_urb);
				ret = bk_aud_uac_hub_dev_request_data(aud_tras_drv_info.voc_info.spk_port_index, USB_UAC_SPEAKER_DEVICE, s_usbh_uac_spk_urb);
				//ret = bk_aud_uac_start_spk();
				if (ret != BK_OK) {
					LOGE("%s, %d, start uac spk fail, ret: %d \r\n", __func__, __LINE__, ret);
					err = BK_ERR_AUD_INTF_UAC_SPK;
					goto audio_start_transfer_exit;
				} else {
					aud_tras_drv_info.uac_spk_open_status = true;
					aud_tras_drv_info.uac_spk_open_current = true;
				}
			} else {
				aud_tras_drv_info.uac_spk_open_status = true;
			}
		}

		/* write two frame data to speaker and ref ring buffer */
		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
			LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto audio_start_transfer_exit;
		}
		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.speaker_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.speaker_samp_rate_points*2) {
			LOGE("%s, %d, the data write to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto audio_start_transfer_exit;
		}

		audio_tras_drv_free(pcm_data);
		pcm_data = NULL;
	}
	LOGI("%s, %d, voice start complete \n", __func__, __LINE__);

	/* change status:
				AUD_TRAS_DRV_VOC_STA_IDLE --> AUD_TRAS_DRV_VOC_STA_START
				AUD_TRAS_DRV_VOC_STA_STOP --> AUD_TRAS_DRV_VOC_STA_START
	*/
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_START;


	return BK_ERR_AUD_INTF_OK;

audio_start_transfer_exit:
	//deinit audio transfer
	if (pcm_data)
		audio_tras_drv_free(pcm_data);

	return err;
}

static bk_err_t aud_tras_drv_voc_stop(void)
{
	bk_err_t ret = BK_OK;

	if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_STOP)
		return ret;

	LOGI("%s \n", __func__);

	/* stop adc and dac dma */
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		ret = bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
		if (ret != BK_OK) {
			LOGE("%s, %d, stop adc dma fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_DMA;
		}
	} else {
		LOGI("%s, %d, stop uac mic \n", __func__, __LINE__);
		aud_tras_drv_info.uac_mic_open_status = false;
		aud_tras_drv_info.uac_mic_open_current = false;
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		ret = bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		if (ret != BK_OK) {
			LOGE("%s, %d, stop dac dma fail \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_DMA;
		}
	} else {
		LOGI("%s, %d, stop uac spk \n", __func__, __LINE__);
		aud_tras_drv_info.uac_spk_open_status = false;
		aud_tras_drv_info.uac_spk_open_current = false;
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		/* disable adc */
		bk_aud_adc_stop();
		bk_aud_adc_deinit();
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		/* disable dac */
		bk_aud_dac_stop();
		aud_tras_dac_pa_ctrl(false);
		bk_aud_dac_deinit();
	}

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD || aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD)
	/* deinit audio driver */
		bk_aud_driver_deinit();

	/* clear adc and dac ring buffer */
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
	ring_buffer_clear(&(aud_tras_drv_info.voc_info.mic_rb));

	uac_mic_read_flag = false;
	uac_spk_write_flag = false;

	/* change status:
				AUD_TRAS_DRV_VOC_STA_IDLE --> AUD_TRAS_DRV_VOC_STA_STOP
				AUD_TRAS_DRV_VOC_STA_STOP --> AUD_TRAS_DRV_VOC_STA_STOP
	*/
	aud_tras_drv_info.voc_info.status = AUD_TRAS_DRV_VOC_STA_STOP;

	LOGI("%s, %d, stop voice transfer complete \n", __func__, __LINE__);

	return ret;
}

static bk_err_t aud_tras_drv_voc_ctrl_mic(aud_intf_voc_mic_ctrl_t mic_en)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;

	GLOBAL_INT_DECLARATION();

	if (mic_en == AUD_INTF_VOC_MIC_OPEN) {
		if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
			LOGI("%s, %d, open onboard mic \n", __func__, __LINE__);
			/* enable adc */
			bk_aud_adc_start();

			ret = bk_dma_start(aud_tras_drv_info.voc_info.adc_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start adc dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto voc_ctrl_mic_fail;
			}
		} else {
			LOGI("%s, %d, open uac mic \n", __func__, __LINE__);
			/* start send uac mic urb */
			//TODO
#if 0
			ret = bk_aud_uac_start_mic();
			if (ret != BK_OK) {
				LOGE("%s, %d, start uac mic fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_MIC;
				goto voc_ctrl_mic_fail;
			}
#endif
		}
	} else if (mic_en == AUD_INTF_VOC_MIC_CLOSE) {
		if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
			LOGI("%s, %d, close onboard mic \n", __func__, __LINE__);
			bk_aud_adc_stop();
			bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
			ring_buffer_clear(&(aud_tras_drv_info.voc_info.mic_rb));
		} else {
			LOGI("%s, %d, close uac mic \n", __func__, __LINE__);
			/* stop send uac mic urb */
			//TODO
#if 0
			ret = bk_aud_uac_stop_mic();
			if (ret != BK_OK) {
				LOGE("%s, %d, stop uac mic fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_MIC;
				goto voc_ctrl_mic_fail;
			}
#endif
			uac_mic_read_flag = false;
		}
	} else {
		err = BK_ERR_AUD_INTF_PARAM;
		goto voc_ctrl_mic_fail;
	}

	GLOBAL_INT_DISABLE();
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD)
		aud_tras_drv_info.voc_info.mic_en = mic_en;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;

voc_ctrl_mic_fail:
	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		bk_aud_adc_stop();
		bk_dma_stop(aud_tras_drv_info.voc_info.adc_dma_id);
	}

	return err;
}

static bk_err_t aud_tras_drv_voc_ctrl_spk(aud_intf_voc_spk_ctrl_t spk_en)
{
	bk_err_t ret = BK_OK;
	bk_err_t err = BK_ERR_AUD_INTF_FAIL;
	uint32_t size = 0;
	uint8_t *pcm_data = NULL;

	GLOBAL_INT_DECLARATION();

	if (spk_en == AUD_INTF_VOC_SPK_OPEN) {
		pcm_data = (uint8_t *)audio_tras_drv_malloc(aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		if (pcm_data == NULL) {
			LOGE("%s, %d, malloc temp pcm_data fial \n", __func__, __LINE__);
			err = BK_ERR_AUD_INTF_MEMY;
			goto voc_ctrl_spk_fail;
		} else {
			os_memset(pcm_data, 0x00, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		}

		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			LOGI("%s, %d, open onboard spk \n", __func__, __LINE__);
			/* enable dac */
			bk_aud_dac_start();
			aud_tras_dac_pa_ctrl(true);

			ret = bk_dma_start(aud_tras_drv_info.voc_info.dac_dma_id);
			if (ret != BK_OK) {
				LOGE("%s, %d, start dac dma fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_DMA;
				goto voc_ctrl_spk_fail;
			}
		} else {
			LOGI("%s, %d, open uac spk \n", __func__, __LINE__);
			/* start send uac spk urb */
			//TODO
#if 0
			//ret = bk_aud_uac_start_spk();
			if (ret != BK_OK) {
				LOGE("%s, %d, open uac spk fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_SPK;
				goto voc_ctrl_spk_fail;
			}
#endif
			uac_spk_write_flag = false;
		}

		/* write two frame data to speaker and ref ring buffer */
		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.mic_samp_rate_points*2) {
			LOGE("%s, %d, the data writeten to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto voc_ctrl_spk_fail;
		}

		size = ring_buffer_write(&(aud_tras_drv_info.voc_info.speaker_rb), (uint8_t *)pcm_data, aud_tras_drv_info.voc_info.mic_samp_rate_points*2);
		if (size != aud_tras_drv_info.voc_info.mic_samp_rate_points*2) {
			LOGE("%s, %d, the data writeten to speaker_ring_buff error, size: %d \n", __func__, __LINE__, size);
			err = BK_ERR_AUD_INTF_RING_BUFF;
			goto voc_ctrl_spk_fail;
		}

		audio_tras_drv_free(pcm_data);
		pcm_data = NULL;
	} else if (spk_en == AUD_INTF_VOC_SPK_CLOSE) {
		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			LOGI("%s, %d, open onboard spk \n", __func__, __LINE__);
			bk_aud_dac_stop();
			aud_tras_dac_pa_ctrl(false);
			bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		} else {
			LOGI("%s, %d, close uac spk \n", __func__, __LINE__);
			/* stop send uac spk urb */
			//TODO
#if 0
			//ret = bk_aud_uac_stop_spk();
			if (ret != BK_OK) {
				LOGE("%s, %d, close uac spk fail \n", __func__, __LINE__);
				err = BK_ERR_AUD_INTF_UAC_SPK;
				goto voc_ctrl_spk_fail;
			}
#endif
		}
		ring_buffer_clear(&(aud_tras_drv_info.voc_info.speaker_rb));
	} else {
		err = BK_ERR_AUD_INTF_PARAM;
		goto voc_ctrl_spk_fail;
	}

	GLOBAL_INT_DISABLE();
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD)
		aud_tras_drv_info.voc_info.spk_en = spk_en;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;

voc_ctrl_spk_fail:
	if (pcm_data)
		audio_tras_drv_free(pcm_data);

	if (spk_en == AUD_INTF_VOC_SPK_OPEN) {
		if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
			bk_aud_dac_stop();
			aud_tras_dac_pa_ctrl(false);
			bk_dma_stop(aud_tras_drv_info.voc_info.dac_dma_id);
		} else {
			/* stop send uac spk urb */
			//TODO
			//bk_aud_uac_stop_spk();
		}
	}

	return err;
}

static bk_err_t aud_tras_drv_voc_ctrl_aec(bool aec_en)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.voc_info.aec_enable = aec_en;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_uac_register_connect_state_cb(void * cb)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	aud_tras_drv_info.uac_connect_state_cb_exist = true;
	aud_tras_drv_info.aud_tras_drv_uac_connect_state_cb = cb;
	GLOBAL_INT_RESTORE();

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_spk_set_samp_rate(uint32_t samp_rate)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;

	bk_aud_dac_stop();
	aud_tras_dac_pa_ctrl(false);
	ret = bk_aud_dac_set_samp_rate(samp_rate);
	bk_aud_dac_start();
	bk_aud_dac_start();
	aud_tras_dac_pa_ctrl(true);

	return ret;
}



bk_err_t aud_tras_drv_set_work_mode(aud_intf_work_mode_t mode)
{
	bk_err_t ret = BK_OK;

	LOGI("%s, %d, set mode: %d \n", __func__, __LINE__, mode);

	switch (mode) {
		case AUD_INTF_WORK_MODE_VOICE:
			aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_VOICE;
			break;

		case AUD_INTF_WORK_MODE_NULL:
			if ((aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_VOICE) && (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL)) {
				ret = aud_tras_drv_voc_deinit();
				if (ret != BK_OK) {
					LOGE("%s, %d, spk deinit fail \n", __func__, __LINE__);
					return BK_ERR_AUD_INTF_FAIL;
				}
			}
			aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_NULL;
			break;

		default:
			return BK_ERR_AUD_INTF_FAIL;
			break;
	}

	return BK_ERR_AUD_INTF_OK;
}

static bk_err_t aud_tras_drv_set_mic_gain(uint8_t value)
{
	bk_err_t ret = BK_ERR_AUD_INTF_OK;

	if (aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_BOARD) {
		bk_aud_adc_set_gain((uint32_t)value);
		ret = BK_ERR_AUD_INTF_OK;
	}

	return ret;
}

#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS
static void uac_mic_spk_recover(void)
{
//TODO
#if 0
	LOGD("%s \n", __func__);
	if (aud_tras_drv_info.work_mode == AUD_INTF_WORK_MODE_GENERAL) {
		/* check mic status */
		if ((aud_tras_drv_info.mic_info.status == AUD_TRAS_DRV_MIC_STA_START) && (aud_tras_drv_info.mic_info.mic_type == AUD_INTF_MIC_TYPE_UAC)) {
			bk_aud_uac_start_mic();
		}

		/* check spk status */
		if ((aud_tras_drv_info.spk_info.status == AUD_TRAS_DRV_SPK_STA_START) && (aud_tras_drv_info.spk_info.spk_type == AUD_INTF_SPK_TYPE_UAC)) {
			bk_aud_uac_start_spk();
		}
	} else {
		if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
			if ((aud_tras_drv_info.voc_info.mic_type == AUD_INTF_MIC_TYPE_UAC) && (aud_tras_drv_info.voc_info.mic_en == AUD_INTF_VOC_MIC_OPEN)) {
				bk_aud_uac_start_mic();
			}
			if ((aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) && (aud_tras_drv_info.voc_info.spk_en == AUD_INTF_VOC_SPK_OPEN)) {
				bk_aud_uac_start_spk();
			}
		}
	}
#endif
}
#endif //CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS

static bk_err_t aud_tras_drv_set_spk_gain(uint16_t value)
{
	bk_err_t ret = BK_OK;
	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_BOARD) {
		bk_aud_dac_set_gain((uint32_t)value);
		return BK_ERR_AUD_INTF_OK;
	}

	if (aud_tras_drv_info.voc_info.spk_type == AUD_INTF_SPK_TYPE_UAC) {
		/* check uac support volume configuration */
		//TODO
#if 0
		if (bk_aud_uac_check_spk_gain_cfg() == BK_OK) {
			LOGI("%s, %d, set uac speaker volume: %d \n", __func__, __LINE__, value);
#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_STOP_UAC_TRAS
			/* step1: stop uac mic and speaker
			   step2: set volume
			   step3: recover uac mic and speaker status
			*/
			/* step1: stop uac mic and speaker */
			bk_aud_uac_stop_mic();
			bk_aud_uac_stop_spk();

			/* step2: set volume */
			ret = bk_aud_uac_set_spk_gain((uint32_t)value);

			/* step3: recover uac mic and speaker status */
			uac_mic_spk_recover();
#endif

#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_DIRECT
			ret = bk_aud_uac_set_spk_gain((uint32_t)value);
#endif

#if CONFIG_AUD_TRAS_UAC_SPK_VOL_CTRL_MODE_MUTE
			bk_aud_uac_ctrl_spk_mute(1);
			ret = bk_aud_uac_set_spk_gain((uint32_t)value);
			bk_aud_uac_ctrl_spk_mute(0);
#endif

			return ret;
		} else {
			LOGW("%s, %d, The uac speaker not support volume configuration \n", __func__, __LINE__);
			return BK_ERR_AUD_INTF_PARAM;
		}
#endif
	}

	return ret;
}


static void aud_tras_drv_main(beken_thread_arg_t param_data)
{
	bk_err_t ret = BK_OK;
	aud_intf_drv_config_t *aud_trs_setup = NULL;

	aud_trs_setup = (aud_intf_drv_config_t *)param_data;

	aud_tras_drv_info.work_mode = aud_trs_setup->setup.work_mode;
	aud_tras_drv_info.aud_tras_tx_mic_data = aud_trs_setup->setup.aud_intf_tx_mic_data;
	aud_tras_drv_info.aud_tras_rx_spk_data = aud_trs_setup->setup.aud_intf_rx_spk_data;

	/* set work status to IDLE */
	aud_tras_drv_info.status = AUD_TRAS_DRV_STA_IDLE;

	rtos_set_semaphore(&aud_tras_drv_task_sem);

//	bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_320M);

	while(1) {
		aud_tras_drv_msg_t msg;
		media_mailbox_msg_t *mailbox_msg = NULL;
		ret = rtos_pop_from_queue(&aud_trs_drv_int_msg_que, &msg, BEKEN_WAIT_FOREVER);
		if (kNoErr == ret) {
			switch (msg.op) {
				case AUD_TRAS_DRV_IDLE:
					break;

				case AUD_TRAS_DRV_EXIT:
					LOGD("%s, %d, goto: AUD_TRAS_DRV_EXIT \n", __func__, __LINE__);
					goto aud_tras_drv_exit;
					break;

				case AUD_TRAS_DRV_SET_MODE:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_work_mode((aud_intf_work_mode_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				/* voc op */
				case AUD_TRAS_DRV_VOC_INIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_init((aud_intf_voc_config_t *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_DEINIT:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					if (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL) {
						ret = aud_tras_drv_voc_deinit();
					} else {
						ret = BK_ERR_AUD_INTF_OK;
					}
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_START:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_start();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_STOP:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_stop();
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_CTRL_MIC:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_ctrl_mic((aud_intf_voc_mic_ctrl_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_CTRL_SPK:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_ctrl_spk((aud_intf_voc_spk_ctrl_t)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_CTRL_AEC:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_voc_ctrl_aec((bool)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_SET_MIC_GAIN:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_mic_gain(*((uint8_t *)mailbox_msg->param));
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_SET_SPK_GAIN:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_set_spk_gain(*((uint16_t *)mailbox_msg->param));
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_SET_AEC_PARA:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_aec_set_para(aud_tras_drv_info.voc_info.aec_info, (aud_intf_voc_aec_ctl_t *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_VOC_GET_AEC_PARA:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_aec_print_para(aud_tras_drv_info.voc_info.aec_info->aec_config);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				/* uac op */
				case AUD_TRAS_DRV_UAC_REGIS_CONT_STATE_CB:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					ret = aud_tras_drv_uac_register_connect_state_cb((void *)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				case AUD_TRAS_DRV_UAC_MIC_CONT:
					aud_tras_uac_mic_connect_handle();
					break;

				case AUD_TRAS_DRV_UAC_MIC_DISCONT:
					aud_tras_uac_mic_disconnect_handle();
					break;

				case AUD_TRAS_DRV_UAC_SPK_CONT:
					aud_tras_uac_spk_connect_handle();
					break;

				case AUD_TRAS_DRV_UAC_SPK_DISCONT:
					aud_tras_uac_spk_disconnect_handle();
					break;

				case AUD_TRAS_DRV_UAC_AUTO_CONT_CTRL:
					mailbox_msg = (media_mailbox_msg_t *)msg.param;
					aud_tras_uac_auto_connect_ctrl((bool)mailbox_msg->param);
					msg_send_rsp_to_media_major_mailbox(mailbox_msg, ret, APP_MODULE);
					break;

				/* voc int op */
				case AUD_TRAS_DRV_AEC:
					if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
						aud_tras_aec();
					}
					break;

				case AUD_TRAS_DRV_ENCODER:
					if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
						aud_tras_enc();
					}
					break;

				case AUD_TRAS_DRV_DECODER:
					if (aud_tras_drv_info.voc_info.status == AUD_TRAS_DRV_VOC_STA_START) {
						aud_tras_dec();
					}
					break;

				default:
					break;
			}
		}
	}

aud_tras_drv_exit:
	/* deinit mic, speaker and voice */
	/* check audio transfer driver work status */
	switch (aud_tras_drv_info.work_mode) {
		case AUD_INTF_WORK_MODE_VOICE:
			/* check voice work status */
			if (aud_tras_drv_info.voc_info.status != AUD_TRAS_DRV_VOC_STA_NULL) {
				/* stop voice transfer and deinit */
				aud_tras_drv_voc_stop();
				aud_tras_drv_voc_deinit();
			}
			break;

		default:
			break;
	}

	aud_tras_drv_info.work_mode = AUD_INTF_WORK_MODE_NULL;
	aud_tras_drv_info.aud_tras_tx_mic_data = NULL;
	aud_tras_drv_info.aud_tras_rx_spk_data = NULL;

	/* set work status to NULL */
	aud_tras_drv_info.status = AUD_TRAS_DRV_STA_NULL;


	/* delete msg queue */
	ret = rtos_deinit_queue(&aud_trs_drv_int_msg_que);
	if (ret != kNoErr) {
		LOGE("%s, %d, delete message queue fail \n", __func__, __LINE__);
	}
	aud_trs_drv_int_msg_que = NULL;
	LOGI("%s, %d, delete message queue complete \n", __func__, __LINE__);

//	bk_pm_module_vote_cpu_freq(PM_DEV_ID_AUDIO, PM_CPU_FRQ_DEFAULT);

	/* reset uac to default */
	aud_tras_drv_info.aud_tras_drv_uac_connect_state_cb = NULL;
	aud_tras_drv_info.uac_mic_status = AUD_INTF_UAC_MIC_NORMAL_DISCONNECTED;
	aud_tras_drv_info.uac_spk_status = AUD_INTF_UAC_SPK_NORMAL_DISCONNECTED;
	aud_tras_drv_info.uac_auto_connect = true;

	rtos_set_semaphore(&aud_tras_drv_task_sem);

	/* delete task */
	aud_trs_drv_thread_hdl = NULL;
	rtos_delete_thread(NULL);
}

aud_intf_drv_config_t aud_trs_drv_setup_bak = {0};

bk_err_t aud_tras_drv_init(aud_intf_drv_config_t *setup_cfg)
{
	bk_err_t ret = BK_OK;

	if (aud_tras_drv_task_sem == NULL) {
		ret = rtos_init_semaphore(&aud_tras_drv_task_sem, 1);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, create audio tras drv task semaphore failed \n", __func__, __LINE__);
			goto fail;
		}
	}

	if ((!aud_trs_drv_thread_hdl) && (!aud_trs_drv_int_msg_que)) {
		LOGD("%s, %d, init audio transfer driver \n", __func__, __LINE__);
		os_memcpy(&aud_trs_drv_setup_bak, setup_cfg, sizeof(aud_intf_drv_config_t));

		ret = rtos_init_queue(&aud_trs_drv_int_msg_que,
							  "aud_tras_int_que",
							  sizeof(aud_tras_drv_msg_t),
							  TU_QITEM_COUNT);
		if (ret != kNoErr) {
			LOGE("%s, %d, create audio transfer driver internal message queue fail \n", __func__, __LINE__);
			goto fail;
		}
		LOGD("%s, %d, create audio transfer driver internal message queue complete \n", __func__, __LINE__);

		//create audio transfer driver task
		ret = rtos_create_thread(&aud_trs_drv_thread_hdl,
							 setup_cfg->setup.task_config.priority,
							 "aud_tras_drv",
							 (beken_thread_function_t)aud_tras_drv_main,
							 4096,
							 (beken_thread_arg_t)&aud_trs_drv_setup_bak);
		if (ret != kNoErr) {
			LOGE("%s, %d, create audio transfer driver task fail \n", __func__, __LINE__);
			rtos_deinit_queue(&aud_trs_drv_int_msg_que);
			aud_trs_drv_int_msg_que = NULL;
			aud_trs_drv_thread_hdl = NULL;
			goto fail;
		}

		ret = rtos_get_semaphore(&aud_tras_drv_task_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
			goto fail;
		}

		LOGD("%s, %d, create audio transfer driver task complete \n", __func__, __LINE__);
	}

	return BK_OK;

fail:
	//LOGE("%s, %d, aud_tras_drv_init fail, ret: %d \n", __func__, __LINE__, ret);

	if(aud_tras_drv_task_sem)
	{
		rtos_deinit_semaphore(&aud_tras_drv_task_sem);
		aud_tras_drv_task_sem = NULL;
	}

	return BK_FAIL;
}

bk_err_t aud_tras_drv_deinit(void)
{
	bk_err_t ret;
	aud_tras_drv_msg_t msg;

	msg.op = AUD_TRAS_DRV_EXIT;
	if (aud_trs_drv_int_msg_que) {
		ret = rtos_push_to_queue_front(&aud_trs_drv_int_msg_que, &msg, BEKEN_NO_WAIT);
		if (kNoErr != ret) {
			LOGE("%s, %d, audio send msg: AUD_TRAS_DRV_EXIT fail \n", __func__, __LINE__);
			return kOverrunErr;
		}

		ret = rtos_get_semaphore(&aud_tras_drv_task_sem, BEKEN_WAIT_FOREVER);
		if (ret != BK_OK)
		{
			LOGE("%s, %d, rtos_get_semaphore\n", __func__, __LINE__);
			return BK_FAIL;
		}

		if(aud_tras_drv_task_sem)
		{
			rtos_deinit_semaphore(&aud_tras_drv_task_sem);
			aud_tras_drv_task_sem = NULL;
		}
	} else {
		LOGW("%s, %d, aud_trs_drv_int_msg_que is NULL\n", __func__, __LINE__);
	}

	return BK_OK;
}


bk_err_t audio_event_handle(media_mailbox_msg_t * msg)
{
	bk_err_t ret = BK_FAIL;

	/* save mailbox msg received from media app */
	LOGD("%s, %d, event: %d \n", __func__, __LINE__, msg->event);

	switch (msg->event)
	{
		case EVENT_AUD_INIT_REQ:
			ret = aud_tras_drv_init((aud_intf_drv_config_t *)msg->param);
			msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
			break;

		case EVENT_AUD_DEINIT_REQ:
			ret = aud_tras_drv_deinit();
			msg_send_rsp_to_media_major_mailbox(msg, ret, APP_MODULE);
			break;

		case EVENT_AUD_SET_MODE_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_SET_MODE, (void *)msg);
			break;

		/* voc op */
		case EVENT_AUD_VOC_INIT_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_INIT, (void *)msg);
			break;

		case EVENT_AUD_VOC_DEINIT_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_DEINIT, (void *)msg);
			break;

		case EVENT_AUD_VOC_START_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_START, (void *)msg);
			break;

		case EVENT_AUD_VOC_STOP_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_STOP, (void *)msg);
			break;

		case EVENT_AUD_VOC_CTRL_MIC_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_CTRL_MIC, (void *)msg);
			break;

		case EVENT_AUD_VOC_CTRL_SPK_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_CTRL_SPK, (void *)msg);
			break;

		case EVENT_AUD_VOC_CTRL_AEC_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_CTRL_AEC, (void *)msg);
			break;

		case EVENT_AUD_VOC_SET_MIC_GAIN_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_SET_MIC_GAIN, (void *)msg);
			break;

		case EVENT_AUD_VOC_SET_SPK_GAIN_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_SET_SPK_GAIN, (void *)msg);
			break;

		case EVENT_AUD_VOC_SET_AEC_PARA_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_SET_AEC_PARA, (void *)msg);
			break;

		case EVENT_AUD_VOC_GET_AEC_PARA_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_VOC_GET_AEC_PARA, (void *)msg);
			break;

		/* uac event */
		case EVENT_AUD_UAC_REGIS_CONT_STATE_CB_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_REGIS_CONT_STATE_CB, (void *)msg);
			break;
#if 0
		case EVENT_AUD_UAC_CONT_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_CONT, (void *)msg->param);
			break;

		case EVENT_AUD_UAC_DISCONT_REQ:
			//aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_CONT, NULL);
			break;
#endif
		case EVENT_AUD_UAC_AUTO_CONT_CTRL_REQ:
			aud_tras_drv_send_msg(AUD_TRAS_DRV_UAC_AUTO_CONT_CTRL, (void *)msg);
			break;

		case EVENT_AUD_MIC_DATA_NOTIFY:
			//GPIO_UP(6);
			//GPIO_DOWN(6);
			break;

		case EVENT_AUD_SPK_DATA_NOTIFY:
			//TODO set sem
			//	;
			break;

		default:
			break;
	}

	return ret;
}

