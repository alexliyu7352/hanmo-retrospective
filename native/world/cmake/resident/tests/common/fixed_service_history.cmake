# 共同资源与执行消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
add_executable(test_native_fixed_service_history tests/resident/fixed_service_history_test.cpp)
target_link_libraries(test_native_fixed_service_history PRIVATE hanmo_resident)
target_include_directories(test_native_fixed_service_history PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_fixed_service_history PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_fixed_service_history COMMAND test_native_fixed_service_history )
