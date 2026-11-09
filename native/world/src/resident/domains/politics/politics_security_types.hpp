// politics_governance/2唯一治安当前值与真实原成果后果；不在M-GOV另存治安。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/domains/growth/growth_skill_qualification_v2.hpp"
namespace hanmo::resident {
struct JurisdictionSecurityInitial {StableId jurisdiction_id;ActorFoundationRuleRef original_jurisdiction_definition;Integer security=0;};
struct PoliticalSecurityCatalog {CatalogIdentity identity;Integer initial_tick=0;OrderedTable<JurisdictionSecurityInitial> initial_security;};
struct JurisdictionSecurityState {StableId jurisdiction_id,registration_fact_id,last_fact_id;ActorFoundationRuleRef original_jurisdiction_definition;Integer security=0,revision=0;};
// 冻结实际Work终点的原技能历史，不用今日人物技能重算旧后果。
// 旧/2候选值只供原物理入口保真；/3只接受原完整source2，不由摘要猜迁移。
struct LegacyPatrolSecuritySkillSnapshot {StableId actor_id,skill_id;Integer experience=0,level=0;std::vector<MomentOwnerSource> original_qualification_sources;};
using PatrolSecuritySkillSnapshot=std::variant<LegacyPatrolSecuritySkillSnapshot,GrowthSkillQualificationV2>;
struct SecurityConsequence {
 StableId id,jurisdiction_id;SourceRef program_ref,actual_work_result;StableId target_line_id,previous_fact_id;
 ActorFoundationRuleRef program_spec_ref;std::optional<PatrolSecuritySkillSnapshot> skill_at_completion;
 Integer before=0,after=0,requested_delta=0,revision=0;WorldMoment applied_at;
};
}
