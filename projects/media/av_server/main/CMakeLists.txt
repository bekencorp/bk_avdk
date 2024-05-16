set(incs .
    include)

set(srcs
    av_server_main.c
    )

if (CONFIG_SYS_CPU0)

    list(APPEND srcs
        src/av_audio_common.c
    )

if (CONFIG_AV_DEMO_MODE_TCP)
    list(APPEND srcs
        src/dual_device_transmission_tcp_server.c
    )
else()
    list(APPEND srcs
        src/dual_device_transmission_udp_server.c
    )
endif()
endif()#if (CONFIG_SYS_CPU0)

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}" PRIV_REQUIRES bk_init lwip_intf_v2_1 aud_intf media media_service wifi_transfer)
