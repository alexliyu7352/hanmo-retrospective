# 本文件只登记旅行/军事目标，绝不触发全库或代其它领域补编。
get_filename_component(HANMO_TRAVEL_NATIVE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(HANMO_TRAVEL_REPO_ROOT "${HANMO_TRAVEL_NATIVE_ROOT}/../.." ABSOLUTE)
include("${HANMO_TRAVEL_NATIVE_ROOT}/cmake/domain_checklist/HanmoDomainChecklist.cmake")
# 本文件自己的纯叶对象核验也需要解释器，不能借函数内部的局部变量。
find_package(Python3 REQUIRED COMPONENTS Interpreter)
hanmo_configure_domain_runtime(
    OWNER travel_military
    REFERENCE_BUILD "${HANMO_REFERENCE_BUILD}"
    BUILD_ROOT "${HANMO_DOMAIN_BUILD_ROOT}")
set(HANMO_CHECKLIST_PREPARED
    "${HANMO_TRAVEL_REPO_ROOT}/builds/native-checklist/shared_world_foundation/prepared.json")
# 原行还需群体迁移/货运/军行的真实来源；此门只验个人/同行道路依赖，不能计入清单。
add_executable(checklist_K4_1 "${CMAKE_CURRENT_LIST_DIR}/checklist_travel_military.cpp")
target_link_libraries(checklist_K4_1 PRIVATE hanmo_domain_runtime)
add_dependencies(checklist_K4_1 hanmo_domain_imports_current)
add_test(NAME native_k4_road_party_dependency
    COMMAND "${CMAKE_COMMAND}"
        "-DHANMO_CHECKLIST_BINARY=$<TARGET_FILE:checklist_K4_1>"
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/K4-1"
        -P "${CMAKE_CURRENT_LIST_DIR}/checklist_travel_military.cmake")
set_tests_properties(native_k4_road_party_dependency
    PROPERTIES LABELS "native_dependency;travel_military" TIMEOUT 7200)
# 合同 §12.1 的地理依赖先真跑；未安装真实军团 Session，不登记整行 checklist_BW06 CTest。
set(HANMO_FIELD_BATTLE_ROOT "${HANMO_TRAVEL_NATIVE_ROOT}/src/resident/domains/military/field_battle")
add_executable(checklist_BW06
    "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_generation.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_terrain.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_generation.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_author_data.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_time.cpp"
    "${HANMO_TRAVEL_NATIVE_ROOT}/src/resident/domains/travel/route_geometry.cpp")
# 纯生成叶不使用完整 ResidentWorld；只读借用经真实编译依赖核验的两个公共对象。
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_objects.py"
        "${HANMO_TRAVEL_NATIVE_ROOT}/cmake/domain_checklist" "${HANMO_REFERENCE_BUILD}"
        "${HANMO_DOMAIN_BUILD_ROOT}/build-storage" --paths
    RESULT_VARIABLE field_object_selection_status
    OUTPUT_VARIABLE field_object_paths OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT field_object_selection_status EQUAL 0)
    message(FATAL_ERROR "Field generation cannot select its original public objects")
