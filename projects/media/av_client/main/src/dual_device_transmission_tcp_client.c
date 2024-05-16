#include <common/bk_include.h>
#include "lwip/tcp.h"
#include <os/mem.h>
#include <os/os.h>
#include <os/str.h>
#include <common/bk_kernel_err.h>
#include <driver/lcd.h>

#include "lwip/sockets.h"

#include <components/video_types.h>
#include "aud_intf.h"

#include "dual_device_transmission.h"
#include "wifi_transfer.h"

#define TAG "dual_dev-TCP_Client"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)


static int video_tcp_client_fd = -1;
static int voice_tcp_client_fd = -1;

static struct sockaddr_in *video_tcp_client_remote = NULL;
static struct sockaddr_in *voice_tcp_client_remote = NULL;

static uint8_t video_tcp_running = 0;
static uint8_t voice_tcp_running = 0;

static beken_thread_t video_tcp_handle = NULL;
static beken_thread_t voice_tcp_handle = NULL;

static video_device_t *video_dev = NULL;

static int voice_tcp_send_packet(unsigned char *data, unsigned int len)
{
	int send_byte = 0;
	send_byte = write(voice_tcp_client_fd, data, len);
	LOGD("len: %d, send_byte: %d \n", len, send_byte);
	if (send_byte < len)
	{
		/* err */
		LOGE("need_send: %d, send_complete: %d, errno: %d \n", len, send_byte, errno);
		send_byte = 0;
	}

	return send_byte;
}

int video_tcp_send_packet(uint8_t *data, uint32_t len)
{
	int send_byte = 0;

	send_byte = write(video_tcp_client_fd, data, len);

	if (send_byte < 0)
	{
		/* err */
		//LOGE("send return fd:%d\n", send_byte);
		send_byte = 0;
	}

	return send_byte;
}

static void voice_tcp_main(void *thread_param)
{
	bk_err_t ret = BK_OK;
	uint32_t *buffer;
	int r_size;
	av_aud_voc_setup_t aud_setup;

	buffer = os_malloc(TRANSFER_BUF_SIZE);
	if (buffer == NULL)
		goto tcp_client_exit;
	os_memset(buffer, 0x00, TRANSFER_BUF_SIZE);

	aud_setup.av_aud_voc_send_packet = voice_tcp_send_packet;

	voice_tcp_client_remote = (struct sockaddr_in *)os_malloc(sizeof(struct sockaddr_in));
	if (!voice_tcp_client_remote)
	{
		LOGE("voice_tcp_client_remote os_malloc failed\n");
		goto tcp_client_exit;
	}

	LOGI("av_tcp_client start \n");

	while (1) {
		voice_tcp_client_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (voice_tcp_client_fd < 0) {
			LOGE("av_tcp_client: create socket failed, err=%d!\n", errno);
		}

		voice_tcp_client_remote->sin_family = PF_INET;
		voice_tcp_client_remote->sin_port = htons(VOICE_PORT);
		voice_tcp_client_remote->sin_addr.s_addr = inet_addr("192.168.0.1");

		while (1) {
			ret = connect(voice_tcp_client_fd, (struct sockaddr *)voice_tcp_client_remote, sizeof(struct sockaddr_in));
			if(ret < 0)
			{
				LOGE("connect err: %d\r\n", ret);
				//goto tcp_client_exit;
				rtos_delay_milliseconds(500);
				continue;
			} else {
				LOGI("av_tcp_client connect complete \n");
				break;
			}
		}
		//LOGI("av_tcp_client connect complete \n");

		aud_voc_start(aud_setup);
		LOGI("voice start... \n");

		voice_tcp_running = 1;

		while (voice_tcp_running) {
			r_size = recv(voice_tcp_client_fd, buffer, TRANSFER_BUF_SIZE, 0);
			if (r_size > 0) {
				LOGD("Rx data from server, r_size: %d \r\n", r_size);
				ret = bk_aud_intf_write_spk_data((uint8_t *)buffer, r_size);
				if (ret != BK_OK)
				{
					LOGE("write speaker data fial\n", r_size);
				}
			} else {
				// close this socket
				LOGI("recv close fd:%d, rcv_len:%d\n", voice_tcp_client_fd, r_size);
				close(voice_tcp_client_fd);
				voice_tcp_client_fd = -1;

				/* close audio */
				aud_voc_stop();

				goto tcp_client_exit;
			}
		}
	}

tcp_client_exit:
	if (buffer) {
		os_free(buffer);
		buffer = NULL;
	}

	if (voice_tcp_client_remote)
	{
		os_free(voice_tcp_client_remote);
		voice_tcp_client_remote = NULL;
	}

	if (voice_tcp_client_fd >= 0) {
		closesocket(voice_tcp_client_fd);
		voice_tcp_client_fd = -1;
	}

	voice_tcp_running = 0;

	voice_tcp_handle = NULL;
	rtos_delete_thread(NULL);
}


