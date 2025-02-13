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
#include "aud_tras_drv.h"
#include <modules/aec.h>
#include <driver/audio_ring_buff.h>
#if (CONFIG_CACHE_ENABLE)
#include "cache.h"
#endif


#define AUD_AEC_TAG "aec"

#define LOGI(...) BK_LOGI(AUD_AEC_TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(AUD_AEC_TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(AUD_AEC_TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(AUD_AEC_TAG, ##__VA_ARGS__)


//#define AEC_DATA_DUMP_BY_UART

#ifdef AEC_DATA_DUMP_BY_UART
#include "uart_util.h"
static uart_util_t g_aec_uart_util = {0};
#define AEC_DATA_DUMP_UART_ID            (1)
#define AEC_DATA_DUMP_UART_BAUD_RATE     (2000000)

#define AEC_DATA_DUMP_OPEN()                    uart_util_create(&g_aec_uart_util, AEC_DATA_DUMP_UART_ID, AEC_DATA_DUMP_UART_BAUD_RATE)
#define AEC_DATA_DUMP_CLOSE()                   uart_util_destroy(&g_aec_uart_util)
#define AEC_DATA_DUMP_DATA(data_buf, len)       uart_util_tx_data(&g_aec_uart_util, data_buf, len)
#else
#define AEC_DATA_DUMP_OPEN()
#define AEC_DATA_DUMP_CLOSE()
#define AEC_DATA_DUMP_DATA(data_buf, len)
#endif


