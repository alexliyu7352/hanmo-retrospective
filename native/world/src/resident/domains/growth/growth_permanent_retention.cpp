#include "growth_permanent_retention.hpp"
#include "growth_model.hpp"
#include "growth_trade_grant_read.hpp"
#include "study_permanent_contribution_read.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include <array>
#include <type_traits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T> s){if(!s.ok)throw Failure{std::move(s.error)};return std::move(s.value);}
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
SourceRef head(const CommonStore& common,const StableId& id,const char* domain,const char* kind){const auto* row=common.state().facts.find(id);if(!row||row->id!=id||row->domain!=domain||row->kind!=kind)fail("GrowthPermanentRetentionActualHeadRequired",id);return {row->domain,row->kind,row->id};}
SourceRef change_source(const GrowthPermanentSources& s,const GrowthState& g,const GrowthCapabilityChangeRef& ref){
 if(const auto* r=std::get_if<InitialConditionSeedRef>(&ref))return initial_condition_seed_source(*r);
 if(const auto* r=std::get_if<ConditionReceiptRefV2>(&ref))return character_condition_receipt_source(*r);
 if(const auto* r=std::get_if<AttributeMilestoneRef>(&ref))return head(s.condition.common,r->milestone_result_id,"growth","attribute_milestone");
 return std::visit([&](const auto& r)->SourceRef{using T=std::decay_t<decltype(r)>;if constexpr(std::is_same_v<T,TrainingResultRef>)return head(s.condition.common,r.action_id,"cultivation","training");else if constexpr(std::is_same_v<T,StudyLessonResultRef>||std::is_same_v<T,StudyPerformanceAdjustmentRef>)return head(s.condition.common,r.action_id,"cultivation","skill_study");else if constexpr(std::is_same_v<T,PracticeAwardRef>)return head(s.condition.common,r.award_id,"growth","practice_award");else if constexpr(std::is_same_v<T,GrowthTradeGrantRef>){if(!s.condition.growth_catalog||!s.condition.shared)fail("GrowthTradeGrantSourcesRequired",r.action_id);return take(read_growth_trade_grant_v1(s.condition.common,g,*s.condition.growth_catalog,*s.condition.shared,r,s.condition.actual_now)).grant->original_trade_result_ref;}else {fail("GrowthPermanentRetentionSourceReaderRequired");}},std::get<LifeGrowthContributionRef>(ref));
}
bool same(const SourcePin& a,const SourcePin& b){return a.consumer.domain==b.consumer.domain&&a.consumer.kind==b.consumer.kind&&a.consumer.id==b.consumer.id&&a.consumer.slot==b.consumer.slot&&same_source_ref(a.source,b.source)&&a.reason==b.reason;}
StableId lesson_id(const StableId& action,std::size_t ordinal){return std::to_string(action.size())+":"+action+":"+std::to_string(ordinal);}
}
RequiredSourceConsumerSlot growth_permanent_consumer_slot(GrowthPermanentConsumerSlot slot){switch(slot){
 case GrowthPermanentConsumerSlot::capability_current:return {"growth","capability_revision","last_change"};
 case GrowthPermanentConsumerSlot::condition_predecessor:return {"actor_foundation","condition_receipt","capability_predecessor"};
 case GrowthPermanentConsumerSlot::training_predecessor:return {"growth","training_result","capability_predecessor"};
 case GrowthPermanentConsumerSlot::study_lesson_predecessor:return {"growth","study_lesson","capability_predecessor"};
 case GrowthPermanentConsumerSlot::study_adjustment_predecessor:return {"growth","study_adjustment","capability_predecessor"};
 case GrowthPermanentConsumerSlot::practice_predecessor:return {"growth","practice_award","capability_predecessor"};
 case GrowthPermanentConsumerSlot::milestone_predecessor:return {"growth","attribute_milestone","capability_predecessor"};
 case GrowthPermanentConsumerSlot::practice_source:return {"growth","practice_award","original_work"};
 case GrowthPermanentConsumerSlot::milestone_source:return {"growth","attribute_milestone","original_work"};
 case GrowthPermanentConsumerSlot::owned_result:return {"growth","permanent_actor_result","unique_outcome"};
 }return {};
}
Status visit_growth_permanent_source_pins(const GrowthPermanentSources& s,const GrowthState& growth,GrowthPermanentConsumerSlot slot,GrowthPermanentPinSink& sink){try{
 take(validate_growth_permanent_results(s));take(validate_growth_capability_chain(s,growth));const auto spec=growth_permanent_consumer_slot(slot);if(spec.domain.empty())fail("UnknownGrowthPermanentConsumerSlot");
 auto emit=[&](const StableId& id,const SourceRef& ref,SourcePinReason reason=SourcePinReason::dependent_state){take(sink.actual_pin({{spec.domain,spec.kind,id,spec.slot},ref,reason}));};
 auto previous=[&](const StableId& id,const std::optional<GrowthCapabilityApplication>& application){if(application)emit(id,change_source(s,growth,application->previous));};
 auto work=[&](const StableId& id,const StableId& actor,const PracticeSource& original){const auto& source=std::get<WorkPracticeContribution>(original);const auto read=take(s.work.read_actual_work_contribution(actor,source,s.condition.actual_now));emit(id,read.original_result,SourcePinReason::unique_outcome);for(const auto& period:read.actual_participation_refs)emit(id,head(s.condition.common,period,"tasks","work_period_receipt"),SourcePinReason::unique_outcome);};
 if(slot==GrowthPermanentConsumerSlot::capability_current){for(const auto& r:s.state.capability_revisions.rows())emit(r.first,change_source(s,growth,r.second.last_change));}
 else if(slot==GrowthPermanentConsumerSlot::condition_predecessor){for(const auto& r:s.condition.state.character_condition_extensions.applied_receipts.rows())previous(r.first,r.second.capability_application);}
 else if(slot==GrowthPermanentConsumerSlot::training_predecessor||slot==GrowthPermanentConsumerSlot::study_lesson_predecessor||slot==GrowthPermanentConsumerSlot::study_adjustment_predecessor){for(const auto& r:growth.facts.rows()){if(const auto* fact=std::get_if<TrainingFact>(&r.second)){if(slot==GrowthPermanentConsumerSlot::training_predecessor)previous(r.first,fact->capability_application);}else if(const auto* fact=std::get_if<StudyFact>(&r.second))for(std::size_t n=0;n<fact->lessons.size();++n)if(fact->lessons[n].segment){const auto& segment=*fact->lessons[n].segment;if(slot==GrowthPermanentConsumerSlot::study_lesson_predecessor)previous(lesson_id(r.first,n+1),segment.capability_application);else if(slot==GrowthPermanentConsumerSlot::study_adjustment_predecessor&&segment.adjustment&&segment.adjustment->exact_binding)previous(lesson_id(r.first,n+1),segment.adjustment->exact_binding->capability_application);}}}
 else if(slot==GrowthPermanentConsumerSlot::practice_predecessor){for(const auto& r:s.state.practice_awards.rows())previous(r.first,r.second.capability_application);}
 else if(slot==GrowthPermanentConsumerSlot::milestone_predecessor){for(const auto& r:s.state.milestones.rows())previous(r.first,r.second.capability_application);}
 else if(slot==GrowthPermanentConsumerSlot::practice_source){for(const auto& r:s.state.practice_awards.rows())work(r.first,r.second.actor_id,r.second.original_source);}
 else if(slot==GrowthPermanentConsumerSlot::milestone_source){for(const auto& r:s.state.milestones.rows())work(r.first,r.second.actor_id,r.second.original_outcome_ref);}
 else if(slot==GrowthPermanentConsumerSlot::owned_result){for(const auto& r:growth.trade_grants.rows()){if(!s.condition.growth_catalog||!s.condition.shared)fail("GrowthTradeGrantSourcesRequired",r.first);auto source=take(read_growth_trade_grant_v1(s.condition.common,growth,*s.condition.growth_catalog,*s.condition.shared,{r.first},s.condition.actual_now));emit(r.first,source.grant->original_trade_result_ref,SourcePinReason::unique_outcome);previous(r.first,source.grant->capability_application);}for(const auto& r:s.state.practice_awards.rows()){if(!s.condition.state.character_origin_records.origins.find(r.second.actor_id))fail("PracticeAwardActorOriginRequired",r.first);emit(r.first,r.second.original_fact_ref,SourcePinReason::unique_outcome);}for(const auto& r:s.state.milestones.rows()){if(!s.condition.state.character_origin_records.origins.find(r.second.actor_id))fail("MilestoneActorOriginRequired",r.first);emit(r.first,head(s.condition.common,r.first,"growth","attribute_milestone"),SourcePinReason::unique_outcome);}}
 return common_success();}catch(Failure& e){return {false,{},std::move(e.error)};}}
