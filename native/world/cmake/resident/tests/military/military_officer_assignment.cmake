# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 军官接替只核本域来源与钱货不动，不替代完整政治World事务验收。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_officer_assignment_test.cpp")
    add_executable(test_resident_military_officer_assignment tests/resident/military_officer_assignment_test.cpp)
    target_include_directories(test_resident_military_officer_assignment PRIVATE src)
    target_link_libraries(test_resident_military_officer_assignment PRIVATE hanmo_resident)
    add_test(NAME resident_military_officer_assignment COMMAND test_resident_military_officer_assignment)
endif()

# 同一非空Military样本核新版本读写、截断和旧profile拒绝；原3/4/5字节另与固定原源码比较。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_officer_codec_compatibility_test.cpp")
    add_executable(test_resident_military_officer_codec_compatibility tests/resident/military_officer_codec_compatibility_test.cpp)
    target_include_directories(test_resident_military_officer_codec_compatibility PRIVATE src)
    target_link_libraries(test_resident_military_officer_codec_compatibility PRIVATE hanmo_resident)
    add_test(NAME resident_military_officer_codec_compatibility COMMAND test_resident_military_officer_codec_compatibility)
endif()
