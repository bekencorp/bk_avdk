#include "cli.h"

#include "dm_gatts.h"
#include "dm_gattc.h"

#include <unistd.h>

#include "hogpd_demo.h"

#if HOGPD_DEMO_ENABLE

#define BASE_CMD_NAME "hogpd_demo"

static void hogpd_demo_usage(void)
{
    CLI_LOGI("Usage:\n");
    CLI_LOGI("%s init\n", BASE_CMD_NAME);
    CLI_LOGI("\n");

    return;
}

static void cmd_hogpd_demo(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    char *msg = NULL;
    int ret = 0;
    cli_gatt_param_t *tmp_param = NULL;

    if (argc == 1)
    {
        goto __usage;
    }
    else if (os_strcmp(argv[1], "-h") == 0 || os_strcmp(argv[1], "--help") == 0)
    {
        goto __usage;
    }

    if (os_strcmp(argv[1], "init") == 0)
    {
        hogpd_demo_init();
    }
    else
    {
        goto __usage;
    }

    if (ret)
    {
        goto __error;
    }

    msg = CLI_CMD_RSP_SUCCEED;
    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
    return;

__usage:
    hogpd_demo_usage();

__error:

    msg = CLI_CMD_RSP_ERROR;
    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

static const struct cli_command s_ble_hogpd_commands[] =
{
    {BASE_CMD_NAME, "see -h", cmd_hogpd_demo},
};

#endif

int cli_ble_hogpd_demo_init(void)
{
#if HOGPD_DEMO_ENABLE
    return cli_register_commands(s_ble_hogpd_commands, sizeof(s_ble_hogpd_commands) / sizeof(s_ble_hogpd_commands[0]));
#else
    return 0;
#endif
}
