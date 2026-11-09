# 检查点与延续消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
add_executable(test_native_exact_rest_world tests/resident/exact_rest_world_test.cpp)
target_link_libraries(test_native_exact_rest_world PRIVATE hanmo_resident)
target_include_directories(test_native_exact_rest_world PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_exact_rest_world PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_exact_rest_world COMMAND test_native_exact_rest_world "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/fixtures/exact_rest_only_catalog.json")

add_executable(test_native_exact_boundary_session tests/resident/exact_boundary_session_test.cpp)
target_link_libraries(test_native_exact_boundary_session PRIVATE hanmo_resident)
target_include_directories(test_native_exact_boundary_session PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_exact_boundary_session PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_exact_boundary_session COMMAND test_native_exact_boundary_session "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/fixtures/exact_rest_only_catalog.json")

add_executable(test_native_exact_profile_initialization tests/resident/exact_profile_initialization_test.cpp)
target_link_libraries(test_native_exact_profile_initialization PRIVATE hanmo_resident)
target_include_directories(test_native_exact_profile_initialization PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_exact_profile_initialization PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_exact_profile_initialization COMMAND test_native_exact_profile_initialization "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/fixtures/exact_rest_only_catalog.json")
