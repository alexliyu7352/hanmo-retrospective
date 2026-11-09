# 检查五个中央文件以及实际拆分片段；任何缺件、空文件或超限都使命名门失败。
if(NOT DEFINED HANMO_NATIVE_ROOT OR NOT IS_DIRECTORY "${HANMO_NATIVE_ROOT}")
    message(FATAL_ERROR "HANMO_NATIVE_ROOT must name the actual native/world source tree")
endif()

set(central_files
    src/resident/kernel/world_copy_plan.cpp
    src/resident/kernel/world_retention.cpp
    src/resident/kernel/source_owned_fields.hpp
    src/resident/kernel/world_codec.cpp
    resident.cmake)
set(fragment_patterns
    src/resident/kernel/world_copy_plan/*
    src/resident/kernel/world_retention/*
    src/resident/kernel/source_owned_fields/*
    src/resident/checkpoint/world_codec/*
    src/resident/domains/*/source_owned_fields/*
    src/resident/domains/*/world_codec/*
    cmake/resident/*)
set(checked ${central_files})
foreach(pattern IN LISTS fragment_patterns)
    file(GLOB_RECURSE fragments LIST_DIRECTORIES FALSE
        RELATIVE "${HANMO_NATIVE_ROOT}" "${HANMO_NATIVE_ROOT}/${pattern}")
    if(NOT fragments)
        message(FATAL_ERROR "Required split file group is absent: ${pattern}")
    endif()
    list(APPEND checked ${fragments})
endforeach()
list(REMOVE_DUPLICATES checked)

set(maximum_bytes 0)
set(maximum_path "")
foreach(relative_path IN LISTS checked)
    set(path "${HANMO_NATIVE_ROOT}/${relative_path}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR "Required split source is absent: ${relative_path}")
    endif()
    file(SIZE "${path}" bytes)
    if(bytes EQUAL 0 OR bytes GREATER 100000)
        message(FATAL_ERROR "Split source must be 1..100000 bytes: ${relative_path} = ${bytes}")
    endif()
    if(bytes GREATER maximum_bytes)
        set(maximum_bytes ${bytes})
        set(maximum_path "${relative_path}")
    endif()
endforeach()
list(LENGTH checked count)
message(STATUS "checklist_A3: ${count} real files checked; maximum ${maximum_bytes} bytes (${maximum_path})")
