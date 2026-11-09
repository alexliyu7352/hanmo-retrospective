# 社交与传播消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 同源核心候选：有限消息投递、job预算及本机准入；不代全游戏验收。
add_executable(test_native_social_time2_codec tests/resident/social_time2_codec_test.cpp)
target_link_libraries(test_native_social_time2_codec PRIVATE hanmo_resident)
target_include_directories(test_native_social_time2_codec PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_social_time2_codec PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_social_time2_codec COMMAND test_native_social_time2_codec)
add_executable(test_native_exact_social_world tests/resident/exact_social_world_test.cpp)
target_link_libraries(test_native_exact_social_world PRIVATE hanmo_resident)
target_include_directories(test_native_exact_social_world PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(test_native_exact_social_world PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME native_exact_social_world COMMAND test_native_exact_social_world "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/fixtures/exact_supply_reports_catalog.json")
