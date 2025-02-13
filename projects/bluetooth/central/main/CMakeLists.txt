function(set_local_library_compile_option input_target)
    set(local_compile_option )

    set(TMP_WARNING_LIST)
    list(APPEND TMP_WARNING_LIST
                unused-but-set-variable
                unused-variable
                unused-function
                unused-but-set-variable
                unused-label
                #uninitialized
    )

    foreach(tmp_loopvar IN LISTS TMP_WARNING_LIST)
        list(APPEND local_compile_option -Wno-error=${tmp_loopvar} -Wno-${tmp_loopvar})
    endforeach()

    target_compile_options(${input_target} PRIVATE ${local_compile_option})
endfunction()



set(incs .
    )
set(srcs
    armino_main.c
    )

set(priv_req_list )
if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

if(CONFIG_SYS_CPU0)
    list(APPEND incs
        a2dp_source
    )

    list(APPEND srcs
        a2dp_source/ring_buffer_particle.c
        a2dp_source/a2dp_source_demo.c
        a2dp_source/a2dp_source_demo_cli.c
        a2dp_source/a2dp_source_demo_avrcp.c
    )

    if(CONFIG_BLE)
        list(APPEND incs
            gatt
			storage
			hogpd
			wifi_boarding
        )

        list(APPEND srcs
            gatt/dm_gatt.c
            gatt/dm_gattc.c
            gatt/dm_gatts.c
            gatt/dm_gatt_cli.c
            gatt/dm_gatt_connection.c
            gatt/dm_gap_utils.c
            hogpd/hogpd_demo.c
            hogpd/hogpd_demo_cli.c
            wifi_boarding/wifi_boarding_demo.c
            wifi_boarding/wifi_boarding_demo_cli.c
        )

    endif()

    if (CONFIG_BLE OR CONFIG_BT)
        list(APPEND srcs
            storage/bluetooth_storage.c
        )
    endif()

    #message(FATAL_ERROR "main")
endif()

if (("${ARMINO_SOC}" STREQUAL "bk7258") OR ("${ARMINO_SOC}" STREQUAL "bk7258_cp1"))
    list(APPEND priv_req_list multimedia media_service)
endif()

list(APPEND priv_req_list bk_init bk_cli)

armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES "${priv_req_list}"
)

get_filename_component(config_component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
armino_component_get_target(target "${config_component_name}")
set_local_library_compile_option(${target})
