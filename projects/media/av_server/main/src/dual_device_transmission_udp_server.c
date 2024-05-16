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

#define TAG "dual_dev-UDP_Server"


#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static int video_udp_server_fd = -1;
static int voice_udp_server_fd = -1;

static struct sockaddr_in video_server;
static struct sockaddr_in voice_sender;

static uint8_t video_udp_running = 0;
static uint8_t voice_udp_running = 0;

static beken_thread_t video_udp_handle = NULL;
static beken_thread_t voice_udp_handle = NULL;

static video_device_t *video_dev = NULL;


extern uint32_t bk_net_send_data(uint8_t *data, uint32_t length, video_send_type_t type);


static int voice_udp_voice_send_packet(unsigned char *data, unsigned int len)
{
	int send_byte = 0;
	send_byte = sendto(voice_udp_server_fd, data, len, 0, (struct sockaddr *)&voice_sender, sizeof(struct sockaddr_in));
	LOGD("len: %d, send_byte: %d \n", len, send_byte);
	if (send_byte < len)
	{
		/* err */
		LOGE("need_send: %d, send_complete: %d, errno: %d \n", len, send_byte, errno);
		send_byte = 0;
	}

	return send_byte;
}


static void voice_udp_main(void *thread_param)
{
	uint8_t *buffer;
	int sender_len, r_size = 0;
	bk_err_t ret = BK_OK;
	av_aud_voc_setup_t aud_setup;
	static struct sockaddr_in server;

	buffer = os_malloc(TRANSFER_BUF_SIZE + 1);
	if (buffer == NULL)
		return;

	voice_udp_server_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (voice_udp_server_fd < 0) {
		LOGE("can't create socket!! exit\n");
		goto udp_server_exit;
	}

	server.sin_family = PF_INET;
	server.sin_port = htons(VOICE_PORT);
	server.sin_addr.s_addr = inet_addr("0.0.0.0");

	voice_sender.sin_family = PF_INET;
	voice_sender.sin_port = htons(VOICE_PORT);
	voice_sender.sin_addr.s_addr = inet_addr("192.168.0.100");
	LOGI("av_udp_server run...\n");

	if (bind(voice_udp_server_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
		LOGE("av_udp_server bind failed!! exit\n");
		goto udp_server_exit;
	}

	aud_setup.av_aud_voc_send_packet = voice_udp_voice_send_packet;
	aud_voc_start(aud_setup);
	voice_udp_running = 1;

	while (voice_udp_running) {
		r_size = recvfrom(voice_udp_server_fd, buffer, TRANSFER_BUF_SIZE, 0, (struct sockaddr *)&voice_sender, (socklen_t *)&sender_len);
		if (r_size > 0) {
			LOGD("Rx data from client, r_size: %d \r\n", r_size);

			ret = bk_aud_intf_write_spk_data((uint8_t *)buffer, r_size);
			if (ret != BK_OK)
			{
				LOGE("write speaker data fial\n");
			}
		} else {
			// close this socket
			LOGI("recv close fd:%d, rcv_len:%d\n", voice_udp_server_fd, r_size);
			close(voice_udp_server_fd);
			voice_udp_server_fd = -1;

			/* close audio */
			aud_voc_stop();

			goto udp_server_exit;
		}
	}

udp_server_exit:
	if (voice_udp_server_fd >= 0)
		closesocket(voice_udp_server_fd);

	if (buffer) {
		os_free(buffer);
		buffer = NULL;
	}

	voice_udp_running = 0;
	voice_udp_handle = NULL;

	rtos_delete_thread(NULL);
}


static void video_udp_main(beken_thread_arg_t data)
{
	int rcv_len = 0;
	socklen_t srvaddr_len = 0;
	//	struct sockaddr_in server;
	struct sockaddr_in sender;

	u8 *rcv_buf = NULL;

	LOGI("%s entry\n", __func__);
	(void)(data);

	rcv_buf = (u8 *) os_malloc((TRANSFER_BUF_SIZE + 1) * sizeof(u8));
	if (!rcv_buf)
	{
		LOGE("udp os_malloc failed\n");
		goto out;
	}

	// for data transfer
	video_udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (video_udp_server_fd == -1)
	{
		LOGE("socket failed\n");
		goto out;
	}

	video_server.sin_family = AF_INET;
	video_server.sin_port = htons(VIDEO_PORT);
	video_server.sin_addr.s_addr = inet_addr("0.0.0.0");

	sender.sin_family = AF_INET;
	sender.sin_port = htons(VIDEO_PORT);
	sender.sin_addr.s_addr = inet_addr("192.168.0.100");

	srvaddr_len = (socklen_t)sizeof(struct sockaddr_in);
	if (bind(video_udp_server_fd, (struct sockaddr *)&video_server, srvaddr_len) == -1)
	{
		LOGE("bind failed\n");
		goto out;
	}
	else
	{
		media_app_camera_open(&video_dev->device);

		media_app_lcd_open(&video_dev->lcd_dev);
        lcd_driver_backlight_open();
	}

	video_udp_running = 1;

	while (video_udp_running)
	{
		rcv_len = recvfrom(video_udp_server_fd, rcv_buf, TRANSFER_BUF_SIZE, 0,
						   (struct sockaddr *)&sender, &srvaddr_len);
		if (rcv_len > 0)
		{
			bk_net_send_data(rcv_buf, rcv_len, TVIDEO_SND_UDP);
		}
		else
		{
			LOGE("recvfrom:%d\r\n", rcv_len);
		}
	}

out:

	LOGE("%s exit %d\n", __func__, video_udp_running);

    lcd_driver_backlight_close();

	media_app_lcd_close();

	media_app_camera_close(video_dev->device.type);

	if (rcv_buf)
	{
		os_free(rcv_buf);
		rcv_buf = NULL;
	}

	if (video_udp_server_fd != -1)
	{
		close(video_udp_server_fd);
		video_udp_server_fd = -1;
	}

	video_udp_running = 0;

	video_udp_handle = NULL;
	rtos_delete_thread(NULL);
}

static bk_err_t voice_udp_server_init(void)
{
	int ret = rtos_create_thread(&voice_udp_handle,
					 BEKEN_APPLICATION_PRIORITY,
					 "voice_udp_server",
					 (beken_thread_function_t)voice_udp_main,
					 THREAD_SIZE,
					 (beken_thread_arg_t)NULL);
	return ret;
}

static bk_err_t video_udp_server_init(void)
{
	int ret = rtos_create_thread(&video_udp_handle,
							 BEKEN_APPLICATION_PRIORITY,
							 "video_udp_server",
							 (beken_thread_function_t)video_udp_main,
							 THREAD_SIZE,
							 (beken_thread_arg_t)NULL);
	if (ret != kNoErr)
	{
		LOGE("Error: Failed to create video_udp_server\r\n");
	}

	return ret;
}

bk_err_t dual_device_transmission_udp_server_init(video_device_t *dev)
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

	ret = voice_udp_server_init();
	if (ret != BK_OK)
	{
		LOGE("Error: create voice_task failed!\n");
		return ret;
	}

	ret = video_udp_server_init();
	if (ret != BK_OK)
	{
		LOGE("Error: create video_task failed!\n");
		dual_device_transmission_udp_server_deinit();
		return ret;
	}

	return kNoErr;
}

bk_err_t dual_device_transmission_udp_server_deinit(void)
{
	GLOBAL_INT_DECLARATION();

	if (video_udp_running == 0)
	{
		goto voice;
	}

	GLOBAL_INT_DISABLE();
	video_udp_running = 0;
	GLOBAL_INT_RESTORE();

	while (video_udp_handle)
	{
		rtos_delay_milliseconds(10);
	}

voice:
	if (voice_udp_running)
	{
		return BK_OK;
	}

	GLOBAL_INT_DISABLE();
	voice_udp_running = 0;
	GLOBAL_INT_RESTORE();

	while (voice_udp_handle)
	{
		rtos_delay_milliseconds(10);
	}

	return BK_OK;
}