endif()
string(REPLACE "\n" ";" field_public_objects "${field_object_paths}")
set_source_files_properties(${field_public_objects} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_sources(checklist_BW06 PRIVATE ${field_public_objects})
get_target_property(field_includes hanmo_domain_runtime INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(field_options hanmo_domain_runtime INTERFACE_COMPILE_OPTIONS)
get_target_property(field_link_options hanmo_domain_runtime INTERFACE_LINK_OPTIONS)
target_include_directories(checklist_BW06 PRIVATE ${field_includes})
target_compile_features(checklist_BW06 PRIVATE cxx_std_20)
target_compile_options(checklist_BW06 PRIVATE ${field_options})
target_link_options(checklist_BW06 PRIVATE ${field_link_options})
# 同一命名构建门编译实际会话前置叶；它们未接公共安装，所以不链接入地理测试冒运行证据。
add_library(field_session_compile_dependency OBJECT
    "${HANMO_FIELD_BATTLE_ROOT}/field_contact.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_formations.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_session_build.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_deployment.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_world.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_install.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_state_codec.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_validation.cpp"
    "${HANMO_FIELD_BATTLE_ROOT}/field_geometry_view.cpp")
target_include_directories(field_session_compile_dependency PRIVATE ${field_includes})
target_compile_features(field_session_compile_dependency PRIVATE cxx_std_20)
target_compile_options(field_session_compile_dependency PRIVATE ${field_options})
add_dependencies(checklist_BW06 field_session_compile_dependency)
add_custom_target(bw06_generation_objects_current
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_objects.py"
        "${HANMO_TRAVEL_NATIVE_ROOT}/cmake/domain_checklist" "${HANMO_REFERENCE_BUILD}"
        "${HANMO_DOMAIN_BUILD_ROOT}/build-storage" --verify ${field_public_objects}
    VERBATIM)
add_dependencies(checklist_BW06 bw06_generation_objects_current)
add_test(NAME native_bw06_generation_dependency COMMAND checklist_BW06
    "${HANMO_CHECKLIST_PREPARED}" "${HANMO_TRAVEL_REPO_ROOT}")
set_tests_properties(native_bw06_generation_dependency
    PROPERTIES LABELS "native_dependency;travel_military" TIMEOUT 7200)
# 共享选择接线落地后才跑真实新局/两条冷恢复；空根回归仍不是完整 Session 门。
add_library(field_selected_compile_dependency OBJECT
    "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_selected.cpp")
target_link_libraries(field_selected_compile_dependency PRIVATE hanmo_domain_runtime)
add_library(field_install_compile_dependency OBJECT
    "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_install.cpp")
target_link_libraries(field_install_compile_dependency PRIVATE hanmo_domain_runtime)
# 未启军事初态的真安装门只编测试 TU，不运行、不登记成功；仍保在本 BW06 构建门。
add_dependencies(checklist_BW06 field_install_compile_dependency)
add_dependencies(hanmo_domain_imports_current
    field_session_compile_dependency field_selected_compile_dependency
    field_install_compile_dependency)
add_dependencies(bw06_generation_objects_current
    field_session_compile_dependency field_selected_compile_dependency
    field_install_compile_dependency)
add_executable(field_battle_selected_dependency
    $<TARGET_OBJECTS:field_selected_compile_dependency>)
target_link_libraries(field_battle_selected_dependency PRIVATE hanmo_domain_runtime)
add_dependencies(field_battle_selected_dependency hanmo_domain_imports_current)
# 本域单独编译仍只用 checklist_BW06；完整六片链接须协调者给窗口后显式打开。
option(HANMO_BW06_RUNTIME_LINK "已有独占链接窗口，接通 BW06 冷入口运行依赖" OFF)
if(HANMO_BW06_RUNTIME_LINK)
    add_dependencies(checklist_BW06 field_battle_selected_dependency)
endif()
add_test(NAME native_bw06_selected_catalog_dependency
    COMMAND field_battle_selected_dependency "${HANMO_CHECKLIST_PREPARED}")
set_tests_properties(native_bw06_selected_catalog_dependency
    PROPERTIES LABELS "native_dependency;travel_military" TIMEOUT 7200)
# 原攻击数值叶的有限提取验证。隔离候选路径必须显式给出，不影响正式 409 片。
set(HANMO_FIELD_ATTACK_REVIEW_ROOT "" CACHE PATH "本域攻击公式候选 native/world 根")
if(HANMO_FIELD_ATTACK_REVIEW_ROOT)
    add_executable(native_field_attack_core
        "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_attack_core.cpp"
        "${HANMO_FIELD_ATTACK_REVIEW_ROOT}/src/resident/domains/military/military_attack_damage_core.cpp"
        ${field_public_objects})
    target_include_directories(native_field_attack_core BEFORE PRIVATE
        "${HANMO_FIELD_ATTACK_REVIEW_ROOT}/src"
        "${HANMO_TRAVEL_NATIVE_ROOT}/src/resident/domains/military")
    target_include_directories(native_field_attack_core PRIVATE ${field_includes})
    target_compile_features(native_field_attack_core PRIVATE cxx_std_20)
    target_compile_options(native_field_attack_core PRIVATE ${field_options})
    target_link_options(native_field_attack_core PRIVATE ${field_link_options})
    add_custom_target(bw06_attack_public_objects_current
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/checklist_field_battle_objects.py"
            "${HANMO_TRAVEL_NATIVE_ROOT}/cmake/domain_checklist" "${HANMO_REFERENCE_BUILD}"
            "${HANMO_DOMAIN_BUILD_ROOT}/build-storage" --verify ${field_public_objects}
        VERBATIM)
    add_dependencies(native_field_attack_core bw06_attack_public_objects_current)
    add_test(NAME native_bw06_attack_formula_dependency
        COMMAND native_field_attack_core "${HANMO_CHECKLIST_PREPARED}")
    set_tests_properties(native_bw06_attack_formula_dependency
        PROPERTIES LABELS "native_dependency;travel_military" TIMEOUT 60)
endif()
