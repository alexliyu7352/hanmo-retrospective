# 本文件只登记互动目标；共享生产对象选择与交叉核验由统一接头负责。
get_filename_component(HANMO_INTERACTION_NATIVE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(HANMO_INTERACTION_REPO_ROOT "${HANMO_INTERACTION_NATIVE_ROOT}/../.." ABSOLUTE)
include("${HANMO_INTERACTION_NATIVE_ROOT}/cmake/domain_checklist/HanmoDomainChecklist.cmake")
hanmo_configure_domain_runtime(
    OWNER interaction
    REFERENCE_BUILD "${HANMO_REFERENCE_BUILD}"
    BUILD_ROOT "${HANMO_DOMAIN_BUILD_ROOT}")
set(HANMO_CHECKLIST_PREPARED
    "${HANMO_INTERACTION_REPO_ROOT}/builds/native-checklist/shared_world_foundation/prepared.json")
hanmo_add_domain_checklist(
    ID INT-1
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/checklist_int_1.cpp"
    TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/checklist_int_1.cmake"
    ARGUMENTS
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/INT-1")
hanmo_add_domain_checklist(
    ID K6-1
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/checklist_k6_1.cpp"
    TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/checklist_k6_1.cmake"
    ARGUMENTS
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/K6-1")
hanmo_add_domain_checklist(
    ID INT-2
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/checklist_int_2.cpp"
    TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/checklist_int_2.cmake"
    ARGUMENTS
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/INT-2")
hanmo_add_domain_checklist(
    ID K6-2
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/checklist_k6_2.cpp"
    TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/checklist_k6_2.cmake"
    ARGUMENTS
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/K6-2")
hanmo_add_domain_checklist(
    ID K8-1
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/checklist_k8_1.cpp"
    TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/checklist_k8_1.cmake"
    ARGUMENTS
        "-DHANMO_CHECKLIST_PREPARED=${HANMO_CHECKLIST_PREPARED}"
        "-DHANMO_CHECKLIST_OUTPUT=${CMAKE_BINARY_DIR}/native-checklist/K8-1")
