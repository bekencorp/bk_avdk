#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <os/mem.h>

#include <components/shell_task.h>
#include "lcd_act.h"
#include "media_app.h"
#include "media_evt.h"
#include <driver/lcd.h>
#include "media_service.h"

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);
#define pdata_addr	0x60000000

extern int rand(void);
beken_timer_t lcd_rgb_timer;

const lcd_open_t lcd_open =
{
	.device_ppi = PPI_320X480,
	.device_name = "st7796s",
};


#if (CONFIG_SYS_CPU1) && (CONFIG_SOC_BK7258)
#include "frame_buffer.h"
#include "yuv_encode.h"

typedef struct
{
    beken_thread_t thread;
    beken_semaphore_t sem;
    bool running;
} lcd_example_t;


lcd_example_t lcd_example;

static void cpu_lcd_fill_test(uint32_t *addr, uint32_t color)
{
	uint32_t *p_addr = addr;
	for(int i=0; i<480*320/2; i++)
	{
		*(p_addr + i) = color;
	}
}

static uint32_t lcd_get_rand_color(void)
{
	uint32_t color_rand = 0;
	uint32_t color_rand_tmp = 0;

	color_rand = (uint32_t)rand();
	color_rand_tmp = (color_rand & 0xffff0000) >> 16; 
	uint32_t color = (color_rand & 0xffff0000) | color_rand_tmp;
    return color;
}

static void lcd_display_example_handle(beken_thread_arg_t data)
{
    rtos_set_semaphore(&lcd_example.sem);
    bk_err_t ret = BK_FAIL;
    lcd_example.running = 1;

    lcd_open_t *lcd_open = (lcd_open_t *)data;
    os_printf("%s %p %x %d\n", __func__, lcd_open, lcd_open->device_ppi, lcd_example.running);

    frame_buffer_t *frame = NULL;

    uint32_t size = ppi_to_pixel_x(lcd_open->device_ppi) * ppi_to_pixel_y(lcd_open->device_ppi) * 2;

    while (lcd_example.running)
    {
        frame = frame_buffer_display_malloc(size);
        if (frame == NULL)
        {
            os_printf("malloc display frame fail\r\n");
            continue;
        }

        frame->width = ppi_to_pixel_x(lcd_open->device_ppi);
        frame->height = ppi_to_pixel_y(lcd_open->device_ppi);
        frame->fmt = PIXEL_FMT_RGB565_LE;
        frame->cb = NULL;

        uint32_t color = lcd_get_rand_color();
        cpu_lcd_fill_test((uint32_t *)frame->frame, color);

        ret = lcd_display_frame_request(frame);
        if (ret != BK_OK)
        {
            os_printf("lcd_display_frame_request fail\r\n");
            frame_buffer_display_free(frame);
            frame = NULL;
            continue;
        }
        rtos_delay_milliseconds(1000);
    }

    lcd_example.thread = NULL;
    rtos_set_semaphore(&lcd_example.sem);
    rtos_delete_thread(NULL);
}

static bk_err_t lcd_task_main(lcd_open_t *lcd_open)
{
    bk_err_t ret = BK_FAIL;
    rtos_init_semaphore(&lcd_example.sem, 1);

    ret = rtos_create_thread(&lcd_example.thread,
                             BEKEN_DEFAULT_WORKER_PRIORITY,
                             "lcd_example_thread",
                             (beken_thread_function_t)lcd_display_example_handle,
                             2 * 1024,
                             (beken_thread_arg_t)lcd_open);
    if (BK_OK != ret)
    {
        os_printf("%s lcd_example_thread creat failed\n", __func__);
        rtos_deinit_semaphore(&lcd_example.sem);
        lcd_example.sem = NULL;
        return ret;
    }

    rtos_get_semaphore(&lcd_example.sem, BEKEN_NEVER_TIMEOUT);
    os_printf("%s lcd_example_thread creat complete\n", __func__);
    return ret;
}

bk_err_t lcd_display_example(media_mailbox_msg_t *msg)
{
    bk_err_t ret = BK_OK;

	lcd_open_t *lcd_open = NULL;
	lcd_open = (lcd_open_t *)os_malloc(sizeof(lcd_open_t));
	os_memcpy(lcd_open, (lcd_open_t *)msg->param, sizeof(lcd_open_t));

    lcd_task_main(lcd_open);
    return ret;
}
#endif

void lcd_rgb_open(void)
{
    bk_err_t ret;

    ret = media_app_lcd_pipeline_disp_open((lcd_open_t *)&lcd_open);
    if (ret != BK_OK) {
        os_printf("media_app_lcd_pipeline_open failed\r\n");
        return;
    }

  ret = media_app_lcd_example_display((lcd_open_t *)&lcd_open);
  if(BK_OK != ret)
  {
      os_printf("%s, lcd display fail\n", __func__);
  }
    os_printf("lcd_rgb_open complete\r\n");
}

void user_app_main(void)
{

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

#if (CONFIG_SYS_CPU0)
	lcd_rgb_open();
#endif

	return 0;
}


