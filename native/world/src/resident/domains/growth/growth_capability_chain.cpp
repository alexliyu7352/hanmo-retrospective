#include "resident/domains/actor_foundation/condition_capacity.hpp"
#include "growth_capability_chain.hpp"
#include "growth_model.hpp"
#include "growth_trade_grant_read.hpp"
#include "growth_trade_grant_inputs.hpp"
#include <algorithm>
#include "study_lesson_capability.hpp"
#include "study_adjustment_time2.hpp"
#include "growth_action_time_read.hpp"
#include "study_permanent_contribution_read.hpp"
#include "resident/domains/actor_foundation/created_origin.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include <limits>
#include <type_traits>
namespace hanmo::resident {
bool same_growth_capability_change(const GrowthCapabilityChangeRef& a,const GrowthCapabilityChangeRef& b){if(a.index()!=b.index())return false;return std::visit([&](const auto& x)->bool{using T=std::decay_t<decltype(x)>;const auto& y=std::get<T>(b);if constexpr(std::is_same_v<T,InitialConditionSeedRef>)return x.seed_key==y.seed_key;else if constexpr(std::is_same_v<T,ConditionReceiptRefV2>)return x.result_id==y.result_id;else if constexpr(std::is_same_v<T,AttributeMilestoneRef>)return x.milestone_result_id==y.milestone_result_id;else{if(x.index()!=y.index())return false;return std::visit([&](const auto& p){using P=std::decay_t<decltype(p)>;const auto& q=std::get<P>(y);if constexpr(std::is_same_v<P,TrainingResultRef>||std::is_same_v<P,GrowthTradeGrantRef>)return p.action_id==q.action_id;else if constexpr(std::is_same_v<P,StudyLessonResultRef>||std::is_same_v<P,StudyPerformanceAdjustmentRef>)return p.action_id==q.action_id&&p.lesson_ordinal==q.lesson_ordinal;else if constexpr(std::is_same_v<P,GroupLessonResultRef>)return p.course_id==q.course_id&&p.period_ordinal==q.period_ordinal;else if constexpr(std::is_same_v<P,PracticeAwardRef>)return p.award_id==q.award_id;else return p.milestone_result_id==q.milestone_result_id;},x);}},a);}
namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> value){if(!value.ok)throw Failure{std::move(value.error)};return std::move(value.value);}
struct Source {const StableId* actor=nullptr;WorldMoment at;const std::optional<GrowthCapabilityApplication>* application=nullptr;bool changes=false;const ConditionReceiptV2* condition=nullptr;};
void selected(const GrowthPermanentSources& s,const GrowthState& growth){const auto& body=s.condition;if(body.growth!=&growth||body.permanent_growth!=&s.state||body.permanent_growth_catalog!=&s.catalog||body.work_practice!=&s.work||!body.growth_catalog||!body.growth_catalog->complete_capability_application_chain||!body.growth_catalog->complete_exact_rest_mouth_sources)fail("GrowthCapabilityChainSelectionRequired");}
Source actual(const GrowthPermanentSources& s,const GrowthState& growth,const GrowthCapabilityChangeRef& ref){const auto& body=s.condition;
 if(const auto* seed=std::get_if<InitialConditionSeedRef>(&ref)){const auto* row=body.state.actor_start_context.initial_condition_seeds.find(seed->seed_key);if(!row||row->seed_key!=seed->seed_key)fail("GrowthCapabilityInitialSourceRequired",seed->seed_key);auto original=take(read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,row->actor_id,body.actual_now,body.born_sources));if(original.condition!=row||!original.origin||original.origin->initial_condition_seed_ref.seed_key!=seed->seed_key)fail("GrowthCapabilityInitialSourceRequired",seed->seed_key);return {&row->actor_id,original.origin->created_at.exact_moment,nullptr,false};}
 if(const auto* condition=std::get_if<ConditionReceiptRefV2>(&ref)){const auto* row=take(read_character_condition_receipt_provenance(body,*condition));return {&row->actor_id,row->effective_at.exact_moment,&row->capability_application,true,row};}
 if(const auto* milestone=std::get_if<AttributeMilestoneRef>(&ref)){const auto* row=take(read_verified_attribute_milestone(s,*milestone));return {&row->actor_id,row->applied_at.exact_moment,&row->capability_application,true};}
 const auto& original=std::get<LifeGrowthContributionRef>(ref);
 if(const auto* trade=std::get_if<GrowthTradeGrantRef>(&original)){if(!body.shared)fail("GrowthTradeGrantSourcesRequired",trade->action_id);auto row=take(read_growth_trade_grant_v1(body.common,growth,*body.growth_catalog,*body.shared,*trade,body.actual_now));return {&row.grant->actor_id,row.terminal->ended_at,&row.grant->capability_application,row.grant->actual_xp_gain>0};}
 if(const auto* lesson=std::get_if<StudyLessonResultRef>(&original)){auto row=take(read_study_lesson_permanent_contribution(body,growth,*lesson));return {&row.fact->terms.student_id,row.effective_at,&row.lesson->segment->capability_application,row.xp_gained>0};}
 if(const auto* adjustment=std::get_if<StudyPerformanceAdjustmentRef>(&original)){auto row=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,*adjustment,body.actual_now));return {&row.source.fact->terms.student_id,row.source.exact->settled_at,&row.source.exact->capability_application,row.xp_gained>0};}
 if(const auto* practice=std::get_if<PracticeAwardRef>(&original)){const auto* row=take(read_verified_practice_award(s,*practice));bool changes=false;for(const auto& skill:row->skill_rows)changes=changes||skill.actual_xp_gain>0;return {&row->actor_id,row->applied_at.exact_moment,&row->capability_application,changes};}
 if(const auto* training=std::get_if<TrainingResultRef>(&original)){const auto* raw=growth.facts.find(training->action_id);const auto* fact=raw?std::get_if<TrainingFact>(raw):nullptr;if(!fact||fact->phase!="completed"||!body.elapsed||!body.scale||!body.shared||!body.action_registrations)fail("GrowthCapabilityTrainingResultRequired",training->action_id);const ActionTimeRegistration* reg=nullptr;for(const auto& r:*body.action_registrations)if(r.owner_domain=="growth"&&r.action_kind==fact->action_kind)reg=&r;if(!reg)fail("GrowthCapabilityTrainingRegistrationRequired",training->action_id);GrowthActionTimeRead read(body.common,growth,*body.growth_catalog,*body.shared,*body.elapsed,*body.scale,body.catalog.identity,body.actual_now,{},body.interaction,body.interaction_catalog);auto owner=take(read.read_action_time_owner(*reg,training->action_id));if(!owner.ended_source)fail("GrowthCapabilityTrainingTerminalRequired",training->action_id);auto at=take(read.read_action_time_source(*owner.ended_source));return {&fact->training.student_id,std::move(at),&fact->capability_application,fact->success&&fact->training.mastery_after!=fact->training.mastery_before};}
 fail("GrowthCapabilityOriginalSourceReaderRequired");
}
// Intrinsic predecessor traversal only. No exported reader treats this as
// source validation: hot writes validate their own actual result; cold closure
// independently validates every enumerated original result and reverse cover.
Source stored(const GrowthPermanentSources& s,const GrowthState& growth,const GrowthCapabilityChangeRef& ref){
 if(std::holds_alternative<InitialConditionSeedRef>(ref))return actual(s,growth,ref);
 const auto& body=s.condition;
 if(const auto* r=std::get_if<ConditionReceiptRefV2>(&ref)){const auto* row=body.state.character_condition_extensions.applied_receipts.find(r->result_id);if(!row||row->result_id!=r->result_id||row->effective_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("GrowthCapabilityConditionSourceRequired",r->result_id);return {&row->actor_id,row->effective_at.exact_moment,&row->capability_application,true,row};}
 if(const auto* r=std::get_if<AttributeMilestoneRef>(&ref)){const auto* row=s.state.milestones.find(r->milestone_result_id);if(!row||row->milestone_result_id!=r->milestone_result_id||row->applied_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("AttributeMilestoneMissing",r->milestone_result_id);return {&row->actor_id,row->applied_at.exact_moment,&row->capability_application,true};}
 const auto& original=std::get<LifeGrowthContributionRef>(ref);
 if(std::holds_alternative<GrowthTradeGrantRef>(original))return actual(s,growth,ref);
 if(const auto* r=std::get_if<PracticeAwardRef>(&original)){const auto* row=s.state.practice_awards.find(r->award_id);if(!row||row->award_id!=r->award_id||row->applied_at.kind!=ActorFoundationAt::Kind::exact_moment)fail("PracticeAwardMissing",r->award_id);bool changed=false;for(const auto& skill:row->skill_rows)changed=changed||skill.actual_xp_gain>0;return {&row->actor_id,row->applied_at.exact_moment,&row->capability_application,changed};}
 if(const auto* r=std::get_if<TrainingResultRef>(&original)){const auto* raw=growth.facts.find(r->action_id);const auto* row=raw?std::get_if<TrainingFact>(raw):nullptr;if(!row||row->phase!="completed")fail("GrowthCapabilityTrainingResultRequired",r->action_id);const LifeFixedServiceBoundary* terminal=nullptr;for(const auto& b:growth.fixed_service_boundaries.rows())if(b.second.action_ref==r->action_id&&(b.second.kind==LifeFixedBoundaryKind::completed||b.second.kind==LifeFixedBoundaryKind::stopped)){if(terminal)fail("GrowthCapabilityTrainingTerminalRequired",r->action_id);terminal=&b.second;}if(!terminal)fail("GrowthCapabilityTrainingTerminalRequired",r->action_id);return {&row->training.student_id,terminal->occurred_at,&row->capability_application,row->success&&row->training.mastery_after!=row->training.mastery_before};}
 const StableId* id=nullptr;Integer ordinal=0;bool adjusted=false;if(const auto* r=std::get_if<StudyLessonResultRef>(&original)){id=&r->action_id;ordinal=r->lesson_ordinal;}else if(const auto* r=std::get_if<StudyPerformanceAdjustmentRef>(&original)){id=&r->action_id;ordinal=r->lesson_ordinal;adjusted=true;}else fail("GrowthCapabilityOriginalSourceReaderRequired");const auto* raw=growth.facts.find(*id);const auto* row=raw?std::get_if<StudyFact>(raw):nullptr;if(!row||ordinal<1||ordinal>static_cast<Integer>(row->lessons.size())||!row->lessons[static_cast<std::size_t>(ordinal-1)].segment)fail("GrowthCapabilityStudySourceRequired",*id);const auto& segment=*row->lessons[static_cast<std::size_t>(ordinal-1)].segment;if(adjusted){if(!segment.adjustment||!segment.adjustment->exact_binding)fail("GrowthCapabilityAdjustmentSourceRequired",*id);return {&row->terms.student_id,segment.adjustment->exact_binding->settled_at,&segment.adjustment->exact_binding->capability_application,segment.adjustment->xp_gained>0};}const LessonCapabilitySource* lesson=nullptr;for(const auto& l:growth.lesson_capabilities.lesson_capability_sources)if(l.action_id==*id&&l.lesson_ordinal==ordinal){if(lesson)fail("DuplicateStudyLessonCapability",*id);lesson=&l;}if(!lesson)fail("GrowthCapabilityStudySourceRequired",*id);return {&row->terms.student_id,lesson->provided_to,&segment.capability_application,segment.xp_gained>0};
}
template<class F>void originals(const GrowthPermanentSources& s,const GrowthState& growth,F visit){
 for(const auto& row:s.condition.state.character_condition_extensions.applied_receipts.rows())visit(GrowthCapabilityChangeRef{ConditionReceiptRefV2{row.first}});
 for(const auto& row:growth.facts.rows()){if(const auto* training=std::get_if<TrainingFact>(&row.second)){if(training->phase=="completed")visit(GrowthCapabilityChangeRef{LifeGrowthContributionRef{TrainingResultRef{row.first}}});}
 else if(const auto* study=std::get_if<StudyFact>(&row.second)){if(!study->study_service_binding){if(!study->lessons.empty())fail("GrowthCapabilityLegacyContributionUnsupported",row.first);continue;}for(std::size_t i=0;i<study->lessons.size();++i){visit(GrowthCapabilityChangeRef{LifeGrowthContributionRef{StudyLessonResultRef{row.first,static_cast<Integer>(i+1)}}});if(study->lessons[i].segment&&study->lessons[i].segment->adjustment)visit(GrowthCapabilityChangeRef{LifeGrowthContributionRef{StudyPerformanceAdjustmentRef{row.first,static_cast<Integer>(i+1)}}});}}}
 for(const auto& row:growth.trade_grants.rows())visit(GrowthCapabilityChangeRef{LifeGrowthContributionRef{GrowthTradeGrantRef{row.first}}});
 for(const auto& row:s.state.practice_awards.rows())visit(GrowthCapabilityChangeRef{LifeGrowthContributionRef{PracticeAwardRef{row.first}}});for(const auto& row:s.state.milestones.rows())visit(GrowthCapabilityChangeRef{AttributeMilestoneRef{row.first}});
}
void link(const GrowthPermanentSources& s,const GrowthState& growth,const GrowthCapabilityChangeRef& ref,const Source& row){
 if(!row.actor||!row.application||!row.changes||!row.application->has_value())fail("GrowthCapabilityApplicationRequired");const auto& application=**row.application;
 if(application.revision_before<0||application.revision_before==std::numeric_limits<Integer>::max()||(application.revision_before==0)!=std::holds_alternative<InitialConditionSeedRef>(application.previous)||same_growth_capability_change(ref,application.previous))fail("GrowthCapabilityPredecessorInvalid",*row.actor);
 auto previous=stored(s,growth,application.previous);if(!previous.actor||*previous.actor!=*row.actor||take(compare_world_moments(previous.at,row.at))>0)fail("GrowthCapabilityPredecessorSourceMismatch",*row.actor);
 if(application.revision_before){if(!previous.changes||!previous.application||!previous.application->has_value()||(**previous.application).revision_before!=application.revision_before-1)fail("GrowthCapabilityPredecessorRevisionMismatch",*row.actor);}
 if(row.condition){
  // The original per-actor body chain is an independent causal witness. A
  // same-M capability tail may not reverse two genuine Condition writes.
  const GrowthCapabilityChangeRef* walk=&application.previous;Integer remaining=application.revision_before;bool condition_found=false;
  while(remaining){auto earlier=stored(s,growth,*walk);if(!earlier.actor||*earlier.actor!=*row.actor||!earlier.application||!earlier.application->has_value()||(**earlier.application).revision_before!=remaining-1)fail("GrowthCapabilityPredecessorRevisionMismatch",*row.actor);if(earlier.condition){if(earlier.condition->condition_revision_after!=row.condition->condition_revision_before)fail("GrowthCapabilityConditionOrderMismatch",*row.actor);condition_found=true;break;}walk=&(**earlier.application).previous;--remaining;}
  if(!condition_found&&row.condition->condition_revision_before!=0)fail("GrowthCapabilityConditionPredecessorMissing",*row.actor);
  const auto* work=std::get_if<WorkOrStudyConditionCause>(&row.condition->cause);std::optional<LifeGrowthContributionRef> own_growth;
  if(work&&work->kind==WorkOrStudyConditionCause::Kind::study_lesson){const auto* raw=growth.facts.find(work->action_id);const auto* fact=raw?std::get_if<StudyFact>(raw):nullptr;if(!fact)fail("GrowthCapabilityOriginalLessonResultRequired",work->action_id);if(fact->terms.student_id==*row.actor)own_growth=StudyLessonResultRef{work->action_id,work->ordinal};}
  else if(work&&work->kind==WorkOrStudyConditionCause::Kind::training_completion){const auto* raw=growth.facts.find(work->action_id);const auto* fact=raw?std::get_if<TrainingFact>(raw):nullptr;if(!fact)fail("GrowthCapabilityOriginalTrainingResultRequired",work->action_id);if(fact->training.student_id==*row.actor)own_growth=TrainingResultRef{work->action_id};}
  if(own_growth){const GrowthCapabilityChangeRef expected=*own_growth;const auto net=stored(s,growth,expected);if(net.changes){bool found=false;walk=&application.previous;remaining=application.revision_before;while(remaining){if(same_growth_capability_change(*walk,expected)){found=true;break;}auto earlier=stored(s,growth,*walk);if(!earlier.application||!earlier.application->has_value()||(**earlier.application).revision_before!=remaining-1)fail("GrowthCapabilityPredecessorRevisionMismatch",*row.actor);walk=&(**earlier.application).previous;--remaining;}if(!found)fail("GrowthCapabilityOriginalGrowthBeforeConditionRequired",*row.actor);}}
 }
}

// Cross-check the immutable XP receipt against both original causal witnesses.
// This is stable validation only: never re-enter qualification or mutate a grant.
Integer commerce_net(const GrowthPermanentSources& s,const GrowthState& growth,
                     const GrowthCapabilityChangeRef& ref) {
 const auto* original=std::get_if<LifeGrowthContributionRef>(&ref);
 if(!original)return 0;
 const auto& body=s.condition;
 if(const auto* r=std::get_if<GrowthTradeGrantRef>(original))
  return take(read_growth_trade_grant_v1(body.common,growth,*body.growth_catalog,*body.shared,*r,body.actual_now)).grant->actual_xp_gain;
 if(const auto* r=std::get_if<StudyLessonResultRef>(original)){
  auto value=take(read_study_lesson_permanent_contribution(body,growth,*r));
  return value.fact->terms.skill_id=="commerce"?value.xp_gained:0;
 }
 if(const auto* r=std::get_if<StudyPerformanceAdjustmentRef>(original)){
  auto value=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,*r,body.actual_now));
  return value.source.fact->terms.skill_id=="commerce"?value.xp_gained:0;
 }
 if(const auto* r=std::get_if<PracticeAwardRef>(original)){
  const auto* value=take(read_verified_practice_award(s,*r));
  for(const auto& skill:value->skill_rows)if(skill.skill_id=="commerce")return skill.actual_xp_gain;
 }
 // Training changes personal mastery, not Common commerce. Other accepted
 // non-Life sources change condition/attributes, not this skill balance.
 return 0;
}
std::size_t source_position(const CommonStore& common,const StableId& id){
 const auto& order=common.state().facts.order();const auto found=std::find(order.begin(),order.end(),id);
 if(found==order.end())fail("GrowthTradeGrantOriginalCutRequired",id);
 return static_cast<std::size_t>(found-order.begin());
}
const ActorConditionHead& trade_body_cut(const GrowthPermanentSources& s,
    const GrowthState& growth,const GrowthTradeGrant& grant,const WorldMoment& at){
 const LifeFixedServiceBoundary* terminal=nullptr;
 for(const auto& row:growth.fixed_service_boundaries.rows())
  if(row.second.action_ref==grant.action_id&&row.second.kind==LifeFixedBoundaryKind::completed){
   const auto& value=row.second;
   if(terminal||row.first!=value.boundary_id||value.action_kind!="trade"||value.source_schema!=1||
      take(compare_world_moments(value.occurred_at,at))||!value.original_outcome_ref||
      value.original_outcome_ref->kind!=LifeFixedOutcomeRef::Kind::life_activity||
      value.original_outcome_ref->id!=grant.action_id)
    fail("GrowthTradeGrantOriginalBodyCutRequired",grant.action_id);
   terminal=&value;
  }
 if(!terminal)fail("GrowthTradeGrantOriginalBodyCutRequired",grant.action_id);
 const ActorConditionHead* cut=nullptr;
 for(const auto& actor:terminal->input_snapshot.actors)if(actor.actor_id==grant.actor_id)
  for(const auto& frame:actor.capability_frames){const auto& h=frame.condition_head_snapshot;
   if(h.actor_id!=grant.actor_id||(cut&&(cut->condition_revision!=h.condition_revision||
      cut->initial_condition_seed_ref.seed_key!=h.initial_condition_seed_ref.seed_key||
      bool(cut->latest_receipt_ref)!=bool(h.latest_receipt_ref)||
      (h.latest_receipt_ref&&cut->latest_receipt_ref->result_id!=h.latest_receipt_ref->result_id))))
    fail("GrowthTradeGrantOriginalBodyCutMismatch",grant.action_id);
   cut=&h;
  }
 if(!cut)fail("GrowthTradeGrantOriginalBodyCutRequired",grant.action_id);
 take(read_character_condition_at_head(s.condition,*cut,at));
 return *cut;
}
bool before_trade_cut(const GrowthPermanentSources& s,const GrowthState& growth,
    const GrowthCapabilityChangeRef& ref,const Source& source,
    const GrowthTradeGrant& grant,const WorldMoment& at,const ActorConditionHead& body_cut,
    std::size_t publication_position){
 const auto temporal=take(compare_world_moments(source.at,at));
 if(temporal)return temporal<0;
 const auto* original=std::get_if<LifeGrowthContributionRef>(&ref);
 if(!original)fail("GrowthTradeGrantCommerceSourceRequired",grant.action_id);
 const auto& body=s.condition;
 if(const auto* r=std::get_if<GrowthTradeGrantRef>(original)){
  if(r->action_id==grant.action_id)return false;
  const auto* published=take(read_growth_trade_grant_publication_v1(body,growth,*r));
  return source_position(body.common,published->publication_id)<publication_position;
 }
 if(const auto* r=std::get_if<PracticeAwardRef>(original)){
  const auto* value=take(read_verified_practice_award(s,*r));
  return source_position(body.common,value->original_fact_ref.id)<publication_position;
 }
 if(const auto* r=std::get_if<StudyPerformanceAdjustmentRef>(original)){
  auto value=take(read_study_adjustment_permanent_contribution(body.common,growth,*body.growth_catalog,*r,body.actual_now));
  if(!value.source.exact->original_fact_prefix||value.source.exact->original_fact_prefix->original_fact_prefix_size<0)
   fail("GrowthTradeGrantAdjustmentCutRequired",r->action_id);
  return publication_position>=static_cast<std::size_t>(value.source.exact->original_fact_prefix->original_fact_prefix_size);
 }
 if(const auto* r=std::get_if<StudyLessonResultRef>(original)){
  auto value=take(read_study_lesson_permanent_contribution(body,growth,*r));
  const auto* condition=body.state.character_condition_extensions.applied_receipts.find(value.original_capability_source->student_condition_receipt_ref.result_id);
  if(!condition||condition->actor_id!=grant.actor_id)
   fail("GrowthTradeGrantStudyBodyCutRequired",r->action_id);
  return condition->condition_revision_after<=body_cut.condition_revision;
 }
 fail("GrowthTradeGrantCommerceSourceRequired",grant.action_id);
}
void validate_trade_xp_order(const GrowthPermanentSources& s,const GrowthState& growth,
                            const GrowthTradeGrantRef& ref){
 const auto& body=s.condition;
 if(!body.shared)fail("GrowthTradeGrantSourcesRequired",ref.action_id);
 const auto read=take(read_growth_trade_grant_v1(body.common,growth,*body.growth_catalog,*body.shared,ref,body.actual_now));
 const auto& grant=*read.grant;const auto& at=read.terminal->ended_at;
 const auto original=take(read_character_initial_values_v2(body.common,body.state,body.catalog,body.family_catalog,body.shared,grant.actor_id,body.actual_now,body.born_sources));
 if(!original.origin||!original.initial_skills)fail("GrowthTradeGrantOriginalInitialXPRequired",grant.action_id);
 const auto initial=growth_skill_xp(*original.initial_skills,"commerce");
 if(initial<0||initial>1200)fail("GrowthTradeGrantOriginalInitialXPRequired",grant.action_id);
 auto add=[&](Integer& total,Integer net){if(net<0||net>1200-total)fail("GrowthTradeGrantPredecessorNetInvalid",grant.action_id);total+=net;};
 // The real trade terminal froze this Condition head before its original XP
 // write. It supplies the same-M lesson cut even for a zero-net grant.
 const auto& body_cut=trade_body_cut(s,growth,grant,at);
 const auto* published=take(read_growth_trade_grant_publication_v1(body,growth,ref));
 const auto position=source_position(body.common,published->publication_id);
 Integer cut_xp=initial;
 originals(s,growth,[&](const GrowthCapabilityChangeRef& candidate){
  const auto source=actual(s,growth,candidate);
  if(!source.actor||*source.actor!=grant.actor_id)return;
  const auto net=commerce_net(s,growth,candidate);
  if(net&&before_trade_cut(s,growth,candidate,source,grant,at,body_cut,position))add(cut_xp,net);
 });
 if(cut_xp!=grant.xp_before)fail("GrowthTradeGrantActualBeforeXPMismatch",grant.action_id);
 if(!grant.actual_xp_gain){if(grant.capability_application)fail("GrowthCapabilityZeroNetApplied",grant.action_id);return;}
 if(!grant.capability_application)fail("GrowthCapabilityApplicationRequired",grant.action_id);
 Integer predecessor_xp=initial,remaining=grant.capability_application->revision_before;
 const auto* current=s.state.capability_revisions.find(grant.actor_id);
 if(!current||remaining<0||remaining>=current->revision)
  fail("GrowthCapabilityCurrentRevisionBehind",grant.actor_id);
 const GrowthCapabilityChangeRef* cursor=&grant.capability_application->previous;
 while(remaining){
  const auto previous=actual(s,growth,*cursor);
  if(!previous.actor||*previous.actor!=grant.actor_id||!previous.changes||
     !previous.application||!previous.application->has_value()||
     (**previous.application).revision_before!=remaining-1)
   fail("GrowthCapabilityPredecessorRevisionMismatch",grant.actor_id);
  const auto net=commerce_net(s,growth,*cursor);
  if(net>0&&!before_trade_cut(s,growth,*cursor,previous,grant,at,body_cut,position))
   fail("GrowthTradeGrantCapabilitySourceCutMismatch",grant.action_id);
  add(predecessor_xp,net);
  cursor=&(**previous.application).previous;--remaining;
 }
 const GrowthCapabilityChangeRef seed=original.origin->initial_condition_seed_ref;
 if(!same_growth_capability_change(*cursor,seed)||predecessor_xp!=grant.xp_before)
  fail("GrowthTradeGrantCapabilityXPOrderMismatch",grant.action_id);
}

