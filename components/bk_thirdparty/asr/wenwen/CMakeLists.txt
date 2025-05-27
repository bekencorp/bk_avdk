set(incs)

if (CONFIG_WENWEN_ASR)

#armino_build_get_property(armino_soc ARMINO_SOC)

if ("${ARMINO_SOC}" STREQUAL "bk7236" OR "${ARMINO_SOC}" STREQUAL "bk7258" OR "${ARMINO_SOC}" STREQUAL "bk7258_cp1")
    list(APPEND incs
				include
                bk7258
    )
endif()

endif()

armino_component_register(INCLUDE_DIRS "${incs}")

if (CONFIG_WENWEN_ASR)

if ("${ARMINO_SOC}" STREQUAL "bk7236" OR "${ARMINO_SOC}" STREQUAL "bk7258" OR "${ARMINO_SOC}" STREQUAL "bk7258_cp1")

if (CONFIG_WENWEN_ASR_VERSION_SINGLE_MIC)
	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7258/single_mic/libmobvoidsp.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7258/single_mic/libmobvoidsp.a")
elseif (CONFIG_WENWEN_ASR_VERSION_DUAL_MIC)
	get_filename_component(lib_name ${CMAKE_CURRENT_SOURCE_DIR}/bk7258/dual_mic/libmobvoidsp.a NAME)
	add_prebuilt_library(${lib_name} "${CMAKE_CURRENT_SOURCE_DIR}/bk7258/dual_mic/libmobvoidsp.a")
endif()
	target_link_libraries(${COMPONENT_LIB} INTERFACE ${lib_name})
endif()

endif()
