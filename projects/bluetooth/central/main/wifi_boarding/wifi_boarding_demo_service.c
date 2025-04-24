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
#include "ble_ota.h"

static beken_thread_t s_boarding_thd = NULL;
static beken_queue_t s_boarding_queue = NULL;

static bk_boarding_info_t *bk_boarding_info = NULL;

static f_ota_t *s_ble_ota = NULL;
static uint8_t *s_ota_data_ptr = NULL;
static beken2_timer_t s_ble_ota_tmr;

static void ble_ota_timer_hdl(void *param1, void *param2)
{
    if(s_ble_ota == NULL){
        return ;
    }
    OTA_FREE(s_ble_ota->magic_code);
    f_ota_fun_ptr->deinit(s_ble_ota);
    OTA_FREE(s_ota_data_ptr);
    wboard_logw("ble disconnect need reboot  !\r\n");
    bk_reboot();   //need do reboot.
}

void ble_ota_start_timer(void)
{
    rtos_init_oneshot_timer(&s_ble_ota_tmr, CONFIG_BLE_OTA_WAIT_TIMEOUT, ble_ota_timer_hdl, 0, 0);
    rtos_start_oneshot_timer(&s_ble_ota_tmr);

    return ;
}

void ble_ota_stop_timer(void)
{
    if (rtos_is_oneshot_timer_init(&s_ble_ota_tmr))
    {
        if (rtos_is_oneshot_timer_running(&s_ble_ota_tmr))
            rtos_stop_oneshot_timer(&s_ble_ota_tmr);
        rtos_deinit_oneshot_timer(&s_ble_ota_tmr);
    }

    return;
}

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

