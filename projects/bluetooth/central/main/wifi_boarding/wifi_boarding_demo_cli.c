#include "cli.h"

#include "dm_gatts.h"
#include "dm_gattc.h"

#include <unistd.h>

#include "wifi_boarding_demo.h"

#if WIFI_BOARDING_DEMO_ENABLE

#define BASE_CMD_NAME "wboarding_demo"

static void wboarding_demo_usage(void)
{
    CLI_LOGI("Usage:\n");
    CLI_LOGI("%s init\n", BASE_CMD_NAME);
    CLI_LOGI("\n");

    return;
}

static void cmd_wboarding_demo(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
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
        wifi_boarding_demo_main(NULL);
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
    wboarding_demo_usage();

__error:

    msg = CLI_CMD_RSP_ERROR;
    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

static int hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

static int cli_hexstr2bin(const char *hex, uint8_t *buf, size_t len)
{
	size_t i;
	int a;
	const char *ipos = hex;
	uint8_t *opos = buf;

	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}
	return 0;
}

extern void bk_boarding_operation_handle(uint16_t opcode, uint16_t length, uint8_t *data);
static void cmd_wboarding_ota_demo(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
	uint16_t opcode  = os_strtoul(argv[1], NULL, 10);
	uint16_t length = os_strtoul(argv[2], NULL, 10);

    CLI_LOGI("opcode %d , length %d:\n",opcode , length);

    uint8_t* in_data = (uint8_t*)os_malloc(length*sizeof(uint8_t));

    memset(in_data, 0, length);
    cli_hexstr2bin(argv[3], in_data, length);

    CLI_LOGI("data to be written:\r\n");
    for(int i = 0; i < length; i++)
    {
        CLI_LOGI("0x%x ",in_data[i]);
        if(i % 8 == 7) 
        {
            CLI_LOGI(" \r\n");
        }
    }
 
    if((opcode == 20)||(opcode == 21)||(opcode == 22))
    {
        bk_boarding_operation_handle(opcode, length, in_data);
    }
    os_free(in_data);
    in_data = NULL;
}


static const struct cli_command s_ble_wboarding_commands[] =
{
    {BASE_CMD_NAME, "see -h", cmd_wboarding_demo},
	{"ble_ota_test", "ble_ota_test [opcode] [length] [data]", cmd_wboarding_ota_demo},
};

#endif

int cli_ble_wboarding_demo_init(void)
{
#if WIFI_BOARDING_DEMO_ENABLE
    return cli_register_commands(s_ble_wboarding_commands, sizeof(s_ble_wboarding_commands) / sizeof(s_ble_wboarding_commands[0]));
#else
    return 0;
#endif
}
