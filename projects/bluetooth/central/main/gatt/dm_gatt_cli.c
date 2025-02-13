#include "cli.h"

#include "dm_gatts.h"
#include "dm_gattc.h"

#include <unistd.h>
#include <getopt.h>

#define BASE_CMD_NAME "ble_gatt_demo"

#define USE_GETOPT 1
#if USE_GETOPT

enum
{
    LONG_OPT_ENUM_INVALID = 0,
    LONG_OPT_ENUM_START = 1000,
    LONG_OPT_ENUM_HELP,
    LONG_OPT_ENUM_RPA,
    LONG_OPT_ENUM_PRIVACY,
    LONG_OPT_ENUM_AUTH,
    LONG_OPT_ENUM_INIT_KEY_DISTR,
    LONG_OPT_ENUM_RSP_KEY_DISTR,
    LONG_OPT_ENUM_PUBLIC_ADDR,
    LONG_OPT_ENUM_IOCAP,
} LONG_OPT_ENUM;

typedef struct
{
    const char *s_opt;
    struct option l_opt;
    const char *param_format;
    const char *desc;
} my_option_t;

static const my_option_t s_gatt_cli_options[] =
{
    //{"h",     .l_opt = {"help",    no_argument,        NULL, LONG_OPT_ENUM_HELP},            "help"},
    {"r:",    .l_opt = {"rpa",    required_argument,  NULL, LONG_OPT_ENUM_RPA},             "<0|1>",    "enable rpa."},
    {NULL,    .l_opt = {"privacy",   required_argument,  NULL, LONG_OPT_ENUM_PRIVACY},         "<0|1>",    "enable privacy."},
    {"i:",    .l_opt = {"iocap",     required_argument,  NULL, LONG_OPT_ENUM_IOCAP},     "0xXX",    "local iocap."},
    {"a:",    .l_opt = {"authentication",   required_argument,  NULL, LONG_OPT_ENUM_AUTH},            "0xXX",     "set authentication req param"},
    {NULL,    .l_opt = {"ikd",    required_argument,  NULL, LONG_OPT_ENUM_INIT_KEY_DISTR},  "0xXX",     "set initiator key distr"},
    {NULL,    .l_opt = {"rkd",    required_argument,  NULL, LONG_OPT_ENUM_RSP_KEY_DISTR},   "0xXX",     "set responder key distr"},
    {"p:",    .l_opt = {"public-addr",     required_argument,  NULL, LONG_OPT_ENUM_PUBLIC_ADDR},     "<0|1>",    "use public addr."},
};

static cli_gatt_param_t s_cli_gatt_param;

