set(incs)

if (CONFIG_WANSON_ASR)

#armino_build_get_property(armino_soc ARMINO_SOC)

if ("${ARMINO_SOC}" STREQUAL "bk7256" OR "${ARMINO_SOC}" STREQUAL "bk7256_cp1")
	string(FIND ${CONFIG_TOOLCHAIN_PATH} "nds32le-elf-mculib-v5f" compiler_type)

	if (${compiler_type} STREQUAL "-1")
		set(WANSON_ASR_LIB_DIR_NAME "nds32le-elf-mculib-v5")
	else()
		set(WANSON_ASR_LIB_DIR_NAME "nds32le-elf-mculib-v5f")
	endif()

	list(APPEND incs
				include
				bk7256/${WANSON_ASR_LIB_DIR_NAME}
	)
endif()

if ("${ARMINO_SOC}" STREQUAL "bk7236" OR "${ARMINO_SOC}" STREQUAL "bk7258" OR "${ARMINO_SOC}" STREQUAL "bk7258_cp1" OR "${ARMINO_SOC}" STREQUAL "bk7258_cp2")
    list(APPEND incs
				include
                bk7258
    )

if (CONFIG_WANSON_ASR_WORDS_VERSION_V1)
    list(APPEND incs
				bk7258/words_version_v1
    )
endif()

if (CONFIG_WANSON_ASR_WORDS_VERSION_V2)
    list(APPEND incs
				bk7258/words_version_v2
    )
endif()

if (CONFIG_WANSON_ASR_WORDS_VERSION_V3)
    list(APPEND incs
				bk7258/words_version_v3
    )
endif()

endif()

endif()	#(CONFIG_WANSON_ASR)

armino_component_register(INCLUDE_DIRS "${incs}")

if (CONFIG_WANSON_ASR)

if ("${ARMINO_SOC}" STREQUAL "bk7256" OR "${ARMINO_SOC}" STREQUAL "bk7256_cp1")
	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7256/${WANSON_ASR_LIB_DIR_NAME}/libasrbase.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7256/${WANSON_ASR_LIB_DIR_NAME}/libasrbase.a")
	target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib_name})
endif()

if ("${ARMINO_SOC}" STREQUAL "bk7236" OR "${ARMINO_SOC}" STREQUAL "bk7258" OR "${ARMINO_SOC}" STREQUAL "bk7258_cp1" OR "${ARMINO_SOC}" STREQUAL "bk7258_cp2")
if (CONFIG_WANSON_ASR_WORDS_VERSION_V3)
	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libasrbase_v3.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libasrbase_v3.a")
else()
	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libasrbase.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7258/libasrbase.a")
endif()
	target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib_name})
endif()

endif()
