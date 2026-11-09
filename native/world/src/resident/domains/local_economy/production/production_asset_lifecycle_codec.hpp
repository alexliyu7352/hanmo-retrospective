// 首次产权根 codec；只有显式新选型可用，实际权源仍须完整 owner 验证。
#pragma once
#include "production_asset_lifecycle.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
Outcome<ProductionAssetLifecycleState>
decode_production_asset_lifecycle(const BoundaryValue &);
Outcome<BoundaryValue>
encode_production_asset_lifecycle(const ProductionAssetLifecycleState &);
Status
emit_production_asset_lifecycle_fields(ProjectionWriter &,
                                       const ProductionAssetLifecycleState &);
Status visit_production_asset_lifecycle(CodecVisitor &,
                                        ProductionAssetLifecycleState &);
} // namespace hanmo::resident
