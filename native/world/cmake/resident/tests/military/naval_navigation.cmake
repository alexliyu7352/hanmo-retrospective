# 军旅与战斗消费者登记：保留本组目标、测试、条件与局部变量的原执行顺序。
# 逐舟实际航行计数、原启程权源、坏档及旧Naval4字节的具名领域门。
add_executable(test_resident_naval_navigation tests/resident/naval_navigation_test.cpp)
target_link_libraries(test_resident_naval_navigation PRIVATE hanmo_resident)
target_include_directories(test_resident_naval_navigation PRIVATE src)
target_compile_options(test_resident_naval_navigation PRIVATE -UNDEBUG -fno-fast-math -ffp-contract=off)
add_test(NAME resident_naval_navigation COMMAND test_resident_naval_navigation)
