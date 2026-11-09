# S5定向库全部从同一新源码编译；保原世界资源原语，不链接其他布局的resident archive。
cmake_minimum_required(VERSION 3.20)
project(hanmo_economic_world_validation LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
get_filename_component(HANMO_WORLD_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${HANMO_WORLD_ROOT}/cmake/optional_toolchain.cmake")
set(HANMO_BUILD_RESIDENT_JOURNEY OFF CACHE BOOL "" FORCE)
set(HANMO_BUILD_GODOT_BRIDGE OFF CACHE BOOL "" FORCE)
add_subdirectory("${HANMO_WORLD_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/world")
file(GLOB ECONOMIC_DOMAIN_SOURCES CONFIGURE_DEPENDS
    "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/*.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/organization/*.cpp")
add_library(hanmo_economy_scoped STATIC ${ECONOMIC_DOMAIN_SOURCES}
    "${HANMO_WORLD_ROOT}/src/resident/domains/military/population_military_validation.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/military/military_integration_assets.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/military/battle_rules.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/military/battle_hex.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/growth/growth_services.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/growth/growth_life_results.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/views/local_economy_view.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/storage/common_store.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/storage/common_codec.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/storage/common_validation.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/storage/common_checkpoint.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/checkpoint/ordered_value.cpp"
    "${HANMO_WORLD_ROOT}/src/resident/checkpoint/canonical_digest.cpp")
target_include_directories(hanmo_economy_scoped PUBLIC "${HANMO_WORLD_ROOT}/src" "${HANMO_WORLD_ROOT}/include" PRIVATE "${HANMO_JSON_ROOT}/single_include" "${HANMO_PICOSHA2_ROOT}")
target_compile_options(hanmo_economy_scoped PRIVATE -fno-fast-math -ffp-contract=off)
hanmo_optional_compile_sections(hanmo_economy_scoped)
target_link_libraries(hanmo_economy_scoped PRIVATE hanmo_world)
foreach(name production_management local_economy_view production_market household_provision)
    add_executable(test_${name} ${name}_test.cpp)
    hanmo_optional_compile_sections(test_${name})
    hanmo_optional_link_option(test_${name} PRIVATE "LINKER:--gc-sections"
        HANMO_RESIDENT_GC_SECTIONS_SUPPORTED)
    target_link_libraries(test_${name} PRIVATE hanmo_economy_scoped)
endforeach()
# 跨owner合同必须来自真实S6头；本域可指定已核只读头覆盖目录，不在库中补造接口。
set(HANMO_FISCAL_CONTRACT_INCLUDE_ROOT "" CACHE PATH "Verified S6 fiscal contract include root")
if(HANMO_FISCAL_CONTRACT_INCLUDE_ROOT OR EXISTS "${HANMO_WORLD_ROOT}/src/resident/domains/politics/politics_fiscal_contract.hpp")
    add_executable(test_fiscal_settlement fiscal_settlement_test.cpp
        "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/fiscal/fiscal_settlement.cpp"
        "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/fiscal/fiscal_settlement_codec.cpp"
        "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/fiscal/fiscal_settlement_view.cpp")
    if(HANMO_FISCAL_CONTRACT_INCLUDE_ROOT)
        target_include_directories(test_fiscal_settlement PRIVATE "${HANMO_FISCAL_CONTRACT_INCLUDE_ROOT}")
    endif()
    hanmo_optional_compile_sections(test_fiscal_settlement)
    hanmo_optional_link_option(test_fiscal_settlement PRIVATE "LINKER:--gc-sections"
        HANMO_RESIDENT_GC_SECTIONS_SUPPORTED)
    target_link_libraries(test_fiscal_settlement PRIVATE hanmo_economy_scoped)
endif()

# 救济只引用逐OID核准的S3/S6合同头；资源实现仍使用当前同源定向库。
set(HANMO_GOVERNANCE_CONTRACT_INCLUDE_ROOT "" CACHE PATH "Verified S3/S6 governance contract include root")
if(HANMO_GOVERNANCE_CONTRACT_INCLUDE_ROOT)
    add_executable(test_public_relief public_relief_test.cpp
        "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/relief/public_relief_rules.cpp"
        "${HANMO_WORLD_ROOT}/src/resident/domains/local_economy/relief/public_relief_codec.cpp")
    target_include_directories(test_public_relief PRIVATE "${HANMO_GOVERNANCE_CONTRACT_INCLUDE_ROOT}")
    hanmo_optional_compile_sections(test_public_relief)
    hanmo_optional_link_option(test_public_relief PRIVATE "LINKER:--gc-sections"
        HANMO_RESIDENT_GC_SECTIONS_SUPPORTED)
    target_link_libraries(test_public_relief PRIVATE hanmo_economy_scoped)
endif()
