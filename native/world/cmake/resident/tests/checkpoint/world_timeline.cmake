# 检查点与延续消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
add_executable(test_native_world_timeline tests/resident/world_timeline_test.cpp)
target_link_libraries(test_native_world_timeline PRIVATE hanmo_resident)
target_include_directories(test_native_world_timeline PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_world_timeline PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_world_timeline COMMAND test_native_world_timeline )
