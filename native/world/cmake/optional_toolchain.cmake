# 可选工具链扩充只在真实探针通过后启用；失败保留普通编译、链接或归档。
# 语言标准、浮点确定性、断言、导出边界和显式加速合同不在此降级。
include_guard(GLOBAL)
include(CheckCompilerFlag)
include(CheckLinkerFlag)
include(CMakePushCheckState)

# 同一 build 切换工具、flags 或归档规则时使用新私有键，不能沿用旧支持结论。
function(hanmo_optional_probe_key output kind language flag)
    file(SHA256 "${CMAKE_CURRENT_FUNCTION_LIST_FILE}" implementation_sha)
    set(identity "${implementation_sha}|${kind}|${language}|${flag}")
    set(variables
        CMAKE_${language}_COMPILER CMAKE_${language}_COMPILER_ARG1
        CMAKE_${language}_COMPILER_ID CMAKE_${language}_COMPILER_VERSION
        CMAKE_${language}_COMPILER_TARGET CMAKE_${language}_COMPILER_EXTERNAL_TOOLCHAIN
        CMAKE_${language}_COMPILER_LAUNCHER CMAKE_${language}_LINKER_LAUNCHER
        CMAKE_${language}_FLAGS CMAKE_EXE_LINKER_FLAGS CMAKE_STATIC_LINKER_FLAGS
        CMAKE_${language}_COMPILE_OBJECT CMAKE_${language}_LINK_EXECUTABLE
        CMAKE_TRY_COMPILE_PLATFORM_VARIABLES CMAKE_TRY_COMPILE_NO_PLATFORM_VARIABLES
        ${CMAKE_TRY_COMPILE_PLATFORM_VARIABLES}
        CMAKE_AR CMAKE_RANLIB CMAKE_LINKER CMAKE_${language}_COMPILER_AR
        CMAKE_${language}_COMPILER_RANLIB CMAKE_${language}_ARCHIVE_CREATE
        CMAKE_${language}_ARCHIVE_APPEND CMAKE_${language}_ARCHIVE_FINISH
        CMAKE_${language}_CREATE_STATIC_LIBRARY CMAKE_TOOLCHAIN_FILE
        CMAKE_SYSTEM_NAME CMAKE_SYSTEM_PROCESSOR CMAKE_SYSROOT
        CMAKE_SYSROOT_COMPILE CMAKE_SYSROOT_LINK CMAKE_BUILD_TYPE
        CMAKE_TRY_COMPILE_CONFIGURATION CMAKE_GENERATOR CMAKE_GENERATOR_PLATFORM
        CMAKE_GENERATOR_TOOLSET CMAKE_OSX_SYSROOT CMAKE_OSX_ARCHITECTURES
        CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_POSITION_INDEPENDENT_CODE
        CMAKE_${language}_STANDARD CMAKE_${language}_STANDARD_REQUIRED
        CMAKE_${language}_EXTENSIONS)
    foreach(configuration DEBUG RELEASE RELWITHDEBINFO MINSIZEREL
            ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE} ${CMAKE_TRY_COMPILE_CONFIGURATION})
        string(TOUPPER "${configuration}" configuration)
        list(APPEND variables CMAKE_${language}_FLAGS_${configuration}
            CMAKE_EXE_LINKER_FLAGS_${configuration} CMAKE_STATIC_LINKER_FLAGS_${configuration})
    endforeach()
    foreach(variable IN LISTS variables)
        string(APPEND identity "\n${variable}=${${variable}}")
    endforeach()
    string(SHA256 fingerprint "${identity}")
    set(${output} "HANMO_OPTIONAL_${kind}_${fingerprint}" PARENT_SCOPE)
endfunction()

# 分别探测目标可能使用的 C/CXX 编译器；单个开关不支持时仅省略该开关。
function(hanmo_optional_compile_sections target)
    cmake_push_check_state(RESET)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    get_property(languages GLOBAL PROPERTY ENABLED_LANGUAGES)
    foreach(language C CXX)
        if(language IN_LIST languages AND CMAKE_${language}_COMPILER_ID MATCHES "GNU|Clang")
            list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
                CMAKE_${language}_COMPILE_OBJECT CMAKE_${language}_ARCHIVE_CREATE
                CMAKE_${language}_ARCHIVE_APPEND CMAKE_${language}_ARCHIVE_FINISH
                CMAKE_${language}_CREATE_STATIC_LIBRARY)
            foreach(section function data)
                string(TOUPPER "${section}" section_upper)
                set(supported "HANMO_${language}_${section_upper}_SECTIONS_SUPPORTED")
                hanmo_optional_probe_key(probe_key COMPILE ${language} "-f${section}-sections")
                check_compiler_flag(${language} "-f${section}-sections" ${probe_key})
                set(${supported} "${${probe_key}}" CACHE INTERNAL "本次编译器扩充探测结果" FORCE)
                if(${supported})
                    target_compile_options(${target} PRIVATE
                        "$<$<COMPILE_LANGUAGE:${language}>:-f${section}-sections>")
                endif()
            endforeach()
        endif()
    endforeach()
    cmake_pop_check_state()
