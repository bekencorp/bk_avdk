#ifndef __AV_CLIENT_TCP_SERVICE_H__
#define __AV_CLIENT_TCP_SERVICE_H__

#include <os/os.h>

#define AV_CLIENT_TCP_BUFFER (1460)
#define AV_CLIENT_TCP_NET_BUFFER (1460)

bk_err_t av_client_tcp_service_init(void);
void av_client_tcp_service_deinit(void);

#endif