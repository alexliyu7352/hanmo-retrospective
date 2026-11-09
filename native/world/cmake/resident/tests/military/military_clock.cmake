# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 本人真实军籍只读资格；生产全链由原正式军旅作者consumer验证。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_clock_test.cpp")
    add_executable(test_resident_military_clock tests/resident/military_clock_test.cpp)
    target_include_directories(test_resident_military_clock PRIVATE src)
    target_link_libraries(test_resident_military_clock PRIVATE hanmo_resident)
    add_test(NAME resident_military_clock COMMAND test_resident_military_clock)
endif()

# 附属行旅只读源：真正共同动作才可读取当前领队坐标。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/resident/military_member_journey_test.cpp")
    add_executable(test_resident_military_member_journey tests/resident/military_member_journey_test.cpp)
    target_include_directories(test_resident_military_member_journey PRIVATE src)
    target_link_libraries(test_resident_military_member_journey PRIVATE hanmo_resident)
    add_test(NAME resident_military_member_journey COMMAND test_resident_military_member_journey)
endif()
