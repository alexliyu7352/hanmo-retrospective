# 本片只构建同一C++17只读库与CLI；依赖必须本地已取得并核实，不自动联网。
cmake_minimum_required(VERSION 3.20)
project(hanmo_inventory_readonly LANGUAGES C CXX)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/optional_toolchain.cmake")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(HANMO_FLECS_ROOT "" CACHE PATH "已核Flecs固定commit源码路径")
set(HANMO_JSON_ROOT "" CACHE PATH "已核nlohmann/json固定commit源码路径")
set(HANMO_PICOSHA2_ROOT "" CACHE PATH "已核PicoSHA2固定commit源码路径")
set(HANMO_LOCK "${CMAKE_CURRENT_SOURCE_DIR}/../dependencies.lock.json")
file(READ "${HANMO_LOCK}" LOCK_TEXT)
file(SHA256 "${HANMO_LOCK}" HANMO_LOCK_SHA256)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HANMO_LOCK}")
foreach(lib flecs json picosha2)
    string(JSON verified GET "${LOCK_TEXT}" libraries ${lib} acquisition_state)
    if(NOT verified STREQUAL "verified")
        message(FATAL_ERROR "依赖${lib}未取得或未核摘要；不得自动下载或宣称可编译")
    endif()
    string(TOUPPER "${lib}" variable)
    set(root "${HANMO_${variable}_ROOT}")
    if(NOT IS_DIRECTORY "${root}")
        message(FATAL_ERROR "必须显式指定已核${lib}本地目录")
    endif()
    if(lib STREQUAL "flecs")
        set(required_files distr/flecs.c distr/flecs.h LICENSE)
    elseif(lib STREQUAL "json")
        set(required_files single_include/nlohmann/json.hpp LICENSE.MIT)
    else()
        set(required_files picosha2.h LICENSE)
    endif()
    foreach(required IN LISTS required_files)
        string(JSON required_sha GET "${LOCK_TEXT}" libraries ${lib} files "${required}")
        if(NOT required_sha MATCHES "^[0-9a-f]+$")
            message(FATAL_ERROR "${lib}/${required}缺实际编译文件或许可证摘要")
        endif()
    endforeach()
    string(JSON size LENGTH "${LOCK_TEXT}" libraries ${lib} files)
    if(size LESS 1)
        message(FATAL_ERROR "${lib}锁缺实际文件摘要")
    endif()
    math(EXPR last "${size}-1")
    foreach(index RANGE ${last})
        string(JSON name MEMBER "${LOCK_TEXT}" libraries ${lib} files ${index})
        string(JSON expected GET "${LOCK_TEXT}" libraries ${lib} files "${name}")
        file(SHA256 "${root}/${name}" actual)
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR "依赖字节不符：${lib}/${name}")
        endif()
    endforeach()
endforeach()

find_package(Threads REQUIRED)
add_library(hanmo_flecs STATIC "${HANMO_FLECS_ROOT}/distr/flecs.c")
target_include_directories(hanmo_flecs PUBLIC "${HANMO_FLECS_ROOT}/distr")
target_link_libraries(hanmo_flecs PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX)
    target_link_libraries(hanmo_flecs PUBLIC m)
