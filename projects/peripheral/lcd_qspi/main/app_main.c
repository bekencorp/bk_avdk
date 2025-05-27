#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include "cli.h"
#include <driver/media_types.h>
#include "media_ipc.h"
#include "media_service.h"
#include "media_app.h"


extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern void bk_set_jtag_mode(uint32_t cpu_id, uint32_t group_id);

static meida_ipc_t ipc = NULL;

const lcd_open_t lcd_open =
{
    .device_ppi = PPI_400X400,
    .device_name = "st77903_h0165y008t",
};

#define CMDS_COUNT  (sizeof(s_qspi_lcd_commands) / sizeof(struct cli_command))

void cli_qspi_lcd_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	os_printf("%s\r\n", __func__);
}

static const struct cli_command s_qspi_lcd_commands[] =
{
	{"qspi_lcd", "qspi_lcd", cli_qspi_lcd_cmd},
};

int cli_qspi_lcd_init(void)
{
	return cli_register_commands(s_qspi_lcd_commands, CMDS_COUNT);
}

#if (CONFIG_SYS_CPU1)
#include <frame_buffer.h>
#include <lcd_qspi_display_service.h>
#include "yuv_encode.h"

static frame_buffer_t *disp_frame = NULL;


#define RED_COLOR       0xF800
#define GREEN_COLOR     0x07E0
#define BLUE_COLOR      0x001F

void lcd_qspi_display_pure_color(uint16_t color)
{
    uint8_t data[2] = {0};

    data[0] = color >> 8;
    data[1] = color;

    for (int i = 0; i < disp_frame->length; i+=2)
    {
        disp_frame->frame[i] = data[0];
        disp_frame->frame[i + 1] = data[1];
    }

    bk_lcd_qspi_display((uint32_t)(disp_frame->frame));    
}
#endif


static int bk_qspi_lcd_example_init(uint8_t *data, uint32_t size, void *param)
{
#if (CONFIG_SYS_CPU1)
    lcd_open_t *lcd_open = (lcd_open_t *)data;

    os_printf("%s %s, size: %d \n", __func__, lcd_open->device_name, size);

    disp_frame = frame_buffer_display_malloc(ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 2);
    if (disp_frame == NULL) {
        os_printf("%s %d disp_frame malloc fail\r\n", __func__, __LINE__);
        return -1;
    }

    disp_frame->fmt = PIXEL_FMT_RGB565;
    disp_frame->width = ppi_to_pixel_x(lcd_open->device_ppi);
    disp_frame->height = ppi_to_pixel_y(lcd_open->device_ppi);
    disp_frame->length = disp_frame->width * disp_frame->height * 2;

    while(1) {
        lcd_qspi_display_pure_color(RED_COLOR);
        rtos_delay_milliseconds(500);
        lcd_qspi_display_pure_color(GREEN_COLOR);
        rtos_delay_milliseconds(500);
        lcd_qspi_display_pure_color(BLUE_COLOR);
        rtos_delay_milliseconds(500);
    }
#endif

    return 0;
}

#if (CONFIG_SYS_CPU0)
void bk_qspi_lcd_main(void)
{
	cli_qspi_lcd_init();

    bk_err_t ret;

    os_printf("!!!BK7258 LCD QSPI DISPLAY EXAMPLE!!!\r\n");

    ret = media_app_lcd_pipeline_disp_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK)
    {
        os_printf("media_app_lcd_pipeline_open failed\r\n");
        return;
    }

    ret = media_ipc_send(&ipc, (void*)&lcd_open, sizeof(lcd_open), MIPC_CHAN_SEND_FLAG_SYNC);
    if (ret != BK_OK)
    {
        os_printf("media_ipc_send failed\n");
    }
}
#endif

void user_app_main(void){

}

int main(void)
{
#if (CONFIG_SYS_CPU0)
	rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
	// bk_set_printf_sync(true);
	// shell_set_log_level(BK_LOG_WARN);
#endif
	bk_init();

    media_service_init();

#if (CONFIG_SYS_CPU0 || CONFIG_SYS_CPU1)
    media_ipc_chan_cfg_t cfg = {0};
    cfg.cb = bk_qspi_lcd_example_init;
    cfg.name = "app";
    cfg.param = NULL;

    if (media_ipc_channel_open(&ipc, &cfg) != BK_OK)
    {
        os_printf("open ipc failed\n");
        return -1;
    }
#endif

#if (CONFIG_SYS_CPU0)
	bk_qspi_lcd_main();
#endif

	return 0;
}
