#include "growth_skill_retention.hpp"
#include "study_permanent_contribution_read.hpp"
#include "growth_permanent_contributions.hpp"
#include "growth_trade_grant_read.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
namespace hanmo::resident {
Status visit_growth_skill_qualification_source_pins(const CharacterConditionSources& body,const GrowthState& growth,const GrowthSkillQualificationV2& value,const ConsumerRef& consumer,GrowthSkillQualificationPinSink& sink){
 if(body.growth!=&growth||value.coverage_kind!=GrowthSkillQualificationV2::Coverage::complete||value.origin_ref!=value.actor_id)return common_failure("GrowthSkillRetentionOriginalSourceRequired",value.actor_id);
 auto initial=read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,value.actor_id,body.actual_now,body.born_sources);if(!initial.ok)return {false,{},initial.error};
 auto seed=initial_condition_seed_source(initial.value.origin->initial_condition_seed_ref);auto schema=read_initial_condition_seed_schema(body.catalog);if(!schema.ok)return {false,{},schema.error};auto verified=read_character_condition_source(body,seed,schema.value);if(!verified.ok)return {false,{},verified.error};
 auto valid=sink.actual_pin({consumer,seed,SourcePinReason::dependent_state});if(!valid.ok)return valid;
 for(const auto& contribution:value.contribution_refs){const StableId* action=nullptr;
  if(const auto* ref=std::get_if<StudyLessonResultRef>(&contribution)){auto source=read_study_lesson_permanent_contribution(body,growth,*ref);if(!source.ok)return {false,{},source.error};if(source.value.fact->terms.student_id!=value.actor_id||source.value.fact->terms.skill_id!=value.skill_id)return common_failure("GrowthSkillRetentionContributionMismatch",ref->action_id);action=&ref->action_id;}
  else if(const auto* ref=std::get_if<StudyPerformanceAdjustmentRef>(&contribution)){if(!body.growth_catalog)return common_failure("GrowthSkillRetentionCatalogRequired");auto source=read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,*ref,body.actual_now);if(!source.ok)return {false,{},source.error};if(source.value.source.fact->terms.student_id!=value.actor_id||source.value.source.fact->terms.skill_id!=value.skill_id)return common_failure("GrowthSkillRetentionContributionMismatch",ref->action_id);action=&ref->action_id;}
  else if(const auto* ref=std::get_if<PracticeAwardSkillContributionRef>(&contribution)){if(!body.permanent_growth||!body.permanent_growth_catalog||!body.work_practice)return common_failure("GrowthPracticeInputSourcesRequired",ref->award_id);GrowthPermanentSources sources{body,*body.permanent_growth_catalog,*body.permanent_growth,*body.work_practice};auto actual=read_verified_practice_award(sources,{ref->award_id});if(!actual.ok)return {false,{},actual.error};const auto& receipt=*actual.value;bool found=false;for(const auto& row:receipt.skill_rows)if(row.skill_id==value.skill_id&&row.skill_id==ref->skill_id&&row.actual_xp_gain>0)found=true;if(receipt.actor_id!=value.actor_id||!found)return common_failure("GrowthSkillRetentionContributionMismatch",ref->award_id);valid=sink.actual_pin({consumer,receipt.original_fact_ref,SourcePinReason::dependent_state});if(!valid.ok)return valid;continue;}
  else if(const auto* ref=std::get_if<VerifiedLegacyTradeGrantRef>(&contribution)){if(!body.growth_catalog||!body.shared)return common_failure("GrowthTradeGrantSourcesRequired",ref->original_trade_result_id);auto source=read_growth_trade_grant_v1(body.common,growth,*body.growth_catalog,*body.shared,{ref->original_trade_result_id},body.actual_now);if(!source.ok)return {false,{},source.error};const auto& grant=*source.value.grant;if(grant.actor_id!=value.actor_id||grant.skill_id!=value.skill_id||grant.actual_xp_gain<=0)return common_failure("GrowthSkillRetentionContributionMismatch",grant.action_id);valid=sink.actual_pin({consumer,grant.original_trade_result_ref,SourcePinReason::dependent_state});if(!valid.ok)return valid;continue;}
  else return common_failure("GrowthSkillRetentionContributionReaderRequired",value.actor_id);
  const auto* original=body.common.state().facts.find(*action);if(!original||original->id!=*action||original->domain!="cultivation"||original->kind!="skill_study")return common_failure("GrowthSkillRetentionOriginalHeadRequired",*action);
  valid=sink.actual_pin({consumer,{original->domain,original->kind,original->id},SourcePinReason::dependent_state});if(!valid.ok)return valid;
 }
 return common_success();
}
}
