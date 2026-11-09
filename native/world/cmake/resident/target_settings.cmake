# 常驻核心编译约束：保留原故障探针开关、语言标准、依赖与浮点选项。
# 故障探针必须是显式独立测试构建；默认产品编译没有注入分支。
option(HANMO_RESIDENT_TEST_FAULTS "Enable exact oracle late-failure test probe" OFF)
if(HANMO_RESIDENT_TEST_FAULTS)
    set_source_files_properties(src/resident/kernel/resident_world.cpp src/resident/kernel/boundary_dispatch.cpp
        PROPERTIES COMPILE_DEFINITIONS HANMO_RESIDENT_TEST_FAULTS=1)
endif()
target_include_directories(hanmo_resident PUBLIC include src PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
# 已有领域接口使用 std::span；生产目标及其消费者必须声明真实语言要求。
target_compile_features(hanmo_resident PUBLIC cxx_std_20)
target_link_libraries(hanmo_resident PRIVATE hanmo_world)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hanmo_resident PRIVATE -Wall -Wextra -Wpedantic -fno-fast-math -ffp-contract=off)
endif()

# Linux 原生链只尝试段布局与归档扩充；逐项不支持时回到普通规则，不改优化级别。
set(HANMO_RESIDENT_LINK_SLIMMING OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target hanmo_resident hanmo_world hanmo_flecs)
        hanmo_optional_compile_sections(${target})
    endforeach()
    hanmo_optional_link_option(hanmo_resident INTERFACE "LINKER:--gc-sections"
        HANMO_RESIDENT_GC_SECTIONS_SUPPORTED)
    # bfd 不支持 ICF 开关；只在支持时显式禁用，绝不改为启用 ICF。
    hanmo_optional_link_option(hanmo_resident INTERFACE "LINKER:--icf=none"
        HANMO_RESIDENT_ICF_NONE_SUPPORTED)
    hanmo_optional_thin_archive(hanmo_resident)
    # 此开关只选择同平台宿主的探测路径，不表示每项扩充均受支持。
    set(HANMO_RESIDENT_LINK_SLIMMING ON)
endif()
# 明确列实际编译输入，避免把未提交在途文件扫描入冻结身份。
