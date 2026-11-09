# 世界内核与共享事务消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 共同底座主体生命周期只消费同一生产库；断言不随构建类型关闭。
add_executable(test_resident_shared_world_foundation tests/resident/shared_world_foundation_test.cpp)
target_include_directories(test_resident_shared_world_foundation PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_shared_world_foundation PRIVATE hanmo_resident)
target_compile_options(test_resident_shared_world_foundation PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
