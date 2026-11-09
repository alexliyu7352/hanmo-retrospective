# 设施占用：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/facility_claims/facility_claim_runtime.cpp。
set(HANMO_RESIDENT_SOURCES_facility_claims_facility_claim_runtime_cpp
    src/resident/facility_claims/facility_claim_runtime.cpp
    src/resident/facility_claims/facility_claims.cpp
    src/resident/facility_claims/facility_claims_codec.cpp
    src/resident/facility_claims/facility_exact_phase.cpp
    src/resident/facility_claims/facility_physical_validation.cpp
    src/resident/facility_claims/validated_facility_index.cpp
)

# 生产身份输入连续登记段：src/resident/facility_claims/facility_claim_runtime.cpp。
set(HANMO_RESIDENT_IDENTITY_facility_claims_facility_claim_runtime_cpp
    src/resident/facility_claims/facility_claim_runtime.cpp
    src/resident/facility_claims/facility_claim_runtime.hpp
    src/resident/facility_claims/facility_claims.cpp
    src/resident/facility_claims/facility_claims.hpp
    src/resident/facility_claims/facility_claims_candidate.hpp
    src/resident/facility_claims/facility_claims_codec.cpp
    src/resident/facility_claims/facility_claims_codec.hpp
    src/resident/facility_claims/facility_claims_codec_fields.hpp
    src/resident/facility_claims/facility_exact_phase.cpp
    src/resident/facility_claims/facility_exact_phase.hpp
    src/resident/facility_claims/facility_physical_validation.cpp
    src/resident/facility_claims/facility_physical_validation.hpp
    src/resident/facility_claims/model.hpp
    src/resident/facility_claims/validated_facility_index.cpp
    src/resident/facility_claims/validated_facility_index.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/facility_claims/world_codec/checkpoint_read_facility_claims.inc
)
