#include <common/bk_include.h>

#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>
#include <driver/int.h>
#include <common/bk_err.h>

#include <driver/psram.h>
#include <driver/jpeg_enc.h>
#include <components/video_transfer.h>

#if (CONFIG_FATFS)
#include "ff.h"
#include "diskio.h"
#endif

#if CONFIG_GENERAL_DMA
#include "bk_general_dma.h"
#endif

#include "cli.h"

#define TAG "capture"

#define LOGI(...) BK_LOGI(TAG, ##__VA_ARGS__)
#define LOGW(...) BK_LOGW(TAG, ##__VA_ARGS__)
#define LOGE(...) BK_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) BK_LOGD(TAG, ##__VA_ARGS__)

#define EJPEG_DATA_ADDR                (0x60010000) // PSRAM

static void dvp_help(void)
{
	LOGI("dvp_image {init|capture file_id|set_cfg|save_image|read|deinit}\r\n");
}

void image_save_dvp(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	bk_err_t err;
	char *msg = CLI_CMD_RSP_SUCCEED;

	if (argc < 2) {
		dvp_help();
		goto error;
	}

	if (os_strcmp(argv[1], "init") == 0) {
		media_camera_device_t device = {0};
		device.type = DVP_CAMERA;
		device.mode = JPEG_MODE;
		device.fmt = PIXEL_FMT_JPEG;

#if (!CONFIG_PSRAM)
		LOGE("Not Support, PSRAM NOT support!\r\n");
#endif

		if (argc == 5)
		{
			media_ppi_t ppi = PPI_DEFAULT;

			if (os_strcmp(argv[2], "dvp") == 0)
			{
				device.type = DVP_CAMERA;
			}
			else
			{
				device.type = UVC_CAMERA;
			}

			ppi = get_string_to_ppi(argv[3]);
			if (ppi == PPI_DEFAULT)
			{
				LOGE("resolution param error\r\n");
				goto error;
			}

			device.info.resolution.width = ppi << 16;
			device.info.resolution.height = ppi & 0xFFFF;
			device.info.fps = get_string_to_fps(argv[4]);
		}

		err = bk_video_buffer_open(&device);
		if (err != kNoErr) {
			os_printf("camera init failed\r\n");
			goto error;
		}

	} else if(os_strcmp(argv[1], "deinit") == 0) {

		err = bk_video_buffer_close();
		if (err != kNoErr) {
			os_printf("video close failed\r\n");
			goto error;
		}

#if (!CONFIG_PSRAM)
		os_printf("Not Support, PSRAM NOT support!\n");
#endif
	} else if (os_strcmp(argv[1], "capture") == 0) {
#if (CONFIG_FATFS)
		int error_code = 0;
		FIL fp1;
		char *file_path = "dvp.jpg";
		uint8_t file_id = 0;
		char cFileName[50];
		unsigned int uiTemp = 0;
		uint32_t frame_len = 60 * 1024;
#if (CONFIG_PSRAM)
		uint8_t *addr = (uint8_t *)EJPEG_DATA_ADDR;
#else
		uint8_t *addr = (uint8_t *)os_malloc(frame_len);
		if (addr == NULL)
		{
			os_printf("malloc frame buffer error!\r\n");
			goto error;
		}
#endif

		if (argc != 3)
		{
			os_printf("input param error\n");
			goto error;
		}

		file_id = os_strtoul(argv[2], NULL, 10) & 0xFF;

		sprintf(cFileName, "%d:/%d_%s", DISK_NUMBER_SDIO_SD, file_id, file_path);

		FRESULT fr = f_open(&fp1, cFileName, FA_CREATE_ALWAYS | FA_WRITE);
		if (fr != FR_OK) {
			os_printf("can not open file:%s!\n", cFileName);
			goto error;
		}
		os_printf("open file:%s!\n", cFileName);

		frame_len = bk_video_buffer_read_frame((uint8_t *)addr, frame_len, &error_code, 1000);
		if (frame_len == 0 || error_code != 0) {
			os_printf("read image failed error_code:%d!!!\n", error_code);
			goto error1;
		} else {
			fr = f_write(&fp1, (char *)addr, frame_len, &uiTemp);
			if (fr != FR_OK) {
				os_printf("f_write failed 1 fr = %d\r\n", fr);
				goto error1;
			}
		}

		f_close(&fp1);
#if (!CONFIG_PSRAM)
		os_free(addr);
		addr = NULL;
#endif
		msg = CLI_CMD_RSP_SUCCEED;
		os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
		return;

error1:

		f_close(&fp1);
#if (!CONFIG_PSRAM)
		os_free(addr);
		addr = NULL;
#endif
		goto error;
#else
		os_printf("Not Support, SDcard not support!\r\n");
		goto error;
#endif
	} else if (os_strcmp(argv[1], "read") == 0) {
		uint8_t *mybuf;
		uint32_t my_len, get_len = 0;
		int get_ret = 0;

		if (argc < 3) {
			os_printf("input param error\n");
			goto error;
		}

		my_len = os_strtoul(argv[2], NULL, 10);
		mybuf = (uint8_t *)os_malloc(my_len);
		
		if (mybuf == NULL) {
			os_printf("vbuf test no buff\r\n");
			goto error;
		}

		get_len = bk_video_buffer_read_frame(mybuf, my_len, &get_ret, 1000);
		os_printf("get frame ret: %d, len:%d\r\n", get_ret, get_len);

		os_free(mybuf);

		if (get_ret != 0)
		{
			goto error;
		}

		msg = CLI_CMD_RSP_SUCCEED;
	} else if (os_strcmp(argv[1], "auto_encode") == 0) {
		uint8_t auto_enable = 0;
		uint16_t up_size = 0, low_size = 0;

		auto_enable = os_strtoul(argv[2], NULL, 10) & 0xF;
		if (auto_enable)
		{
			up_size = os_strtoul(argv[3], NULL, 10) * 1024;
			low_size = os_strtoul(argv[4], NULL, 10) * 1024;
		}

		bk_jpeg_enc_encode_config(auto_enable, up_size, low_size);
		os_printf("set OK!\r\n");
		msg = CLI_CMD_RSP_SUCCEED;

	} else {
		dvp_help();
		goto error;
	}

	os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
	return;

error:
	msg = CLI_CMD_RSP_ERROR;
	os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

#define IMAGE_SAVE_CMD_CNT (sizeof(s_image_save_commands) / sizeof(struct cli_command))
static const struct cli_command s_image_save_commands[] = {
	{"dvp", "dvp init/save_image file_id/read/deinit", image_save_dvp},
};

int cli_image_save_init(void)
{
	return cli_register_commands(s_image_save_commands, IMAGE_SAVE_CMD_CNT);
}