static int32_t cmd_opt_parse(int argc, char **argv)
{
    int32_t ret = 0;

    struct option *l_option_array = NULL;
    char *s_option_string = NULL;

    uint32_t l_option_count = 0;

    int l_option_index = 0;
    int ret_opt = 0;
    uint32_t tmp_hex = 0;

    if (argc <= 0)
    {
        CLI_LOGW("not optarg param\n");
        return 0;
    }

    os_memset(&s_cli_gatt_param, 0, sizeof(s_cli_gatt_param));
    s_option_string = os_malloc(8 * sizeof(s_gatt_cli_options) / sizeof(s_gatt_cli_options[0]));
    l_option_array = os_malloc(sizeof(struct option) * (sizeof(s_gatt_cli_options) / sizeof(s_gatt_cli_options[0]) + 1));

    if (!s_option_string || !l_option_array)
    {
        CLI_LOGE("malloc fail\n");
        ret = -1;
        goto __error;
    }

    os_memset(s_option_string, 0, 8 * sizeof(s_gatt_cli_options) / sizeof(s_gatt_cli_options[0]));
    os_memset(l_option_array, 0, sizeof(struct option) * (sizeof(s_gatt_cli_options) / sizeof(s_gatt_cli_options[0]) + 1));

    for (int i = 0; i < sizeof(s_gatt_cli_options) / sizeof(s_gatt_cli_options[0]); ++i)
    {
        if (s_gatt_cli_options[i].s_opt)
        {
            strcat(s_option_string, s_gatt_cli_options[i].s_opt);
        }

        if (s_gatt_cli_options[i].l_opt.name)
        {
            os_memcpy(&l_option_array[l_option_count], &s_gatt_cli_options[i].l_opt, sizeof(s_gatt_cli_options[i].l_opt));
            l_option_count++;
        }
    }

    while ((ret_opt = getopt_long(argc, argv, s_option_string, (const struct option *)l_option_array, &l_option_index)) >= 0)
    {
        CLI_LOGI("%s %d optarg %p %s\n", __func__, ret_opt, optarg, optarg ? optarg : NULL);

        switch (ret_opt)
        {
        case 'r':
        case LONG_OPT_ENUM_RPA:
            ret = sscanf(optarg, "%hhu", &s_cli_gatt_param.rpa);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.p_rpa = &s_cli_gatt_param.rpa;
            break;

        case LONG_OPT_ENUM_PRIVACY:
            ret = sscanf(optarg, "%hhu", &s_cli_gatt_param.privacy);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.p_privacy = &s_cli_gatt_param.privacy;
            break;

        case 'a':
        case LONG_OPT_ENUM_AUTH:
            ret = sscanf(optarg, "%x", &tmp_hex);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.auth = (typeof(s_cli_gatt_param.auth))tmp_hex;
            s_cli_gatt_param.p_auth = &s_cli_gatt_param.auth;
            break;

        case LONG_OPT_ENUM_INIT_KEY_DISTR:
            ret = sscanf(optarg, "%x", &tmp_hex);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.ikd = (typeof(s_cli_gatt_param.ikd))tmp_hex;
            s_cli_gatt_param.p_ikd = &s_cli_gatt_param.ikd;
            break;

        case LONG_OPT_ENUM_RSP_KEY_DISTR:
            ret = sscanf(optarg, "%x", &tmp_hex);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.rkd = (typeof(s_cli_gatt_param.rkd))tmp_hex;
            s_cli_gatt_param.p_rkd = &s_cli_gatt_param.rkd;
            break;

        case 'p':
        case LONG_OPT_ENUM_PUBLIC_ADDR:
            ret = sscanf(optarg, "%hhu", &s_cli_gatt_param.pa);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.p_pa = &s_cli_gatt_param.pa;
            break;

        case 'i':
        case LONG_OPT_ENUM_IOCAP:
            ret = sscanf(optarg, "%x", &tmp_hex);

            if (ret != 1)
            {
                CLI_LOGE("invalid param\n");
                ret = -1;
                goto __error;
            }

            s_cli_gatt_param.iocap = (typeof(s_cli_gatt_param.iocap))tmp_hex;
            s_cli_gatt_param.p_iocap = &s_cli_gatt_param.iocap;
            break;

        default:
        case '?':
            CLI_LOGW("not optarg param\n");
            ret = -1;
            goto __error;
            break;
        }

        ret = 0;
    }

__error:;

    if (ret)
    {
        os_memset(&s_cli_gatt_param, 0, sizeof(s_cli_gatt_param));
    }

    if (s_option_string)
    {
        os_free(s_option_string);
        s_option_string = NULL;
    }

    if (l_option_array)
    {
        os_free(l_option_array);
        l_option_array = NULL;
    }

    optind = 0;
    return ret;
}

#endif

