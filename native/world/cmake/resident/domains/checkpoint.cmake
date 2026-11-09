# 检查点与延续：显式保存本领域生产文件与身份清单，顺序由登记入口组合。
# 连续段不合并重排，确保原目标和摘要的路径顺序保持不变。

# 生产输入连续登记段：src/resident/checkpoint/checkpoint_job_budget.cpp。
set(HANMO_RESIDENT_SOURCES_checkpoint_checkpoint_job_budget_cpp
    src/resident/checkpoint/checkpoint_job_budget.cpp
    src/resident/checkpoint/checkpoint_capacity.cpp
    src/resident/checkpoint/checkpoint_json_decode.cpp
)

# 生产输入连续登记段：src/resident/checkpoint/canonical_digest.cpp。
set(HANMO_RESIDENT_SOURCES_checkpoint_canonical_digest_cpp
    src/resident/checkpoint/canonical_digest.cpp
    src/resident/checkpoint/checkpoint_envelope.cpp
    src/resident/checkpoint/command_boundary.cpp
    src/resident/checkpoint/ordered_value.cpp
)

# 生产输入连续登记段：src/resident/checkpoint/checkpoint_copy_shape.cpp。
set(HANMO_RESIDENT_SOURCES_checkpoint_checkpoint_copy_shape_cpp
    # Complete native source owners; explicit compiled files, no runtime discovery.
    src/resident/checkpoint/checkpoint_copy_shape.cpp
    src/resident/checkpoint/checkpoint_domain_list.cpp
    src/resident/checkpoint/checkpoint_job_memory.cpp
    src/resident/checkpoint/checkpoint_memory.cpp
    src/resident/checkpoint/checkpoint_retention.cpp
    src/resident/checkpoint/common_codec_selection.cpp
    src/resident/checkpoint/projection_composition_index.cpp
    src/resident/checkpoint/projection_writer.cpp
)

# 生产身份输入连续登记段：src/resident/checkpoint/checkpoint_job_budget.cpp。
set(HANMO_RESIDENT_IDENTITY_checkpoint_checkpoint_job_budget_cpp
    src/resident/checkpoint/checkpoint_job_budget.cpp
    src/resident/checkpoint/checkpoint_job_budget.hpp
    src/resident/checkpoint/checkpoint_not_implemented.hpp
)

# 生产身份输入连续登记段：src/resident/checkpoint/checkpoint_capacity.cpp。
set(HANMO_RESIDENT_IDENTITY_checkpoint_checkpoint_capacity_cpp
    src/resident/checkpoint/checkpoint_capacity.cpp
    src/resident/checkpoint/checkpoint_capacity.hpp
)

# 生产身份输入连续登记段：src/resident/checkpoint/checkpoint_json_decode.hpp。
set(HANMO_RESIDENT_IDENTITY_checkpoint_checkpoint_json_decode_hpp
    src/resident/checkpoint/checkpoint_json_decode.hpp
    src/resident/checkpoint/checkpoint_catalog_json_kind.hpp
    src/resident/checkpoint/checkpoint_json_decode.cpp
    src/resident/checkpoint/checkpoint_stream.hpp
)

# 生产身份输入连续登记段：src/resident/checkpoint/canonical_digest.cpp。
set(HANMO_RESIDENT_IDENTITY_checkpoint_canonical_digest_cpp
    src/resident/checkpoint/canonical_digest.cpp
    src/resident/checkpoint/canonical_digest.hpp
    src/resident/checkpoint/checkpoint_envelope.cpp
    src/resident/checkpoint/checkpoint_envelope.hpp
    src/resident/checkpoint/checkpoint_selection.hpp
    src/resident/checkpoint/codec_visitor.hpp
    src/resident/checkpoint/command_boundary.cpp
    src/resident/checkpoint/completion_codec.hpp
    src/resident/checkpoint/ordered_value.cpp
    src/resident/checkpoint/ordered_value.hpp
)

# 生产身份输入连续登记段：src/resident/checkpoint/checkpoint_copy_shape.cpp。
set(HANMO_RESIDENT_IDENTITY_checkpoint_checkpoint_copy_shape_cpp
    # Explicit compiled/central successor identity, not an author-selected file scan.
    src/resident/checkpoint/checkpoint_copy_shape.cpp
    src/resident/checkpoint/checkpoint_domain_list.cpp
    src/resident/checkpoint/checkpoint_job_memory.cpp
    src/resident/checkpoint/checkpoint_memory.cpp
    src/resident/checkpoint/checkpoint_retention.cpp
    src/resident/checkpoint/common_codec_selection.cpp
    src/resident/checkpoint/common_codec_selection.hpp
    src/resident/checkpoint/projection_composition_index.cpp
    src/resident/checkpoint/projection_record_archive.hpp
    src/resident/checkpoint/projection_writer.cpp
    src/resident/checkpoint/projection_writer.hpp
)

# 生产身份输入连续登记段：src/resident/checkpoint/checkpoint_catalog_rows.hpp。
set(HANMO_RESIDENT_IDENTITY_checkpoint_checkpoint_catalog_rows_hpp
    src/resident/checkpoint/checkpoint_catalog_rows.hpp
)

# 本领域的拆分实现片段：只随原翻译单元包含，同时进入目标与生产身份清单。
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
    src/resident/checkpoint/world_codec/catalog_domain_registry.inc
    src/resident/checkpoint/world_codec/catalog_module_registration.inc
    src/resident/checkpoint/world_codec/catalog_native_registration.inc
    src/resident/checkpoint/world_codec/catalog_prepare_registry.inc
    src/resident/checkpoint/world_codec/catalog_time_registration.inc
    src/resident/checkpoint/world_codec/checkpoint_read_profile_deliveries.inc
    src/resident/checkpoint/world_codec/checkpoint_read_registry.inc
    src/resident/checkpoint/world_codec/checkpoint_selection.inc
    src/resident/checkpoint/world_codec/checkpoint_snapshot_envelope.inc
    src/resident/checkpoint/world_codec/checkpoint_stream_decode.inc
    src/resident/checkpoint/world_codec/checkpoint_write_registry.inc
    src/resident/checkpoint/world_codec/envelope_inspection.inc
    src/resident/checkpoint/world_codec/export_composition_registry.inc
    src/resident/checkpoint/world_codec/export_fact_bindings.inc
    src/resident/checkpoint/world_codec/identity_domain_entry.inc
    src/resident/checkpoint/world_codec/identity_domain_registry.inc
    src/resident/checkpoint/world_codec/result_receipts.inc
    src/resident/checkpoint/world_codec/seed_decode_ownership.inc
    src/resident/checkpoint/world_codec/seed_decode_registry.inc
    src/resident/checkpoint/world_codec/selection_support.inc
)

# H1：原默认形状声明与原 get 片段显式登记；仍只有原唯一实现 TU。
list(APPEND HANMO_RESIDENT_IDENTITY_checkpoint_checkpoint_json_decode_hpp
)
list(APPEND HANMO_RESIDENT_SPLIT_IMPLEMENTATION_SOURCES
)
