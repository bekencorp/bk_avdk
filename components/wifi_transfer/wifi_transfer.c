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
#include <components/log.h>
#include <components/video_types.h>
#include "bk_wifi.h"
#include "media_app.h"

#define TAG "wifi_trs"

#define LOGI(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

typedef struct
{
	uint8_t id;
	uint8_t eof;
	uint8_t cnt;
	uint8_t size;
	uint8_t data[];
} transfer_data_t;

#if CONFIG_WIFI_ENABLE
extern void rwnxl_set_video_transfer_flag(uint32_t video_transfer_flag);
#else
#define rwnxl_set_video_transfer_flag(...)
#endif

static uint8_t transfer_enable = 0;
const media_transfer_cb_t *media_transfer_callback = NULL;
static transfer_data_t *transfer_app_data = NULL;
static video_setup_t *transfer_app_config = NULL;

static int send_frame_buffer_packet(uint8_t *data, uint32_t size, uint32_t retry_max)
{
	int ret = BK_FAIL;
	uint16_t retry_cnt = 0;

	if (media_transfer_callback == NULL)
		return ret;

	if (media_transfer_callback->prepare)
	{
		media_transfer_callback->prepare(data, size);
	}

	do
	{
		ret = media_transfer_callback->send(data, size, &retry_cnt);

		if (ret == size)
		{
			break;
		}
		rtos_delay_milliseconds(TRAN_RETRY_DELAY_TIME);
	}
	while (retry_max-- && transfer_enable);

	return ret == size ? BK_OK : BK_FAIL;
}

static void wifi_transfer_send_frame(frame_buffer_t *buffer)
{
	int ret = kNoErr;

	uint32_t i;
	uint32_t count = buffer->length / transfer_app_config->pkt_size;
	uint32_t tail = buffer->length % transfer_app_config->pkt_size;

	uint8_t *src_address = buffer->frame;
	LOGD("seq: %u, length: %u, size: %u\n", buffer->sequence, buffer->length, buffer->size);

#if CONFIG_MEDIA_DROP_STRATEGY_ENABLE
	// check whether this frame should be dropped for some reasones such as no enough buffer
	if (media_transfer_callback->drop_check && media_transfer_callback->drop_check(buffer,(count + (tail ? 1 : 0)),transfer_app_config->pkt_header_size))
	{
		return;
	}
#endif

	transfer_app_data->id = (buffer->sequence & 0xFF);
	transfer_app_data->eof = 0;
	transfer_app_data->cnt = 0;//package seq num
	transfer_app_data->size = count + (tail ? 1 : 0);//one frame package counts

	for (i = 0; i < count && transfer_enable; i++)
	{
		transfer_app_data->cnt = i + 1;

		if ((tail == 0) && (i == count - 1))
		{
			transfer_app_data->eof = 1;
		}

		os_memcpy_word((uint32_t *)transfer_app_data->data, (uint32_t *)(src_address + (transfer_app_config->pkt_size * i)),
						transfer_app_config->pkt_size);

		ret = send_frame_buffer_packet((uint8_t *)transfer_app_data, transfer_app_config->pkt_size + transfer_app_config->pkt_header_size,
										TRAN_MAX_RETRY_TIME);
		if (ret != BK_OK)
		{
			LOGD("send failed\n");
		}
	}

	if (tail)
	{
		transfer_app_data->eof = 1;
		transfer_app_data->cnt = count + 1;

		os_memcpy_word((uint32_t *)transfer_app_data->data, (uint32_t *)(src_address + (transfer_app_config->pkt_size * i)),
						(tail % 4) ? ((tail / 4 + 1) * 4) : tail);

		if (1)//(transfer_app_config->send_type ==  TVIDEO_SND_UDP)
			ret = send_frame_buffer_packet((uint8_t *)transfer_app_data, tail + transfer_app_config->pkt_header_size, TRAN_MAX_RETRY_TIME);
		else
			ret = send_frame_buffer_packet((uint8_t *)transfer_app_data, VIDEO_TCP_TRAN_LEN, TRAN_MAX_RETRY_TIME);

		if (ret != BK_OK)
		{
			LOGD("send failed\n");
		}
	}

	LOGD("length: %u, tail: %u, count: %u\n", buffer->length, tail, count);
}

void wifi_transfer_read_frame_callback(frame_buffer_t *frame)
{
	wifi_transfer_send_frame(frame);
	if (frame->sequence < 8)
	{
		LOGI("%s, %d\r\n", __func__, frame->sequence);
	}
}

static bk_err_t wifi_transfer_buffer_init(const media_transfer_cb_t *cb)
{
	if (cb == NULL)
	{
		return BK_FAIL;
	}

	if (transfer_app_config == NULL)
	{
		transfer_app_config = (video_setup_t *)os_malloc(sizeof(video_setup_t));
		if (transfer_app_config == NULL)
		{
			LOGE("% malloc failed\r\n", __func__);
			return BK_ERR_NO_MEM;
		}

		os_memset(transfer_app_config, 0, sizeof(video_setup_t));
		transfer_app_config->pkt_header_size = sizeof(transfer_data_t);
	}

	if (cb->get_tx_buf)
	{
		transfer_app_data = cb->get_tx_buf();
		transfer_app_config->pkt_size = cb->get_tx_size() - transfer_app_config->pkt_header_size;

		if (transfer_app_data == NULL
			|| transfer_app_config->pkt_size <= 0)
		{
			LOGE("%s transfer_data: %p, size: %d\n", __func__, transfer_app_data, transfer_app_config->pkt_size);
			return BK_FAIL;
		}
	}
	else
	{
		if (transfer_app_data == NULL)
		{
			transfer_app_data = os_malloc(1472);
		}

		transfer_app_config->pkt_size = 1472 - transfer_app_config->pkt_header_size;
	}

	LOGI("%s transfer_data: %p, size: %d\n", __func__, transfer_app_data, transfer_app_config->pkt_size);

	media_transfer_callback = cb;

	return BK_OK;
}

static void wifi_transfer_buffer_deinit(void)
{
	if (transfer_app_config)
	{
		os_free(transfer_app_config);
		transfer_app_config = NULL;
	}

#ifdef CONFIG_INTEGRATION_DOORBELL
	media_transfer_callback = NULL;
	transfer_app_data = NULL;
#else
	if (transfer_app_data)
	{
		os_free(transfer_app_data);
		transfer_app_data = NULL;
	}
#endif
}

bk_err_t bk_wifi_transfer_frame_open(const media_transfer_cb_t *cb)
{
	int ret = BK_FAIL;

	if (transfer_enable)
	{
		LOGE("%s, have been opened!\r\n", __func__);
		return ret;
	}

	rwnxl_set_video_transfer_flag(true);

	bk_wifi_set_video_quality(0);

	ret = wifi_transfer_buffer_init(cb);
	if (ret != BK_OK)
	{
		return ret;
	}

	ret = media_app_register_read_frame_callback(cb->fmt, wifi_transfer_read_frame_callback);

	if (ret == BK_OK)
	{
		transfer_enable = 1;
	}

	return ret;
}

bk_err_t bk_wifi_transfer_frame_close(void)
{
	int ret = BK_FAIL;

	if (!transfer_enable)
	{
		LOGE("%s, have been close!\r\n", __func__);
		return ret;
	}

	transfer_enable = 0;

	ret = media_app_unregister_read_frame_callback();

	wifi_transfer_buffer_deinit();

	media_transfer_callback = NULL;

	rwnxl_set_video_transfer_flag(false);

	bk_wifi_set_video_quality(2);

	return ret;
}

