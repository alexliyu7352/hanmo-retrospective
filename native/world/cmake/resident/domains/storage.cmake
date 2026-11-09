# 存储与槽介质：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/storage/actor_selection_execution.cpp。
set(HANMO_RESIDENT_SOURCES_storage_actor_selection_execution_cpp
    src/resident/storage/actor_selection_execution.cpp
)

# 生产输入连续登记段：src/resident/storage/common_checkpoint.cpp。
set(HANMO_RESIDENT_SOURCES_storage_common_checkpoint_cpp
    src/resident/storage/common_checkpoint.cpp
    src/resident/storage/common_codec.cpp
    src/resident/storage/common_store.cpp
    src/resident/storage/resource_execution_coverage.cpp
    src/resident/storage/actor_condition_execution_coverage.cpp
)

# 生产输入连续登记段：src/resident/storage/common_validation.cpp。
set(HANMO_RESIDENT_SOURCES_storage_common_validation_cpp
    src/resident/storage/common_validation.cpp
)

# 生产输入连续登记段：src/resident/storage/common_projection.cpp。
set(HANMO_RESIDENT_SOURCES_storage_common_projection_cpp
    src/resident/storage/common_projection.cpp
    src/resident/storage/inventory_entry_evidence.cpp
    src/resident/storage/inventory_entry_evidence_codec.cpp
    src/resident/storage/selected_resource_execution.cpp
    src/resident/storage/money_transfer_recipe.cpp
)

# 生产输入连续登记段：src/resident/storage/unique_item_transfer.cpp。
set(HANMO_RESIDENT_SOURCES_storage_unique_item_transfer_cpp
    src/resident/storage/unique_item_transfer.cpp
)

# 生产身份输入连续登记段：src/resident/storage/actor_selection_execution.cpp。
set(HANMO_RESIDENT_IDENTITY_storage_actor_selection_execution_cpp
    src/resident/storage/actor_selection_execution.cpp
    src/resident/storage/actor_selection_execution.hpp
)

# 生产身份输入连续登记段：src/resident/storage/common_checkpoint.cpp。
set(HANMO_RESIDENT_IDENTITY_storage_common_checkpoint_cpp
    src/resident/storage/common_checkpoint.cpp
    src/resident/storage/common_checkpoint.hpp
    src/resident/storage/common_codec.cpp
    src/resident/storage/common_location_scoped_codec.hpp
    src/resident/storage/common_codec.hpp
    src/resident/storage/common_store.cpp
    src/resident/storage/resource_execution_coverage.cpp
    src/resident/storage/actor_condition_execution_coverage.cpp
)

# 生产身份输入连续登记段：src/resident/storage/common_store.hpp。
set(HANMO_RESIDENT_IDENTITY_storage_common_store_hpp
    src/resident/storage/common_store.hpp
    src/resident/storage/common_capture_read_set.hpp
    src/resident/storage/protected_freight_resources.hpp
    src/resident/storage/protected_market_resources.hpp
    src/resident/storage/resource_execution_draft.hpp
    src/resident/storage/actor_condition_execution_draft.hpp
    src/resident/storage/actor_condition_savepoint_boundary.hpp
    src/resident/storage/resource_execution_coverage.hpp
    src/resident/storage/actor_condition_execution_coverage.hpp
)

# 生产身份输入连续登记段：src/resident/storage/common_validation.cpp。
set(HANMO_RESIDENT_IDENTITY_storage_common_validation_cpp
    src/resident/storage/common_validation.cpp
)

# 生产身份输入连续登记段：src/resident/storage/money_transfer_recipe.hpp。
set(HANMO_RESIDENT_IDENTITY_storage_money_transfer_recipe_hpp
    src/resident/storage/money_transfer_recipe.hpp
)

# 生产身份输入连续登记段：src/resident/storage/common_projection.cpp。
set(HANMO_RESIDENT_IDENTITY_storage_common_projection_cpp
    src/resident/storage/common_projection.cpp
    src/resident/storage/inventory_entry_evidence.cpp
    src/resident/storage/inventory_entry_evidence_codec.cpp
    src/resident/storage/selected_resource_execution.cpp
    src/resident/storage/money_transfer_recipe.cpp
)

# 生产身份输入连续登记段：src/resident/storage/unique_item_transfer.cpp。
set(HANMO_RESIDENT_IDENTITY_storage_unique_item_transfer_cpp
    src/resident/storage/unique_item_transfer.cpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
