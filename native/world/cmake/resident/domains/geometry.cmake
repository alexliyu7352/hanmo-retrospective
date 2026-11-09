# 空间几何：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/geometry/route_cache.cpp。
set(HANMO_RESIDENT_SOURCES_geometry_route_cache_cpp
    src/resident/geometry/route_cache.cpp
    src/resident/geometry/terrain.cpp
)

# 生产身份输入连续登记段：src/resident/geometry/route_cache.cpp。
set(HANMO_RESIDENT_IDENTITY_geometry_route_cache_cpp
    src/resident/geometry/route_cache.cpp
    src/resident/geometry/route_cache.hpp
    src/resident/geometry/terrain.cpp
    src/resident/geometry/terrain.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