void bk_boarding_operation_handle(uint16_t opcode, uint16_t length, uint8_t *data)
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

    case BOARDING_OP_OTA_START_DOWNLOAD:
    {
        boarding_msg_t msg;

        msg.event = DBEVT_OTA_START_DOWNLOAD;
        msg.length = length;

        OTA_MALLOC_WITHOUT_RETURN(s_ota_data_ptr, length);
        os_memcpy(s_ota_data_ptr, data, length);
        msg.param = (uint32_t)(s_ota_data_ptr);
        boarding_send_msg(&msg);
    }
    break;

    case BOARDING_OP_OTA_DO_DOWNLOADING:
    {
        boarding_msg_t msg;
        msg.event = DBEVT_OTA_DO_DOWNLOADING;
        msg.length = length;

        OTA_MALLOC_WITHOUT_RETURN(s_ota_data_ptr, length);
        os_memcpy(s_ota_data_ptr, data, length);
        msg.param = (uint32_t)(s_ota_data_ptr);
        boarding_send_msg(&msg);

    }
    break;

    case BOARDING_OP_OTA_COMPLETE_DOWNLOAD:
    {
        boarding_msg_t msg;
        msg.event = DBEVT_OTA_COMPLETE_DOWNLOAD;
        msg.length = length;

        OTA_MALLOC_WITHOUT_RETURN(s_ota_data_ptr, length);
        os_memcpy(s_ota_data_ptr, data, length);
        msg.param = (uint32_t)(s_ota_data_ptr);
        boarding_send_msg(&msg);
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

            case DBEVT_OTA_START_DOWNLOAD:
            {
                wboard_logw("start ota dl!\r\n");
                OTA_MALLOC_WITHOUT_RETURN(s_ble_ota, sizeof(f_ota_t));
                OTA_MALLOC_WITHOUT_RETURN(s_ble_ota->magic_code, OTA_START_MAGIC_LENGTH);
                if(msg.length == (OTA_START_MAGIC_LENGTH + OTA_STORE_ENTIRE_IMAGE_SIZE))
                {
                    os_memcpy(s_ble_ota->magic_code, (uint8_t *)(msg.param) , OTA_START_MAGIC_LENGTH);
                    if(os_memcmp(s_ble_ota->magic_code, OTA_START_MAGIC, OTA_START_MAGIC_LENGTH) == 0)
                    {
                        os_memcpy(&(s_ble_ota->image_size), (uint8_t *)(msg.param + OTA_START_MAGIC_LENGTH) , OTA_STORE_ENTIRE_IMAGE_SIZE);
                        wboard_logw("ota_image_size :0x%x!\r\n", s_ble_ota->image_size);
                        if(f_ota_fun_ptr->init(s_ble_ota) == BK_OK)
                        {
                            bk_boarding_event_notify(BOARDING_OP_OTA_START_DOWNLOAD, F_OTA_COMM_OK);
                        }
                        else
                        {
                            bk_boarding_event_notify(BOARDING_OP_OTA_START_DOWNLOAD, F_OTA_COMM_DATA_ERROR);
                        }
                    }
                    else
                    {
                        wboard_loge("magic is error!\r\n");
                        bk_boarding_event_notify(BOARDING_OP_OTA_START_DOWNLOAD, F_OTA_START_MAGIC_ERROR);
                    }
                }
                else
                {
                    wboard_loge("length is error!\r\n");
                    bk_boarding_event_notify(BOARDING_OP_OTA_START_DOWNLOAD, F_OTA_COMM_LENGTH_ERROR);
                }
                wboard_logi("s_ble_ota->sequence_number :%d !\r\n",s_ble_ota->curr_sequence_number);
                OTA_FREE(s_ble_ota->magic_code);
                OTA_FREE(s_ota_data_ptr);
            }
            break;

            case DBEVT_OTA_DO_DOWNLOADING:
            {
                wboard_logi("start ota dling!\r\n");
                if(msg.length > 0)
                {
                    os_memcpy(&(s_ble_ota->new_sequence_number), (uint8_t *)msg.param, OTA_SEQUENCE_NUMBER);

                    os_memcpy(s_ble_ota->wr_tmp_buf, (uint8_t *)(msg.param + OTA_SEQUENCE_NUMBER), (msg.length - OTA_SEQUENCE_NUMBER));

                    if(f_ota_fun_ptr->data_process(s_ble_ota, (msg.length - OTA_SEQUENCE_NUMBER)) == BK_OK)
                    {
                        bk_boarding_event_notify_with_data(BOARDING_OP_OTA_DO_DOWNLOADING, F_OTA_COMM_OK, (char*)&s_ble_ota->curr_sequence_number, msg.length);
                    }
                    else
                    {
                        bk_boarding_event_notify_with_data(BOARDING_OP_OTA_DO_DOWNLOADING, F_OTA_COMM_DATA_ERROR, (char*)&s_ble_ota->new_sequence_number, msg.length);
                    }
                }
                else
                {
                    wboard_loge("dling length is error !\r\n");
                    bk_boarding_event_notify_with_data(BOARDING_OP_OTA_DO_DOWNLOADING, F_OTA_COMM_LENGTH_ERROR, (char*)&s_ble_ota->new_sequence_number, msg.length);
                }

                OTA_FREE(s_ota_data_ptr);
            }
            break;

            case DBEVT_OTA_COMPLETE_DOWNLOAD:
            {
                wboard_logw("complete ota dled!\r\n");
                uint32_t in_crc = 0;

                OTA_MALLOC_WITHOUT_RETURN(s_ble_ota->magic_code, OTA_COMPLETE_MAGIC_LENGTH);
                if(msg.length == (OTA_COMPLETE_MAGIC_LENGTH + OTA_CHECK_CRC_LENGTH))
                {
                    os_memcpy(s_ble_ota->magic_code, (uint8_t *)(msg.param) , OTA_COMPLETE_MAGIC_LENGTH);
                    os_memcpy(&in_crc, (uint8_t *)(msg.param + OTA_COMPLETE_MAGIC_LENGTH) , OTA_CHECK_CRC_LENGTH);
                    if(os_memcmp(s_ble_ota->magic_code, OTA_COMPLETE_MAGIC, OTA_COMPLETE_MAGIC_LENGTH) == 0)
                    {
                        if(f_ota_fun_ptr->crc(s_ble_ota, in_crc) != BK_OK)
                        {
                            bk_boarding_event_notify(BOARDING_OP_OTA_COMPLETE_DOWNLOAD, F_OTA_COMM_CRC_ERROR);
                            break;
                        }
                        OTA_FREE(s_ble_ota->magic_code);
                        f_ota_fun_ptr->deinit(s_ble_ota);
                        OTA_FREE(s_ota_data_ptr);
                        bk_boarding_event_notify(BOARDING_OP_OTA_COMPLETE_DOWNLOAD, F_OTA_COMM_OK);
                        wboard_logw("ota success !\r\n");
                        bk_reboot();   //success need do reboot.
                    }
                    else
                    {
                        wboard_loge("finish magic is error !\r\n");
                        bk_boarding_event_notify(BOARDING_OP_OTA_COMPLETE_DOWNLOAD, F_OTA_START_MAGIC_ERROR);
                    }
                }
                else
                {
                    wboard_loge("length is error !\r\n");
                    bk_boarding_event_notify(BOARDING_OP_OTA_COMPLETE_DOWNLOAD, F_OTA_COMM_LENGTH_ERROR);
                }
                OTA_FREE(s_ble_ota->magic_code);
                OTA_FREE(s_ota_data_ptr);
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
