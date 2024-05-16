#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include "media_service.h"
#if (CONFIG_SYS_CPU0)
#include <components/shell_task.h>
#include "bk_private/bk_wifi.h"
#include "dual_device_transmission.h"

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);

video_device_t video_dev = {
	.device.type = NET_CAMERA,
	.device.mode = JPEG_MODE,
	.device.fmt = PIXEL_FMT_JPEG,
	.device.info.resolution.width = 800,
	.device.info.resolution.height = 480,
	.device.info.fps = FPS25,
	.lcd_dev.device_ppi = PPI_480X272,
	.lcd_dev.device_name = "st7282",
};


void user_app_main(void){
	rtos_delay_milliseconds(100);
	os_printf("start av server \r\n");
	demo_softap_app_init("av_demo", "", "13");
	os_printf("start ap: av_demo \r\n");

#if CONFIG_AV_DEMO_MODE_TCP
	dual_device_transmission_tcp_server_init(&video_dev);
#else
	dual_device_transmission_udp_server_init(&video_dev);
#endif

	os_printf("av server run \r\n");
}
#endif

int main(void)
{
#if (CONFIG_SYS_CPU0)
	rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
	// bk_set_printf_sync(true);
	// shell_set_log_level(BK_LOG_WARN);
#endif

	bk_init();
	media_service_init();

	return 0;
}
