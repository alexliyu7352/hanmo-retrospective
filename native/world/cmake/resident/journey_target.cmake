# 常驻行程工具：只消费同一生产库、身份摘要与完整核心标识。
add_executable(hanmo_resident_journey tools/resident_journey_main.cpp)
target_include_directories(hanmo_resident_journey PRIVATE "${HANMO_PICOSHA2_ROOT}")
target_link_libraries(hanmo_resident_journey PRIVATE hanmo_resident)
target_compile_definitions(hanmo_resident_journey PRIVATE HANMO_RESIDENT_SOURCE_SHA="${HANMO_RESIDENT_SOURCE_SHA}")
# 实际factory与全部领域编译到同一目标；是否等价/达标仍由真实oracle执行决定。
target_compile_definitions(hanmo_resident_journey PRIVATE HANMO_RESIDENT_CORE_LINKED=1)

# 真新局薄宿主，不消费oracle；每个cold模式由独立进程运行原session。
add_executable(hanmo_resident_complete_thin tools/resident_complete_thin_main.cpp)
target_link_libraries(hanmo_resident_complete_thin PRIVATE hanmo_resident)
if(HANMO_RESIDENT_LINK_SLIMMING)
    # 原守门宿主同样分段并移除链接符号；不改变世界库或存档的业务内容。
    hanmo_optional_compile_sections(hanmo_resident_complete_thin)
    hanmo_optional_link_option(hanmo_resident_complete_thin PRIVATE -s
        HANMO_RESIDENT_STRIP_SUPPORTED)
endif()
