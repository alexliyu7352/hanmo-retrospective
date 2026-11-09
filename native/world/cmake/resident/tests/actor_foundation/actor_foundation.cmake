# 人物与控制底座消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 精确rest与新封套正常功能批；共享复制异常门仍未准入，不能据此宣称全部故障验证。
add_executable(test_native_actor_foundation tests/resident/actor_foundation_test.cpp)
target_link_libraries(test_native_actor_foundation PRIVATE hanmo_resident)
target_include_directories(test_native_actor_foundation PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_actor_foundation PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_actor_foundation COMMAND test_native_actor_foundation )