static bk_err_t aec_buff_cfg(aec_info_t *aec_info)
{
	uint16_t samp_rate_points = aec_info->samp_rate_points;

    if (aec_info == NULL) {
        LOGE("%s, %d, aec_info is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

	/* malloc aec ref ring buffer to save ref data */
	LOGI("%s, %d, ref_ring_buff size: %d \n", __func__, __LINE__, samp_rate_points * 2 * 2);
	aec_info->aec_ref_ring_buff = (int16_t *)audio_tras_drv_malloc(samp_rate_points * 2 * 2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aec_info->aec_ref_ring_buff == NULL) {
		LOGE("%s, %d, malloc ref ring buffer fail \n", __func__, __LINE__);
		goto exit;
	}

	/* malloc aec out ring buffer to save mic data has been aec processed */
	aec_info->aec_out_ring_buff = (int16_t *)audio_tras_drv_malloc(samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL);
	if (aec_info->aec_out_ring_buff == NULL) {
		LOGE("%s, %d, malloc aec out ring buffer fail \n", __func__, __LINE__);
		goto exit;
	}

	/* init ref_ring_buff */
	ring_buffer_init(&(aec_info->ref_rb), (uint8_t*)aec_info->aec_ref_ring_buff, samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

	/* init aec_ring_buff */
	ring_buffer_init(&(aec_info->aec_rb), (uint8_t*)aec_info->aec_out_ring_buff, samp_rate_points*2*2 + CONFIG_AUD_RING_BUFF_SAFE_INTERVAL, DMA_ID_MAX, RB_DMA_TYPE_NULL);

	return BK_OK;

exit:
	if (aec_info->aec_ref_ring_buff != NULL) {
		audio_tras_drv_free(aec_info->aec_ref_ring_buff);
		aec_info->aec_ref_ring_buff = NULL;
	}
	if (aec_info->aec_out_ring_buff != NULL) {
		audio_tras_drv_free(aec_info->aec_out_ring_buff);
		aec_info->aec_out_ring_buff = NULL;
	}
	return BK_FAIL;
}

static bk_err_t aec_buff_decfg(aec_info_t *aec_info)
{
    if (!aec_info) {
        return BK_OK;
    }

	ring_buffer_clear(&aec_info->ref_rb);
	ring_buffer_clear(&aec_info->aec_rb);

	if (aec_info->aec_ref_ring_buff) {
		audio_tras_drv_free(aec_info->aec_ref_ring_buff);
		aec_info->aec_ref_ring_buff = NULL;
	}

	if (aec_info->aec_out_ring_buff) {
		audio_tras_drv_free(aec_info->aec_out_ring_buff);
		aec_info->aec_out_ring_buff = NULL;
	}

    return BK_OK;
}

bk_err_t aud_aec_init(aec_info_t *aec_info)
{
	uint32_t aec_context_size = 0;
	uint32_t val = 0;

    if (aec_info == NULL) {
        LOGE("%s, %d, aec_info is NULL\n", __func__, __LINE__);
        return BK_FAIL;
    }

	/* config sample rate, default is 8K */
	if (aec_info->samp_rate != 8000 && aec_info->samp_rate != 16000) {
        LOGE("%s, %d, aec_info->samp_rate: %d is not support\n", __func__, __LINE__, aec_info->samp_rate);
        return BK_FAIL;
	}

	/* init aec context */
	aec_context_size = aec_size(1000);
	aec_info->aec = (AECContext*)audio_tras_drv_malloc(aec_context_size);
	LOGI("%s, %d, aec: %p, size: %d\n", __func__, __LINE__, aec_info->aec, aec_context_size);
	if (aec_info->aec == NULL) {
		LOGE("%s, %d, malloc aec fail, aec_context_size: %d \n", __func__, __LINE__, aec_context_size);
		return BK_FAIL;
	}

    aec_init(aec_info->aec, aec_info->samp_rate);

	/* 获取处理帧长，16000采样率320点(640字节)，8000采样率160点(320字节)  (对应20毫秒数据) */
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_GET_FRAME_SAMPLE, (uint32_t)(&(aec_info->samp_rate_points)));

	/* 获取结构体内部可以复用的ram作为每帧tx,rx,out数据的临时buffer; ram很宽裕的话也可以在外部单独申请获取 */
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_GET_TX_BUF, (uint32_t)(&val)); aec_info->mic_addr = (int16_t*)val;
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_GET_RX_BUF, (uint32_t)(&val)); aec_info->ref_addr = (int16_t*)val;
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_GET_OUT_BUF,(uint32_t)(&val)); aec_info->out_addr = (int16_t*)val;

	/* 以下是参数调节示例,aec_init中都已经有默认值,可以直接先用默认值 */
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_FLAGS, aec_info->aec_config->init_flags);							//库内各模块开关; aec_init内默认赋值0x1f;

	/* 回声消除相关 */
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, aec_info->aec_config->mic_delay);						//设置参考信号延迟(采样点数，需要dump数据观察)
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_EC_DEPTH, aec_info->aec_config->ec_depth);							//建议取值范围1~50; 后面几个参数建议先用aec_init内的默认值，具体需要根据实际情况调试; 总得来说回声越大需要调的越大
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_TxRxThr, aec_info->aec_config->TxRxThr);							//建议取值范围10~64
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_TxRxFlr, aec_info->aec_config->TxRxFlr);							//建议取值范围1~10

	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_REF_SCALE, aec_info->aec_config->ref_scale);						//取值0,1,2；rx数据如果幅值太小的话适当放大
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_VOL, aec_info->aec_config->voice_vol);								//通话过程中如果需要经常调节喇叭音量就设置下当前音量等级
	/* 降噪相关 */
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_NS_LEVEL, aec_info->aec_config->ns_level);							//建议取值范围1~8；值越小底噪越小
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_NS_PARA, aec_info->aec_config->ns_para);							//只能取值0,1,2; 降噪由弱到强，建议默认值
	/* drc(输出音量相关) */
	aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_DRC, aec_info->aec_config->drc);									//建议取值范围0x10~0x1f;   越大输出声音越大

    if (BK_OK != aec_buff_cfg(aec_info)) {
       goto fail;
    }

    /* aec debug */
    AEC_DATA_DUMP_OPEN();

	return BK_OK;

fail:

    aec_buff_decfg(aec_info);

	if (aec_info->aec) {
		audio_tras_drv_free(aec_info->aec);
		aec_info->aec = NULL;
	}

    return BK_FAIL;
}


bk_err_t aud_aec_deinit(aec_info_t *aec_info)
{
    if (!aec_info) {
        return BK_OK;
    }

    AEC_DATA_DUMP_CLOSE();

	if (aec_info->aec) {
		audio_tras_drv_free(aec_info->aec);
		aec_info->aec = NULL;
	}

	if (aec_info->aec_config) {
		audio_tras_drv_free(aec_info->aec_config);
		aec_info->aec_config = NULL;
	}

    aec_buff_decfg(aec_info);

	if (aec_info) {
		audio_tras_drv_free(aec_info);
	}

    return BK_OK;
}


