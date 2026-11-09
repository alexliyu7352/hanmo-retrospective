// FSL§2同版完整边界的成对冷编码；只接真实service/rest，不另开世界存档。
#pragma once
#include "growth_fixed_service_model.hpp"
#include "growth_result_ref_fields.hpp"
#include <string_view>
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident {
class ProjectionWriter;
class ScopedCheckpointJsonDecode;
Status emit_growth_fixed_service_boundary_projection(ProjectionWriter&,std::string_view,const LifeFixedServiceBoundary&);
// 可嵌入Growth原段；失败须撤销整个私有decode候选，外层负责完整finish。
Status visit_growth_fixed_service_boundary(CodecVisitor&,LifeFixedServiceBoundary&);
// 单边界JSON保持全部必填槽和显式null；拒绝未知字段、未接分支及非正规时间。
Outcome<BoundaryValue> encode_growth_fixed_service_boundary(const LifeFixedServiceBoundary&);
Outcome<LifeFixedServiceBoundary> decode_growth_fixed_service_boundary(const BoundaryValue&);
// 独立验证封套消费完整字节，正式World仍调用上方同一visitor。
Outcome<CodecVisitor::Buffer> encode_growth_fixed_service_boundary_checkpoint(const LifeFixedServiceBoundary&);
Outcome<LifeFixedServiceBoundary> decode_growth_fixed_service_boundary_checkpoint(const CodecVisitor::Buffer&);
} // 不在codec重建历史、应用业务或生成任何来源。

namespace hanmo::resident {
Status emit_growth_complete_fixed_service_boundary_projection(ProjectionWriter&,std::string_view,const LifeFixedServiceBoundary&,bool exact_periods=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Outcome<BoundaryValue> encode_growth_complete_fixed_service_boundary(const LifeFixedServiceBoundary&,bool exact_periods=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Outcome<LifeFixedServiceBoundary> decode_growth_complete_fixed_service_boundary(const BoundaryValue&,bool exact_periods=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Status visit_growth_complete_fixed_service_boundary(CodecVisitor&,LifeFixedServiceBoundary&,bool exact_periods=false,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
}

namespace hanmo::resident {
// FH5.3B 的 Care 原命令来源借同字段表；这些入口只选完整 Actor route。
Status visit_life_actual_command_binding(CodecVisitor&,LifeActualCommandBinding&);
Status emit_life_actual_command_binding_fields(ProjectionWriter&,const LifeActualCommandBinding&);
Outcome<LifeActualCommandBinding> decode_life_actual_command_binding(const BoundaryValue&);
Status visit_life_actor_route(CodecVisitor&,LifeActorRoute&,const StableId& actual_actor);
Status emit_life_actor_route_fields(ProjectionWriter&,const LifeActorRoute&,const StableId& actual_actor);
Outcome<LifeActorRoute> decode_life_actor_route(const BoundaryValue&,const StableId& actual_actor);
Outcome<LifeActorRoute> decode_life_actor_route(const BoundaryValue&,const StableId& actual_actor,ScopedCheckpointJsonDecode&);
Outcome<LifeActualCommandBinding> decode_life_actual_command_binding(const BoundaryValue&,ScopedCheckpointJsonDecode&);
}
