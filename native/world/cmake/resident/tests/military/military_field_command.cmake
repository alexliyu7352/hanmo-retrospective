# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 两步野外交接沿同一军团与Fleet的事务源门。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_field_command_test.cpp")
    add_executable(test_resident_military_field_command tests/resident/military_field_command_test.cpp)
    target_include_directories(test_resident_military_field_command PRIVATE src)
    target_link_libraries(test_resident_military_field_command PRIVATE hanmo_resident)
    add_test(NAME resident_military_field_command COMMAND test_resident_military_field_command)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_field_codec_compatibility_test.cpp")
    add_executable(test_resident_military_field_codec tests/resident/military_field_codec_compatibility_test.cpp)
    target_include_directories(test_resident_military_field_codec PRIVATE src)
    target_link_libraries(test_resident_military_field_codec PRIVATE hanmo_resident)
    add_test(NAME resident_military_field_codec COMMAND test_resident_military_field_codec)
endif()
