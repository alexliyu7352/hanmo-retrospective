# 人物与控制底座消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 本节点只编译对应本人选择/自主调度边界，断言在Release下保持有效。
add_executable(test_resident_native_actor_selection tests/resident/native_actor_selection_test.cpp)
target_include_directories(test_resident_native_actor_selection PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_native_actor_selection PRIVATE hanmo_resident)
target_compile_options(test_resident_native_actor_selection PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)

# 本节点只编译对应本人选择/自主调度边界，断言在Release下保持有效。
add_executable(test_resident_native_player_control tests/resident/native_player_control_test.cpp)
target_include_directories(test_resident_native_player_control PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_native_player_control PRIVATE hanmo_resident)
target_compile_options(test_resident_native_player_control PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)

# 本人有限只读投影：只运行受影响的当前身份/隐私/零写定点。
add_executable(test_resident_self_profile_view tests/resident/self_profile_view_test.cpp)
target_include_directories(test_resident_self_profile_view PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_self_profile_view PRIVATE hanmo_resident)
target_compile_options(test_resident_self_profile_view PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)

# 本人纯读路由只接受空参数，补当前来源/继任/参与行动的受影响边界。
add_executable(test_resident_self_profile_route tests/resident/self_profile_route_test.cpp)
target_include_directories(test_resident_self_profile_route PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(test_resident_self_profile_route PRIVATE hanmo_resident)
target_compile_options(test_resident_self_profile_route PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
