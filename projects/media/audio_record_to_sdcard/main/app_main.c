#include "bk_private/bk_init.h"
#include <components/system.h>
#include <os/os.h>
#include <components/shell_task.h>
#include "cli.h"
#include "audio_record.h"
#include "media_service.h"

extern void user_app_main(void);
extern void rtos_set_user_app_entry(beken_thread_function_t entry);


#if (CONFIG_SYS_CPU0)
static void cli_audio_record_to_sdcard_help(void)
{
	os_printf("audio_record_to_sdcard {start|stop file_name sample_rate} \r\n");
}

void cli_audio_record_to_sdcard_cmd(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	uint32_t samp_rate = 8000;

	if (argc < 2)
    {
		cli_audio_record_to_sdcard_help();
		return;
	}

	if (os_strcmp(argv[1], "start") == 0) {
        if (argc < 4)
        {
            cli_audio_record_to_sdcard_help();
            return;
        }

		samp_rate = strtoul(argv[3], NULL, 10);
		if (BK_OK != audio_record_to_sdcard_start(argv[2], samp_rate))
			os_printf("start audio record to sdcard fail\n");
		else
			os_printf("start audio record to sdcard ok\n");
	}else if (os_strcmp(argv[1], "stop") == 0) {
		audio_record_to_sdcard_stop();
	} else {
		cli_audio_record_to_sdcard_help();
	}

}


#define AUDIO_RECORD_CMD_CNT  (sizeof(s_audio_record_commands) / sizeof(struct cli_command))
static const struct cli_command s_audio_record_commands[] =
{
	{"audio_record_to_sdcard", "audio_record_to_sdcard {start|stop file_name sample_rate}", cli_audio_record_to_sdcard_cmd},
};

int cli_audio_record_init(void)
{
	return cli_register_commands(s_audio_record_commands, AUDIO_RECORD_CMD_CNT);
}


void user_app_main(void)
{
	cli_audio_record_init();
}
#endif

int main(void)
{
#if (CONFIG_SYS_CPU0)
	rtos_set_user_app_entry((beken_thread_function_t)user_app_main);
#endif
	bk_init();
    int media_service_init(void);
	media_service_init();

	return 0;
}
