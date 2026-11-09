#include "resident/domains/interaction/interaction_context.hpp"
// 本人授课的当前请求、实际课程与有限历史，不导出学生钱包或完整经验。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
Outcome<BoundaryValue> growth_study_teaching_view(const CommonStore&,const GrowthState&,const GrowthCatalog&,const CommonCatalog&,const SocialState&,const GrowthAccess&,const StableId& actor,const std::optional<InteractionContext>&,Integer page);
}