endfunction()

# 必须真正链接可执行文件，不能把只编译成功误作链接器支持；失败不加选项。
function(hanmo_optional_link_option target visibility flag supported)
    cmake_push_check_state(RESET)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_CXX_COMPILE_OBJECT CMAKE_CXX_LINK_EXECUTABLE)
    hanmo_optional_probe_key(probe_key LINK CXX "${flag}")
    check_linker_flag(CXX "${flag}" ${probe_key})
    set(${supported} "${${probe_key}}" CACHE INTERNAL "本次链接器扩充探测结果" FORCE)
    if(${supported})
        target_link_options(${target} ${visibility} "${flag}")
    endif()
    cmake_pop_check_state()
endfunction()

# 使用当前 CXX 的真实归档规则和 ranlib，检查薄归档魔数，不依赖 --help 文案。
# 不支持时保留 CMake 原普通归档规则；探针对象只留在当前构建目录。
function(hanmo_optional_thin_archive target)
    # 显式传播当前规则，同时将自定义平台变量和规则计入缓存身份。
    list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
        CMAKE_CXX_COMPILE_OBJECT CMAKE_CXX_ARCHIVE_CREATE CMAKE_CXX_ARCHIVE_APPEND
        CMAKE_CXX_ARCHIVE_FINISH CMAKE_CXX_CREATE_STATIC_LIBRARY)
    hanmo_optional_probe_key(probe_key ARCHIVE CXX --thin)
    if(NOT DEFINED ${probe_key})
        set(probe "${CMAKE_BINARY_DIR}/CMakeFiles/hanmo-thin-archive-probe/${probe_key}")
        file(MAKE_DIRECTORY "${probe}")
        file(WRITE "${probe}/archive_probe.cpp" "int hanmo_archive_probe() { return 0; }\n")
        set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
        # try_compile 会加载旧编译器文件，可能覆盖 -DCMAKE_AR/-DCMAKE_RANLIB。
        # 在仅属于本函数的规则副本中绑定本次实际工具，保留自定义规则其余部分。
        foreach(rule CMAKE_CXX_ARCHIVE_CREATE CMAKE_CXX_ARCHIVE_APPEND
                CMAKE_CXX_ARCHIVE_FINISH CMAKE_CXX_CREATE_STATIC_LIBRARY)
            if(DEFINED ${rule})
                string(REPLACE "<CMAKE_AR>" "\"${CMAKE_AR}\"" ${rule} "${${rule}}")
                string(REPLACE "<CMAKE_RANLIB>" "\"${CMAKE_RANLIB}\"" ${rule} "${${rule}}")
            endif()
        endforeach()
        try_compile(hanmo_thin_archive_built "${probe}/build" "${probe}/archive_probe.cpp"
            CMAKE_FLAGS
                "-DCMAKE_AR:FILEPATH=${CMAKE_AR}"
                "-DCMAKE_RANLIB:FILEPATH=${CMAKE_RANLIB}"
            LINK_OPTIONS --thin
            COPY_FILE "${probe}/archive_probe.a"
            COPY_FILE_ERROR copy_error
            OUTPUT_VARIABLE probe_output)
        file(WRITE "${probe}/output.log" "${probe_output}\n${copy_error}\n")
        set(supported OFF)
        if(hanmo_thin_archive_built AND NOT copy_error AND EXISTS "${probe}/archive_probe.a")
            file(READ "${probe}/archive_probe.a" archive_magic LIMIT 8 HEX)
            if(archive_magic STREQUAL "213c7468696e3e0a")
                set(supported ON)
            endif()
        endif()
        set(${probe_key} "${supported}" CACHE INTERNAL "当前工具链支持真实薄归档")
    endif()
    set(HANMO_RESIDENT_THIN_ARCHIVE_SUPPORTED "${${probe_key}}" CACHE INTERNAL
        "本次 CXX 归档器与 ranlib 的薄归档探测结果" FORCE)
    if(HANMO_RESIDENT_THIN_ARCHIVE_SUPPORTED)
        set_property(TARGET ${target} APPEND PROPERTY STATIC_LIBRARY_OPTIONS --thin)
    else()
        message(STATUS "Thin archives unavailable; using the ordinary static archive rule")
    endif()
endfunction()
