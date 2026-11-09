# 独立领域工程直接 include；不进入旧整树 target，不自动构建跨域生产片。
include_guard(GLOBAL)
set(HANMO_DOMAIN_HELPER "${CMAKE_CURRENT_LIST_DIR}/domain_objects.py")

# 原编译数据库、六个私有 build 目录与链接器由明确输入绑定；不得导入旧单体 resident 库。
function(hanmo_configure_domain_runtime)
    cmake_parse_arguments(ARG "" "OWNER;REFERENCE_BUILD;BUILD_ROOT" "" ${ARGN})
    foreach(key OWNER REFERENCE_BUILD BUILD_ROOT)
        if(NOT ARG_${key})
            message(FATAL_ERROR "Domain runtime requires ${key}")
        endif()
    endforeach()
    set(owners travel_military economy character_family politics interaction storage)
    if(NOT ARG_OWNER IN_LIST owners)
        message(FATAL_ERROR "Unknown domain owner: ${ARG_OWNER}")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_package(Threads REQUIRED)
    get_filename_component(reference "${ARG_REFERENCE_BUILD}" ABSOLUTE)
    get_filename_component(root "${ARG_BUILD_ROOT}" ABSOLUTE)
    string(REPLACE "_" "-" directory_owner "${ARG_OWNER}")
    set(build "${root}/build-${directory_owner}")
    if(NOT CMAKE_BINARY_DIR STREQUAL build)
        message(FATAL_ERROR "Use the owner's existing build directory: ${build}")
    endif()
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${HANMO_DOMAIN_HELPER}" describe
        --reference "${reference}" --output "${CMAKE_BINARY_DIR}/domain-settings.cmake"
        RESULT_VARIABLE described COMMAND_ECHO STDOUT)
    if(NOT described EQUAL 0)
        message(FATAL_ERROR "Original domain build settings unavailable")
    endif()
    include("${CMAKE_BINARY_DIR}/domain-settings.cmake")
    if(NOT CMAKE_CXX_COMPILER STREQUAL HANMO_DOMAIN_COMPILER)
        message(FATAL_ERROR "Domain compiler differs from the actual source cache")
    endif()
    add_custom_target(hanmo_domain_prepare
        COMMAND "${Python3_EXECUTABLE}" "${HANMO_DOMAIN_HELPER}" build
            --reference "${reference}" --owner "${ARG_OWNER}" --build "${build}"
        BYPRODUCTS "${build}/libhanmo_domain_${ARG_OWNER}.a"
        USES_TERMINAL VERBATIM)
    add_custom_target(hanmo_domain_imports_current
        COMMAND "${Python3_EXECUTABLE}" "${HANMO_DOMAIN_HELPER}" verify
            --reference "${reference}" --root "${root}"
        DEPENDS hanmo_domain_prepare USES_TERMINAL VERBATIM)
    foreach(owner IN LISTS owners)
        string(REPLACE "_" "-" directory_owner "${owner}")
        add_library(hanmo_piece_${owner} STATIC IMPORTED GLOBAL)
        set_target_properties(hanmo_piece_${owner} PROPERTIES
            IMPORTED_LOCATION "${root}/build-${directory_owner}/libhanmo_domain_${owner}.a")
        list(APPEND pieces hanmo_piece_${owner})
    endforeach()
    foreach(name world flecs)
        add_library(hanmo_original_${name} STATIC IMPORTED GLOBAL)
        set_target_properties(hanmo_original_${name} PROPERTIES
            IMPORTED_LOCATION "${reference}/libhanmo_${name}.a")
    endforeach()
    add_library(hanmo_domain_runtime INTERFACE)
    target_include_directories(hanmo_domain_runtime INTERFACE
        "${HANMO_DOMAIN_NATIVE_ROOT}/include" "${HANMO_DOMAIN_NATIVE_ROOT}/src"
        "${HANMO_DOMAIN_NATIVE_ROOT}/../godot" "${HANMO_DOMAIN_JSON_ROOT}/single_include"
        "${HANMO_DOMAIN_PICOSHA2_ROOT}")
    separate_arguments(original_flags UNIX_COMMAND "${HANMO_DOMAIN_CXX_FLAGS}")
    separate_arguments(original_link_flags UNIX_COMMAND "${HANMO_DOMAIN_LINK_FLAGS}")
    get_filename_component(checklist_repository_root "${HANMO_DOMAIN_NATIVE_ROOT}/../.." ABSOLUTE)
    target_compile_definitions(hanmo_domain_runtime INTERFACE
        HANMO_CHECKLIST_REPOSITORY_ROOT="${checklist_repository_root}")
    target_compile_features(hanmo_domain_runtime INTERFACE cxx_std_20)
    target_compile_options(hanmo_domain_runtime INTERFACE ${original_flags}
        -UNDEBUG -fno-fast-math -ffp-contract=off)
    target_link_options(hanmo_domain_runtime INTERFACE ${original_link_flags})
    target_link_libraries(hanmo_domain_runtime INTERFACE
        "-Wl,--start-group" ${pieces} hanmo_original_world hanmo_original_flecs
        "-Wl,--end-group" Threads::Threads ${CMAKE_DL_LIBS} m)
    set(HANMO_DOMAIN_NATIVE_ROOT "${HANMO_DOMAIN_NATIVE_ROOT}" PARENT_SCOPE)
endfunction()

# 测试可带已编好的本域 OBJECTS；真实命名目标先补本域片，再核跨域片并链接。
function(hanmo_add_domain_checklist)
    cmake_parse_arguments(ARG "" "ID;TEST_SCRIPT" "SOURCES;OBJECTS;ARGUMENTS" ${ARGN})
    if(NOT ARG_ID OR NOT TARGET hanmo_domain_runtime)
        message(FATAL_ERROR "Configure the domain runtime and give an original checklist ID")
    endif()
    string(REPLACE "-" "_" row "${ARG_ID}")
    set(target "checklist_${row}")
    if(TARGET ${target})
        message(FATAL_ERROR "Checklist target already exists; retain its object using OBJECTS: ${target}")
    endif()
    foreach(object IN LISTS ARG_OBJECTS)
        set_source_files_properties("${object}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    endforeach()
    add_executable(${target} ${ARG_SOURCES} ${ARG_OBJECTS})
    set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
    target_link_libraries(${target} PRIVATE hanmo_domain_runtime)
    add_dependencies(${target} hanmo_domain_imports_current)
    if(ARG_TEST_SCRIPT)
        add_test(NAME ${target} COMMAND "${CMAKE_COMMAND}"
            "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:${target}>"
            ${ARG_ARGUMENTS} -P "${ARG_TEST_SCRIPT}")
    else()
        add_test(NAME ${target} COMMAND ${target} ${ARG_ARGUMENTS})
    endif()
    set_tests_properties(${target} PROPERTIES LABELS "native_checklist;domain" TIMEOUT 7200)
endfunction()
