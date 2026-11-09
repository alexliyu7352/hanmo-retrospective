# 单行命名门仍构建并运行同一个真实共用世界消费者，不以旧结果或空循环代替。
foreach(required HANMO_BUILD_DIR HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED HANMO_CHECKLIST_ROW)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Required common-world input is missing: ${required}")
    endif()
endforeach()
if(NOT EXISTS "${HANMO_CHECKLIST_PREPARED}")
    message(FATAL_ERROR "Original common prepared fixture is missing")
endif()
# 当前行唯一 F 固定其原字节，不允许改名夹具或逐行改值偷偷取得通过。
if(HANMO_CHECKLIST_ROW STREQUAL "K1-1" OR HANMO_CHECKLIST_ROW STREQUAL "K1-2" OR HANMO_CHECKLIST_ROW STREQUAL "ECO-6")
    file(SHA256 "${HANMO_CHECKLIST_PREPARED}" actual_fixture_sha256)
    if(NOT actual_fixture_sha256 STREQUAL "2840aad48f2993ce55634df727e8b15e8ace8076b47e9deeb139cb0debc10677")
        message(FATAL_ERROR "${HANMO_CHECKLIST_ROW} requires the selected unchanged common fixture F")
    endif()
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${HANMO_BUILD_DIR}"
    --target test_resident_checklist_shared_world --parallel 1 RESULT_VARIABLE built)
if(NOT built EQUAL 0)
    message(FATAL_ERROR "Common world consumer build failed: ${built}")
endif()
set(extra_args)
if(DEFINED HANMO_CHECKLIST_TERRAIN_ROOT AND NOT HANMO_CHECKLIST_TERRAIN_ROOT STREQUAL "")
    list(APPEND extra_args --terrain-root "${HANMO_CHECKLIST_TERRAIN_ROOT}")
endif()
set(output "${HANMO_BUILD_DIR}/native-checklist/${HANMO_CHECKLIST_ROW}")
execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" "${HANMO_CHECKLIST_ROW}"
    "${HANMO_CHECKLIST_PREPARED}" "${output}" 0 ${extra_args}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${HANMO_CHECKLIST_ROW} real shared-world contract failed: ${result}")
endif()
# K1-1 的第二进程必须真正从磁盘恢复；生产进程失败时不得读取旧输出冒成功。
if(HANMO_CHECKLIST_ROW STREQUAL "K1-1")
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" K1-1-CONSUME
        "${HANMO_CHECKLIST_PREPARED}" "${output}" 0 ${extra_args}
        RESULT_VARIABLE cold_result)
    if(NOT cold_result EQUAL 0)
        message(FATAL_ERROR "K1-1 original cold consumer failed: ${cold_result}")
    endif()
endif()

# K5 只在本次真实生产成功后启动独立冷消费者，失败不得借用历史输出。
if(HANMO_CHECKLIST_ROW STREQUAL "K5-1")
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" K5-1-CONSUME
        "${HANMO_CHECKLIST_PREPARED}" "${output}" 0 ${extra_args}
        RESULT_VARIABLE k5_cold_result)
    if(NOT k5_cold_result EQUAL 0)
        message(FATAL_ERROR "K5-1 original cold consumer failed: ${k5_cold_result}")
    endif()
endif()

# ECO-6 真实生产成功后才启动第二冷进程，沿唯一 F 和原生命周期复核完整续行。
if(HANMO_CHECKLIST_ROW STREQUAL "ECO-6")
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" ECO-6-CONSUME
        "${HANMO_CHECKLIST_PREPARED}" "${output}" 0 ${extra_args}
        RESULT_VARIABLE eco_cold_result)
    if(NOT eco_cold_result EQUAL 0)
        message(FATAL_ERROR "ECO-6 original cold consumer failed: ${eco_cold_result}")
    endif()
endif()

# K1-2 仅在本次生产者完整成功后，另启进程从原已付款银行恢复并继续。
if(HANMO_CHECKLIST_ROW STREQUAL "K1-2")
    execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" K1-2-CONSUME
        "${HANMO_CHECKLIST_PREPARED}" "${output}" 0 ${extra_args}
        RESULT_VARIABLE k1_retry_cold_result)
    if(NOT k1_retry_cold_result EQUAL 0)
        message(FATAL_ERROR "K1-2 original cold consumer failed: ${k1_retry_cold_result}")
    endif()
endif()