bk_err_t aud_aec_proc(aec_info_t *aec_info)
{
	if (!aec_info) {
        LOGI("aec_info is NULL\n");
		return BK_FAIL;
    }

    /* aec debug, dump aec data by uart */
	AEC_DATA_DUMP_DATA(aec_info->mic_addr, aec_info->samp_rate_points*2);
	AEC_DATA_DUMP_DATA(aec_info->ref_addr, aec_info->samp_rate_points*2);

	/* aec process data */
	aec_proc(aec_info->aec, aec_info->ref_addr, aec_info->mic_addr, aec_info->out_addr);

    /* aec debug, dump aec data by uart */
	AEC_DATA_DUMP_DATA(aec_info->out_addr, aec_info->samp_rate_points*2);

	return BK_OK;
}

bk_err_t aud_aec_set_para(aec_info_t *aec_info, aud_intf_voc_aec_ctl_t *aec_ctl)
{
	if (!aec_info || !aec_ctl) {
        LOGI("aec_info: %p, aec_ctl: %p\n", aec_info, aec_ctl);
		return BK_FAIL;
    }

	switch (aec_ctl->op) {
		case AUD_INTF_VOC_AEC_MIC_DELAY:
			aec_info->aec_config->mic_delay = aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_MIC_DELAY, aec_info->aec_config->mic_delay);
			break;

		case AUD_INTF_VOC_AEC_EC_DEPTH:
			aec_info->aec_config->ec_depth = aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_EC_DEPTH, aec_info->aec_config->ec_depth);
			break;

		case AUD_INTF_VOC_AEC_REF_SCALE:
			aec_info->aec_config->ref_scale = (uint8_t)aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_REF_SCALE, aec_info->aec_config->ref_scale);
			break;

		case AUD_INTF_VOC_AEC_VOICE_VOL:
			aec_info->aec_config->voice_vol = (uint8_t)aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_VOL, aec_info->aec_config->voice_vol);
			break;

		case AUD_INTF_VOC_AEC_TXRX_THR:
			aec_info->aec_config->TxRxThr = aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_TxRxThr, aec_info->aec_config->TxRxThr);
			break;

		case AUD_INTF_VOC_AEC_TXRX_FLR:
			aec_info->aec_config->TxRxFlr = aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_TxRxFlr, aec_info->aec_config->TxRxFlr);
			break;

		case AUD_INTF_VOC_AEC_NS_LEVEL:
			aec_info->aec_config->ns_level = (uint8_t)aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_NS_LEVEL, aec_info->aec_config->ns_level);
			break;

		case AUD_INTF_VOC_AEC_NS_PARA:
			aec_info->aec_config->ns_para = (uint8_t)aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_NS_PARA, aec_info->aec_config->ns_para);
			break;

		case AUD_INTF_VOC_AEC_DRC:
			aec_info->aec_config->drc = (uint8_t)aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_DRC, aec_info->aec_config->drc);
			break;

		case AUD_INTF_VOC_AEC_INIT_FLAG:
			aec_info->aec_config->init_flags = (uint16_t)aec_ctl->value;
			aec_ctrl(aec_info->aec, AEC_CTRL_CMD_SET_FLAGS, aec_info->aec_config->init_flags);
			break;

		default:
			break;
	}

	return BK_ERR_AUD_INTF_OK;
}

bk_err_t aud_aec_set_mic_delay(AECContext *aec, uint32_t value)
{
    if (!aec) {
        return BK_FAIL;
    }

    aec_ctrl(aec, AEC_CTRL_CMD_SET_MIC_DELAY, value);

    return BK_OK;
}

bk_err_t aud_aec_print_para(aec_config_t *aec_config)
{
    if (!aec_config) {
        return BK_FAIL;
    }

	LOGI("aec params: \n");
	LOGI("init_flags: %d \n", aec_config->init_flags);
	LOGI("ec_depth: %d \n", aec_config->ec_depth);
	LOGI("ref_scale: %d \n", aec_config->ref_scale);
	LOGI("voice_vol: %d \n", aec_config->voice_vol);
	LOGI("TxRxThr: %d \n", aec_config->TxRxThr);
	LOGI("TxRxFlr: %d \n", aec_config->TxRxFlr);
	LOGI("ns_level: %d \n", aec_config->ns_level);
	LOGI("ns_para: %d \n", aec_config->ns_para);
	LOGI("drc: %d \n", aec_config->drc);
	LOGI("end \n");

	return BK_OK;
}

