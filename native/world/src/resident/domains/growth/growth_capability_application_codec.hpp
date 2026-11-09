#pragma once
#include <string_view>
#include <type_traits>
#include "growth_capability_application.hpp"
#include "growth_result_ref_fields.hpp"
namespace hanmo::resident { struct CodecVisitor; }
namespace hanmo::resident::growth_capability_codec {
template<class C>Status change(C& c,GrowthCapabilityChangeRef& value,GrowthContributionWireVersion version=GrowthContributionWireVersion::original){if(version!=GrowthContributionWireVersion::original&&version!=GrowthContributionWireVersion::trade_grants)return c.reject("GrowthContributionWireVersionUnsupported");Integer tag=static_cast<Integer>(value.index());c.field("tag",tag);if(!c.status().ok)return c.status();if(tag<0||tag>3)return c.reject("GrowthCapabilityChangeTagInvalid");if(c.reading()){switch(tag){case 0:value=InitialConditionSeedRef{};break;case 1:value=ConditionReceiptRefV2{};break;case 2:value=AttributeMilestoneRef{};break;case 3:value=LifeGrowthContributionRef{};break;}}if(tag==0)return growth_result_ref_codec::ref(c,"seed_key",std::get<InitialConditionSeedRef>(value).seed_key);if(tag==1)return growth_result_ref_codec::ref(c,"result_id",std::get<ConditionReceiptRefV2>(value).result_id);if(tag==2)return growth_result_ref_codec::ref(c,"milestone_result_id",std::get<AttributeMilestoneRef>(value).milestone_result_id);auto& original=std::get<LifeGrowthContributionRef>(value);
// 新显式来源格式的JSON字典分开两层tag；binary按序的原字段和旧格式均不变。
if constexpr(!std::is_same_v<C,CodecVisitor>){if(version==GrowthContributionWireVersion::trade_grants){c.object("original_result_ref",[&](auto& x){return growth_result_ref_codec::fields(x,original,version);});return c.status().ok&&std::holds_alternative<AttributeMilestoneRef>(original)?c.reject("GrowthCapabilityMilestoneCanonicalRefRequired"):c.status();}}
growth_result_ref_codec::fields(c,original,version);return c.status().ok&&std::holds_alternative<AttributeMilestoneRef>(original)?c.reject("GrowthCapabilityMilestoneCanonicalRefRequired"):c.status();}
template<class C>Status application(C& c,GrowthCapabilityApplication& value,GrowthContributionWireVersion version=GrowthContributionWireVersion::original){c.field("revision_before",value.revision_before);c.object("previous",[&](auto& x){return change(x,value.previous,version);});if(c.status().ok&&(value.revision_before<0||(value.revision_before==0)!=std::holds_alternative<InitialConditionSeedRef>(value.previous)))return c.reject("GrowthCapabilityPredecessorInvalid");return c.status();}
}

namespace hanmo::resident {
class ProjectionWriter;
Status emit_growth_capability_application(ProjectionWriter&,std::string_view,const GrowthCapabilityApplication&,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
Outcome<GrowthCapabilityApplication> decode_growth_capability_application(const BoundaryValue&,GrowthContributionWireVersion=GrowthContributionWireVersion::original);
}
