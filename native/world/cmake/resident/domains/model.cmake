# 世界模型：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/model/commitment_read.cpp。
set(HANMO_RESIDENT_SOURCES_model_commitment_read_cpp
    src/resident/model/commitment_read.cpp
    src/resident/model/commitment_ref.cpp
    src/resident/model/objective_evidence.cpp
    src/resident/model/professional_outcome_read.cpp
)

# 生产身份输入连续登记段：src/resident/model/dynamic_contract.hpp。
set(HANMO_RESIDENT_IDENTITY_model_dynamic_contract_hpp
    src/resident/model/dynamic_contract.hpp
    src/resident/model/offer_ref.hpp
    src/resident/model/regional_life_contract.hpp
)

# 生产身份输入连续登记段：src/resident/model/procurement_timing.hpp。
set(HANMO_RESIDENT_IDENTITY_model_procurement_timing_hpp
    src/resident/model/procurement_timing.hpp
)

# 生产身份输入连续登记段：src/resident/model/common_model.hpp。
set(HANMO_RESIDENT_IDENTITY_model_common_model_hpp
    src/resident/model/common_model.hpp
    src/resident/model/common_operations.hpp
    src/resident/model/common_rng.hpp
    src/resident/model/common_rng_LICENSE.txt
    src/resident/model/common_string_hash.hpp
)

# 生产身份输入连续登记段：src/resident/model/commitment_read.cpp。
set(HANMO_RESIDENT_IDENTITY_model_commitment_read_cpp
    src/resident/model/commitment_read.cpp
    src/resident/model/commitment_ref.cpp
    src/resident/model/objective_evidence.cpp
    src/resident/model/professional_outcome_read.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