std::optional<GrowthCapabilityApplication>& mutable_slot(AuthoredActorFoundationState& f,GrowthState& g,GrowthPermanentState& state,const GrowthCapabilityChangeRef& ref){
 if(const auto* r=std::get_if<ConditionReceiptRefV2>(&ref)){auto* row=f.character_condition_extensions.applied_receipts.find(r->result_id);if(!row)fail("GrowthCapabilityConditionSourceRequired",r->result_id);return row->capability_application;}
 if(const auto* r=std::get_if<AttributeMilestoneRef>(&ref)){auto* row=state.milestones.find(r->milestone_result_id);if(!row)fail("AttributeMilestoneMissing",r->milestone_result_id);return row->capability_application;}
 const auto* original=std::get_if<LifeGrowthContributionRef>(&ref);if(!original)fail("GrowthCapabilityInitialMutationDenied");
 if(const auto* r=std::get_if<GrowthTradeGrantRef>(original)){auto* row=g.trade_grants.find(r->action_id);if(!row||row->action_id!=r->action_id)fail("GrowthTradeGrantMissing",r->action_id);return row->capability_application;}
 if(const auto* r=std::get_if<PracticeAwardRef>(original)){auto* row=state.practice_awards.find(r->award_id);if(!row)fail("PracticeAwardMissing",r->award_id);return row->capability_application;}
 if(const auto* r=std::get_if<TrainingResultRef>(original)){auto* raw=g.facts.find(r->action_id);auto* row=raw?std::get_if<TrainingFact>(raw):nullptr;if(!row)fail("GrowthCapabilityTrainingResultRequired",r->action_id);return row->capability_application;}
 const StableId* action=nullptr;Integer ordinal=0;bool adjustment=false;if(const auto* r=std::get_if<StudyLessonResultRef>(original)){action=&r->action_id;ordinal=r->lesson_ordinal;}else if(const auto* r=std::get_if<StudyPerformanceAdjustmentRef>(original)){action=&r->action_id;ordinal=r->lesson_ordinal;adjustment=true;}else fail("GrowthCapabilityOriginalSourceReaderRequired");auto* raw=g.facts.find(*action);auto* row=raw?std::get_if<StudyFact>(raw):nullptr;if(!row||ordinal<1||ordinal>static_cast<Integer>(row->lessons.size())||!row->lessons[static_cast<std::size_t>(ordinal-1)].segment)fail("GrowthCapabilityStudySourceRequired",*action);auto& segment=*row->lessons[static_cast<std::size_t>(ordinal-1)].segment;if(!adjustment)return segment.capability_application;if(!segment.adjustment||!segment.adjustment->exact_binding)fail("GrowthCapabilityAdjustmentSourceRequired",*action);return segment.adjustment->exact_binding->capability_application;
}
void record(CommonStore& common,AuthoredActorFoundationState& foundation,GrowthState& growth,GrowthPermanentState& state,const GrowthPermanentSources& sources,const StableId& actor,const GrowthCapabilityChangeRef& ref,const WorldMoment& at){selected(sources,growth);if(&sources.condition.common!=&common||&sources.condition.state!=&foundation||&sources.state!=&state||take(compare_world_moments(at,sources.condition.actual_now)))fail("GrowthCapabilityActualCandidateRequired",actor);auto source=actual(sources,growth,ref);if(!source.actor||*source.actor!=actor||take(compare_world_moments(source.at,at)))fail("GrowthCapabilityActualApplySourceRequired",actor);auto& slot=mutable_slot(foundation,growth,state,ref);if(!source.changes){if(slot)fail("GrowthCapabilityZeroNetApplied",actor);return;}auto* current=state.capability_revisions.find(actor);if(!current||current->actor_id!=actor)fail("GrowthCapabilityInitialRevisionRequired",actor);if(slot){link(sources,growth,ref,source);if(current->revision<=slot->revision_before)fail("GrowthCapabilityCurrentRevisionBehind",actor);const GrowthCapabilityChangeRef* walk=&current->last_change;Integer revision=current->revision;while(revision>slot->revision_before+1){const auto previous=stored(sources,growth,*walk);link(sources,growth,*walk,previous);if(!previous.actor||*previous.actor!=actor||(**previous.application).revision_before!=revision-1)fail("GrowthCapabilityFinalChainMismatch",actor);walk=&(**previous.application).previous;--revision;}if(!same_growth_capability_change(*walk,ref))fail("GrowthCapabilityReplayOutsideActualChain",actor);return;}if(current->revision<0||current->revision==std::numeric_limits<Integer>::max())fail("GrowthCapabilityRevisionOverflow",actor);slot=GrowthCapabilityApplication{current->revision,current->last_change};link(sources,growth,ref,source);current->revision+=1;current->last_change=ref;}
}
Status initialize_growth_capability_revision_for_actor_in_candidate(CommonStore& common,GrowthPermanentState& state,const CharacterConditionSources& body,const StableId& actor){try{if(&body.common!=&common||body.permanent_growth!=&state||!body.growth_catalog||!body.growth_catalog->complete_capability_application_chain)fail("GrowthCapabilityChainSelectionRequired",actor);auto initial=take(read_character_initial_values_v2(common,body.state,body.catalog,body.family_catalog,body.shared,actor,body.actual_now,body.born_sources));const auto* head=body.state.character_condition_extensions.actor_condition_heads.find(actor);if(!initial.origin||!head||head->condition_revision||head->latest_receipt_ref||state.capability_revisions.find(actor))fail("GrowthCapabilityOriginalInitializationRequired",actor);const auto* current=common.state().actors.find(actor);if(!current||!initial.initial_skills||!body.shared)fail("GrowthCapabilityOriginalInitializationRequired",actor);for(const auto& skill:growth_skill_ids(body.shared->extended_skills))if(growth_skill_xp(current->skills,skill)!=growth_skill_xp(*initial.initial_skills,skill))fail("GrowthCapabilityInitialSkillsChanged",actor);if(body.growth)for(const auto& row:body.growth->trade_grants.rows())if(row.second.actor_id==actor)fail("GrowthCapabilityLateInitialization",actor);for(const auto& row:state.practice_awards.rows())if(row.second.actor_id==actor)fail("GrowthCapabilityLateInitialization",actor);for(const auto& row:state.milestones.rows())if(row.second.actor_id==actor)fail("GrowthCapabilityLateInitialization",actor);GrowthCapabilityRevision revision{actor,0,initial.origin->initial_condition_seed_ref};if(!state.capability_revisions.insert(actor,std::move(revision)))fail("GrowthCapabilityDuplicateActor",actor);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_growth_capability_chain(const GrowthPermanentSources& sources,const GrowthState& growth){try{selected(sources,growth);take(validate_complete_character_conditions(sources.condition));
 for(const auto& origin:sources.condition.state.character_origin_records.origins.rows())if(!sources.state.capability_revisions.find(origin.first))fail("GrowthCapabilityActorCoverageMissing",origin.first);
 for(const auto& row:sources.state.capability_revisions.rows()){const auto& current=row.second;if(row.first!=current.actor_id||current.revision<0)fail("GrowthCapabilityRevisionInvalid",row.first);auto initial=take(read_character_initial_values_v2(sources.condition.common,sources.condition.state,sources.condition.catalog,sources.condition.family_catalog,sources.condition.shared,row.first,sources.condition.actual_now,sources.condition.born_sources));if(!initial.origin)fail("GrowthCapabilityOriginRequired",row.first);const GrowthCapabilityChangeRef seed=initial.origin->initial_condition_seed_ref;const GrowthCapabilityChangeRef* cursor=&current.last_change;Integer remaining=current.revision;
  while(remaining){const auto source=actual(sources,growth,*cursor);link(sources,growth,*cursor,source);const auto& application=**source.application;if(*source.actor!=row.first||application.revision_before!=remaining-1)fail("GrowthCapabilityFinalChainMismatch",row.first);cursor=&application.previous;--remaining;}
  if(!same_growth_capability_change(*cursor,seed))fail("GrowthCapabilityInitialChainMismatch",row.first);
  Integer count=0;originals(sources,growth,[&](const GrowthCapabilityChangeRef& ref){const auto source=actual(sources,growth,ref);if(!source.actor||*source.actor!=row.first)return;if(const auto* original=std::get_if<LifeGrowthContributionRef>(&ref))if(const auto* trade=std::get_if<GrowthTradeGrantRef>(original))validate_trade_xp_order(sources,growth,*trade);if(!source.changes){if(source.application&&source.application->has_value())fail("GrowthCapabilityZeroNetApplied",row.first);return;}link(sources,growth,ref,source);++count;const auto revision=(**source.application).revision_before+1;if(revision>current.revision)fail("GrowthCapabilityCurrentRevisionBehind",row.first);const GrowthCapabilityChangeRef* walk=&current.last_change;Integer n=current.revision;while(n>revision){auto previous=actual(sources,growth,*walk);if(!previous.application||!previous.application->has_value())fail("GrowthCapabilityApplicationRequired",row.first);walk=&(**previous.application).previous;--n;}if(!same_growth_capability_change(*walk,ref))fail("GrowthCapabilityReverseCoverageMismatch",row.first);});if(count!=current.revision)fail("GrowthCapabilitySourceCountMismatch",row.first);
 }
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
bool NativeGrowthCapabilityRevisionWrite::bound_to(const CommonStore& common,const GrowthPermanentState& state)const noexcept{return &common==&common_&&&state==&state_&&&sources_.condition.common==&common_&&&sources_.condition.state==&foundation_&&sources_.condition.growth==&growth_&&&sources_.state==&state_;}
Status NativeGrowthCapabilityRevisionWrite::record_actual_growth_change(const StableId& actor,const LifeGrowthContributionRef& source,const WorldMoment& at){try{GrowthCapabilityChangeRef ref=source;if(const auto* milestone=std::get_if<AttributeMilestoneRef>(&source))ref=*milestone;record(common_,foundation_,growth_,state_,sources_,actor,ref,at);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status NativeGrowthCapabilityRevisionWrite::record_actual_condition_change(const BoundActorConditionWrite& source){try{const auto* receipt=take(read_actual_actor_condition_write(common_,foundation_,source));if(condition_purpose_capacity_selected(sources_.condition.catalog))take(record_actual_condition_capacity_changes_in_candidate(common_,foundation_,sources_.condition,source));record(common_,foundation_,growth_,state_,sources_,receipt->actor_id,GrowthCapabilityChangeRef{source.receipt_ref},receipt->effective_at.exact_moment);return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
}

namespace hanmo::resident {
Status record_growth_fixed_capability_result_in_candidate(const CharacterConditionSources& body,const GrowthState& growth,const GrowthFixedStepPermit& gate,const GrowthFixedStudyApplication& applied,GrowthCapabilityRevisionWrite& writer){try{
 if(!body.growth_catalog||!body.growth_catalog->complete_capability_application_chain||body.growth!=&growth||!body.permanent_growth||!writer.bound_to(body.common,*body.permanent_growth)||take(compare_world_moments(gate.at(),body.actual_now)))fail("GrowthCapabilityActualFixedOwnerRequired",gate.action_id());
 const LifeFixedServiceBoundary* last=nullptr;for(const auto& r:growth.fixed_service_boundaries.rows())if(r.second.action_ref==gate.action_id()&&(!last||r.second.boundary_ordinal>last->boundary_ordinal))last=&r.second;
 if(!last||last->kind==LifeFixedBoundaryKind::started||take(compare_world_moments(last->occurred_at,gate.at())))fail("GrowthCapabilityOriginalFixedResultRequired",gate.action_id());
 if(const auto* raw=growth.facts.find(gate.action_id())){
  if(const auto* fact=std::get_if<StudyFact>(raw)){if(gate.ordinal()<=static_cast<Integer>(fact->lessons.size())){if(gate.ordinal()!=static_cast<Integer>(fact->lessons.size()))fail("GrowthCapabilityOriginalLessonOrdinalRequired",gate.action_id());take(writer.record_actual_growth_change(fact->terms.student_id,LifeGrowthContributionRef{StudyLessonResultRef{gate.action_id(),gate.ordinal()}},gate.at()));}else if(fact->phase!="completed"||fact->success||gate.ordinal()!=static_cast<Integer>(fact->lessons.size())+1)fail("GrowthCapabilityOriginalLessonResultRequired",gate.action_id());}
  else if(const auto* fact=std::get_if<TrainingFact>(raw)){if(!gate.final_gate()||fact->phase!="completed")fail("GrowthCapabilityOriginalTrainingResultRequired",gate.action_id());take(writer.record_actual_growth_change(fact->training.student_id,LifeGrowthContributionRef{TrainingResultRef{gate.action_id()}},gate.at()));}
 }else if(const auto* life=growth.life_activities.find(gate.action_id());life&&life->view.kind==LifeActivityKind::trade){if(!gate.final_gate()||!applied.result.completed||life->view.phase!=LifeActivityPhase::completed||!life->view.success)fail("GrowthCapabilityOriginalTradeResultRequired",gate.action_id());take(writer.record_actual_growth_change(life->view.actor_id,LifeGrowthContributionRef{GrowthTradeGrantRef{gate.action_id()}},gate.at()));}
 Integer previous=0;for(const auto& actual:applied.actual_condition_writes){if(actual.capture_sequence<=previous)fail("GrowthCapabilityOriginalConditionOrderRequired",actual.receipt_ref.result_id);previous=actual.capture_sequence;take(writer.record_actual_condition_change(actual));}
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status record_study_adjustment_capability_for_commit_in_candidate(const CharacterConditionSources& body,const GrowthState& growth,const CommitResult& result,GrowthCapabilityRevisionWrite& writer){try{
 if(!body.growth_catalog||!body.growth_catalog->complete_capability_application_chain||body.growth!=&growth||!body.permanent_growth||!writer.bound_to(body.common,*body.permanent_growth)||!result.ok)fail("GrowthCapabilityActualAdjustmentOwnerRequired",result.result_id);
 Integer count=0;for(const auto& r:growth.facts.rows())if(const auto* fact=std::get_if<StudyFact>(&r.second))for(const auto& lesson:fact->lessons)if(lesson.segment&&lesson.segment->adjustment&&lesson.segment->adjustment->exact_binding&&lesson.segment->adjustment->exact_binding->result_id==result.result_id)++count;
 if(count>6)fail("StudyAdjustmentActualApplicationOrderMismatch",result.result_id);
 for(Integer ordinal=1;ordinal<=count;++ordinal){bool found=false;for(const auto& r:growth.facts.rows())if(const auto* fact=std::get_if<StudyFact>(&r.second))for(std::size_t n=0;n<fact->lessons.size();++n){const auto& lesson=fact->lessons[n];if(!lesson.segment||!lesson.segment->adjustment||!lesson.segment->adjustment->exact_binding)continue;const auto& binding=*lesson.segment->adjustment->exact_binding;if(binding.result_id!=result.result_id||binding.application_ordinal!=ordinal)continue;if(found||take(compare_world_moments(binding.settled_at,body.actual_now)))fail("StudyAdjustmentActualApplicationOrderMismatch",result.result_id);found=true;auto actual=take(read_study_adjustment_time2_source(body.common,growth,*body.growth_catalog,{r.first,static_cast<Integer>(n+1)},body.actual_now));const auto* cached=body.common.state().results.find(binding.actor_id+":"+std::to_string(binding.request_seq));if(!cached||cached->get()!=&result||actual.exact!=&binding)fail("GrowthCapabilityActualCachedAdjustmentRequired",result.result_id);take(writer.record_actual_growth_change(fact->terms.student_id,LifeGrowthContributionRef{StudyPerformanceAdjustmentRef{r.first,static_cast<Integer>(n+1)}},binding.settled_at));}if(!found)fail("StudyAdjustmentActualApplicationOrderMismatch",result.result_id);}
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
}
