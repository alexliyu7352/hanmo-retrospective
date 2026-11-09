# Field 第三门只链接正式 production 库；三进程沿真实 bank/完整摘要重放。
# 输入与输出目录必须由集中验证显式固定；不生成或预写战场初态。
add_executable(test_resident_field_third_gate
    tests/resident/field_third_gate_test.cpp
    tests/resident/field_pc_sampler.cpp
    tests/resident/field_pc_sampler.hpp
    tests/resident/field_initial_settlement_controller_test.cpp
    tests/resident/field_reinforcement_component_test.cpp
    tests/resident/local_move_component_test.cpp
    tests/resident/field_custody_source_component_test.cpp
    tests/resident/field_army_exit_rendezvous_test.cpp)
target_link_libraries(test_resident_field_third_gate PRIVATE hanmo_resident)
target_compile_features(test_resident_field_third_gate PRIVATE cxx_std_20)
target_include_directories(test_resident_field_third_gate PRIVATE src "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_resident_field_third_gate PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
set(HANMO_FIELD_THIRD_GATE_FIXTURE "" CACHE FILEPATH "已冻结青川 Field 守门作者输入")
set(HANMO_FIELD_THIRD_GATE_TERRAIN_ROOT "" CACHE PATH "原青川细地形资源所在 repo 绝对根")
set(HANMO_FIELD_THIRD_GATE_OUTPUT_ROOT "" CACHE PATH "本次三进程原件的全新输出根")
set(HANMO_FIELD_THIRD_GATE_SCENARIO "complete_market::scenario.complete_market" CACHE STRING "显式第三门母场景 ID")
if(HANMO_FIELD_THIRD_GATE_FIXTURE AND HANMO_FIELD_THIRD_GATE_TERRAIN_ROOT AND HANMO_FIELD_THIRD_GATE_OUTPUT_ROOT)
    foreach(path HANMO_FIELD_THIRD_GATE_FIXTURE HANMO_FIELD_THIRD_GATE_TERRAIN_ROOT HANMO_FIELD_THIRD_GATE_OUTPUT_ROOT)
        if(NOT IS_ABSOLUTE "${${path}}")
            message(FATAL_ERROR "${path} 必须是本次显式固定的绝对路径")
        endif()
    endforeach()
    if(NOT EXISTS "${HANMO_FIELD_THIRD_GATE_FIXTURE}" OR NOT IS_DIRECTORY "${HANMO_FIELD_THIRD_GATE_TERRAIN_ROOT}")
        message(FATAL_ERROR "Field 守门必须借已存在的真实 fixture 与地形根")
    endif()
    add_test(NAME resident_field_third_gate_warm COMMAND test_resident_field_third_gate
        warm "${HANMO_FIELD_THIRD_GATE_FIXTURE}" "${HANMO_FIELD_THIRD_GATE_SCENARIO}"
        "${HANMO_FIELD_THIRD_GATE_TERRAIN_ROOT}" "${HANMO_FIELD_THIRD_GATE_OUTPUT_ROOT}/warm")
    set_tests_properties(resident_field_third_gate_warm PROPERTIES FIXTURES_SETUP field_third_gate_original RUN_SERIAL TRUE)
    foreach(mode cold-before-contact cold-after-segment)
        add_test(NAME resident_field_third_gate_${mode} COMMAND test_resident_field_third_gate
            ${mode} "${HANMO_FIELD_THIRD_GATE_FIXTURE}" "${HANMO_FIELD_THIRD_GATE_SCENARIO}"
            "${HANMO_FIELD_THIRD_GATE_TERRAIN_ROOT}" "${HANMO_FIELD_THIRD_GATE_OUTPUT_ROOT}/${mode}"
            "${HANMO_FIELD_THIRD_GATE_OUTPUT_ROOT}/warm")
        set_tests_properties(resident_field_third_gate_${mode} PROPERTIES FIXTURES_REQUIRED field_third_gate_original RUN_SERIAL TRUE)
    endforeach()
endif()
