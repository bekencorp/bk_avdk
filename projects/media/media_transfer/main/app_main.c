#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>

#include "media_service.h"

extern void rtos_set_user_app_entry(beken_thread_function_t entry);
extern void demo_rtsp_tcp_init();
void user_app_main(void)
{
	demo_rtsp_tcp_init();
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

	return 0;
}