static void video_tcp_main(beken_thread_arg_t data)
{
	bk_err_t ret = BK_OK;

	LOGI("%s entry\n", __func__);
	(void)(data);


	video_tcp_client_remote = (struct sockaddr_in *)os_malloc(sizeof(struct sockaddr_in));
	if (!video_tcp_client_remote)
	{
		LOGE("udp os_malloc failed\n");
		goto out;
	}

	// for data transfer
	video_tcp_client_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (video_tcp_client_fd == -1)
	{
		LOGE("socket failed\n");
		goto out;
	}

	video_tcp_client_remote->sin_family = AF_INET;
	video_tcp_client_remote->sin_port = htons(VIDEO_PORT);
	video_tcp_client_remote->sin_addr.s_addr = inet_addr("192.168.0.1");

	while (1) {
		ret = connect(video_tcp_client_fd, (struct sockaddr *)video_tcp_client_remote, sizeof(struct sockaddr_in));
		if(ret < 0)
		{
			LOGE("connect err: %d\r\n", ret);
			rtos_delay_milliseconds(500);
			continue;
		} else {
			LOGI("doorbell_tcp_client connect complete \n");
			break;
		}
	}

	media_transfer_cb_t transfer_tcp_client = DEFAULT_VIDEO_TRAS_CONFIG();

	transfer_tcp_client.send = video_tcp_send_packet;
	transfer_tcp_client.fmt = video_dev->device.fmt;

	media_app_camera_open(&video_dev->device);

	bk_wifi_transfer_frame_open(&transfer_tcp_client);

	media_app_lcd_open(&video_dev->lcd_dev);
    lcd_driver_backlight_open();

	video_tcp_running = 1;

	while (video_tcp_running)
	{
		rtos_delay_milliseconds(500);
	}

out:

	LOGE("demo_doorbell_tcp_main exit %d\n", video_tcp_running);

    lcd_driver_backlight_close();

	media_app_lcd_close();

	bk_wifi_transfer_frame_close();

	media_app_camera_close(video_dev->device.type);

	if (video_tcp_client_remote)
	{
		os_free(video_tcp_client_remote);
		video_tcp_client_remote = NULL;
	}

	if (video_tcp_client_fd != -1)
	{
		close(video_tcp_client_fd);
		video_tcp_client_fd = -1;
	}

	video_tcp_running = 0;

	if (video_dev)
	{
		os_free(video_dev);
		video_dev = NULL;
	}

	video_tcp_handle = NULL;
	rtos_delete_thread(NULL);
}

static bk_err_t voice_tcp_client_init(void)
{
	int ret = rtos_create_thread(&voice_tcp_handle,
							 BEKEN_APPLICATION_PRIORITY,
							 "voice_tcp_client",
							 (beken_thread_function_t)voice_tcp_main,
							 THREAD_SIZE,
							 (beken_thread_arg_t)NULL);

	return ret;
}

static bk_err_t video_tcp_client_init(void)
{
	int ret = rtos_create_thread(&video_tcp_handle,
							 BEKEN_APPLICATION_PRIORITY,
							 "video_tcp_client",
							 (beken_thread_function_t)video_tcp_main,
							 THREAD_SIZE,
							 (beken_thread_arg_t)NULL);
	if (ret != kNoErr)
	{
		LOGE("Error: Failed to create video_tcp_client\r\n");
	}

	return ret;
}

bk_err_t dual_device_transmission_tcp_client_init(video_device_t *dev)
{
	int ret;

	if (video_dev == NULL)
	{
		video_dev = (video_device_t *)os_malloc(sizeof(video_device_t));

		if (video_dev == NULL)
		{
			LOGE("Error: malloc video_dev failed!\n");
			return BK_FAIL;
		}

		os_memcpy(video_dev, dev, sizeof(video_device_t));
	}

	ret = voice_tcp_client_init();
	if (ret != BK_OK)
	{
		LOGE("Error: create voice_task failed!\n");
		return ret;
	}

	ret = video_tcp_client_init();
	if (ret != BK_OK)
	{
		LOGE("Error: create video_task failed!\n");
		dual_device_transmission_tcp_client_deinit();
		return ret;
	}

	return kNoErr;
}

bk_err_t dual_device_transmission_tcp_client_deinit(void)
{
	GLOBAL_INT_DECLARATION();

	if (video_tcp_running == 0)
	{
		goto voice;
	}

	GLOBAL_INT_DISABLE();
	video_tcp_running = 0;
	GLOBAL_INT_RESTORE();

	while (video_tcp_handle)
	{
		rtos_delay_milliseconds(10);
	}

voice:
	if (voice_tcp_running)
	{
		return BK_OK;
	}

	GLOBAL_INT_DISABLE();
	voice_tcp_running = 0;
	GLOBAL_INT_RESTORE();

	while (voice_tcp_handle)
	{
		rtos_delay_milliseconds(10);
	}

	return BK_OK;
}

