# 公共接口：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：include/hanmo/resident/checkpoint_set.hpp。
set(HANMO_RESIDENT_SOURCES_public_api_checkpoint_set_hpp
    include/hanmo/resident/checkpoint_set.hpp
)

# 生产身份输入连续登记段：include/hanmo/resident/checkpoint_inline_function.hpp。
set(HANMO_RESIDENT_IDENTITY_public_api_checkpoint_inline_function_hpp
    include/hanmo/resident/checkpoint_inline_function.hpp
)

# 生产身份输入连续登记段：include/hanmo/resident/world_moment.hpp。
set(HANMO_RESIDENT_IDENTITY_public_api_world_moment_hpp
    include/hanmo/resident/world_moment.hpp
)

# 生产身份输入连续登记段：include/hanmo/resident/command_contract.hpp。
set(HANMO_RESIDENT_IDENTITY_public_api_command_contract_hpp
    include/hanmo/resident/command_contract.hpp
    include/hanmo/resident/model_contract.hpp
    include/hanmo/resident/phase_contract.hpp
    include/hanmo/resident/presentation_contract.hpp
    include/hanmo/resident_world_api.hpp
)
