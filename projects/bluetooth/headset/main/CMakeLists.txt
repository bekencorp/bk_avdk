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

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        vendor_flash.c
    )
endif()

if(CONFIG_BLE)
    list(APPEND incs
        ../../central/main/gatt
        ../../central/main/wifi_boarding
        ../../central/main/ble_ota
    )

    list(APPEND srcs
        ../../central/main/gatt/dm_gatt.c
        ../../central/main/gatt/dm_gattc.c
        ../../central/main/gatt/dm_gatts.c
        ../../central/main/gatt/dm_gatt_cli.c
        ../../central/main/gatt/dm_gatt_connection.c
		../../central/main/gatt/dm_gap_utils.c
		../../central/main/hogpd/hogpd_demo.c
		../../central/main/hogpd/hogpd_demo_cli.c
        ../../central/main/wifi_boarding/wifi_boarding_demo.c
        ../../central/main/wifi_boarding/wifi_boarding_demo_service.c
        ../../central/main/wifi_boarding/wifi_boarding_network.c
        ../../central/main/wifi_boarding/wifi_boarding_demo_cli.c
    )

endif()


if (CONFIG_BT)
    list(APPEND incs
        hfp_hf
        a2dp_sink
		../../central/main/a2dp_source/
    )

	list(APPEND srcs
		a2dp_sink/a2dp_sink_demo.c
		a2dp_sink/ring_buffer_node.c
		a2dp_sink/mpeg4_latm_dec.c
		a2dp_sink_demo_cli.c
		hfp_hf/hfp_hf_demo.c
		../../central/main/a2dp_source/ring_buffer_particle.c
        bt_manager.c
		)
endif()

if (CONFIG_BLE OR CONFIG_BT)
    list(APPEND incs
        ../../central/main/storage
    )

	list(APPEND srcs
		../../central/main/storage/bluetooth_storage.c
	)
endif()


armino_component_register(
	SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_init bk_cli media_service multimedia aac_decoder audio_play audio_record lwip_intf_v2_1 ota
)

get_filename_component(config_component_name ${CMAKE_CURRENT_LIST_DIR} NAME)
armino_component_get_target(target "${config_component_name}")
set_local_library_compile_option(${target})
