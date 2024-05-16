#include <common/bk_include.h>
#include "lwip/tcp.h"
#include <os/mem.h>
#include <os/os.h>
#include <os/str.h>
#include <common/bk_kernel_err.h>

#include "lwip/sockets.h"
#include <driver/lcd.h>

#include <components/video_types.h>
#include "aud_intf.h"

#include "dual_device_transmission.h"

#define TAG "doorbell-TCP-Server"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

static int video_tcp_server_fd = -1;
static int voice_tcp_server_fd = -1;
static int new_cli_sockfd = -1;
static int cli_sockfd = -1;

static struct sockaddr_in *video_tcp_server_remote = NULL;
static struct sockaddr_in *voice_tcp_server_remote = NULL;

static uint8_t video_tcp_running = 0;
static uint8_t voice_tcp_running = 0;

static beken_thread_t video_tcp_handle = NULL;
static beken_thread_t voice_tcp_handle = NULL;

static video_device_t *video_dev = NULL;


extern uint32_t bk_net_send_data(uint8_t *data, uint32_t length, video_send_type_t type);

static void tcp_set_keepalive(int fd)
{
	int opt = 1, ret;
	// open tcp keepalive
	ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(int));

	opt = 30;  // 5 second
	ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(int));

	opt = 1;  // 1s second for intval
	ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(int));

	opt = 3;  // 3 times
	ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(int));
	ret = ret;
}

static int voice_tcp_voice_send_packet(unsigned char *data, unsigned int len)
{
	int send_byte = 0;
	send_byte = write(new_cli_sockfd, data, len);
	LOGD("len: %d, send_byte: %d \n", len, send_byte);
	if (send_byte < len)
	{
		/* err */
		LOGE("need_send: %d, send_complete: %d, errno: %d \n", len, send_byte, errno);
		send_byte = 0;
	}

	return send_byte;
}

static void voice_tcp_main(void *thread_param)
{
	uint32_t *buffer;
	int r_size = 0;
	bk_err_t ret = BK_OK;
	fd_set watchfd;
	av_aud_voc_setup_t aud_setup;

	buffer = os_malloc(TRANSFER_BUF_SIZE + 1);
	if (buffer == NULL)
		return;

	voice_tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (voice_tcp_server_fd < 0) {
		LOGE("can't create socket!! exit\n");
		goto tcp_server_exit;
	}

	voice_tcp_server_remote = (struct sockaddr_in *)os_malloc(sizeof(struct sockaddr_in));
	if (!voice_tcp_server_remote)
	{
		LOGE("voice_tcp_server_remote os_malloc failed\n");
		goto tcp_server_exit;
	}

	voice_tcp_server_remote->sin_family = PF_INET;
	voice_tcp_server_remote->sin_port = htons(VOICE_PORT);
	voice_tcp_server_remote->sin_addr.s_addr = inet_addr("0.0.0.0");

	if (bind(voice_tcp_server_fd, (struct sockaddr *)voice_tcp_server_remote, sizeof(struct sockaddr_in)) < 0) {
		LOGE("av_udp_server bind failed!! exit\n");
		goto tcp_server_exit;
	}

	if (listen(voice_tcp_server_fd, 0) == -1)
	{
		LOGE("listen failed\n");
		goto tcp_server_exit;
	}

	aud_setup.av_aud_voc_send_packet = voice_tcp_voice_send_packet;
	LOGI("%s: start listen \n", __func__);

	while (1) {
		FD_ZERO(&watchfd);
		FD_SET(voice_tcp_server_fd, &watchfd);

		LOGI("select fd\n");
		ret = select(voice_tcp_server_fd + 1, &watchfd, NULL, NULL, NULL);
		if (ret <= 0)
		{
			LOGE("select ret:%d\n", ret);
			continue;
		}
		else
		{
			// is new connection
			if (FD_ISSET(voice_tcp_server_fd, &watchfd))
			{
				struct sockaddr_in client_addr;
				socklen_t cliaddr_len = 0;

				cliaddr_len = sizeof(client_addr);
				new_cli_sockfd = accept(voice_tcp_server_fd, (struct sockaddr *)&client_addr, &cliaddr_len);
				if (new_cli_sockfd < 0)
				{
					LOGE("accept return fd:%d\n", new_cli_sockfd);
					break;
				}

				LOGI("new accept fd:%d\n", new_cli_sockfd);

				tcp_set_keepalive(new_cli_sockfd);
				break;
			}
		}
	}
	LOGI("connect client complete \n");

	aud_voc_start(aud_setup);
	LOGI("voice start... \n");
	voice_tcp_running = 1;

	while(voice_tcp_running) {
		r_size = recv(new_cli_sockfd, buffer, TRANSFER_BUF_SIZE, 0);
		if (r_size > 0) {
			LOGD("Rx data from server, r_size: %d \r\n", r_size);
			ret = bk_aud_intf_write_spk_data((uint8_t *)buffer, r_size);
			if (ret != BK_OK)
			{
				LOGE("write speaker data fial\n");
			}
		} else {
			// close this socket
			LOGI("recv close fd:%d, rcv_len:%d\n", new_cli_sockfd, r_size);
			close(new_cli_sockfd);
			new_cli_sockfd = -1;

			/* close audio */
			aud_voc_stop();

			goto tcp_server_exit;
		}
	}

tcp_server_exit:

	if (buffer) {
		os_free(buffer);
		buffer = NULL;
	}

	if (voice_tcp_server_fd != -1)
	{
		close(voice_tcp_server_fd);
		voice_tcp_server_fd = -1;
	}

	if (voice_tcp_server_remote)
	{
		os_free(voice_tcp_server_remote);
		voice_tcp_server_remote = NULL;
	}

	voice_tcp_running = 0;

	voice_tcp_handle = NULL;
	rtos_delete_thread(NULL);
}

