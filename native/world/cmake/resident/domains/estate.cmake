# 产业与设施：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/estate/estate_private_dependencies.cpp。
set(HANMO_RESIDENT_SOURCES_estate_estate_private_dependencies_cpp
    src/resident/domains/estate/estate_private_dependencies.cpp
)

# 生产输入连续登记段：src/resident/domains/estate/estate_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_estate_estate_checkpoint_cpp
    src/resident/domains/estate/estate_checkpoint.cpp
    src/resident/domains/estate/estate_codec.cpp
    src/resident/domains/estate/estate_commands.cpp
    src/resident/domains/estate/estate_construction.cpp
    src/resident/domains/estate/estate_rules.cpp
)

# 生产输入连续登记段：src/resident/domains/estate/employment/estate_employment_boundary.cpp。
set(HANMO_RESIDENT_SOURCES_estate_estate_employment_boundary_cpp
    src/resident/domains/estate/employment/estate_employment_boundary.cpp
    src/resident/domains/estate/employment/estate_employment_codec.cpp
    src/resident/domains/estate/employment/estate_employment_internal.cpp
    src/resident/domains/estate/employment/estate_employment_read.cpp
    src/resident/domains/estate/employment/estate_employment_rules.cpp
    src/resident/domains/estate/employment/estate_employment_validation.cpp
)

# 生产输入连续登记段：src/resident/domains/estate/estate_views.cpp。
set(HANMO_RESIDENT_SOURCES_estate_estate_views_cpp
    src/resident/domains/estate/estate_views.cpp
)

# 生产输入连续登记段：src/resident/domains/estate/estate_rest_adoption_codec.cpp。
set(HANMO_RESIDENT_SOURCES_estate_estate_rest_adoption_codec_cpp
    src/resident/domains/estate/estate_rest_adoption_codec.cpp
    src/resident/domains/estate/estate_rest_history.cpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/estate_detachment_release.hpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_detachment_release_hpp
    src/resident/domains/estate/estate_detachment_release.hpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/estate_private_dependencies.cpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_private_dependencies_cpp
    src/resident/domains/estate/estate_private_dependencies.cpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/estate_private_dependencies.hpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_private_dependencies_hpp
    src/resident/domains/estate/estate_private_dependencies.hpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/estate_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_checkpoint_cpp
    src/resident/domains/estate/estate_checkpoint.cpp
    src/resident/domains/estate/estate_codec.cpp
    src/resident/domains/estate/estate_commands.cpp
    src/resident/domains/estate/estate_construction.cpp
    src/resident/domains/estate/estate_model.hpp
    src/resident/domains/estate/estate_rules.cpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/employment/estate_employment.hpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_employment_hpp
    src/resident/domains/estate/employment/estate_employment.hpp
    src/resident/domains/estate/employment/estate_employment_boundary.hpp
    src/resident/domains/estate/employment/estate_employment_codec.hpp
    src/resident/domains/estate/employment/estate_employment_contract.hpp
    src/resident/domains/estate/employment/estate_employment_internal.hpp
    src/resident/domains/estate/employment/estate_employment_profile.hpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/employment/estate_employment_boundary.cpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_employment_boundary_cpp
    src/resident/domains/estate/employment/estate_employment_boundary.cpp
    src/resident/domains/estate/employment/estate_employment_codec.cpp
    src/resident/domains/estate/employment/estate_employment_internal.cpp
    src/resident/domains/estate/employment/estate_employment_read.cpp
    src/resident/domains/estate/employment/estate_employment_rules.cpp
    src/resident/domains/estate/employment/estate_employment_validation.cpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/estate_views.cpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_views_cpp
    src/resident/domains/estate/estate_views.cpp
)

# 生产身份输入连续登记段：src/resident/domains/estate/estate_rest_adoption_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_estate_estate_rest_adoption_codec_cpp
    src/resident/domains/estate/estate_rest_adoption_codec.cpp
    src/resident/domains/estate/estate_rest_history.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/estate/source_owned_fields/domain_fields.inc
    src/resident/domains/estate/world_codec/catalog_employment.inc
    src/resident/domains/estate/world_codec/catalog_original.inc
    src/resident/domains/estate/world_codec/checkpoint_read_estate.inc
    src/resident/domains/estate/world_codec/checkpoint_read_estate_employment.inc
    src/resident/domains/estate/world_codec/seed_employment.inc
    src/resident/domains/estate/world_codec/seed_original.inc
)
