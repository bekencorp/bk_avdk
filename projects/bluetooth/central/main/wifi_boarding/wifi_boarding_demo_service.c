#include "wifi_boarding_demo_service.h"
#include "wifi_boarding_demo.h"
#include "wifi_boarding_network.h"

#include "components/bluetooth/bk_dm_bluetooth.h"

#include <modules/wifi.h>
#include <components/event.h>
#include <components/netif.h>

#include <components/log.h>
#include <os/mem.h>
#include <os/str.h>
#include <os/os.h>

static beken_thread_t s_boarding_thd = NULL;
static beken_queue_t s_boarding_queue = NULL;

static bk_boarding_info_t *bk_boarding_info = NULL;

void bk_boarding_event_notify(uint16_t opcode, int status)
{
    uint8_t data[] =
    {
        opcode & 0xFF, opcode >> 8,     /* opcode           */
                              status & 0xFF,                                                          /* status           */
                              0, 0,                                                                   /* payload length   */
    };

    wboard_logi("%d, %d", opcode, status);
    wifi_boarding_notify(data, sizeof(data));
}

void bk_boarding_event_notify_with_data(uint16_t opcode, int status, char *payload, uint16_t length)
{
    uint8_t data[1024] =
    {
        opcode & 0xFF, opcode >> 8,     /* opcode           */
                              status & 0xFF,                  /* status           */
                              length & 0xFF, length >> 8,     /* payload length   */
                              0,
    };

    if (length > 1024 - 5)
    {
        wboard_loge("size %d over flow", length);
        return;
    }

    os_memcpy(&data[5], payload, length);

    wboard_logi("%d, %d", opcode, status);
    wifi_boarding_notify(data, length + 5);
}

bk_err_t boarding_send_msg(boarding_msg_t *msg)
{
    bk_err_t ret = BK_OK;

    if (s_boarding_queue)
    {
        ret = rtos_push_to_queue(&s_boarding_queue, msg, BEKEN_NO_WAIT);

        if (BK_OK != ret)
        {
            wboard_loge("push queue failed %d", ret);
            return BK_FAIL;
        }

        return ret;
    }
    else
    {
        wboard_loge("queue NULL");
        return BK_FAIL;
    }

    return ret;
}

static void bk_boarding_operation_handle(uint16_t opcode, uint16_t length, uint8_t *data)
{
    wboard_logw("opcode: %04X, length: %u", opcode, length);

    switch (opcode)
    {
    case BOARDING_OP_STATION_START:
    {
        boarding_msg_t msg;

        msg.event = DBEVT_WIFI_STATION_CONNECT;
        msg.param = (uint32_t)bk_boarding_info;
        boarding_send_msg(&msg);
    }
    break;

    case BOARDING_OP_SOFT_AP_START:
    {
        boarding_msg_t msg;

        msg.event = DBEVT_WIFI_SOFT_AP_TURNING_ON;
        msg.param = (uint32_t)bk_boarding_info;
        boarding_send_msg(&msg);
    }
    break;

    case BOARDING_OP_BLE_DISABLE:
    {
        boarding_msg_t msg;

        msg.event = DBEVT_BLE_DISABLE;
        msg.param = 0;
        boarding_send_msg(&msg);
    }
    break;

    case BOARDING_OP_SET_WIFI_CHANNEL:
    {
        STREAM_TO_UINT16(bk_boarding_info->channel, data);

        wboard_logi("BOARDING_OP_SET_WIFI_CHANNEL: %u", bk_boarding_info->channel);

    }
    break;

    default:
    {
        wboard_loge("unsupported opcode: 0x%04X !!!", __func__, opcode);
    }
    break;

    }
}


