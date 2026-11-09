# 个人行动：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/personal/personal_ai.cpp。
set(HANMO_RESIDENT_SOURCES_personal_personal_ai_cpp
    src/resident/domains/personal/personal_ai.cpp
    src/resident/domains/personal/personal_codec.cpp
    src/resident/domains/personal/personal_defeat_delay.cpp
    src/resident/domains/personal/personal_geometry.cpp
    src/resident/domains/personal/personal_round.cpp
    src/resident/domains/personal/personal_simultaneous_outcomes.cpp
    src/resident/domains/personal/personal_battle_lifecycle.cpp
    src/resident/domains/personal/personal_battle_lifecycle_codec.cpp
    src/resident/domains/personal/personal_session.cpp
    src/resident/domains/personal/personal_views.cpp
)

# 生产身份输入连续登记段：src/resident/domains/personal/personal_ai.cpp。
set(HANMO_RESIDENT_IDENTITY_personal_personal_ai_cpp
    src/resident/domains/personal/personal_ai.cpp
    src/resident/domains/personal/personal_codec.cpp
    src/resident/domains/personal/personal_defeat_delay.cpp
    src/resident/domains/personal/personal_geometry.cpp
    src/resident/domains/personal/personal_geometry.hpp
    src/resident/domains/personal/personal_battle_lifecycle.hpp
    src/resident/domains/personal/personal_battle_lifecycle_codec.hpp
    src/resident/domains/personal/personal_model.hpp
    src/resident/domains/personal/personal_simultaneous_outcomes.hpp
    src/resident/domains/personal/personal_round.cpp
    src/resident/domains/personal/personal_simultaneous_outcomes.cpp
    src/resident/domains/personal/personal_rules_internal.hpp
    src/resident/domains/personal/personal_battle_lifecycle.cpp
    src/resident/domains/personal/personal_battle_lifecycle_codec.cpp
    src/resident/domains/personal/personal_session.cpp
    src/resident/domains/personal/personal_views.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/domains/personal/source_owned_fields/domain_fields.inc
    src/resident/domains/personal/world_codec/catalog_original.inc
    src/resident/domains/personal/world_codec/checkpoint_read_personal.inc
    src/resident/domains/personal/world_codec/seed_original_lifecycle.inc
)
