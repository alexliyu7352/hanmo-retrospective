# 原生桥接生产库与协议消费者：保留同一核心依赖和原编译约束。
# 纯原生队列与调度实现进入独立生产库，不要求Godot SDK，也不以协议测试替代编译。
add_library(hanmo_resident_bridge_native STATIC
    ../godot/resident_bridge_queue.cpp
    ../godot/resident_bridge_live_jobs.cpp
)
target_include_directories(hanmo_resident_bridge_native PUBLIC include src ../godot)
target_link_libraries(hanmo_resident_bridge_native PUBLIC hanmo_resident Threads::Threads)

# 协议替身门只核队列行为，不实例化未链接的世界，不宣称P1玩法或性能通过。
add_executable(test_resident_bridge tests/resident/boundary_bridge_test.cpp ../godot/resident_bridge_queue.cpp)
target_include_directories(test_resident_bridge PRIVATE ../godot)
target_link_libraries(test_resident_bridge PRIVATE hanmo_resident Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_resident_bridge PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME resident_bridge_protocol COMMAND test_resident_bridge)
