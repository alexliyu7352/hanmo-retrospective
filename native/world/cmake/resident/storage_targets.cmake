# 原生存储生产库与独立故障测试支持库：保留 Linux 条件和原测试登记。
# Linux内部槽介质与后台服务无Godot SDK依赖；完整语义核验/桥接另接，不能直接当保存成功API。
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(HANMO_RESIDENT_STORAGE_SOURCES
        ../godot/resident_storage_diagnostic.cpp
        ../godot/resident_profile_admission.cpp
        ../godot/resident_profile_store.cpp
        ../godot/resident_profile_coordinator.cpp
        ../godot/resident_slot_media.cpp
        ../godot/resident_storage_service.cpp
        ../godot/resident_slot_codec.cpp
        ../godot/resident_slot_coordinator.cpp
        ../godot/resident_profile_complete_codec.cpp
        ../godot/resident_profile_copy.cpp
        ../godot/resident_profile_creation.cpp
        ../godot/resident_profile_merge.cpp
        ../godot/resident_profile_transaction.cpp
        ../godot/resident_slot_envelope.cpp
        ../godot/resident_slot_stream_save.cpp
        ../godot/resident_world_import.cpp
    )
    add_library(hanmo_resident_storage STATIC ${HANMO_RESIDENT_STORAGE_SOURCES})
    target_include_directories(hanmo_resident_storage PUBLIC include src ../godot PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(hanmo_resident_storage PUBLIC Threads::Threads PRIVATE hanmo_resident)
    # 故障钩子仅进入独立测试支持库，生产目标始终不定义该开关。
    add_library(hanmo_resident_storage_test_support STATIC ${HANMO_RESIDENT_STORAGE_SOURCES})
    target_include_directories(hanmo_resident_storage_test_support PUBLIC include src ../godot PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(hanmo_resident_storage_test_support PUBLIC Threads::Threads PRIVATE hanmo_resident)
    target_compile_definitions(hanmo_resident_storage_test_support PUBLIC HANMO_STORAGE_TEST_FAULTS=1)
    foreach(name resident_slot_media resident_storage_service)
        add_executable(test_${name} tests/resident/${name}_test.cpp)
        target_link_libraries(test_${name} PRIVATE hanmo_resident_storage_test_support)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror -UNDEBUG)
        endif()
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
endif()

# 原HM-RESIDENT-SAVE物理格式与真实同核消费者；不用测试替身代替保存语义。
if(TARGET hanmo_resident_storage)
    add_executable(test_resident_slot_codec_native tests/resident/resident_slot_codec_native_test.cpp)
    target_include_directories(test_resident_slot_codec_native PRIVATE "${HANMO_JSON_ROOT}/single_include")
    target_link_libraries(test_resident_slot_codec_native PRIVATE hanmo_resident_storage)
    add_test(NAME resident_slot_codec_native COMMAND test_resident_slot_codec_native)
    add_executable(test_resident_slot_coordinator_core tests/resident/resident_slot_coordinator_core_test.cpp)
    target_include_directories(test_resident_slot_coordinator_core PRIVATE "${HANMO_JSON_ROOT}/single_include")
    target_link_libraries(test_resident_slot_coordinator_core PRIVATE hanmo_resident_storage_test_support)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_resident_slot_codec_native PRIVATE -Wall -Wextra -Werror -UNDEBUG)
        target_compile_options(test_resident_slot_coordinator_core PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    endif()
    if(HANMO_RESIDENT_LIFECYCLE_ORACLE)
        add_test(NAME resident_slot_coordinator_core COMMAND test_resident_slot_coordinator_core "${HANMO_RESIDENT_LIFECYCLE_ORACLE}")
    endif()
endif()