static void ble_gatt_demo_usage(void)
{
    CLI_LOGI("Usage:\n");
    CLI_LOGI("%s gatts init\n", BASE_CMD_NAME);
    CLI_LOGI("%s gatts disconnect <xx:xx:xx:xx:xx:xx>\n", BASE_CMD_NAME);
    CLI_LOGI("\n");
    CLI_LOGI("%s gattc init\n", BASE_CMD_NAME);
    CLI_LOGI("%s gattc connect <xx:xx:xx:xx:xx:xx>\n", BASE_CMD_NAME);
    CLI_LOGI("%s gattc disconnect <xx:xx:xx:xx:xx:xx>\n", BASE_CMD_NAME);
    CLI_LOGI("%s gattc connect_cancel\n", BASE_CMD_NAME);
    CLI_LOGI("\n");
    CLI_LOGI("%s create_bond <xx:xx:xx:xx:xx:xx>\n", BASE_CMD_NAME);
    CLI_LOGI("%s passkey <key>\n", BASE_CMD_NAME);
    CLI_LOGI("%s show_bond\n", BASE_CMD_NAME);
    CLI_LOGI("%s update_param <xx:xx:xx:xx:xx:xx> <interval> <timeout>\n", BASE_CMD_NAME);
    CLI_LOGI("\n");

#if USE_GETOPT

#define SHORT_OPT_INDEX 0
#define LONG_OPT_INDEX 5
#define SHORT_LONG_GAP 4
#define OPT_MAX_LEN 30

    CLI_LOGI("%s gatt[s|c] init [OPTION...]\n", BASE_CMD_NAME);

    for (int i = 0; i < sizeof(s_gatt_cli_options) / sizeof(s_gatt_cli_options[0]); ++i)
    {
        char tmp_buff[128] = {[0 ... sizeof(tmp_buff) - 2] = ' '};
        int index = 0;

        if (s_gatt_cli_options[i].s_opt)
        {
            sprintf(tmp_buff + SHORT_OPT_INDEX, "  -%c", s_gatt_cli_options[i].s_opt[0]);
        }

        if (s_gatt_cli_options[i].s_opt && s_gatt_cli_options[i].l_opt.name)
        {
            sprintf(tmp_buff + SHORT_LONG_GAP, ",");
        }

        if (s_gatt_cli_options[i].l_opt.name)
        {
            sprintf(tmp_buff + LONG_OPT_INDEX, " --%s", s_gatt_cli_options[i].l_opt.name);

            if (s_gatt_cli_options[i].param_format)
            {
                strcat(tmp_buff, "=");
                strcat(tmp_buff, s_gatt_cli_options[i].param_format);
            }
        }

        if ((index = strlen(tmp_buff)) <= OPT_MAX_LEN)
        {
            os_memset(tmp_buff + index, ' ', OPT_MAX_LEN - index);
            sprintf(tmp_buff + OPT_MAX_LEN, "%s", s_gatt_cli_options[i].desc);
            CLI_LOGI("%s\n", tmp_buff);
        }
        else
        {
            CLI_LOGI("%s\n", tmp_buff);
            CLI_LOGI("%30s", s_gatt_cli_options[i].desc);
        }
    }

#endif
    return;
}

