# 世界索引：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/indexes/contact_geometry.cpp。
set(HANMO_RESIDENT_SOURCES_indexes_contact_geometry_cpp
    src/resident/indexes/contact_geometry.cpp
)

# 生产身份输入连续登记段：src/resident/indexes/contact_geometry.cpp。
set(HANMO_RESIDENT_IDENTITY_indexes_contact_geometry_cpp
    src/resident/indexes/contact_geometry.cpp
    src/resident/indexes/contact_model.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
