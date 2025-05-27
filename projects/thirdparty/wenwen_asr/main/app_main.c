#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include "cli.h"
#include "wenwen_asr.h"
#include <driver/pwr_clk.h>


extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);


#if (CONFIG_SYS_CPU0)
void user_app_main(void)
{
}
#endif


int main(void)
{
#if (CONFIG_SYS_CPU0)
	rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
#endif
	bk_init();
	extern int media_service_init(void);
	media_service_init();

#if (CONFIG_SYS_CPU0)
	bk_pm_module_vote_boot_cp1_ctrl(PM_BOOT_CP1_MODULE_NAME_AUDP_AUDIO, PM_POWER_MODULE_STATE_ON);
#endif

#if (CONFIG_WENWEN_ASR)
	wenwen_asr_init();
	wenwen_asr_start();
#endif
	return 0;
}