static void boarding_message_handle(void)
{
    bk_err_t ret = BK_OK;
    boarding_msg_t msg;

    while (1)
    {
        ret = rtos_pop_from_queue(&s_boarding_queue, &msg, BEKEN_WAIT_FOREVER);

        if (kNoErr == ret)
        {
            switch (msg.event)
            {
            case DBEVT_WIFI_STATION_CONNECT:
            {
                wboard_logi("DBEVT_WIFI_STATION_CONNECT");

                bk_boarding_info_t *wifi_info = (bk_boarding_info_t *) msg.param;
                boarding_wifi_sta_connect(wifi_info->boarding_info.ssid_value,
                                          wifi_info->boarding_info.password_value);
            }
            break;

            case DBEVT_WIFI_STATION_CONNECTED:
            {
                wboard_logi("DBEVT_WIFI_STATION_CONNECTED");

                netif_ip4_config_t ip4_config;
                extern uint32_t uap_ip_is_start(void);

                os_memset(&ip4_config, 0x0, sizeof(netif_ip4_config_t));
                bk_netif_get_ip4_config(NETIF_IF_AP, &ip4_config);

                if (uap_ip_is_start())
                {
                    bk_netif_get_ip4_config(NETIF_IF_AP, &ip4_config);
                }
                else
                {
                    bk_netif_get_ip4_config(NETIF_IF_STA, &ip4_config);
                }

                wboard_logi("ip: %s\n", ip4_config.ip);

                bk_boarding_event_notify_with_data(BOARDING_OP_STATION_START, BK_OK, ip4_config.ip, strlen(ip4_config.ip));
            }
            break;

            case DBEVT_WIFI_STATION_DISCONNECTED:
            {
                wboard_logi("DBEVT_WIFI_STATION_DISCONNECTED");
            }
            break;

            case DBEVT_WIFI_SOFT_AP_TURNING_ON:
            {
                wboard_logi("DBEVT_WIFI_SOFT_AP_TURNING_ON");
                bk_boarding_info_t *wifi_info = (bk_boarding_info_t *) msg.param;
                int ret = boarding_wifi_soft_ap_start(wifi_info->boarding_info.ssid_value,
                                                      wifi_info->boarding_info.password_value,
                                                      wifi_info->channel);

                if (ret == BK_OK)
                {
                    bk_boarding_event_notify(BOARDING_OP_SOFT_AP_START, EVT_STATUS_OK);
                }
                else
                {
                    bk_boarding_event_notify(BOARDING_OP_SOFT_AP_START, EVT_STATUS_ERROR);
                }
            }
            break;


            case DBEVT_BLE_DISABLE:
            {
#if CONFIG_BLUETOOTH
                bk_bluetooth_deinit();
                wboard_logi("close bluetooth finish!\r\n");
#endif
            }
            break;

            case DBEVT_EXIT:
                goto exit;
                break;

            default:
                break;
            }
        }
    }

exit:

    /* delate msg queue */
    ret = rtos_deinit_queue(&s_boarding_queue);

    if (ret != kNoErr)
    {
        wboard_loge("delete message queue fail");
    }

    s_boarding_queue = NULL;

    wboard_loge("delete message queue complete");

    /* delate task */
    rtos_delete_thread(NULL);

    s_boarding_thd = NULL;

    wboard_loge("delete task complete");
}

int32_t wifi_boarding_demo_service_main(void)
{
    bk_err_t ret = BK_OK;

    ret = rtos_init_queue(&s_boarding_queue,
                          "boarding_queue",
                          sizeof(boarding_msg_t),
                          10);

    if (ret != BK_OK)
    {
        wboard_loge("create boarding message queue failed");
        return -1;
    }

    ret = rtos_create_thread(&s_boarding_thd,
                             BEKEN_DEFAULT_WORKER_PRIORITY,
                             "boarding_thd",
                             (beken_thread_function_t)boarding_message_handle,
                             2560,
                             NULL);

    if (ret != BK_OK)
    {
        wboard_loge("create boarding major thread fail");
        return -1;
    }

    if (bk_boarding_info == NULL)
    {
        bk_boarding_info = os_malloc(sizeof(bk_boarding_info_t));

        if (bk_boarding_info == NULL)
        {
            wboard_loge("bk_boarding_info malloc failed\n");

            return -1;
        }

        os_memset(bk_boarding_info, 0, sizeof(bk_boarding_info_t));
    }

    bk_boarding_info->boarding_info.cb = bk_boarding_operation_handle;
    wifi_boarding_demo_main(&bk_boarding_info->boarding_info);

    return ret;
}
