# 原生完成清单的骨架门：真实构建完整生产目标，不以旧日志或目标存在代替通过。
add_test(NAME checklist_A1
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}"
        --target hanmo_native_complete --parallel 1)
set_tests_properties(checklist_A1 PROPERTIES
    LABELS "native_checklist;skeleton"
    FIXTURES_SETUP native_checklist_complete_build
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 拆分门同时要求同一工作树的 A1 通过；只运行 A3 时 CTest 也会补跑所需构建。
add_test(NAME checklist_A3
    COMMAND "${CMAKE_COMMAND}" "-DHANMO_NATIVE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_split_files.cmake")
set_tests_properties(checklist_A3 PROPERTIES
    LABELS "native_checklist;skeleton"
    FIXTURES_REQUIRED native_checklist_complete_build
    DEPENDS checklist_A1
    TIMEOUT 60)

# 所有业务清单共用同一生成输出；不得悄悄降级为另一份逐行小夹具。
get_filename_component(HANMO_CHECKLIST_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(HANMO_CHECKLIST_PREPARED "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/fixtures/exact_rest_only_catalog.json")
set(HANMO_CHECKLIST_SCENARIO "complete_market::scenario.complete_market")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/checklist_a4.cmake")
add_executable(test_resident_checklist_shared_world tests/resident/checklist_shared_world.cpp)
target_include_directories(test_resident_checklist_shared_world PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_checklist_shared_world PRIVATE hanmo_resident hanmo_resident_storage)
target_compile_options(test_resident_checklist_shared_world PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
foreach(test_name native_shared_world_roundtrip)
    set(days 0)
    add_test(NAME ${test_name}
        COMMAND "${CMAKE_COMMAND}"
            "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
            "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
            "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/${test_name}"
            "-DHANMO_CHECKLIST_DAYS=${days}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_shared_world.cmake")
    set_tests_properties(${test_name} PROPERTIES
        LABELS "native_checklist;shared_world"
        RESOURCE_LOCK native_checklist_build
        TIMEOUT 7200)
endforeach()

# 先核对全部有限登记合同，再执行同一共用世界上合法可达的公开失败出口。
add_test(NAME checklist_A2
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_REPO_ROOT=${HANMO_CHECKLIST_REPO_ROOT}"
        "-DHANMO_A2_TEST_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/resident"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_a2.cmake")
set_tests_properties(checklist_A2 PROPERTIES
    LABELS "native_checklist;skeleton;shared_world"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 完整存读的直接核心依赖：真实领域提交、原缓存及跨域晚失败。
foreach(row K1-1 K1-2)
    string(REPLACE "-" "_" test_id "${row}")
    add_test(NAME checklist_${test_id}
        COMMAND "${CMAKE_COMMAND}"
            "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
            "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
            "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
            "-DHANMO_CHECKLIST_ROW=${row}"
            "-DHANMO_CHECKLIST_TERRAIN_ROOT=${HANMO_CHECKLIST_TERRAIN_ROOT}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
    set_tests_properties(checklist_${test_id} PROPERTIES
        LABELS "native_checklist;shared_world;core_save_dependency"
        RESOURCE_LOCK native_checklist_build
        TIMEOUT 7200)
endforeach()

# 双腿交易复用唯一 F 与原宿主；单行驱动必须再启动真正独立的冷消费者。
add_test(NAME checklist_K5_1
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_ROW=K5-1"
        "-DHANMO_CHECKLIST_TERRAIN_ROOT=${HANMO_CHECKLIST_TERRAIN_ROOT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(checklist_K5_1 PROPERTIES
    LABELS "native_checklist;shared_world;resources;trade"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# ECO-6 原报价/成交行只用同一共享宿主；不引入旧独立目标或逐行夹具。
add_test(NAME checklist_ECO_6
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_ROW=ECO-6"
        "-DHANMO_CHECKLIST_TERRAIN_ROOT=${HANMO_CHECKLIST_TERRAIN_ROOT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(checklist_ECO_6 PROPERTIES
    LABELS "native_checklist;shared_world;economy;trade"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# K9 前置的辅助门，不冒充原 79 行的命名验收。
add_test(NAME native_checkpoint_cache_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=CACHE
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_checkpoint_cache_boundary PROPERTIES
    LABELS "native_dependency;shared_world;checkpoint_cache"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 原受影响组件测试的生成副本只改入口名；原源码和夹具不变，避免复制大型可执行文件。
function(hanmo_checklist_original_component source entry)
    set(original "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/${source}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${original}")
    file(READ "${original}" component_source)
    string(REGEX MATCHALL "int main\\(" component_entries "${component_source}")
    list(LENGTH component_entries entry_count)
    if(NOT entry_count EQUAL 1)
        message(FATAL_ERROR "Original component must have exactly one main: ${source}")
    endif()
    string(REPLACE "int main(" "int ${entry}(" component_source "${component_source}")
    set(generated "${CMAKE_BINARY_DIR}/native-checklist/components/${entry}.cpp")
    file(CONFIGURE OUTPUT "${generated}" CONTENT "${component_source}" @ONLY)
    target_sources(test_resident_checklist_shared_world PRIVATE "${generated}")
endfunction()
target_include_directories(test_resident_checklist_shared_world PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident")
hanmo_checklist_original_component(travel_route_cache.cpp checklist_original_route_cache)
hanmo_checklist_original_component(travel_cache_catalog.cpp checklist_original_travel_catalog)
hanmo_checklist_original_component(military_field_terrain_test.cpp checklist_original_battle_terrain)
add_test(NAME native_checkpoint_map_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=COMPONENTS
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_checkpoint_map_components PROPERTIES
    LABELS "native_dependency;checkpoint_map;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 当前正式槽/2：真实 worker 保存、独立冷进程载入、坏包/错版本与原槽保留。
add_test(NAME checklist_K9_2
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_slot2.cmake")
set_tests_properties(checklist_K9_2 PROPERTIES
    LABELS "native_checklist;shared_world;storage"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 同一真实 native session 的公共预览接线回归，不计入原 79 行通过数。
add_test(NAME native_public_preview_boundary
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=NATIVE-BOUNDARY
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_public_preview_boundary PROPERTIES
    LABELS "native_dependency;shared_world;public_preview"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 原 K9-4：真实 worker 写盘、旧执行代回调拒绝及冷读继续，仍用唯一共用配置。
add_test(NAME checklist_K9_4
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_k9_4.cmake")
set_tests_properties(checklist_K9_4 PROPERTIES
    LABELS "native_checklist;shared_world;storage;worker"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# K2 直接前序目录组件；使用同一 prepared 的真实负例，不替代 K2 三条命名门。
add_test(NAME native_k2_spatial_catalog_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=K2-SPATIAL
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_k2_spatial_catalog_components PROPERTIES
    LABELS "native_dependency;military_spatial;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# K2 直接前序目录组件；使用同一 prepared 的真实负例，不替代 K2 三条命名门。
add_test(NAME native_k2_session_definition_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=K2-SESSION
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_k2_session_definition_components PROPERTIES
    LABELS "native_dependency;military_session;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 原 FH 冻结输入的强型/codec/拥有图组件，不替代完整来源或任一79行命名门。
add_test(NAME native_frozen_condition_input_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=FROZEN-CONDITION
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_frozen_condition_input_components PROPERTIES
    LABELS "native_dependency;actor_foundation;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 原实船名义引用值/身份组件；不替代运输、Care 或79行命名门。
add_test(NAME native_vessel_reference_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=VESSEL-REFERENCE
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_vessel_reference_components PROPERTIES
    LABELS "native_dependency;naval_identity;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 原字段共用登记组件，不声明完整 canonical 布局或任何 K2 命名门通过。
add_test(NAME native_k2_canonical_layout_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=K2-CANONICAL
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_k2_canonical_layout_components PROPERTIES
    LABELS "native_dependency;military_canonical;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# 原六槽数组的有限组件，借同一 sealed 内容，不注册完整 canonical 世界。
add_test(NAME native_k2_fortification_node_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=K2-FORTIFICATION
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_k2_fortification_node_components PROPERTIES
    LABELS "native_dependency;fortification_nodes;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)

# Family 原值/历法组件沿既有独立 TU 入口改名，不另建 World 或复制可执行文件。
hanmo_checklist_original_component(child_maturation_exact_boundary_test.cpp checklist_family_maturation_boundary)
hanmo_checklist_original_component(care_due_value_storage_test.cpp checklist_family_due_value_storage)
add_test(NAME native_family_due_value_components
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_BUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:test_resident_checklist_shared_world>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        -DHANMO_CHECKLIST_ROW=FAMILY-DUE-COMPONENTS
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/checklist_single_world.cmake")
set_tests_properties(native_family_due_value_components PROPERTIES
    LABELS "native_dependency;family;original_components"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)