static void cmd_ble_gatt_demo(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
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

    if (os_strcmp(argv[1], "gatts") == 0 && argc >= 3)
    {
        if (os_strcmp(argv[2], "init") == 0)
        {
#if USE_GETOPT

            if (cmd_opt_parse(argc - 2, argv + 2) < 0)
            {
                goto __usage;
            }

            tmp_param = &s_cli_gatt_param;
#endif
            dm_gatt_main(tmp_param);
            dm_gatts_main(tmp_param);
        }
        else if (os_strcmp(argv[2], "disconnect") == 0 && argc >= 4)
        {
            uint32_t mac[6] = {0};
            uint8_t mac_final[6] = {0};

            //sscanf bug: cant detect %hhx
            ret = sscanf(argv[3], "%02x:%02x:%02x:%02x:%02x:%02x",
                         mac + 5,
                         mac + 4,
                         mac + 3,
                         mac + 2,
                         mac + 1,
                         mac);

            if (ret != 6)
            {
                CLI_LOGE("%s addr err %d\n", __func__, ret);
                goto __usage;
            }

            for (int i = 0; i < sizeof(mac_final); ++i)
            {
                mac_final[i] = (uint8_t)mac[i];
            }

            ret = dm_gatts_disconnect(mac_final);
        }
        else if (!os_strcmp(argv[2], "enable_service") && argc >= 5)
        {
            uint32_t service = 0;
            uint8_t enable = 0;

            ret = sscanf(argv[3], "%hhu", &enable);

            if (ret != 1)
            {
                CLI_LOGE("%s enable param err %d\n", __func__, ret);
                goto __usage;
            }

            ret = sscanf(argv[4], "%u", &service);

            if (ret != 1)
            {
                CLI_LOGE("%s service param err %d\n", __func__, ret);
                goto __usage;
            }

            ret = dm_gatts_enable_service(service, enable);
        }
        else if (!os_strcmp(argv[2], "enable_adv") && argc >= 4)
        {
            uint8_t enable = 0;

            ret = sscanf(argv[3], "%hhu", &enable);

            if (ret != 1)
            {
                CLI_LOGE("%s enable param err %d\n", __func__, ret);
                goto __usage;
            }

            ret = dm_gatts_enable_adv(enable);
        }
        else
        {
            goto __usage;
        }
    }
    else if (os_strcmp(argv[1], "gattc") == 0 && argc >= 3)
    {
        if (os_strcmp(argv[2], "init") == 0)
        {
#if USE_GETOPT

            if (cmd_opt_parse(argc - 2, argv + 2) < 0)
            {
                goto __usage;
            }

            tmp_param = &s_cli_gatt_param;
#endif
            dm_gatt_main(tmp_param);
            dm_gattc_main(tmp_param);
        }
        else if (os_strcmp(argv[2], "connect") == 0 && argc >= 4)
        {
            uint32_t mac[6] = {0};
            uint8_t mac_final[6] = {0};
            uint32_t addr_type = BLE_ADDR_TYPE_RANDOM;
            //sscanf bug: cant detect %hhx
            ret = sscanf(argv[3], "%02x:%02x:%02x:%02x:%02x:%02x",
                         mac + 5,
                         mac + 4,
                         mac + 3,
                         mac + 2,
                         mac + 1,
                         mac);

            if (ret != 6)
            {
                CLI_LOGE("%s addr err %d\n", __func__, ret);
                goto __usage;
            }

            for (int i = 0; i < sizeof(mac_final); ++i)
            {
                mac_final[i] = (uint8_t)mac[i];
            }

            if (argc >= 5)
            {
                ret = sscanf(argv[4], "%u", &addr_type);

                if (ret != 1)
                {
                    CLI_LOGE("%s addr_type err %d\n", __func__, ret);
                    goto __usage;
                }
            }

            ret = dm_gattc_connect(mac_final, addr_type);
        }
        else if (os_strcmp(argv[2], "disconnect") == 0 && argc >= 4)
        {
            uint32_t mac[6] = {0};
            uint8_t mac_final[6] = {0};

            //sscanf bug: cant detect %hhx
            ret = sscanf(argv[3], "%02x:%02x:%02x:%02x:%02x:%02x",
                         mac + 5,
                         mac + 4,
                         mac + 3,
                         mac + 2,
                         mac + 1,
                         mac);

            if (ret != 6)
            {
                CLI_LOGE("%s addr err %d\n", __func__, ret);
                goto __usage;
            }

            for (int i = 0; i < sizeof(mac_final); ++i)
            {
                mac_final[i] = (uint8_t)mac[i];
            }

            ret = dm_gattc_disconnect(mac_final);
        }
        else if (os_strcmp(argv[2], "connect_cancel") == 0)
        {
            ret = dm_gattc_connect_cancel();
        }
        else if (os_strcmp(argv[2], "discover") == 0 && argc >= 4)
        {
            uint16_t conn_id = 0;

            ret = sscanf(argv[3], "%hu", &conn_id);

            if (ret != 1)
            {
                CLI_LOGE("%s conn_id err %d\n", __func__, ret);
                goto __usage;
            }

            ret = dm_gattc_discover(conn_id);
        }
        else if (os_strcmp(argv[2], "write") == 0 && argc >= 6)
        {
            uint16_t conn_id = 0, attr_handle = 0;
            uint32_t len = 255;

            ret = sscanf(argv[3], "%hu", &conn_id);

            if (ret != 1)
            {
                CLI_LOGE("%s conn_id err %d\n", __func__, ret);
                goto __usage;
            }

            ret = sscanf(argv[4], "%hu", &attr_handle);

            if (ret != 1)
            {
                CLI_LOGE("%s attr_handle err %d\n", __func__, ret);
                goto __usage;
            }

            len = strnlen(argv[5], len);

            ret = dm_gattc_write(conn_id, attr_handle, (uint8_t *)argv[5], len);
        }
        else
        {
            goto __usage;
        }
    }
    else if (os_strcmp(argv[1], "passkey") == 0)
    {
        uint8_t accept = 0;
        uint32_t passkey = 0;

        if (argc >= 3)
        {
            ret = sscanf(argv[2], "%u", &passkey);

            if (ret != 1)
            {
                goto __usage;
            }

            accept = 1;
        }

        ret = dm_gatt_passkey_reply(accept, passkey);
    }
    else if (os_strcmp(argv[1], "security_method") == 0)
    {
        uint8_t iocap = 0;
        uint8_t authen_req = 0;
        uint8_t key_distr = 0;

        if (argc >= 4)
        {
            ret = sscanf(argv[2], "%hhu", &iocap);

            if (ret != 1)
            {
                goto __usage;
            }

            ret = sscanf(argv[3], "%hhu", &authen_req);

            if (ret != 1)
            {
                goto __usage;
            }

            if (argc >= 5)
            {
                ret = sscanf(argv[4], "%hhu", &key_distr);

                if (ret != 1)
                {
                    goto __usage;
                }
            }
        }
        else
        {
            goto __usage;
        }

        ret = dm_gatt_set_security_method(iocap, authen_req, key_distr);
    }
    else if (os_strcmp(argv[1], "create_bond") == 0 && argc >= 3)
    {
        uint32_t mac[6] = {0};
        uint8_t mac_final[6] = {0};
        //sscanf bug: cant detect %hhx
        ret = sscanf(argv[2], "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac + 5,
                     mac + 4,
                     mac + 3,
                     mac + 2,
                     mac + 1,
                     mac);

        if (ret != 6)
        {
            CLI_LOGE("%s addr err %d\n", __func__, ret);
            goto __usage;
        }

        for (int i = 0; i < sizeof(mac_final); ++i)
        {
            mac_final[i] = (uint8_t)mac[i];
        }

        ret = dm_ble_gap_create_bond(mac_final);
    }
    else if (os_strcmp(argv[1], "remove_bond") == 0 && argc >= 3)
    {
        uint32_t mac[6] = {0};
        uint8_t mac_final[6] = {0};
        //sscanf bug: cant detect %hhx
        ret = sscanf(argv[2], "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac + 5,
                     mac + 4,
                     mac + 3,
                     mac + 2,
                     mac + 1,
                     mac);

        if (ret != 6)
        {
            CLI_LOGE("%s addr err %d\n", __func__, ret);
            goto __usage;
        }

        for (int i = 0; i < sizeof(mac_final); ++i)
        {
            mac_final[i] = (uint8_t)mac[i];
        }

        ret = dm_ble_gap_remove_bond(mac_final);
    }
    else if (os_strcmp(argv[1], "clean_bond") == 0)
    {
        ret = dm_ble_gap_clean_bond();
    }
    else if (os_strcmp(argv[1], "show_bond") == 0)
    {
        ret = dm_ble_gap_show_bond_list();
    }
    else if (os_strcmp(argv[1], "clean_local_key") == 0)
    {
        ret = dm_ble_gap_clean_local_key();
    }
    else if (os_strcmp(argv[1], "update_param") == 0 && argc >= 3)
    {
        uint32_t mac[6] = {0};
        uint8_t mac_final[6] = {0};
        uint16_t interval = 0x20;
        uint16_t tout = 500;

        //sscanf bug: cant detect %hhx
        ret = sscanf(argv[2], "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac + 5,
                     mac + 4,
                     mac + 3,
                     mac + 2,
                     mac + 1,
                     mac);

        if (ret != 6)
        {
            CLI_LOGE("%s addr err %d\n", __func__, ret);
            goto __usage;
        }

        for (int i = 0; i < sizeof(mac_final); ++i)
        {
            mac_final[i] = (uint8_t)mac[i];
        }

        if (argc >= 4)
        {
            ret = sscanf(argv[3], "%hu", &interval);

            if (ret != 1)
            {
                goto __usage;
            }

            if (argc >= 5)
            {
                ret = sscanf(argv[3], "%hu", &tout);

                if (ret != 1)
                {
                    goto __usage;
                }
            }
        }

        ret = dm_ble_gap_update_param(mac_final, interval, tout);
    }
    else if (os_strcmp(argv[1], "test") == 0)
    {

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
    ble_gatt_demo_usage();

__error:
    CLI_LOGE("%s argc %d\n", __func__, argc);
    msg = CLI_CMD_RSP_ERROR;
    os_memcpy(pcWriteBuffer, msg, os_strlen(msg));
}

static const struct cli_command s_ble_gatt_commands[] =
{
    {BASE_CMD_NAME, "see -h", cmd_ble_gatt_demo},
};

int cli_ble_gatt_demo_init(void)
{
    return cli_register_commands(s_ble_gatt_commands, sizeof(s_ble_gatt_commands) / sizeof(s_ble_gatt_commands[0]));
}

