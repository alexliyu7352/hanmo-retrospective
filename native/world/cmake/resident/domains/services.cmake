# 工作服务：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/domains/services/work_service_scope_read.cpp。
set(HANMO_RESIDENT_SOURCES_services_work_service_scope_read_cpp
    src/resident/domains/services/work_service_scope_read.cpp
    src/resident/domains/services/study_service_scope_read.cpp
    src/resident/domains/services/service_actual_termination_result.cpp
    src/resident/domains/services/repair_service_scope_read.cpp
)

# 生产输入连续登记段：src/resident/domains/services/service_actual_termination_codec.cpp。
set(HANMO_RESIDENT_SOURCES_services_service_actual_termination_codec_cpp
    src/resident/domains/services/service_actual_termination_codec.cpp
    src/resident/domains/services/service_actual_termination_contract.cpp
    src/resident/domains/services/service_actual_termination_result_codec.cpp
)

# 生产身份输入连续登记段：src/resident/domains/services/work_service_scope_read.hpp。
set(HANMO_RESIDENT_IDENTITY_services_work_service_scope_read_hpp
    src/resident/domains/services/work_service_scope_read.hpp
    src/resident/domains/services/work_service_scope_read.cpp
    src/resident/domains/services/study_service_scope_read.hpp
    src/resident/domains/services/study_service_scope_read.cpp
    src/resident/domains/services/service_actual_termination_result_fields.hpp
    src/resident/domains/services/service_actual_termination_result.hpp
    src/resident/domains/services/service_actual_termination_result.cpp
    src/resident/domains/services/repair_service_scope_read.hpp
    src/resident/domains/services/repair_service_scope_read.cpp
)

# 生产身份输入连续登记段：src/resident/domains/services/service_actual_termination_codec.cpp。
set(HANMO_RESIDENT_IDENTITY_services_service_actual_termination_codec_cpp
    src/resident/domains/services/service_actual_termination_codec.cpp
    src/resident/domains/services/service_actual_termination_contract.cpp
    src/resident/domains/services/service_actual_termination_result_codec.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