static void video_tcp_main(beken_thread_arg_t data)
{
	int rcv_len = 0;
	//  struct sockaddr_in server;
	bk_err_t ret = BK_OK;
	uint8_t *rcv_buf = NULL;
	fd_set watchfd;

	LOGI("%s entry\n", __func__);
	(void)(data);

	rcv_buf = (uint8_t *) os_malloc(TRANSFER_BUF_SIZE + 1);
	if (!rcv_buf)
	{
		LOGE("tcp os_malloc failed\n");
		goto out;
	}

	video_tcp_server_remote = (struct sockaddr_in *)os_malloc(sizeof(struct sockaddr_in));
	if (!video_tcp_server_remote)
	{
		LOGE("video_tcp_server_remote os_malloc failed\n");
		goto out;
	}

	// for data transfer
	video_tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (video_tcp_server_fd == -1)
	{
		LOGE("socket failed\n");
		goto out;
	}

	video_tcp_server_remote->sin_family = AF_INET;
	video_tcp_server_remote->sin_port = htons(VIDEO_PORT);
	video_tcp_server_remote->sin_addr.s_addr = inet_addr("0.0.0.0");

	if (bind(video_tcp_server_fd, (struct sockaddr *)video_tcp_server_remote, sizeof(struct sockaddr_in)) == -1)
	{
		LOGE("bind failed\n");
		goto out;
	}

	if (listen(video_tcp_server_fd, 0) == -1)
	{
		LOGE("listen failed\n");
		goto out;
	}

	LOGI("%s: start listen \n", __func__);

	while (1) {
		FD_ZERO(&watchfd);
		FD_SET(video_tcp_server_fd, &watchfd);

		LOGI("select fd\n");
		ret = select(video_tcp_server_fd + 1, &watchfd, NULL, NULL, NULL);
		if (ret <= 0)
		{
			LOGE("select ret:%d\n", ret);
			continue;
		}
		else
		{
			// is new connection
			if (FD_ISSET(video_tcp_server_fd, &watchfd))
			{
				struct sockaddr_in client_addr;
				socklen_t cliaddr_len = 0;

				cliaddr_len = sizeof(client_addr);
				cli_sockfd = accept(video_tcp_server_fd, (struct sockaddr *)&client_addr, &cliaddr_len);
				if (cli_sockfd < 0)
				{
					LOGE("accept return fd:%d\n", cli_sockfd);
					break;
				}

				LOGI("new accept fd:%d\n", cli_sockfd);

				tcp_set_keepalive(cli_sockfd);
				break;
			}
		}
	}
	LOGI("connect client complete \n");


	media_app_camera_open(&video_dev->device);

	media_app_lcd_open(&video_dev->lcd_dev);
    lcd_driver_backlight_open();

	video_tcp_running = 1;

	while (video_tcp_running)
	{
		rcv_len = recv(cli_sockfd, rcv_buf, TRANSFER_BUF_SIZE, 0);
		if (rcv_len > 0)
		{
			bk_net_send_data(rcv_buf, rcv_len, TVIDEO_SND_TCP);
		}
		else
		{
			// close this socket
			LOGI("recv close fd:%d, rcv_len:%d\n", cli_sockfd, rcv_len);
			close(cli_sockfd);
			cli_sockfd = -1;

			goto out;
		}

	}

out:

	LOGE("%s exit %d\n", __func__, video_tcp_running);

    lcd_driver_backlight_close();

	media_app_lcd_close();

	media_app_camera_close(video_dev->device.type);

	if (rcv_buf)
	{
		os_free(rcv_buf);
		rcv_buf = NULL;
	}

	if (video_tcp_server_remote)
	{
		os_free(video_tcp_server_remote);
		video_tcp_server_remote = NULL;
	}

	if (video_tcp_server_fd != -1)
	{
		close(video_tcp_server_fd);
		video_tcp_server_fd = -1;
	}

	video_tcp_running = 0;

	video_tcp_handle = NULL;
	rtos_delete_thread(NULL);
}


static bk_err_t voice_tcp_server_init(void)

{
	int ret = rtos_create_thread(&voice_tcp_handle,
					 BEKEN_APPLICATION_PRIORITY,
					 "voice_tcp_server",
					 (beken_thread_function_t)voice_tcp_main,
					 THREAD_SIZE,
					 (beken_thread_arg_t)NULL);
	return ret;
}

static bk_err_t video_tcp_server_init(void)
{
	int ret = rtos_create_thread(&video_tcp_handle,
							 BEKEN_APPLICATION_PRIORITY,
							 "video_tcp_server",
							 (beken_thread_function_t)video_tcp_main,
							 THREAD_SIZE,
							 (beken_thread_arg_t)NULL);
	if (ret != kNoErr)
	{
		LOGE("Error: Failed to create video_tcp_server\r\n");
	}

	return ret;
}




bk_err_t dual_device_transmission_tcp_server_init(video_device_t *dev)
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

	ret = voice_tcp_server_init();
	if (ret != BK_OK)
	{
		LOGE("Error: create voice_task failed!\n");
		return ret;
	}

	ret = video_tcp_server_init();
	if (ret != BK_OK)
	{
		LOGE("Error: create video_task failed!\n");
		dual_device_transmission_tcp_server_deinit();
		return ret;
	}

	return kNoErr;
}

bk_err_t dual_device_transmission_tcp_server_deinit(void)
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


