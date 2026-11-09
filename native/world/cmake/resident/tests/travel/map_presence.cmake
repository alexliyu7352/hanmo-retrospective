# 旅行与空间消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 地图有限页和原交通下一步用例只链接同一常驻实现，不另立模拟器。
foreach(name map_presence map_company travel_navigation)
    add_executable(test_resident_${name} tests/resident/${name}_test.cpp)
    target_include_directories(test_resident_${name} PRIVATE src "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
    target_link_libraries(test_resident_${name} PRIVATE hanmo_resident)
    target_compile_options(test_resident_${name} PRIVATE -UNDEBUG)
    add_test(NAME native_${name}_contract COMMAND test_resident_${name})
endforeach()