Status validate_growth_permanent_source_pin(const GrowthPermanentSources& s,const GrowthState& g,GrowthPermanentConsumerSlot slot,const SourcePin& pin){struct Find final:GrowthPermanentPinSink{const SourcePin& target;std::size_t count=0;explicit Find(const SourcePin& p):target(p){}Status actual_pin(const SourcePin& p)override{if(same(p,target))++count;return common_success();}}find(pin);auto valid=visit_growth_permanent_source_pins(s,g,slot,find);if(!valid.ok)return valid;return find.count==1?common_success():common_failure("GrowthPermanentPinNotActual",pin.consumer.id);}
Outcome<bool> growth_permanent_source_owner_may_release(const GrowthPermanentSources& s,const GrowthState& g,const SourceRef& ref){try{if(ref.domain!="growth")fail("GrowthPermanentSourceOwnerMismatch",ref.id);if(ref.kind=="practice_award")take(read_verified_practice_award(s,{ref.id}));else if(ref.kind=="attribute_milestone")take(read_verified_attribute_milestone(s,{ref.id}));else fail("GrowthPermanentSourceOwnerMismatch",ref.id);struct Find final:GrowthPermanentPinSink{const SourceRef& target;bool found=false;explicit Find(const SourceRef& r):target(r){}Status actual_pin(const SourcePin& p)override{if(same_source_ref(p.source,target))found=true;return common_success();}}find(ref);take(visit_growth_permanent_source_pins(s,g,GrowthPermanentConsumerSlot::owned_result,find));return {true,!find.found,{}};}catch(Failure& e){return {false,false,std::move(e.error)};}}
}