endif()
add_library(hanmo_world STATIC src/wire.cpp src/typed_wire.cpp src/inventory_core.cpp src/legacy_projection_codec.cpp src/legacy_projection_store.cpp src/legacy_projection_views.cpp src/actor_commitments_result.cpp src/inventory_projection.cpp src/travel_schedule.cpp src/military_schedule.cpp)
target_include_directories(hanmo_world PUBLIC include PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(hanmo_world PRIVATE hanmo_flecs)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(hanmo_world PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 官方源码归档没有 .git；显式归档构建不伪造提交身份，真实文件仍参加下方源摘要。
option(HANMO_SOURCE_ARCHIVE "显式允许从无Git历史的源码归档构建；不声明提交身份" OFF)
if(HANMO_SOURCE_ARCHIVE)
    set(HANMO_SOURCE_COMMIT "source-archive-uncommitted")
    message(STATUS "Source archive build: no Git commit identity is claimed")
else()
    execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE HANMO_SOURCE_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
endif()
set(source_identity "")
foreach(path CMakeLists.txt include/hanmo/inventory_projection.hpp include/hanmo/projection_types.hpp include/hanmo/legacy_projection.hpp include/hanmo/travel_schedule.hpp src/travel_schedule.cpp include/hanmo/military_schedule.hpp src/military_schedule.cpp src/wire.hpp src/wire.cpp src/typed_wire.hpp src/typed_wire.cpp src/inventory_core.hpp src/inventory_core.cpp src/legacy_projection_codec.hpp src/legacy_projection_codec.cpp src/legacy_projection_store_internal.hpp src/legacy_projection_store.cpp src/legacy_projection_views.cpp src/actor_commitments_result.hpp src/actor_commitments_result.cpp src/inventory_projection.cpp tools/sim_main.cpp src/build_identity.hpp.in)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
    file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${path}" sha)
    string(APPEND source_identity "${path}:${sha}\n")
endforeach()
string(SHA256 HANMO_SOURCE_SHA256 "${source_identity}")
configure_file(src/build_identity.hpp.in generated/build_identity.hpp @ONLY)
add_executable(hanmo_sim tools/sim_main.cpp)
target_include_directories(hanmo_sim PRIVATE src "${HANMO_JSON_ROOT}/single_include" "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(hanmo_sim PRIVATE hanmo_world)

enable_testing()
add_executable(test_inventory_projection tests/test_inventory_projection.cpp)
target_include_directories(test_inventory_projection PRIVATE "${HANMO_JSON_ROOT}/single_include")
target_link_libraries(test_inventory_projection PRIVATE hanmo_world)
add_test(NAME native_inventory_contract COMMAND test_inventory_projection)

# 显式选择的隔离Linux桥复用以上唯一库；默认构建不会查找或加载Godot SDK。
option(HANMO_BUILD_RESIDENT_JOURNEY "构建常驻行旅P1开发组件与CLI" OFF)
option(HANMO_BUILD_GODOT_BRIDGE "构建隔离Linux只读Godot桥" OFF)
if(HANMO_BUILD_GODOT_BRIDGE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../godot" "${CMAKE_CURRENT_BINARY_DIR}/godot-bridge")
endif()

# 独立typed fixture替身只链接这两个测试；生产库始终链接真实decoder。
foreach(target test_legacy_projection_store test_actor_commitments)
    add_executable(${target} tests/${target}.cpp src/wire.cpp src/typed_wire.cpp src/inventory_core.cpp src/legacy_projection_store.cpp src/legacy_projection_views.cpp)
    target_include_directories(${target} PRIVATE include src "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
    target_compile_definitions(${target} PRIVATE HANMO_TYPED_FIXTURE_DECODER=1)
    target_link_libraries(${target} PRIVATE hanmo_flecs)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endforeach()

# 真实新profile解码门使用独立golden，不链接C typed测试桩。
add_executable(test_legacy_projection_codec tests/test_legacy_projection_codec.cpp)
target_include_directories(test_legacy_projection_codec PRIVATE src "${HANMO_JSON_ROOT}/single_include")
target_link_libraries(test_legacy_projection_codec PRIVATE hanmo_world)
add_test(NAME native_legacy_codec_contract COMMAND test_legacy_projection_codec "${CMAKE_CURRENT_SOURCE_DIR}/tests/legacy_projection_codec/legacy_golden.json")

# 旅行提案只经同一库的纯函数，专测不安装另一份世界或复制旧查询实现。
add_executable(test_travel_schedule tests/test_travel_schedule.cpp)
target_include_directories(test_travel_schedule PRIVATE "${HANMO_JSON_ROOT}/single_include")
target_link_libraries(test_travel_schedule PRIVATE hanmo_world)
add_test(NAME native_travel_schedule_contract COMMAND test_travel_schedule)

# 军政提案由同一纯函数库执行，独立测试保原分支和作者顺序。
add_executable(test_military_schedule tests/test_military_schedule.cpp)
target_include_directories(test_military_schedule PRIVATE "${HANMO_JSON_ROOT}/single_include")
target_link_libraries(test_military_schedule PRIVATE hanmo_world)
add_test(NAME native_military_schedule_contract COMMAND test_military_schedule)

# P1只在明确开发构建中启用；旧离线库、原Godot profile和正式游戏默认不接此实验。
if(HANMO_BUILD_RESIDENT_JOURNEY)
    include(resident.cmake)
    # 原生层完整编译入口覆盖规则核心、独立存储库、纯原生桥队列及两个真实CLI。
    # Godot SDK/UI不属于本轮C++原生层；本目标不运行测试或放宽领域失败门。
    add_custom_target(hanmo_native_complete
        DEPENDS
            hanmo_resident_journey
            hanmo_sim
            hanmo_resident_bridge_native
    )
    if(TARGET hanmo_resident_storage)
        add_dependencies(hanmo_native_complete hanmo_resident_storage)
    endif()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/native_checklist.cmake")
endif()

# 两个显式选项同时开启才注册可写常驻桥；默认旧只读目标不链接或创建resident世界。
if(HANMO_BUILD_GODOT_BRIDGE AND HANMO_BUILD_RESIDENT_JOURNEY)
    target_sources(hanmo_world_bridge PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../godot/resident_bridge_runtime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../godot/resident_bridge_live_jobs.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../godot/resident_cold_value.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../godot/resident_installed_catalog.cpp")
    target_include_directories(hanmo_world_bridge PRIVATE "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(hanmo_world_bridge PRIVATE hanmo_resident)
    target_compile_definitions(hanmo_world_bridge PRIVATE HANMO_RESIDENT_CORE_LINKED=1)
    # 单独绑定真实resident输入；旧HanmoWorldBridge的只读身份不能充当常驻core身份。
    set(resident_bridge_identity "enabled=ON\ncore=${HANMO_RESIDENT_SOURCE_SHA}\n")
    foreach(path resident_bridge_runtime.cpp resident_bridge_runtime.hpp resident_bridge_live_jobs.cpp resident_bridge_live_jobs.hpp resident_cold_value.cpp resident_cold_value.hpp resident_installed_catalog.cpp resident_installed_catalog.hpp build_profile_resident.json register_types.cpp)
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/../godot/${path}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
        file(SHA256 "${input}" sha)
        string(APPEND resident_bridge_identity "${path}:${sha}\n")
    endforeach()
    # 原装载入口及本次同snapshot传输实现属于真实生产身份，不只哈希一个native头。
    foreach(path core/content_catalog.gd core/installed_content.gd core/installed_artifact_snapshot.gd core/runtime/capability_contract.gd)
        set(input "${CMAKE_CURRENT_SOURCE_DIR}/../../game/${path}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${input}")
        file(SHA256 "${input}" sha)
        string(APPEND resident_bridge_identity "game/${path}:${sha}\n")
    endforeach()
    string(SHA256 HANMO_RESIDENT_BRIDGE_SOURCE_SHA "${resident_bridge_identity}")
    if(HANMO_RESIDENT_TEST_FAULTS)
        set(resident_bridge_faults true)
    else()
        set(resident_bridge_faults false)
    endif()
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/godot-bridge/resident_bridge_identity.json"
        "{\"format\":\"HM-RESIDENT-BRIDGE-BUILD/1\",\"resident_enabled\":true,\"source_commit\":\"${HANMO_SOURCE_COMMIT}\",\"source_sha256\":\"${HANMO_RESIDENT_BRIDGE_SOURCE_SHA}\",\"resident_core_source_sha256\":\"${HANMO_RESIDENT_SOURCE_SHA}\",\"dependency_lock_sha256\":\"${HANMO_LOCK_SHA256}\",\"test_faults\":${resident_bridge_faults},\"compiler\":\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\",\"configuration\":\"${CMAKE_BUILD_TYPE}\"}\n")
elseif(HANMO_BUILD_GODOT_BRIDGE)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/godot-bridge/resident_bridge_identity.json"
        "{\"format\":\"HM-RESIDENT-BRIDGE-BUILD/1\",\"resident_enabled\":false,\"source_commit\":\"${HANMO_SOURCE_COMMIT}\"}\n")
endif()

# 新运行加速领域：所有实际targets注册后显式配置；默认OFF不改历史构建。
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/resident/build_acceleration.cmake")
