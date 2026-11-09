#include "resident/domains/growth/growth_capability_application.hpp"
// v1b 4.1 original permanent contributions. Common remains the only skill balance.
#pragma once
#include "growth_result_refs.hpp"
#include <string_view>
#include <span>
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "resident/domains/tasks/work_execution/work_result_source.hpp"
namespace hanmo::resident {
class CommonStore;class CodecVisitor;class ProjectionWriter;struct CharacterConditionSources;struct GrowthState;struct BoundActorConditionWrite;
// Only the actual Work adapter is compiled here. Its source tag is the fixed
// WorkContribution tag 2; no arbitrary SourceRef can select another profession.
struct WorkPracticeContribution {WorkResultSource result;std::vector<StableId> participation_refs;};
using PracticeSource=std::variant<WorkPracticeContribution>;
enum class PracticeContributionRole {work_leader};
struct PracticeSkillAward {StableId skill_id;Integer requested_xp=0;};
struct WorkPracticeCriterion {WorkKind work_kind=WorkKind::patrol;Integer minimum_credited_half_days=0;};
struct PracticeAwardRule {
 ActorFoundationRuleRef rule_ref;StableId rule_family;WorkPracticeCriterion criterion;
 PracticeContributionRole contribution_role=PracticeContributionRole::work_leader;
 std::vector<PracticeSkillAward> skill_awards;
};
struct AttributeMilestoneRule {
 ActorFoundationRuleRef rule_ref;StableId unique_milestone_id,attribute_id;
 WorkPracticeCriterion criterion;
};
struct GrowthPermanentCatalog {
 CatalogIdentity identity;OrderedTable<PracticeAwardRule> practice_rules;
 OrderedTable<AttributeMilestoneRule> milestone_rules;
};
struct PracticeSkillReceipt {StableId skill_id;Integer xp_before=0,requested_xp=0,actual_xp_gain=0,xp_after=0;};
struct PracticeAwardReceipt {
 StableId award_id,actor_id;ActorFoundationRuleRef practice_rule_ref;PracticeSource original_source;
 PracticeContributionRole contribution_role=PracticeContributionRole::work_leader;
 std::vector<PracticeSkillReceipt> skill_rows;ActorFoundationAt applied_at;SourceRef original_fact_ref;
 std::optional<GrowthCapabilityApplication> capability_application;
};
struct AttributeMilestone {
 StableId milestone_result_id,actor_id,attribute_id;ActorFoundationRuleRef milestone_rule_ref;
 PracticeSource original_outcome_ref;StableId unique_milestone_id;Integer delta=1;ActorFoundationAt applied_at;
 std::optional<GrowthCapabilityApplication> capability_application;
};
struct GrowthCapabilityRevision {StableId actor_id;Integer revision=0;GrowthCapabilityChangeRef last_change;};
// The first three roots of growth_permanent_contributions/1. Its last two
// roots borrow the existing CardAcquisitionState, never a second owned copy.
struct GrowthPermanentState {
 OrderedTable<AttributeMilestone> milestones;OrderedTable<PracticeAwardReceipt> practice_awards;
 OrderedTable<GrowthCapabilityRevision> capability_revisions;
};
struct WorkPracticeContributionView {
 WorldMoment completed_at;WorkKind work_kind=WorkKind::patrol;Integer credited_half_days=0;
 SourceRef original_result;std::span<const StableId> actual_participation_refs;
};
// Original S3 owns full execution/period validation. This leg must not recurse
// through the skill qualification stored in the same WorkResult.
class WorkPracticeContributionRead {
public:virtual ~WorkPracticeContributionRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<WorkPracticeContributionView> read_actual_work_contribution(const StableId& actor,const WorkPracticeContribution&,const WorldMoment& actual_now)const=0;
};
class GrowthCapabilityRevisionWrite {
public:virtual ~GrowthCapabilityRevisionWrite()=default;
 virtual bool bound_to(const CommonStore&,const GrowthPermanentState&)const noexcept=0;
 virtual Status record_actual_growth_change(const StableId& actor,const LifeGrowthContributionRef&,const WorldMoment&)=0;
 virtual Status record_actual_condition_change(const BoundActorConditionWrite&)=0;
};
struct GrowthPermanentSources {
 const CharacterConditionSources& condition;const GrowthPermanentCatalog& catalog;
 const GrowthPermanentState& state;const WorkPracticeContributionRead& work;
};
// Central typed author installation supplies the already-locked catalogue identity.
Outcome<PracticeAwardRule> decode_work_practice_definition(const BoundaryValue&,const CatalogIdentity&);
Outcome<AttributeMilestoneRule> decode_work_attribute_milestone_definition(const BoundaryValue&,const CatalogIdentity&);
// data.practice_award_rules, data.attribute_milestone_rules and the explicitly
// selected scenario.growth_permanent_contributions; no implicit selection.
Outcome<GrowthPermanentCatalog> decode_growth_permanent_catalog(const BoundaryValue& practice_definitions,const BoundaryValue& milestone_definitions,const BoundaryValue& selected_config,const CatalogIdentity&,const CommonCatalog&);
Status validate_growth_permanent_catalog(const GrowthPermanentCatalog&,const CommonCatalog&);
Outcome<const PracticeAwardReceipt*> read_verified_practice_award(const GrowthPermanentSources&,const PracticeAwardRef&);
Outcome<const AttributeMilestone*> read_verified_attribute_milestone(const GrowthPermanentSources&,const AttributeMilestoneRef&);
Status validate_growth_permanent_results(const GrowthPermanentSources&);
struct PracticeAwardApplication {PracticeAwardRef result;bool already_applied=false;};
// Rule and source are locked actual owner inputs. The caller owns the complete
// World candidate; every Common XP/receipt/header/revision change rolls back together.
Outcome<PracticeAwardApplication> apply_work_practice_in_candidate(CommonStore&,GrowthPermanentState&,const GrowthPermanentSources&,const StableId& actor,const ActorFoundationRuleRef&,const WorkPracticeContribution&,GrowthCapabilityRevisionWrite&);
Outcome<AttributeMilestoneRef> apply_work_attribute_milestone_in_candidate(CommonStore&,GrowthPermanentState&,const GrowthPermanentSources&,const StableId& actor,const ActorFoundationRuleRef&,const WorkPracticeContribution&,GrowthCapabilityRevisionWrite&);
Status visit_growth_permanent_contributions(CodecVisitor&,GrowthPermanentState&);
Outcome<BoundaryValue> encode_growth_permanent_contributions(const GrowthPermanentState&);
Outcome<GrowthPermanentState> decode_growth_permanent_contributions(const BoundaryValue&);
Status emit_growth_permanent_contributions_projection(ProjectionWriter&,std::string_view,const GrowthPermanentState&);
}

namespace hanmo::resident {
Status visit_growth_permanent_contributions_v2(CodecVisitor&,GrowthPermanentState&);
Outcome<BoundaryValue> encode_growth_permanent_contributions_v2(const GrowthPermanentState&);
Outcome<GrowthPermanentState> decode_growth_permanent_contributions_v2(const BoundaryValue&);
Status emit_growth_permanent_contributions_v2_projection(ProjectionWriter&,std::string_view,const GrowthPermanentState&);
}
