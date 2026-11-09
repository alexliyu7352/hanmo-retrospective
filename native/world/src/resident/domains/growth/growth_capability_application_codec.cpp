// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "growth_capability_application_codec.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
Status emit_growth_capability_application(ProjectionWriter& out,std::string_view name,const GrowthCapabilityApplication& value,GrowthContributionWireVersion version){return emit_projection_record<ActorFoundationProjectionArchive>(out,name,value,[&](auto& c,auto& x){return growth_capability_codec::application(c,x,version);});}
Outcome<GrowthCapabilityApplication> decode_growth_capability_application(const BoundaryValue& input,GrowthContributionWireVersion version){GrowthCapabilityApplication value;ActorFoundationJsonVisitor reader(input);auto status=growth_capability_codec::application(reader,value,version);if(status.ok)status=reader.finish();return {status.ok,std::move(value),status.error};}
}
