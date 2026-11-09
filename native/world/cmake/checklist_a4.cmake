# A4 只复用已有薄宿主与唯一共用输入，不生成逐行夹具或隐式构建生产库。
get_filename_component(HANMO_CHECKLIST_A4_NATIVE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(test_resident_checklist_a4
    "${HANMO_CHECKLIST_A4_NATIVE_ROOT}/tools/resident_complete_thin_main.cpp")
target_include_directories(test_resident_checklist_a4 PRIVATE
    "${HANMO_CHECKLIST_A4_NATIVE_ROOT}/src")
target_link_libraries(test_resident_checklist_a4 PRIVATE hanmo_resident)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_resident_checklist_a4 PRIVATE -UNDEBUG)
    hanmo_optional_compile_sections(test_resident_checklist_a4)
endif()
add_test(NAME checklist_A4
    COMMAND "${Python3_EXECUTABLE}"
        "${HANMO_CHECKLIST_A4_NATIVE_ROOT}/tests/resident/checklist_a4.py"
        --binary "$<TARGET_FILE:test_resident_checklist_a4>"
        --fixture "${HANMO_CHECKLIST_PREPARED}"
        --scenario "${HANMO_CHECKLIST_SCENARIO}"
        --output "${CMAKE_BINARY_DIR}/native-checklist/checklist_A4"
        --days 30)
set_tests_properties(checklist_A4 PROPERTIES
    LABELS "native_checklist;shared_world;skeleton"
    RESOURCE_LOCK native_checklist_build
    TIMEOUT 7200)
