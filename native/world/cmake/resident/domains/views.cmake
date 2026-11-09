# 只读视图：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/views/dynamic_world_view.cpp。
set(HANMO_RESIDENT_SOURCES_views_dynamic_world_view_cpp
    src/resident/views/dynamic_world_view.cpp
    src/resident/views/regional_life_view.cpp
    src/resident/views/world_observation.cpp
    src/resident/views/map_presence.cpp
    src/resident/views/map_company.cpp
    src/resident/views/map_presence_codec.cpp
)

# 生产输入连续登记段：src/resident/views/membership_view.cpp。
set(HANMO_RESIDENT_SOURCES_views_membership_view_cpp
    src/resident/views/membership_view.cpp
    src/resident/views/self_profile_view.cpp
)

# 生产输入连续登记段：src/resident/views/character_presentation.cpp。
set(HANMO_RESIDENT_SOURCES_views_character_presentation_cpp
    src/resident/views/character_presentation.cpp
    src/resident/views/common_presentation.cpp
    src/resident/views/journey_character.cpp
    src/resident/views/journey_interaction.cpp
    src/resident/views/journey_motion.cpp
    src/resident/views/journey_view.cpp
    src/resident/views/player_frame_seed.cpp
    src/resident/views/scene_catalog.cpp
    src/resident/views/scene_presentation.cpp
)

# 生产身份输入连续登记段：src/resident/views/regional_life_view.hpp。
set(HANMO_RESIDENT_IDENTITY_views_regional_life_view_hpp
    src/resident/views/regional_life_view.hpp
    src/resident/views/world_observation.hpp
    src/resident/views/map_presence.hpp
    src/resident/views/map_company.hpp
)

# 生产身份输入连续登记段：src/resident/views/dynamic_world_view.cpp。
set(HANMO_RESIDENT_IDENTITY_views_dynamic_world_view_cpp
    src/resident/views/dynamic_world_view.cpp
    src/resident/views/regional_life_view.cpp
    src/resident/views/world_observation.cpp
    src/resident/views/map_presence.cpp
    src/resident/views/map_company.cpp
    src/resident/views/map_presence_codec.cpp
)

# 生产身份输入连续登记段：src/resident/views/membership_view.cpp。
set(HANMO_RESIDENT_IDENTITY_views_membership_view_cpp
    src/resident/views/membership_view.cpp
    src/resident/views/self_profile_view.cpp
    src/resident/views/dynamic_world_view.hpp
    src/resident/views/membership_view.hpp
    src/resident/views/self_profile_view.hpp
)

# 生产身份输入连续登记段：src/resident/views/character_presentation.cpp。
set(HANMO_RESIDENT_IDENTITY_views_character_presentation_cpp
    src/resident/views/character_presentation.cpp
    src/resident/views/character_presentation.hpp
    src/resident/views/common_presentation.cpp
    src/resident/views/common_presentation.hpp
    src/resident/views/journey_character.cpp
    src/resident/views/journey_character.hpp
    src/resident/views/journey_interaction.cpp
    src/resident/views/journey_interaction.hpp
    src/resident/views/journey_motion.cpp
    src/resident/views/journey_motion.hpp
    src/resident/views/journey_view.cpp
    src/resident/views/journey_view.hpp
    src/resident/views/player_frame_seed.cpp
    src/resident/views/player_frame_seed.hpp
    src/resident/views/scene_catalog.cpp
    src/resident/views/scene_presentation.cpp
    src/resident/views/scene_presentation.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
