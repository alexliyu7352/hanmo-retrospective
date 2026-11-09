#include "growth_supervision_qualification.hpp"
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
Outcome<bool> read_growth_supervision_skill_eligible(const CharacterConditionSources& body,const GrowthState& growth,const GrowthSkillQualificationV2& q,const GrowthSkillBoundarySourceRead& source,const StableId& actor,const GrowthSupervisionSkillRequirement& requirement,const MomentSpan& span){
 auto window=make_moment_span(span.from,span.to);if(!window.ok)return {false,{},window.error};auto starts=compare_world_moments(q.observed_at,span.from);if(!starts.ok)return {false,{},starts.error};auto future=compare_world_moments(span.to,body.actual_now);if(!future.ok)return {false,{},future.error};
 if(body.growth!=&growth||q.actor_id!=actor||q.skill_id!=requirement.skill_id||requirement.minimum_level<0||requirement.minimum_level>4||q.coverage_kind!=GrowthSkillQualificationV2::Coverage::complete||starts.value>0||future.value>0)return {false,{},common_failure("SupervisionOriginalSkillRequirementMismatch",actor).error};
 auto valid=validate_growth_skill_qualification_before_source_v2(body,growth,q,source);if(!valid.ok)return {false,{},valid.error};
 // The unique qualification reader closes every actual net contribution against
 // the original initial value and rejects negative/unexplained XP. A verified
 // old complete qualification is therefore a lower bound throughout this span.
 // Body capability remains a separate original normal-work reducer with null
 // skill. Level never scales its efficiency or creates a new exertion amount.
 return {true,q.minimum_level>=requirement.minimum_level,{}};
}
}
