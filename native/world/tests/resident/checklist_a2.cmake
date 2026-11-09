# A2 命名门复用唯一共用世界二进制；静态合同与实际执行分别失败、分别报告。
foreach(required HANMO_BUILD_DIR HANMO_CHECKLIST_BINARY HANMO_CHECKLIST_PREPARED HANMO_REPO_ROOT HANMO_A2_TEST_DIR)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Required A2 input is missing: ${required}")
    endif()
endforeach()
find_program(HANMO_A2_PYTHON NAMES python3)
if(NOT HANMO_A2_PYTHON)
    message(FATAL_ERROR "Python 3 is required for the finite A2 source contract")
endif()
if(NOT EXISTS "${HANMO_CHECKLIST_PREPARED}")
    message(FATAL_ERROR "A2 must use the original common prepared fixture: ${HANMO_CHECKLIST_PREPARED}")
endif()
execute_process(COMMAND "${HANMO_A2_PYTHON}"
    "${HANMO_A2_TEST_DIR}/checklist_a2_registry.py" "${HANMO_REPO_ROOT}"
    RESULT_VARIABLE checked)
if(NOT checked EQUAL 0)
    message(FATAL_ERROR "A2 finite 51-entry source/registry contract failed: ${checked}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${HANMO_BUILD_DIR}"
    --target test_resident_checklist_shared_world --parallel 1
    RESULT_VARIABLE built)
if(NOT built EQUAL 0)
    message(FATAL_ERROR "A2 existing shared-world consumer build failed: ${built}")
endif()
execute_process(COMMAND "${HANMO_CHECKLIST_BINARY}" a2
    "${HANMO_CHECKLIST_PREPARED}" "${HANMO_REPO_ROOT}" 0
    RESULT_VARIABLE executed)
if(NOT executed EQUAL 0)
    message(FATAL_ERROR "A2 real shared-world public rejection contracts failed: ${executed}")
endif()
message(STATUS "A2: finite 51 source contracts plus separately reported public runtime subset")
